#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *hello = "Hello from server";

	/* [S1]
	 * Explaint the following here.
	 * The function socket is opening up a TCP socket with ipv4 address family.
	 * Using the protocol Internet Protocol. The function will return the file descriptor
	 * for that socket and store it into the variable server_fd. Then the if statement
	 * checks whether or not the value of the file descriptor is < 0, because
	 * socket will return -1 if there is any error when creating a new socket. If the value
	 * is -1 then it will print the error message and exit out the program with status 1.
	 */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	/* [S2]
	 * Explaint the following here.
	 * Since this is a server socket, we will have to set some socket options for the file descriptor,
	 * is optional but is good to set it up.
	 * The first parameter is the socket file descriptor we are setting the option for.
	 * The second parameter specifies the level of option we are setting, we can
	 * set many level of settings like options for TCP protocol, but in this case
	 * we are just setting it for the socket level.
	 * The third parameter OR together many of the socket options,
	 * SO_REUSEADDR -> it allows reuse of local address, i.e. 127.0.0.1
	 * SO_REUSEPORT -> Allows TCP sockets to have multiple listening sockets
	 * to be bound to the same port. 
	 * The fourth parameter and fifth parameter is just an pointer
	 * to the int and the size of the int to enable the use of
	 * boolean option like SO_REUSEADDR and SO_REUSEPORT
	 * 
	 * All in all, the if statement below just sets some socket settings
	 * for the server's socket. setsockopt return 0 on success, -1 if error,
	 * hence if it returns -1 it will print out error and exit with EXIT_FAILURE status.
	 */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		       &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	/* [S3]
	 * Explaint the following here.
	 * This is the address struct setting on the server side. It tells the information
	 * about the address that the socket will be connecting. Address family is ipv4.
	 * It also assign the acutal address information in the address struct to be
	 * INADDR_ANY which basically says that the server socket will be listening on
	 * all network interfaces on port 5984 for connections.
	 * 
	 * And lastly, it also assign the port that the server socket will be listening on
	 * which is 5984 with again host byte order to network byte order as is required
	 */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	/* [S4]
	 * Explaint the following here.
	 * For server sockets we will have to bind the socket to allow
	 * the client socket connect to the server socket, otherwise
	 * the sockaddr_in struct we have configured for server socket would be pointless.
	 * 
	 * The server socket will receive the local address and the other configuration
	 * such as which network interface will it be listening from, and which port will
	 * the server be sitting on from the struct address, and the last parameter
	 * is just the size of the address struct
	 * 
	 * bind will return 0 if successfully binded the socket to the address
	 * -1 if there is an error, and hence the if statement handles that error
	 * by printing out an error message and exit with status EXIT_FAILURE
	 */
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	/* [S5]
	 * Explaint the following here.
	 * Sets the server socket to be on passive listening mode, it will be
	 * accepting incoming connection requests. The function listen also
	 * define the maximum number of allowed request to be queued to be 3
	 * so only 3 requests can be queued up before they are dropped by the server
	 * socket. 
	 * 
	 * And again listen return 0 on success, -1 if error and the if
	 * statement handles it by printing the error before exiting the program
	 * with status EXIT_FAILURE
	 */
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* [S6]
	 * Explaint the following here.
	 * The function accept() will block until it have received a connection
	 * request from the client. It reuses the address struct as a place
	 * to store the address information about the accepted client.
	 * The third parameter is just the length of the address struct 
	 * after converting to type socklen_t.
	 * 
	 * accept on success return the file descriptor for the accepted socket
	 * it is an nonnegative integer, and -1 if error. Hence this if just check
	 * if it have receive a valid connection if it errored out it will
	 * print the error message and exit with EXIT_FAILURE.
	 * 
	 * The accepted client connection's file descriptor is stored into variable new_socket
	 */
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
				 (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	/* [S7]
	 * Explaint the following here.
	 * printf just prints out a simple user prompt asking
	 * the user to press any key to continue. The function getchar will
	 * block until the user enter in a character, or multiple characters and hit enter.
	 * Serve as a que to tell the server that the connection is established with the client
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [S8]
	 * Explaint the following here.
	 * The function read will read from the client socket 1024 byte of data and storing
	 * it into the buffer named buffer. It will return -1 if there is any error.
	 * Return 0 for EOF. And return the number of bytes that it has read normally.
	 * So the if statement checks if the function read returns -1 for error, if so
	 * print an error message and return -1 before exiting.
	 * 
	 * If it read from the socket successfully it will just print out the message
	 * that the client has sent over.
	 * 
	 * read will also block until it has receive data.
	 */
	if (read( new_socket , buffer, 1024) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	printf("Message from a client: %s\n",buffer );

	/* [S9]
	 * Explaint the following here.
	 * After printing the message from client it will send the message
	 * "Hello from server" to the client through the connected socket. 
	 * The third parameter is the length of the string
	 * The fourth parameter is for special flag, but no flag is specified 
	 * for this case.
	 * 
	 * After sending the message the program exits successfully.
	 * 
	 */
	send(new_socket , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");
	return 0;
}
