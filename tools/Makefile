# BBC-FDC tools

CC = gcc

HARDWARE = $(shell bash ./hw.sh Hardware)
REVISION = $(shell bash ./hw.sh Revision)

DEBUGFLAGS = -g -W -Wall
BUILDFLAGS = $(DEBUGFLAGS) -D$(HARDWARE) -D$(REVISION)

all: drivetest bbcfdc checkfsd

drivetest: drivetest.o hardware.o
	$(CC) $(BUILDFLAGS) -o drivetest drivetest.o hardware.o -lbcm2835

drivetest.o: drivetest.c hardware.h
	$(CC) $(BUILDFLAGS) -c -o drivetest.o drivetest.c

checkfsd: checkfsd.o
	$(CC) $(BUILDFLAGS) -o checkfsd checkfsd.o

checkfsd.o: checkfsd.c
	$(CC) $(BUILDFLAGS) -c -o checkfsd.o checkfsd.c

bbcfdc: bbcfdc.o crc.o dfs.o diskstore.o fm.o fsd.o hardware.o jsmn.o mfm.o rfi.o
	$(CC) $(BUILDFLAGS) -o bbcfdc bbcfdc.o crc.o dfs.o diskstore.o fm.o fsd.o hardware.o jsmn.o mfm.o rfi.o -lbcm2835

bbcfdc.o: bbcfdc.c dfs.h diskstore.h fm.h fsd.h hardware.h jsmn.h mfm.h rfi.h
	$(CC) $(BUILDFLAGS) -c -o bbcfdc.o bbcfdc.c

##########################

bbcfdc-nopi: bbcfdc-nopi.o crc.o dfs.o diskstore.o fm.o fsd.o jsmn.o mfm.o nopi.o rfi.o
	$(CC) $(BUILDFLAGS) -DNOPI -o bbcfdc-nopi bbcfdc-nopi.o crc.o dfs.o diskstore.o fm.o fsd.o jsmn.o mfm.o nopi.o rfi.o

bbcfdc-nopi.o: bbcfdc.c dfs.h diskstore.h fm.h fsd.h hardware.h jsmn.h mfm.h rfi.h
	$(CC) $(BUILDFLAGS) -DNOPI -c -o bbcfdc-nopi.o bbcfdc.c

nopi.o: nopi.c hardware.h jsmn.h rfi.h
	$(CC) $(BUILDFLAGS) -DNOPI -c -o nopi.o nopi.c

##########################

crc.o: crc.c crc.h
	$(CC) $(BUILDFLAGS) -c -o crc.o crc.c

dfs.o: dfs.c diskstore.h
	$(CC) $(BUILDFLAGS) -c -o dfs.o dfs.c

diskstore.o: diskstore.c
	$(CC) $(BUILDFLAGS) -c -o diskstore.o diskstore.c

fm.o: fm.c crc.h diskstore.h dfs.h fm.h hardware.h
	$(CC) $(BUILDFLAGS) -c -o fm.o fm.c

mfm.o: mfm.c crc.h diskstore.h mfm.h hardware.h
	$(CC) $(BUILDFLAGS) -c -o mfm.o mfm.c

fsd.o: fsd.c diskstore.h fsd.h
	$(CC) $(BUILDFLAGS) -c -o fsd.o fsd.c

jsmn.o: jsmn.c jsmn.h
	$(CC) $(BUILDFLAGS) -c -o jsmn.o jsmn.c

rfi.o: rfi.c jsmn.h rfi.h
	$(CC) $(BUILDFLAGS) -c -o rfi.o rfi.c

hardware.o: hardware.c hardware.h pins.h
	$(CC) $(BUILDFLAGS) -c -o hardware.o hardware.c

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
	rm -f checkfsd
	rm -f bbcfdc-nopi
