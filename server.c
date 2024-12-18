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
#include "header.h"
#include <fcntl.h>

int mysocket; // socket used to listen for incoming connections
int consocket;

static void sigintCatcher(int signal,  siginfo_t* si, void *arg){
    printf("\n\n************** Caught SIG_INT: shutting down the server ********************\n");

    close(consocket);
    close(mysocket);
    exit(0);
}

int main(int argc, char *argv[]){

    // 0. initialize variables, argv error checking
    if (argc < 2) {
        printf("Usage: %s port-number optionalbufSize\n", argv[0]);
        exit(0);
    }

    int bytes_read, port_number, buffer_size;
    struct sockaddr_in dest; // socket info about the machine connecting to us
    struct sockaddr_in serv; // socket info about our server

    struct sigaction signaler; // set up the sigint handler

    port_number = atoi(argv[1]);
    if (!port_number || port_number < 0){
        printf("error parsing port_number\n");
        exit(0);
    }

    if (argc == 3){
        buffer_size = atoi(argv[2]);
    }
    else{
        buffer_size = DEFAULTBFFRSIZE;
    }

    memset(&signaler, 0, sizeof(struct sigaction));
    sigemptyset(&signaler.sa_mask);

    signaler.sa_sigaction = sigintCatcher;
    signaler.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &signaler, NULL);

    // Set up socket info
    socklen_t socksize = sizeof(struct sockaddr_in);
    memset(&serv, 0, sizeof(serv));           // zero the struct before filling the fields
    serv.sin_family = AF_INET;                // IPv4 address family
    serv.sin_addr.s_addr = htonl(INADDR_ANY); // Set our address to any interface
    serv.sin_port = htons(port_number);           // Set the server port number

    mysocket = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP socket
    if (mysocket == -1) {
        printf("error creating socket\n");
        exit(0);
    }

    int flag = 1;
    if (setsockopt(mysocket,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag)) == -1){
        printf("setsockopt() failed\n");
        printf("%s\n", strerror(errno));
        exit(0);
    }

    if (bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) != 0){
        printf("Unable to open TCP socket on port_number:%d\n", port_number);
        printf("%s\n", strerror(errno));
        close(mysocket);
        exit(0);
    }

    // 1. start listening, allowing a queue of up to 20 pending connections, connect when client begins communicating
    if (listen(mysocket, MAXCLIENTS) == -1){
        printf("error in listen\n");
        close(mysocket);
        exit(0);
    }

    printf("[Server] is listening on port %d\n", port_number);

    consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize); // create socket to communicate with connected client

    while(consocket){

        // 2. get the file name, size, and number of pieces
        printf("[Server] incoming connection from %s on port %d\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
        char file_info[MAXRCVLEN + 1];
        bytes_read = recv(consocket, file_info, MAXRCVLEN, 0); // recieve file name from client
        file_info[bytes_read] = '\0';

        char file_name[MAXSTRLEN];
        int file_size;
        int file_pieces;
        char* regex;

        regex = strtok(file_info, ",");
        if (!regex){
            printf("strtok error\n");
            close(consocket);
            close(mysocket);
            exit(0);
        }
        strcpy(file_name, regex);

        regex = strtok(NULL, ",");
        if (!regex) {
            printf("strtok error\n");
            close(consocket);
            close(mysocket);
            exit(0);
        }
        file_size = atoi(regex);

        regex = strtok(NULL, ",");
        if (!regex) {
            printf("strtok error\n");
            close(consocket);
            close(mysocket);
            exit(0);
        }
        file_pieces = atoi(regex);

        printf("[Server] currently receiving file: %s from client %s:%d in %d pieces, total file size is %d\n", file_name, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port), file_pieces, file_size);

        char recv_file[file_size + 1];

        // 2. reassemble the file
        for (int i = 0; i < file_pieces; ++i){

            // sync with client side - near the end of the file, the bytes remaining
            // may be less than send_buffer, in that case, just get the amount remaining
            size_t bytes_to_read = 0;
            int bytes_remaining = file_size - (i * buffer_size);
            if (bytes_remaining >= buffer_size){
                bytes_to_read = buffer_size;
            }
            else{
                bytes_to_read = bytes_remaining;
            }

            printf("[Server] expected to read %d bytes for piece %d\n", bytes_to_read, i + 1);
            char recv_buffer[bytes_to_read + 1];
            bytes_read = recv(consocket, recv_buffer, bytes_to_read, 0);
            recv_buffer[bytes_read] = '\0';
            printf("[Server] actual bytes read from recv() is %d for piece %d\n\n", bytes_read, i + 1);
            if (bytes_read != bytes_to_read){
                printf("error in recv, number of bytes read %d is not equal to amount specified %d\n", bytes_read, bytes_to_read);
                close(consocket);
                close(mysocket);
                exit(0);
            }

            // reassemble file
            if (i == 0){
                strcpy(recv_file, recv_buffer);
            }
            else{
                strcat(recv_file, recv_buffer);
            }

            // 3. send confirmation to client
            send(consocket, "Completed piece", strlen("Completed piece"), 0);
        }

        // 4. writing to the output file name that was first recieved
        /*
            NOTE: the commented code below (lines 186-191) was used for testing the
            copy that the server produces. if you would like to use it, please uncomment
            and change 'file_name' on line 193 to 'file_name_copy'
        */
        // char file_name_copy[MAXRCVLEN + 1];
        // strcpy(file_name_copy, file_name);
        // file_name_copy[strlen(file_name) - 4] = '\0';
        // strcat(file_name_copy, "_server_copy.txt");
        // file_name_copy[strlen(file_name_copy)] = '\0';
        // printf("file name copy: %s\n", file_name_copy);

        int file_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); // REPLACE WITH FILE NAME
        if (file_fd == -1) {
            printf("error in open() file_name\n");
            close(mysocket);
            close(consocket);
            exit(0);
        }
        recv_file[strlen(recv_file)] = '\0';
        // printf("file contents: [%s]\n", recv_file);
        write(file_fd, recv_file, file_size); // ADD ERROR HANDLING -1
        close(file_fd);

        // 3. send confirmation to client
        send(consocket, "Completed file transfer", strlen("Completed file transfer"), 0);
        printf("[Server] completed file transfer\n");
        printf("-------------------------------------------------------------\n");

        // 5. close current socket and continue listening for incoming connections
        close(consocket);
        consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
    }

    close(mysocket);
    return EXIT_SUCCESS;
}