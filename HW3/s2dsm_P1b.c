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

#define MAX_SIZE 50

/* Use to indicate whether this process is the first process or not */
static int first_process = -1;
static int accepted_socket; /* Global socket to talk to other process */
static int page_size;
static unsigned long len;
static char * mmap_addr;

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
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server_port);
    
    /*Bind the sockfd to the address struct  */
    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(sockfd, 3)) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    /* Wait for connection */
    if ((accepted_socket = accept(sockfd, (struct sockaddr *)&address, 
            (socklen_t *)&address_len)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Accepted connection\n");
    
    /* Read from the pid to compare pid */
    if ((bytes_read = read(accepted_socket, pid_buffer, sizeof(pid_t))) < 0) {
        perror("Reading error");
        exit(EXIT_FAILURE);
    }
    
    printf("Current pid: %d other pid: %d\n", current_pid, *pid_buffer);
    if (current_pid < *pid_buffer)
        first_process = 1;
    else
        first_process = 0;
    
    pthread_exit(NULL);    
}


/* Used for second process only to receive handshake msg */
static void * second_process_receive(void * arg) {
    /* This thread will be handle the reading from socket for 2nd process */    
    int bytes_read;
    
    struct init_info info;
    if ((bytes_read = read(accepted_socket, &info, sizeof(struct init_info)) < 0)) {
        perror("Read error");
        exit(EXIT_FAILURE);
    }
    
    /* Do the mmap for the second process using first process' mmap_addr */
    len = info.len;
    mmap_addr = mmap(info.mmap_addr, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    printf("-----------------------------------------------------\n");
    printf("Second process\nmmap_address: %p size: %ld\n", mmap_addr, len);
    
    /* Carry out the rest of the operation */
    pthread_exit(NULL);
}


int main(int argc, char ** argv) {
    int connect_socket; /* Used for connecting to the other process */
    struct sockaddr_in address_out;
    // char * inputBuf; /* Used for storing the user input */
    int bytes_write;
    int pages; /* Pages input from user */
    int items_read; /* Used for scanf */
    
    /* Used for creating the thread.
     * The thread acts as the server to accept incoming connections
     * The main thread will act as the message sender
     */
    pthread_t thread_id;
    
    int pid_buffer;
    
    if (argc != 3) {
        printf("You will need to specify 2 arguments!\n");
        exit(EXIT_FAILURE);
    }
    
    /* Parse the first number and second number */
    errno = 0;
    int * listen_port = malloc(sizeof(int));
    *listen_port = strtol(argv[1], NULL, 0);
    if (errno) {
        perror("Converting number failed");
        exit(EXIT_FAILURE);
    }
    
    errno = 0;
    int * send_port = malloc(sizeof(int));
    *send_port = strtol(argv[2], NULL, 0);
    if (errno) {
        perror("Converting number failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %d sending on port %d\n", *listen_port, *send_port);
    
    page_size = sysconf(_SC_PAGE_SIZE);
    
    /* Need to spin up a thread to handle the connection */    
    pthread_create(&thread_id, NULL, handshake, (void *) listen_port);
    
    /* Then connect to the other server in the main thread */
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    address_out.sin_family = AF_INET;
    address_out.sin_port = htons(*send_port);
    
    if(inet_pton(AF_INET, "127.0.0.1", &address_out.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }
    
    /* Client connect here to the other port */
    while(connect(connect_socket, (struct sockaddr *)&address_out, sizeof(address_out)) < 0) {
        // perror("Connection failed retrying");
        sleep(1);
    }
    
    /* Get current pid */
    pid_buffer = getpid();
    
    /* Handshake doing */
    if ((bytes_write = write(connect_socket, &pid_buffer, sizeof(pid_t))) < 0) {
        perror("Writing error");
        exit(EXIT_FAILURE);
    }
    
    /* Wait for handshake to complete */
    pthread_join(thread_id, NULL);
    
    /* Start the thread to receive the message if you're not the 1st process */    
    if (!first_process)
        pthread_create(&thread_id, NULL, second_process_receive, NULL);
    else {
        printf("> How many pages would you like to allocate (greater than 0)? ");
        
        /* Big enough to store a pointer and an integer */
        struct init_info info;
        
        if ((items_read = scanf("%d", &pages) < 0)) {
            perror("Scanf error");
            exit(EXIT_FAILURE);
        }
        
        len = page_size * pages; 
        
        mmap_addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mmap_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }
        
        printf("-----------------------------------------------------\n");
        printf("First process\nmmap_address: %p size: %ld\n", mmap_addr, len);
        
        /* Write both mmap_address + len into struct */
        info.mmap_addr = mmap_addr;
        info.len = len;
        
        /* Send over as the first message after handshake*/
        if ((bytes_write = write(connect_socket, &info, sizeof(struct init_info))) < 0) {
            perror("Writing error");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Wait for reading 7 printing to be done by 2nd process thread */
    pthread_join(thread_id, NULL);
    
    return 0;
}