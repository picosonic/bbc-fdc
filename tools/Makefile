# BBC-FDC tools

CC = gcc

all: drivetest bbcfdc

drivetest: drivetest.o
	$(CC) -g -o drivetest drivetest.o -lbcm2835 -lpthread -lrt

bbcfdc: bbcfdc.o
	$(CC) -g -o bbcfdc bbcfdc.o -lbcm2835 -lpthread -lrt

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
