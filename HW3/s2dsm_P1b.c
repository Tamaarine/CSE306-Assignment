#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char ** argv) {
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
    
    printf("The number returned %d %d\n", local_port, remote_port);
    
    return 0;
}