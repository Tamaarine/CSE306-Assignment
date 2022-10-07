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

static void * server_thread(void * arg) {
    /* This server thread will be accepting the connections */
    struct sockaddr_in address; /* Used for setting up the server accept socket */
    int server_port = *((int *)arg); /* Get the port that's passed into the thread */
    int sockfd; /* Socket used for listening */
    int opt = 1; /* Enable boolean options */
    int accepted_socket; /* Socket that's accepted */
    int address_len = sizeof(address);
    
    printf("I'm the server thread and im going to listen on %d\n", server_port);
    
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
        
    pthread_exit(NULL);
}

int main(int argc, char ** argv) {
    int connect_socket; /* Used for connecting to the other process */
    struct sockaddr_in address_out;
    // char * inputBuf; /* Used for storing the user input */
    // int address_len = sizeof(address);
    
    /* Used for creating the thread.
     * The thread acts as the server to accept incoming connections
     * The main thread will act as the message sender
     */
    pthread_t thread_id;
    
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
    pthread_create(&thread_id, NULL, server_thread, (void *) listen_port);
    
    // inputBuf = malloc(sizeof(char) * MAX_SIZE);
    
    // inputBuf = fgets(inputBuf, MAX_SIZE, stdin);
    
    // printf("The user entered -%s-\n", inputBuf);
    
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
    
    pthread_join(thread_id, NULL);
    
    return 0;
}