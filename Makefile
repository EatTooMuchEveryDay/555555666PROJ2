all: sender receiver

sender: sendfile.c
	gcc -o sendfile sendfile.c

receiver: recvfile.c
	gcc -o recvfile recvfile.c

clean:
	rm -f *.o
	rm -f *~
	rm -f sendfile
	rm -f recvfile
