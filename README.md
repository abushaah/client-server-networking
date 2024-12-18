# Computer Networking

## How to run the code

How to run server.c and sendFile.c:

    0. make all

How to run the server:

    [./server port-number optionalBufSize]
    example: ./server 50693

How to run client:

    [./sendFile fileName server-IP-address:port-number optionalBufSize]
    example: ./sendFile wonderland.txt 127.0.1.1:50693

How to run the bash script (which calls multiple clients, sendFile.c) in Linux:

    0. Change the script.sh file with required command line arguments
    1. Set the script executable permission by running chmod command in Linux:
        [chmod +x script.sh]
    2. Execute shell script in Linux:
        [./script.sh]

NOTE: due to intermittent internet connection, when running a client script with multiple connections on a different machine, I'd recommend a smaller buffer size (512) for always successful file transfer

Sources:

- https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#    
- https://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
- CIS Lecture 5: introduction to network programming slides and examples

## Personal documentation

Test cases:

    1. The client and server are running on the same SoCS Linux server.
        
        Input commands:
            a. ./server 50693
            b. ./sendFile wonderlandSample.txt 127.0.1.1:50693
        Expected results:
            c. file is reassembled completely, then written on disk under the file name given from the server (aka overwriting the original file)

    2. The client and the server are running on separate SoCS Linux servers.

        Input commands:
            a. ssh user@linux.ca
            b. hostname -I | awk '{print $1}'
                result is hostname2
            c. ./server 50693
            d. ssh user@linux.ca
            e. ./sendFile wonderlandSample.txt hostname2:50693
            f. ssh user@linux.ca
            g. ./sendFile wonderlandSample.txt hostname2:50693
        Expected results:
            c. file is written on disk under the same name as file on the client machine

    3. Handling multiple requests.

        3.1. On same server:
        
        Input commands:
            a. ./server 50693
            b. ./script.sh
        Expected results:
            c. server should handle the client connections sequentially and accept connections from multiple clients

        3.2. On different server:

        Input commands:
            a. ssh user@linux.ca
            b. hostname -I | awk '{print $1}'
                result is hostname2
            c. ./server 50693
            d. ssh user@linux.ca
            e. ./scriptRemote.sh
        Expected results:
            f. server should handle the client connections sequentially and accept connections from multiple clients

    4. Setting buffer size (same and different server).

        Input commands:
            a. ./server 50693 2048
            b. ./sendFile wonderlandSample.txt 127.0.1.1:50693 2048
        Expected results:
            c. number of pieces should be greater than the default (4096) and number of confirmation message should reflect that

Limitations (none of these are required by the assignment instructions - just general limitations):
- char file_info[MAXRCVLEN];, char address[MAXSTRLEN]; (fixed length for file name)
- char int_to_str[10]; (fixed length for file size, max 999999999)
- Assumes that when the user inputs an optional buffer size for the server and client, it is the same value
- If the value returned by send() doesn’t match the original bytes_to_send, my program will exit with an error as per the assignment instructions:
        "NOTE: This returned value (e.g. number of bytes read or written) can be less than the length specified in the function call. You need to handle this and determine whether this indicates an error"

Improvements:
- Allocating space as needed rather than using fixed max length (see limitations)
- Making the code modular by adding functions to handle different features (right now everything is in the main)
- Send the file_size data separately as a long int rather than concat with file_size

Debugging log:
- Problem: why the send() and recv() number of bytes is different
- Solution: call to recv(consocket, recv_buffer, buffer_size, 0); instead of recv(consocket, recv_buffer, bytes_to_read, 0);, where bytes_to_read would change near the end of the file (since there is less than buffer_size to be read at the end of file)

- Problem: memory leaks
- Solution: upon exit() due to error, added close for all sockets & files, freed all allocated memory, initialized values that were uninitialized

- Problem: number of bytes sent and received are different. when I run the client and server on different machines that are "far" (ex. linux-01 and linux-06), and the specified buffer is large (2048+ bytes), sometimes the server receives expected bytes successfully, and sometimes not. when the specified buffer size is small (-1024), it is always successful. when the client and server are running on the same or "close" machine (ex. linux-05 and linux-06), it is always successful no matter buffer size
- Solution: for the same test scenario, sometimes it works as expected, sometimes the number of bytes sent are different than received, so I have come to the conclusion that it is due to Network Speed and Congestion