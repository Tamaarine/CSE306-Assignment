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

/* Use to indicate whether this process is the first process or not */
static int first_process = -1;
static int accepted_socket; /* Global socket to talk to other process */
static int page_size;
static unsigned long len;
static char * mmap_addr;
static long uffd;

/* Struct to be send over socket */
struct init_info {
    char * mmap_addr;
    unsigned long len;
};


/* This function is used to establish which process is first */
static void * handshake(void * arg) {
    int * pid_buffer = malloc(sizeof(pid_t)); /* Read other process' pid*/
    int current_pid = getpid();   /* Get current process' pid */
    
    /* Socket stuff */
    int sockfd; /* Socket used for listening */
    struct sockaddr_in address; /* Used for setting up the server accept socket */
    int opt = 1;
    int server_port = *((int *)arg);
    int address_len = sizeof(address);
    
    int bytes_read;
    
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
    if (current_pid < *pid_buffer)
        first_process = 1;
    else
        first_process = 0;
    
    pthread_exit(NULL);    
}


static void * fault_handler_thread(void * arg) {
    long uffd = (long)arg;
    
    for (;;) {
        struct pollfd pollfd;
        int nready;
        
        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("Poll failed");
        
        printf("\nfault_handler_thread():\n");
		printf("    poll() returns: nready = %d; "
                       "POLLIN = %d; POLLERR = %d\n", nready,
                       (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);
    }
}


/* Used for second process only to receive handshake msg */
static void * second_process_receive(void * arg) {
    /* This thread will be handle the reading from socket for 2nd process */    
    int bytes_read;
    pthread_t thread_id;
    
    struct init_info info;
    if ((bytes_read = read(accepted_socket, &info, sizeof(struct init_info))) < 0)
        errExit("Read error");
    else if (bytes_read == 0)
        errExit("The connection was resetted \n");
    
    /* Do the mmap for the second process using first process' mmap_addr */
    len = info.len;
    mmap_addr = mmap(info.mmap_addr, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
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


int main(int argc, char ** argv) {
    int connect_socket; /* Used for connecting to the other process */
    struct sockaddr_in address_out;
    // char * inputBuf; /* Used for storing the user input */
    int bytes_write;
    int pages; /* Pages input from user */
    char pages_raw[50];
    char * fgets_ret;
    
    /* Used for creating the thread.
     * The thread acts as the server to accept incoming connections
     * The main thread will act as the message sender
     */
    pthread_t thread_id;
    pthread_t fault_thread_id;
    
    int pid_buffer;
    
    if (argc != 3)
        errExit("You will need to specify 2 arguments!\n");
    
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
    
    printf("Listening on port %d sending on port %d\n", *listen_port, *send_port);
    
    page_size = sysconf(_SC_PAGE_SIZE);
    
    /* Need to spin up a thread to handle the connection */    
    pthread_create(&thread_id, NULL, handshake, (void *) listen_port);
    
    /* Then connect to the other server in the main thread */
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
    
    /* Handshake doing */
    if ((bytes_write = write(connect_socket, &pid_buffer, sizeof(pid_t))) < 0)
        errExit("Writing error");
    
    /* Wait for handshake to complete */
    pthread_join(thread_id, NULL);
    
    /* Start the thread to receive the message if you're not the 1st process */    
    if (!first_process)
        pthread_create(&thread_id, NULL, second_process_receive, NULL);
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
    }
    
    printf("-----------------------------------------------------\n");
    
    /* Used for the while loop for reading userinput */
    char op[MAX_SIZE];
    char which_page_raw[MAX_SIZE];
    int which_page;    
    
    /* Now the messages printed are synced we are begin while loop */
    while (1) {
        printf("> Which command should I run? (r:read, w:write): ");
        if ((fgets_ret = fgets(op, MAX_SIZE, stdin)) < 0)
            errExit("fgets failed");
        else if (fgets_ret == 0) {
            break;
        }
        op[strcspn(op, "\n")] = 0;
        
        printf("> For which page? (0-%d, or -1 for all): ", (int)(len / page_size));
        if ((fgets_ret = fgets(which_page_raw, MAX_SIZE, stdin)) < 0)
            errExit("fgets failed");
        else if (fgets_ret == 0) {
            break;
    }
    
        errno = 0;
        which_page = strtol(which_page_raw, NULL, 10);
        if (errno)
            errExit("Converting number failed");
        
        printf("user entered %s with page %d\n", op, which_page);
        
        if ((bytes_write = write(accepted_socket, op, MAX_SIZE)) < 0)
            errExit("Writing error");
        
        mmap_addr[page_size * which_page] = 69;
    }
    // pthread_join(thread_id, NULL);
    return 0;
}