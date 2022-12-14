#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/userfaultfd.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>

#define MAX_SIZE 50
#define errExit(str) do { \
    perror(str); \
    exit(EXIT_FAILURE); \
} while(0)

static int first_process = -1;      /* Indicate if this process is first/not */
static int accepted_socket;         /* Global socket to talk to other client */
static int connect_socket;          /* Global socket to talk to server */
static int page_size;               /* How big a page is */
static unsigned long len;           /* numpage * page_size */
static char * mmap_addr;            /* global mmap address returned */
static long uffd;                   /* userfaultfd file descriptor */
static char * msi_array;            /* An array of chars. The size of the array is = num pages */

#define MODIFIED 1
#define SHARED 2
#define INVALID 3
#define MODIFIED_S "Modified"
#define SHARED_S "Shared"
#define INVALID_S "Invalid"

/* Struct to be send over socket */
struct init_info {
    char * mmap_addr;
    unsigned long len;
};

/*
 * 'F': For fetching specified page
 * 'I': For invalidating specified page
 */
struct msg_request {
    char request_type;           /* Hold the request type */
    int which_page;              /* Which page is it requesting */
};


/* Used to abstract away the request sending */
static void sent_request(char request_type, int which_page) {
    int bytes_write;
    struct msg_request request;
    request.request_type = request_type;
    request.which_page = which_page;
    
    if ((bytes_write = write(connect_socket, &request, sizeof(request))) < 0)
        errExit("Writing error");
}


/* This function is used to establish which process is first */
static void * handshake(void * arg) {
    int * pid_buffer = malloc(sizeof(pid_t)); /* Read other process' pid*/
    int current_pid = getpid();               /* Get current process' pid */
    
    /* Socket stuff */
    int sockfd;                         /* Socket used for listening */
    struct sockaddr_in address;         /* Used for setting up the server accept socket */
    int opt = 1;                        /* Enable boolean option */
    int server_port = *((int *)arg);    /* Get server port from arg */
    int address_len = sizeof(address);
    
    int bytes_read;                     /* Used for read */
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errExit("Socket creation error");
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                &opt, sizeof(opt)))
        errExit("Setsockopt failed");
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server_port);
    
    /*Bind the sockfd to the address struct  */
    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0)
        errExit("Bind failed");
    
    if (listen(sockfd, 3))
        errExit("Listen failed");
    
    /* Wait for connection */
    if ((accepted_socket = accept(sockfd, (struct sockaddr *)&address, 
            (socklen_t *)&address_len)) < 0)
        errExit("Accept failed");
    
    printf("Accepted connection\n");
    
    /* Read from the pid to compare pid */
    if ((bytes_read = read(accepted_socket, pid_buffer, sizeof(pid_t))) < 0)
        errExit("Reading error");
    
    printf("Current pid: %d other pid: %d\n", current_pid, *pid_buffer);
    first_process = current_pid < *pid_buffer ? 1 : 0;
    
    pthread_exit(NULL);    
}


static void * fault_handler_thread(void * arg) {
    struct uffd_msg msg;            /* Data read from userfaultfd */
    struct uffdio_copy uffdio_copy; /* Struct used for resolving page fault */
    ssize_t nread;                  /* Used for poll() */
    long uffd = (long)arg;          /* Retrieve uffd from thread arg */
    static char *page = NULL;       /* Page used to copy */
    int page_faulted;               /* Used to store which page faulted */
    int bytes_read;
    
    /* This page will be used to resolve the page fault. handle by kernel for its page fault */
    if (page == NULL) {
        page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
            errExit("mmap");
    }
    
    for (;;) {
        struct pollfd pollfd;
        int nready;
        
        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("Poll failed");
        
        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0)
            errExit("EOF on userfaultfd!");
        else if (nread == -1)
            errExit("Read failed");
        
        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }
        
        printf("  [x]  PAGEFAULT\n");
        page_faulted = ((char *)msg.arg.pagefault.address - mmap_addr) / page_size;
        
        memset(page, 0, page_size); /* Clear out the page to 0 */
        
        /* Page is invalid, go ask the other process */
        if (msi_array[page_faulted] == INVALID) {
            char response;
            
            /* Fetch request */
            sent_request('F', page_faulted);

            if ((bytes_read = read(connect_socket, &response, sizeof(response))) < 0)
                errExit("Reading error");
            
            if (response == '0') {
                /* If response is 0 means their page is also invalid */
                /* Set it to be shared because it is going to be the default page */
                msi_array[page_faulted] = SHARED;
            }
            else if (response == '1') {
                /* If response is 1 means they are giving back some pages */
                /* Read the page send by the server thread */
                if ((bytes_read = read(connect_socket, page, page_size)) < 0)
                    errExit("Reading error");
                msi_array[page_faulted] = SHARED;
            }
            
            /* Then the page content will be copied to resolve the page fault */
        }
        uffdio_copy.src = (unsigned long) page;
        uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
            ~(page_size - 1);
        uffdio_copy.len = page_size;
        uffdio_copy.mode = 0;
        uffdio_copy.copy = 0;
        
        /* Copy the allocated page to the faulted page */
        if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
            errExit("ioctl-UFFDIO_COPY");
    }
}


/* Used for second process only to receive handshake msg */
static void * second_process_receive(void * arg) {
    /* This thread will be handle the reading from socket for 2nd process */    
    int bytes_read;
    pthread_t thread_id;
    
    struct init_info info;          /* Used for storing the bytes read from first process */
    if ((bytes_read = read(accepted_socket, &info, sizeof(struct init_info))) < 0)
        errExit("Read error");
    else if (bytes_read == 0) {
        printf("The connection was resetted by peer\n");
        exit(EXIT_FAILURE);
    }
    
    /* Do the mmap for the second process using first process' mmap_addr */
    len = info.len;
    mmap_addr = mmap(info.mmap_addr, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_addr == MAP_FAILED)
            errExit("mmap failed");
    
    msi_array = malloc(sizeof(char) * (len / page_size)); /* Allocate a char per page for MSI protocol */
    for (char * ptr = msi_array; ptr < msi_array + (len / page_size); ptr++)
        *ptr = INVALID;
    
    printf("-----------------------------------------------------\n");
    printf("Second process\nmmap_address: %p size: %ld\n", mmap_addr, len);
    
    /* Register the userfaultfd here for first process */
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;
    
    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit("Userfaultfd error");
    
    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit("itctl-UFFDIO_API error");
    
    uffdio_register.range.start = (unsigned long) mmap_addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("itctl-UFFDIO_REGISTER error");
    
    pthread_create(&thread_id, NULL, fault_handler_thread, (void *)uffd);
    
    /* Carry out the rest of the operation */
    pthread_exit(NULL);
}


/* A third thread solely for receiving messages */
static void * server_thread(void * arg) {
    int bytes_write;
    int bytes_read;
    struct msg_request request; /* For storing message */
    
    while (1) {
        if ((bytes_read = read(accepted_socket, &request, sizeof(request))) < 0)
            errExit("Reading error");
        else if (bytes_read == 0) {
            printf("Connection resetted\n");
            exit(EXIT_FAILURE);
        }
        
        if (request.request_type == 'F') {
            if (msi_array[request.which_page] == INVALID) {
                /* My page is also invalid */
                if ((bytes_write = write(accepted_socket, "0", 1)) < 0)
                    errExit("Writing error");
                /* Set it to be shared */
                msi_array[request.which_page] = SHARED;
            }
            else {
                /* If the requested page is not invalid, just send it back */
                if ((bytes_write = write(accepted_socket, "1", 1)) < 0)
                    errExit("Writing error");
                
                char * address_loc = mmap_addr + (request.which_page * page_size);

                if ((bytes_write = write(accepted_socket, address_loc, page_size)) < 0)
                    errExit("Writing error");
                /* And mark it as shared because both are the same now */
                msi_array[request.which_page] = SHARED;
            }
        }
        else if (request.request_type == 'I') {
            /* Just invalidate the page that it is told to */
            char * address_loc = mmap_addr + (request.which_page * page_size);
            if (madvise(address_loc, page_size, MADV_DONTNEED))
                errExit("Madvise failed");
            msi_array[request.which_page] = INVALID;
            
        }
    }
    
    pthread_exit(NULL);
}


int main(int argc, char ** argv) {
    struct sockaddr_in address_out;     /* Address of other process */
    int bytes_write;                    /* Used for write() */
    int pages;                          /* Pages input from user */
    char pages_raw[50];                 /* Buffer for storing fgets for # pages */
    char * fgets_ret;                   /* fgets_ret */
    
    pthread_t thread_id;
    pthread_t fault_thread_id;      /* fault handler id*/
    
    int pid_buffer;                 /* Storing current pid */
    
    if (argc != 3) {
        printf("You will need to specify 2 arguments!\n");
        exit(EXIT_FAILURE);
    }
    
    /* Parse the first number and second number */
    errno = 0;
    int * listen_port = malloc(sizeof(int));
    *listen_port = strtol(argv[1], NULL, 0);
    if (errno)
        errExit("Converting number failed");
    
    errno = 0;
    int * send_port = malloc(sizeof(int));
    *send_port = strtol(argv[2], NULL, 0);
    if (errno)
        errExit("Converting number failed");
    
    if (listen_port == send_port) {
        printf("Cannot be listening and sending to same port\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %d sending on port %d\n", *listen_port, *send_port);
    
    page_size = sysconf(_SC_PAGE_SIZE);
    
    /* Do the handshake that listen to establish who is first/second */
    pthread_create(&thread_id, NULL, handshake, (void *) listen_port);
    
    /* Socket to connect to other thread */
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    address_out.sin_family = AF_INET;
    address_out.sin_port = htons(*send_port);
    
    if(inet_pton(AF_INET, "127.0.0.1", &address_out.sin_addr) <= 0)
        errExit("inet_pton failed");
    
    /* Client connect here to the other port */
    while(connect(connect_socket, (struct sockaddr *)&address_out, sizeof(address_out)) < 0) {
        // perror("Connection failed retrying");
        sleep(1);
    }
    
    /* Get current pid */
    pid_buffer = getpid();
    
    /* Sending the current pid to the other thread. Will receive by handshake thread */
    if ((bytes_write = write(connect_socket, &pid_buffer, sizeof(pid_t))) < 0)
        errExit("Writing error");
    
    /* Wait for handshake to complete */
    pthread_join(thread_id, NULL);
    
    /* Start the thread to receive the message if you're not the 1st process */    
    if (!first_process) {
        pthread_create(&thread_id, NULL, second_process_receive, NULL);
        pthread_join(thread_id, NULL);
        pthread_create(&thread_id, NULL, server_thread, NULL);
    }
    else {
        printf("> How many pages would you like to allocate (greater than 0)? ");
        
        /* Big enough to store a pointer and an integer */
        struct init_info info;
        
        if ((fgets_ret = fgets(pages_raw, MAX_SIZE, stdin)) < 0)
            errExit("fgets failed");
        
        errno = 0;
        pages = strtol(pages_raw, NULL, 10);
        if (errno)
            errExit("Converting number failed");
        
        len = page_size * pages;
        
        mmap_addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mmap_addr == MAP_FAILED)
            errExit("mmap failed");
        
        msi_array = malloc(sizeof(char) * pages); /* Allocate a char per page for MSI protocol */
        for (char * ptr = msi_array; ptr < msi_array + (len / page_size); ptr++)
            *ptr = INVALID;
        
        printf("-----------------------------------------------------\n");
        printf("First process\nmmap_address: %p size: %ld\n", mmap_addr, len);
        
        /* Write both mmap_address + len into struct */
        info.mmap_addr = mmap_addr;
        info.len = len;
        
        /* Send over as the first message after handshake*/
        if ((bytes_write = write(connect_socket, &info, sizeof(struct init_info))) < 0)
            errExit("Writing error");
        
        /* Register the userfaultfd here for first process */
        struct uffdio_api uffdio_api;
        struct uffdio_register uffdio_register;
        long uffd;
        
        uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
        if (uffd == -1)
            errExit("Userfaultfd error");
        
        uffdio_api.api = UFFD_API;
        uffdio_api.features = 0;
        if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
            errExit("itctl-UFFDIO_API error");
        
        uffdio_register.range.start = (unsigned long) mmap_addr;
        uffdio_register.range.len = len;
        uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
        if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
            errExit("itctl-UFFDIO_REGISTER error");
        
        pthread_create(&fault_thread_id, NULL, fault_handler_thread, (void *)uffd);
        pthread_create(&thread_id, NULL, server_thread, NULL);
    }
    
    printf("-----------------------------------------------------\n");
    
    /* Used for the while loop for reading userinput */
    char op[MAX_SIZE];              /* Storing user input for operation */
    char which_page_raw[MAX_SIZE];  /* Storing page input */
    int which_page;                 /* Parsed of which_page_raw */
    char * msg = malloc(sizeof(char) * page_size);      /* Msg buffer for writing to page */
    
    int max_page = (int)(len / page_size);
    
    while (1) {
        printf("> Which command should I run? (r:read, w:write, v:view msi array): ");
        if ((fgets_ret = fgets(op, MAX_SIZE, stdin)) < 0)
            errExit("fgets failed");
        else if (fgets_ret == 0) {
            break;
        }
        op[strcspn(op, "\n")] = 0;
        
        if (op[0] != 'r' && op[0] != 'w' && op[0] != 'v') {
            printf("Invalid operation specified (r:read, w:write, v:view msi array)\n");
            continue;
        }
        else if (op[0] == 'w') {
            printf("> Type your new message: ");
            if ((fgets_ret = fgets(msg, page_size, stdin)) < 0)
                errExit("fgets failed");
            else if (fgets_ret == 0) {
                break;
            }
            msg[strcspn(msg, "\n")] = 0;
        }
        else if (op[0] == 'v') {
            /* Print out all MSI array */
            for (int i=0;i < max_page;i++) {
                switch (msi_array[i]) {
                    case INVALID:
                    printf("  [*]  Page %d:\n%s\n", i, INVALID_S);
                    break;
                    
                    case SHARED:
                    printf("  [*]  Page %d:\n%s\n", i, SHARED_S);
                    break;
                    
                    case MODIFIED:
                    printf("  [*]  Page %d:\n%s\n", i, MODIFIED_S);
                    break;
                    
                    default:
                    break;
                }
            }
            continue;
        }
        
        printf("> For which page? (0-%d, or -1 for all): ", max_page - 1);
        if ((fgets_ret = fgets(which_page_raw, MAX_SIZE, stdin)) < 0)
            errExit("fgets failed");
        else if (fgets_ret == 0)
            break;
        
        errno = 0;
        which_page = strtol(which_page_raw, NULL, 10);
        if (errno)
            errExit("Converting number failed");
        if (which_page < -1 || which_page >= max_page) {
            printf("Invalid page number specified (0-%d, or -1 for all)\n", max_page - 1);
            continue;
        }
        
        if (which_page == -1) {
            /* Print out everything */
            for (int i=0;i<max_page;i++) {
                char * address_loc = mmap_addr + (i * page_size);
                
                if (op[0] == 'r') {
                    char page_buffer[page_size];
                    strncpy(page_buffer, address_loc, page_size);  /* Will trigger page-fault on initial read */
                    printf("  [*]  Page %d:\n%s\n", i, address_loc);
                }
                else {
                    strncpy(address_loc, msg, page_size);
                    msi_array[i] = MODIFIED;
                    printf("  [*]  Page %d:\n%s\n", i, address_loc);
                    
                    sent_request('I', i);
                }
            }
        }
        else {
            char * address_loc = mmap_addr + (which_page * page_size);
            
            if (op[0] == 'r') {
                char page_buffer[page_size];
                strncpy(page_buffer, address_loc, page_size);  /* Will trigger page-fault on initial read */
                printf("  [*]  Page %d:\n%s\n", which_page, page_buffer);
            }
            else {
                strncpy(address_loc, msg, page_size);
                msi_array[which_page] = MODIFIED;
                printf("  [*]  Page %d:\n%s\n", which_page, address_loc);
                
                sent_request('I', which_page);
            }
        }
    }
    return 0;
}