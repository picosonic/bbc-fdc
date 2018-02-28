# BBC-FDC tools

CC = gcc

HARDWARE = $(shell bash ./hw.sh Hardware)
REVISION = $(shell bash ./hw.sh Revision)

DEBUGFLAGS = -g -W -Wall
BUILDFLAGS = $(DEBUGFLAGS) -D$(HARDWARE) -D$(REVISION)

all: drivetest bbcfdc

drivetest: drivetest.o hardware.o
	$(CC) $(BUILDFLAGS) -o drivetest drivetest.o hardware.o -lbcm2835 -lpthread -lrt

drivetest.o: drivetest.c hardware.h
	$(CC) $(BUILDFLAGS) -c -o drivetest.o drivetest.c

bbcfdc: bbcfdc.o dfs.o diskstore.o fsd.o hardware.o
	$(CC) $(BUILDFLAGS) -o bbcfdc bbcfdc.o dfs.o diskstore.o fsd.o hardware.o -lbcm2835 -lpthread -lrt

bbcfdc.o: bbcfdc.c dfs.h diskstore.h fsd.h hardware.h
	$(CC) $(BUILDFLAGS) -c -o bbcfdc.o bbcfdc.c

dfs.o: dfs.c diskstore.h
	$(CC) $(BUILDFLAGS) -c -o dfs.o dfs.c

diskstore.o: diskstore.c
	$(CC) $(BUILDFLAGS) -c -o diskstore.o diskstore.c

fsd.o: fsd.c diskstore.h
	$(CC) $(BUILDFLAGS) -c -o fsd.o fsd.c

hardware.o: hardware.c hardware.h pins.h
	$(CC) $(BUILDFLAGS) -c -o hardware.o hardware.c

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
