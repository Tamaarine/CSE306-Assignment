#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "Hello from client";
	char buffer[BUFF_SIZE] = {0};

	/* [C1]
	 * Explaint the following here.
	 * The function socket() is used to open up a TCP socket that will communicate using
	 * IPV4 IP addresses (32 bit address). Then communication protocol will be Internet Protocol, hence
	 * the third argument of the creation of the socket is just 0.
	 * The return value will be a file descriptor which is stored into variable sock.
	 * Then sock is used to check whether or not the socket creation failed or not, it failed
	 * if the return value of file descriptor is -1, otherwise it is a value greater than 0.
	 * This just checks if the socket is created successfully with the specified protocol, if
	 * it failed it will print the error and return -1 as the program exits.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	/* [C2]
	 * Explaint the following here.
	 * memset will for each byte of the (size of the struct serv_addr) set the byte
	 * to be the ASCII value '0', or integer value 48. So basically each byte
	 * of the struct serv_addr will be 48.
	 * Then after doing the memory set, the next two line of code sets the
	 * information on the sockaddr_in struct. The address family is set
	 * to be AF_INET because we are using ipv4, and the port number that
	 * the client will be connecting to on the server.
	 * 
	 * In summary, the struct serv_addr is used to tell the address information
	 * that the socket will be connecting to. We set the address family to be AF_INET
	 * and the port number that the client will be connecting to on the server 
	 * is just 5984. The port number assignment requires the function htons
	 * which converts given short integer from host byte order to network byte order.
	 * It is a requirement by the socket.
	 */
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	/* [C3]
	 * Explaint the following here.
	 * The function inet_pton converts the local ip address 127.0.0.1
	 * from its text form to its binary form.
	 * The converted binary form is stored into the serv_addr struct's address field.
	 * The conversion return 1 on success. <= 0 if it couldn't do the conversion, in which
	 * the client prints the error message and return -1 as the program exits.
	 */
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	/* [C4]
	 * Explaint the following here.
	 * Finally the actual connection from the client socket to the server
	 * occurs here with the connect function. It takes in the socket file descriptor
	 * as its first parameter and establish connection to the address provided
	 * in the second argument called addr. The third argument refers to the size of
	 * the socket address.
	 * 
	 * For the second parameter because our struct is actually type sockaddr_in, the second parameter
	 * requires sockaddr, we will just do a simple type conversion in order to pass it.
	 * Don't worry in memory they are the same sizes. 
	 * 
	 * Lastly, the size of the serv_addr is passed evaluated by the sizeof macro.
	 * 
	 * Then if the connection is established successful 0 will be returned,
	 * on error -1 is returned, thus the check with < 0. If an error occurs
	 * the program prints out the error message and return -1 before exiting.
	 * 
	 */
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	/* [C5]
	 * Explaint the following here.
	 * printf just prints out a simple user prompt asking
	 * the user to press any key to continue. The function getchar will
	 * block until the user enter in a character, or multiple characters and hit enter.
	 * Serve as a que to tell the user that the connection is established with the server
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [C6]
	 * Explaint the following here.
	 * The function send will be sending a message across the socket file descriptor
	 * specifically it will be sending the message "Hello from client" through the socket.
	 * The first parameter is the socket that you are sending the message with.
	 * The second parameter is the message buffer.
	 * The third parameter is the length of the buffer.
	 * The fourth parameter specifies some special flags, but in this case is 0, there are
	 * no special flag being used.  
	 */
	send(sock , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");

	/* [C7]
	 * Explaint the following here.
	 * The function read will be reading from the socket file descriptor, 1024 byte 
	 * of data into the buffer named buffer. It will return -1 if there is any error.
	 * Return 0 for EOF. And return the number of bytes that it has read normally.
	 * So the if statement checks if the function read returns -1 for error, if so
	 * print an error message and return -1 before exiting.
	 * 
	 * Otherwise, if no error after reading the message from the socket into the buffer
	 * it will print out the message with 'Message from a server: <the msg>'
	 * 
	 * And finally exit the program.
	 */
	if (read( sock , buffer, 1024) < 0) {
		printf("\nRead Failed \n");
		return -1;
    }
	printf("Message from a server: %s\n",buffer );
	return 0;
}
