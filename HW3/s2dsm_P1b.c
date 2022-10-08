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

#define MAX_SIZE 50

/* Use to indicate whether this process is the first process or not */
static int first_process = -1;
static int accepted_socket; /* Global socket to talk to other process */


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
    
    printf("Current %d the other %d\n", current_pid, *pid_buffer);
    if (current_pid < *pid_buffer)
        first_process = 1;
    else
        first_process = 0;
    
    pthread_exit(NULL);    
}


/* Used to handle the rest of the server reading logic */
static void * server_thread(void * arg) {
    /* This thread will be handle the reading from socket */    
    // char * buffer = malloc(sizeof(char) * MAX_SIZE); /* Used for storing messages from socket */
    int * page_buffer;
    int bytes_read;
    
    if (!first_process) {
        page_buffer = malloc(sizeof(int));
        if ((bytes_read = read(accepted_socket, page_buffer, sizeof(int)))) {
            printf("The first process said %d pages\n", *page_buffer);
        }
    }
    
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
    
    printf("Listening port is %d sending port is %d\n", *listen_port, *send_port);
    
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
    
    /* Start the server thread to handle reading from socket */
    pthread_create(&thread_id, NULL, server_thread, NULL);
    
    if (first_process) {
        printf("> How many pages would you like to allocate (greater than 0)? ");
        
        if ((items_read = scanf("%d", &pages) < 0)) {
            perror("Scanf error");
            exit(EXIT_FAILURE);
        }
        
        if ((bytes_write = write(connect_socket, &pages, sizeof(pages))) < 0) {
            perror("Writing error");
            exit(EXIT_FAILURE);
        }
    }
    
    pthread_join(thread_id, NULL);
    
    return 0;
}