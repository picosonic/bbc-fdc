# BBC-FDC tools

CC = gcc

all: drivetest bbcfdc

drivetest: drivetest.o hardware.o
	$(CC) -g -o drivetest drivetest.o hardware.o -lbcm2835 -lpthread -lrt

drivetest.o: drivetest.c hardware.h
	$(CC) -g -c -o drivetest.o drivetest.c

bbcfdc: bbcfdc.o dfs.o diskstore.o fsd.o hardware.o
	$(CC) -g -o bbcfdc bbcfdc.o dfs.o diskstore.o fsd.o hardware.o -lbcm2835 -lpthread -lrt

bbcfdc.o: bbcfdc.c dfs.h diskstore.h fsd.h hardware.h
	$(CC) -g -c -o bbcfdc.o bbcfdc.c

dfs.o: dfs.c diskstore.h
	$(CC) -g -c -o dfs.o dfs.c

diskstore.o: diskstore.c
	$(CC) -g -c -o diskstore.o diskstore.c

fsd.o: fsd.c diskstore.h
	$(CC) -g -c -o fsd.o fsd.c

hardware.o: hardware.c hardware.h pins.h
	$(CC) -g -c -o hardware.o hardware.c

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
