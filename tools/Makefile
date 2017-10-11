# BBC-FDC tools

CC = gcc

all: drivetest

drivetest: drivetest.o
	$(CC) -g -o drivetest drivetest.o -lbcm2835  -lpthread -lrt

clean:
	rm -f *.o
	rm -f drivetest
