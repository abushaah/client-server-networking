#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAXRCVLEN 500
#define PORTNUM 50693

int main(int argc, char *argv[]){

	char buffer[MAXRCVLEN + 1]; /* +1 so we can add null terminator */
	char* msg = "Hello Worlds!\n";
	int len, mysocket;
	struct sockaddr_in dest; // socket info about the machine connecting to us
 
	/* Create a socket:
           int socket(int domain, int type, int protocol)
           returns file descriptor/handle for the socket
           AF_INET = IPv4, AF_INET6 = IPv6
           SOCK_STREAM: reliable byte stream (TCP)
           SOCK_DGRAM: message-oriented service (UDP)
           AF_UNSPEC: unspecified protocol = 0, used for brevity

	   The arguments indicate that this is an IPv4, TCP socket, unspecified protocol
           AF_INET, SOCK_STREAM implies TCP
	*/
	mysocket = socket(AF_INET, SOCK_STREAM, 0);
  
	memset(&dest, 0, sizeof(dest));                // zero the struct, initialize the socket descriptor
	
	/* Initialize the destination socket information from command line
           Associate socket wth server address/port
           aquire local port number (assigned by OS)
        */
	dest.sin_family = AF_INET;					   // Use the IPv4 address family
	if (argc == 1){
		dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Set destination IP number - localhost, 127.0.0.1
	}else if (argc == 2){
		inet_aton(argv[1], &dest.sin_addr); // Set destination IP number from command line arg
	}else{
		printf("Invalid number of arguments\n");
		printf("Usage: %s <ip>, where ip is an optional IP4 address in the dot-decimal notation\n", argv[0]);
		printf("If the IP address is not provided, client attempts to connect to localhost\n");
		exit(0);
	}

	dest.sin_port = htons(PORTNUM);                // Set destination port number
 	
	/* Connect to the server
           blocking call - will return only after connection i established or definately failed

           int connect(int sockfd, struct sockaddr *server_address, socketlen_t addrlen);
           Args: socket descriptor, server address, and address size
           Returns 0 on success, and -1 if an error occurs
        */
	connect(mysocket, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));
  

        /*
            int send( int sockfd, void *msg, size_t len, int flags)
            Arguments: socket descriptor, pointer to buffer of data to send, and length of the buffer
            Returns the number of bytes written, and -1 on error
            send is blocking: return only after data is sen
        */
	send(mysocket, msg, strlen(msg), 0); 

        /*
            int recv( int sockfd, void *buf, size_t maxLen, int flags);
            Arguments: socket descriptor, pointer to buffer to place the data, size of the buffer
            Returns the number of characters read (where 0 implies "end of file"), and -1 on error
            maxLen should indicate max buffer size
            recv is also blocking: return only after data is receive
        */
	len = recv(mysocket, buffer, MAXRCVLEN, 0); // recieve the response from the server
        // len is the length of the response message
 
	/* We have to null terminate the received data ourselves */
	buffer[len] = '\0';

        /*
            network byte order = big endian
            host byte order = little endian
            htons() & htonl() (host to network short and long)
            ntohs() & ntohl() (network to host short and long)
            when putting data onto the wir
        */
 
	printf("Received %s (%d bytes) from %s on port %d.\n", buffer, len, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
 
	close(mysocket);
	return EXIT_SUCCESS;
}
