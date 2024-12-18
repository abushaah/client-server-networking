#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORTNUM 50693
#define MAXRCVLEN 500

int mysocket;            // socket used to listen for incoming connections
int consocket;

static void sigintCatcher(int signal,  siginfo_t* si, void *arg)
{
    printf("\n\n************** Caught SIG_INT: shutting down the server ********************\n");

	close(consocket);
	close(mysocket);
	exit(0);
}

int main(int argc, char *argv[])
{
	int len;
	char buffer[MAXRCVLEN + 1]; // +1 so we can add null terminator
  
	struct sockaddr_in dest; // socket info about the machine connecting to us
	struct sockaddr_in serv; // socket info about our server


	// set up the sigint handler
	struct sigaction signaler;
    
        memset(&signaler, 0, sizeof(struct sigaction));
        sigemptyset(&signaler.sa_mask);

        signaler.sa_sigaction = sigintCatcher;
        signaler.sa_flags = SA_SIGINFO;
        sigaction(SIGINT, &signaler, NULL);

	//Set up socket info

	socklen_t socksize = sizeof(struct sockaddr_in);

	memset(&serv, 0, sizeof(serv));           // zero the struct before filling the fields
	
	serv.sin_family = AF_INET;                // Use the IPv4 address family
	serv.sin_addr.s_addr = htonl(INADDR_ANY); // Set our address to any interface 
	serv.sin_port = htons(PORTNUM);           // Set the server port number 

	/* Create a socket.
   	   The arguments indicate that this is an IPv4, TCP socket
	*/
	mysocket = socket(AF_INET, SOCK_STREAM, 0);
  
	// bind serv information to mysocket
        // bind socket to the local address and port number
        // int bind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
	// Unlike all other function calls in this example, this call to bind()
	// does some basic error handling

	int flag=1;

	if (setsockopt(mysocket,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag)) == -1) {
    	    printf("setsockopt() failed\n");
	    printf("%s\n", strerror(errno));
    	    exit(1);
	} 

	if (bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) != 0){
		printf("Unable to open TCP socket on localhost:%d\n", PORTNUM);

	/* 	strerror() returns a string representation of the system variable "errno"
	 	errno is the integer code of the error that occured during the last system call from this process
	 	need to include errno.h to use this function
		*/
		printf("%s\n", strerror(errno));
		close(mysocket);
		return 0;
	}

	// start listening, allowing a queue of up to 1 pending connection
        /*
            many client requests may arrive, server cannot handle them all at the same time
            server could reject some requests or let them wait
            int listen(int sockfd, int backlog)
            defines how many connections can be pending
            Arguments: socket descriptor and acceptable backlog
            Returns a 0 on success, and -1 on error
            Listen is non-blocking: returns immediately = 0
        */
	listen(mysocket, 0);

        /*
            server has to wait for connection request to arrive
            in blocking until the request arrives, and then accept new request
        */
	
	// Create a socket to communicate with the client that just connected, accept connection
        /*
            int accept( int sockfd, struct sockaddr *addr, socketlen_t *addrlen)
            Arguments: sockfd, structure that will provide client address and port,
                       and length of the structure
            Returns descriptor of socket for this new connection
        */
	consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
  
	while(consocket)
	{
		//dest contains information - IP address, port number, etc. - in network byte order = big endian
		//We need to convert it to host byte order = little endian before displaying it
		printf("Incoming connection from %s on port %d\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
		
                /*
                    both sides read and write, undirectional stream of data
                    in practice client write, server read, server write, client read, etc
                */

		// Receive data from the client
		len = recv(consocket, buffer, MAXRCVLEN, 0);

                /* recv = socket specific, read () for generic file descriptor
                   if flag passed to recv() = 0, identical to read() (like above)
                   else, flag can alter behavior (ie read message content without removing from buffer)
                */

		buffer[len] = '\0';
		printf("Received %s\n", buffer);
		
		//Convert data to upper case
		int bufLen = strlen(buffer);
		for(int i = 0; i < bufLen; i++){
			buffer[i] = toupper(buffer[i]);
		}
		
		//Send data to client
		send(consocket, buffer, strlen(buffer), 0); 

                /*
                    send = socket specific
                    write = generic file descriptor
                    send() with flag 0 = write()
                */
		
		//Close current connection
                // data in flight still reaches other end, so server can close before client finishes reading
		close(consocket);
		
		//Continue listening for incoming connections
		consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
	}

	close(mysocket);
	return EXIT_SUCCESS;
}

/*

processes:
multiple clients = fork() = multiple processes
go to a loop and accept connections using accept()
after a connection is established, call fork() to create a new child process to handle it
go back and listen for another socket in the parent process
close() when complete
heavy-weight

threads:
one thread per client is more flexible and efficient

getaddrinfo() to get the IP address or protocol
    - server known by name and service: "www.wikipedia.org" and "http"
    - translate to ip address and port number: "208.80.153.224" and "80"
    - int getaddrinfo(char *node, char *service, struct addrinfo *hints, struct addrinfo **result)
        • node: host name (e.g., "www.cnn.com") or IP address
        • service: port number or service listed in /etc/services (e.g. ftp)
        • hints: points to a struct addrinfo with known information
        • result: contains the result in linked list form
        • returns an error code

data structures to host address information:
struct addrinfo {
    int ai_flags;
    int ai_family; //e.g. AF_INET for IPv4
    int ai_socketype; //e.g. SOCK_STREAM for TCP
    int ai_protocol;//e.g. IPPROTO_TCP
    size_t ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr; // point to sockaddr struct
    struct addrinfo *ai_next;
}

example:
• hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
• hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
• int status = getaddrinfo("www.wikipedia.org", "80", &hints, &result);
• The result now points to a linked list of 1 or more addrinfos, etc

resource: https://beej.us/guide/bgnet/

*/
