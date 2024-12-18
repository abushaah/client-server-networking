all: TCPclient TCPserver

TCPclient: TCPclient.o
	gcc -Wpedantic -g -std=gnu99 TCPclient.o -o TCPclient

TCPclient.o: TCPclient.c
	gcc -Wpedantic -g -std=gnu99 -c TCPclient.c

TCPserver: TCPserver.o
	gcc -Wpedantic -g -std=gnu99 TCPserver.o -o TCPserver

TCPserver.o: TCPserver.c
	gcc -Wpedantic -g -std=gnu99 -c TCPserver.c

clean:
	rm *.o TCPserver TCPclient
