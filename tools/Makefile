# BBC-FDC tools

CC = gcc

all: drivetest bbcfdc

drivetest: drivetest.o hardware.o
	$(CC) -g -o drivetest drivetest.o hardware.o -lbcm2835 -lpthread -lrt

drivetest.o: drivetest.c hardware.h
	$(CC) -g -c -o drivetest.o drivetest.c

bbcfdc: bbcfdc.o hardware.o
	$(CC) -g -o bbcfdc bbcfdc.o hardware.o -lbcm2835 -lpthread -lrt

bbcfdc.o: bbcfdc.c hardware.h
	$(CC) -g -c -o bbcfdc.o bbcfdc.c

hardware.o: hardware.c hardware.h pins.h
	$(CC) -g -c -o hardware.o hardware.c

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
