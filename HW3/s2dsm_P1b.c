#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void server_logic(int fd) {
    
}

void client_logic(int fd) {
    
}

int main(int argc, char ** argv) {
    int connect_socket; /* Used for creating the two way socket connection */
	struct sockaddr_in address; /* Used for configuring the socket */
	struct sockaddr_in address_out; /* Used for connecting to the other socket */
    int opt = 1; /* Used in setsocketopt to enable boolean option */
    int address_len = sizeof(address);
    
    if (argc != 3) {
        printf("You will need to specify 2 arguments!\n");
        exit(EXIT_FAILURE);
    }
    
    /* Parse the first number and second number */
    errno = 0;
    int listen_port = strtol(argv[1], NULL, 0);
    if (errno) {
        printf("Converting number failed\n");
        exit(EXIT_FAILURE);
    }
    
    errno = 0;
    int send_port = strtol(argv[2], NULL, 0);
    if (errno) {
        printf("Converting number failed\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening port is %d sending port is %d\n", listen_port, send_port);
    
    /* Create the two way socket that we will be using
     * The communication domain will be AF_INET. It will be connecting
     * to the same network reaching a specified process port for communication
     */
    if ((connect_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        exit(EXIT_FAILURE);
    }
    
    /* Socket option setting to allow reuse of address and reuse of port */
    if (setsockopt(connect_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                &opt, sizeof(opt))) {
        printf("Setsocket failed\n");
        exit(EXIT_FAILURE);
    }
    
    /* address struct for the current socket.
     * Listen on all network interface for connection
     * The current socket is assigned the listening_port.
     */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(listen_port);
    
    /*Bind the connect_socket to the address struct  */
    if (bind(connect_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed %d\n", errno);
        exit(EXIT_FAILURE);
    }
    
    /* Configures the address that this socket is connecting to */
    address_out.sin_family = AF_INET;
    address_out.sin_port = htons(send_port);
    
    if (inet_pton(AF_INET, "127.0.0.1", &address_out.sin_addr) <= 0) {
        printf("Invalid address or error from inet_pton\n");
        exit(EXIT_FAILURE);
    }
    
    /* If this connect failed then that means the server doesn't exist
     * Then the first instance of this program will be declared as the server
     */
    if (connect(connect_socket, (struct sockaddr *)&address_out, sizeof(address_out)) < 0) {
        /* Make sure it error out due to server not existing */
        if (errno == ECONNREFUSED) {
            /* Reach here means server, set connect_socekt as server fd */
            if (listen(connect_socket, 3)) {
                printf("Listen failed\n");
                exit(EXIT_FAILURE);
            }
            
            int accepted_socket;
            
            /* Wait for connection */
            if ((accepted_socket = accept(connect_socket, (struct sockaddr *)&address, 
                    (socklen_t *)&address_len)) < 0) {
                printf("Accept failed %d\n", errno);
            }
            
            printf("Accepted connection\n");
            server_logic(accepted_socket);
            
            /* Close both the accepted socket and server socket */
            close(connect_socket);
            close(accepted_socket);
        }
        else {
            /* If other error then exit program */
            printf("Connection failed %d\n", errno);
            exit(EXIT_FAILURE);
        }
        
    }
    else {
        printf("Connected sucessfully to server!\n");
        client_logic(connect_socket);
        
        /* Close the socket */
        close(connect_socket);
    }
    
    return 0;
}