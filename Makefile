all: sendFile server

sendFile: sendFile.o
	gcc -Wpedantic -g sendFile.o -o sendFile

sendFile.o: sendFile.c header.h
	gcc -Wpedantic -g -c sendFile.c

server: server.o
	gcc -Wpedantic -g server.o -o server

server.o: server.c header.h
	gcc -Wpedantic -g -c server.c

clean:
	rm *.o server sendFile
