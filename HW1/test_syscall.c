#include <sys/syscall.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char ** argv) {
    int opt; /* For storing the option character parsed from getopt */
    int encryption_key; /* Encryption key, 1-5 */
    char * plaintext; /* Plaintext passed to the syscall for encryption */
    
    /* Initialization */
    plaintext = NULL;
    encryption_key = -1;

    while((opt = getopt(argc, argv, "s:k:")) != -1) {
        switch (opt) {
            case 's':
                /* The plaintext */
                plaintext = optarg;
                break;
            case 'k':
                /* 
                 * The encryption key. Parsing it using atoi(). Would
                 * have done the conversion myself but atoi is safer I guess
                 */
                encryption_key = atoi(optarg);
                break;
            case '?':
                /* Let getopt handle the error message then exit with failure */
                exit(EXIT_FAILURE);
            default:
                printf("Parsing argument fatal failure\n");
                exit(EXIT_FAILURE);
        }
    }
    
    if (plaintext) {
        
    }
    
    int ret = syscall(449, "hi", encryption_key);
    
    printf("The return value from sys_s2_encrypt is: %d\n", ret);
    return 0;
}