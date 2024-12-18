#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "header.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include <time.h> // for the report

int get_file_size(const char *file_name) {

    struct stat file_status;

    if (stat(file_name, &file_status) == 0){
        return file_status.st_size;
    }

    printf("Cannot determine size of %s\n", file_name);
    return -1;
}

int main(int argc, char *argv[]){

    if (argc < 3 || argc > 4) {
        printf("Invalid number of arguments\n");
        printf("Usage: $./%s sendingFileName server-IP-address:port-number optionalBufSize\n", argv[0]);
        exit(0);
    }

    // 0. initialize variables, argv error checking
    char recv_buffer[MAXRCVLEN + 1];
    char* regex;
    char* file_name;
    char address[MAXSTRLEN];
    char file_info[MAXRCVLEN + 1]; // to include file name, size, and number of pieces
    struct sockaddr_in dest; // socket info about the machine connecting to us
    int bytes_recv, mysocket, port, buffer_size, bytes_sent, file_size, file_pieces, pieces;
    char int_to_str[10]; // max number is 999999999

    if (argc == 4){
        buffer_size = atoi(argv[3]);
    }
    else{
        buffer_size = DEFAULTBFFRSIZE;
    }

    file_name = argv[1];
    if (!file_name || (strlen(file_name) >= MAXSTRLEN)){
        printf("argv error or file name > 256\n");
        exit(0);
    }

    regex = strtok(argv[2], ":");
    if (!regex){
        printf("strtok error\n");
        exit(0);
    }
    strcpy(address, regex);

    regex = strtok(NULL, ":");
    if (!regex) {
        printf("Usage: $./%s sendingFileName server-IP-address:port-number optionalBufSize\n", argv[0]);
        exit(0);
    }
    port = atoi(regex);

    if (!port || !address){
        printf("error parsing server-IP-address:port-number\n");
        exit(0);
    }

    mysocket = socket(AF_INET, SOCK_STREAM, 0); // this is IPv4, TCP socket, unspecified protocol
    if (mysocket == -1) {
        printf("error creating socket\n");
        exit(0);
    }

    memset(&dest, 0, sizeof(dest)); // initialize the socket descriptor
    dest.sin_family = AF_INET; // Use the IPv4 address family
    dest.sin_port = htons(port);

    // set destination IP number from command line arg
    if (inet_pton(AF_INET, address, &dest.sin_addr) < 1){
        printf("inet_pton error or invalid IP\n");
        exit(0);
    }

    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        printf("error in open(): cannot open %s\n", file_name);
        close(mysocket);
        exit(0);
    }

    // 1. connect to the server - start time
    printf("[Client] connecting to server %s:%d, starting stopwatch\n", address, port);
    clock_t start = clock();
    if (connect(mysocket, (struct sockaddr *)&dest, sizeof(struct sockaddr_in)) == -1){
        printf("error connecting to server\n");
        exit(0);
    }
    else{
        printf("[Client] successful connection\n");
    }

    // 2. determine size, determine number of pieces, open file for reading
    file_size = get_file_size(file_name);
    if (file_size == -1){
        close(mysocket);
        exit(0);
    }
    pieces = file_size / buffer_size; // number of pieces will be the file size in bytes / buffer_size
    if ((file_size % buffer_size) != 0) {
        pieces++;
    }
    file_pieces = (pieces > 1) ? pieces : 1; // in case of small files

    // send file info to server in a string format "file_name,file_size,file_pieces"
    strcpy(file_info, file_name);
    strcat(file_info, ",");
    sprintf(int_to_str, "%d", file_size);
    strcat(file_info, int_to_str);
    strcat(file_info, ",");
    sprintf(int_to_str, "%d", file_pieces);
    strcat(file_info, int_to_str);

    file_info[strlen(file_info)] = '\0';

    printf("[Client] sending file information: file name is %s, size is %d, sending in %d pieces\n", file_name, file_size, file_pieces);

    int bytes_to_send = strlen(file_info);
    bytes_sent = send(mysocket, file_info, bytes_to_send, 0);
    if (bytes_sent != bytes_to_send){
        printf("error in send, number of bytes sent %d is not equal to amount specified %d\n", bytes_sent, bytes_to_send);
        close(mysocket);
        exit(0);
    }

    // 3. break file into pieces in size of buffer input or default size
    for (int i = 0; i < file_pieces; ++i){

        // near the end of the file, the bytes remaining may be less than send_buffer
        // in that case, just get the amount remaining
        size_t bytes_to_read = 0;
        int bytes_remaining = file_size - (i * buffer_size);
        if (bytes_remaining >= buffer_size){
            bytes_to_read = buffer_size;
        }
        else{
            bytes_to_read = bytes_remaining;
        }

        char send_buffer[bytes_to_read + 1]; // +1 for null terminating the string

        // printf("[Client] bytes to read: %d\n", bytes_to_read);
        size_t bytes_read;
        bytes_read = read(file_fd, send_buffer, bytes_to_read);
        send_buffer[bytes_read] = '\0';
        printf("[Client] expected to send %d bytes for piece %d\n", bytes_read, i + 1);
        // printf("read: [%s]\n", send_buffer);
        if (bytes_read != bytes_to_read){
            printf("error in fread, number of bytes read %d is not equal to amount specified %d\n", bytes_read, bytes_to_read);
            close(file_fd);
            close(mysocket);
            exit(0);
        }
        
        bytes_sent = send(mysocket, send_buffer, strlen(send_buffer), 0);
        // printf("[Client] actual bytes sent from send() is %d for piece %d\n", bytes_sent, i + 1);
        if (bytes_sent != bytes_to_read){
            printf("error in send, number of bytes sent %d is not equal to amount specified %d\n", bytes_sent, bytes_to_read);
            close(file_fd);
            close(mysocket);
            exit(0);
        }

        // 4. recieve confirmation info
        bytes_recv = recv(mysocket, recv_buffer, MAXRCVLEN, 0); // recieve the response from the server
        recv_buffer[bytes_recv] = '\0';
        if (bytes_recv <= 0){
            printf("[Client] unsuccessful transfer of piece %d, %d bytes sent: server confirmation not received: \"%s\" from %s on port %d.\n", i + 1, bytes_sent, recv_buffer, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
            close(file_fd);
            close(mysocket);
            exit(0);
        }
        else{
            printf("[Client] successful transfer of piece %d, %d bytes sent: server confirmation received: \"%s\" from %s on port %d.\n\n", i + 1, bytes_sent, recv_buffer, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
        }
    }

    bytes_recv = recv(mysocket, recv_buffer, MAXRCVLEN, 0); // recieve the response from the server
    recv_buffer[bytes_recv] = '\0';
    if (bytes_recv <= 0){
        printf("[Client] unsuccessful return: received \"%s\" (%d bytes) from %s on port %d.\n", recv_buffer, bytes_recv, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
        close(file_fd);
        close(mysocket);
        exit(0);
    }
    else{
        printf("[Client] successful return: received \"%s\" (%d bytes) from %s on port %d.\n", recv_buffer, bytes_recv, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
    }

    clock_t end = clock();
    double elapsed_time = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC; // in milliseconds
    printf("[Client] elapsed time for file transfer: %f\n", elapsed_time);
    printf("-------------------------------------------------------------\n");

    // 5. close socket and file
    close(file_fd);
    close(mysocket);
    return EXIT_SUCCESS;
}