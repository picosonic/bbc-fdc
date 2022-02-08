# BBC-FDC tools

CC = gcc

HARDWARE = $(shell bash ./hw.sh Hardware)
REVISION = $(shell bash ./hw.sh Revision)
CPU = $(shell bash ./hw.sh CPU)

DEBUGFLAGS = -g -W -Wall
BUILDFLAGS = $(DEBUGFLAGS) -D$(HARDWARE) -D$(REVISION)

all: drivetest bbcfdc checkfsd checkhfe checktd0 checkscp bbcfdc-nopi

checktools: checkfsd checkhfe checktd0 checkscp

drivetest: drivetest.o hardware.o
	$(CC) $(BUILDFLAGS) -o drivetest drivetest.o hardware.o -lbcm2835

drivetest.o: drivetest.c hardware.h
	$(CC) $(BUILDFLAGS) -c -o drivetest.o drivetest.c

checkfsd: checkfsd.o
	$(CC) $(BUILDFLAGS) -o checkfsd checkfsd.o

checkfsd.o: checkfsd.c fsd.h
	$(CC) $(BUILDFLAGS) -c -o checkfsd.o checkfsd.c

checkhfe: checkhfe.o
	$(CC) $(BUILDFLAGS) -o checkhfe checkhfe.o

checkhfe.o: checkhfe.c hfe.h
	$(CC) $(BUILDFLAGS) -c -o checkhfe.o checkhfe.c

checkscp: checkscp.o
	$(CC) $(BUILDFLAGS) -o checkscp checkscp.o

checkscp.o: checkscp.c scp.h
	$(CC) $(BUILDFLAGS) -c -o checkscp.o checkscp.c

checktd0: checktd0.o crc.o lzhuf.o
	$(CC) $(BUILDFLAGS) -o checktd0 checktd0.o crc.o lzhuf.o

checktd0.o: checktd0.c teledisk.h crc.h lzhuf.h
	$(CC) $(BUILDFLAGS) -c -o checktd0.o checktd0.c

bbcfdc: bbcfdc.o adfs.o amigados.o amigamfm.o appledos.o applegcr.o atarist.o common.o crc.o crc32.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o gcr.o hardware.o jsmn.o mfm.o mod.o pll.o rfi.o scp.o teledisk.o
	$(CC) $(BUILDFLAGS) -o bbcfdc adfs.o amigados.o amigamfm.o appledos.o applegcr.o atarist.o bbcfdc.o common.o crc.o crc32.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o gcr.o hardware.o jsmn.o mfm.o mod.o pll.o rfi.o scp.o teledisk.o -lbcm2835 -lm

bbcfdc.o: bbcfdc.c adfs.h amigados.h amigamfm.h appledos.h applegcr.h atarist.h common.h dfi.h dfs.h diskstore.h dos.h fm.h fsd.h gcr.h hardware.h jsmn.h mfm.h mod.h pll.h rfi.h scp.h teledisk.h
	$(CC) $(BUILDFLAGS) -c -o bbcfdc.o bbcfdc.c

##########################

bbcfdc-nopi: bbcfdc-nopi.o adfs.o amigados.o amigamfm.o appledos.o applegcr.o atarist.o common.o crc.o crc32.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o gcr.o hfe.o jsmn.o mfm.o mod.o nopi.o pll.o rfi.o scp.o teledisk.o
	$(CC) $(BUILDFLAGS) -DNOPI -o bbcfdc-nopi bbcfdc-nopi.o adfs.o amigados.o amigamfm.o appledos.o applegcr.o atarist.o common.o crc.o crc32.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o gcr.o hfe.o jsmn.o mfm.o mod.o nopi.o pll.o rfi.o scp.o teledisk.o -lm

bbcfdc-nopi.o: bbcfdc.c adfs.h appledos.h applegcr.h amigados.h amigamfm.h atarist.h common.h dfi.h dfs.h diskstore.h dos.h fm.h fsd.h gcr.h hardware.h hfe.h jsmn.h mfm.h mod.h pll.h rfi.h scp.o teledisk.h
	$(CC) $(BUILDFLAGS) -DNOPI -c -o bbcfdc-nopi.o bbcfdc.c

nopi.o: nopi.c hardware.h jsmn.h rfi.h scp.h
	$(CC) $(BUILDFLAGS) -DNOPI -c -o nopi.o nopi.c

##########################

adfs.o: adfs.c adfs.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o adfs.o adfs.c

amigados.o: amigados.c amigados.h amigamfm.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o amigados.o amigados.c

amigamfm.o: amigamfm.c amigamfm.h diskstore.h hardware.h mod.h pll.h
	$(CC) $(BUILDFLAGS) -c -o amigamfm.o amigamfm.c

appledos.o: appledos.c appledos.h
	$(CC) $(BUILDFLAGS) -c -o appledos.o appledos.c

applegcr.o: applegcr.c applegcr.h pll.h
	$(CC) $(BUILDFLAGS) -c -o applegcr.o applegcr.c

atarist.o: atarist.c atarist.h
	$(CC) $(BUILDFLAGS) -c -o atarist.o atarist.c

crc.o: crc.c crc.h
	$(CC) $(BUILDFLAGS) -c -o crc.o crc.c

crc32.o: crc32.c crc32.h
	$(CC) $(BUILDFLAGS) -c -o crc32.o crc32.c

common.o: common.c common.h
	$(CC) $(BUILDFLAGS) -c -o common.o common.c

dfi.o: dfi.c dfi.h
	$(CC) $(BUILDFLAGS) -c -o dfi.o dfi.c

dfs.o: dfs.c dfs.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o dfs.o dfs.c

dos.o: dos.c dos.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o dos.o dos.c

diskstore.o: diskstore.c crc32.h diskstore.h hardware.h mod.h
	$(CC) $(BUILDFLAGS) -c -o diskstore.o diskstore.c

fm.o: fm.c crc.h diskstore.h dfs.h fm.h hardware.h mod.h pll.h
	$(CC) $(BUILDFLAGS) -c -o fm.o fm.c

fsd.o: fsd.c diskstore.h fsd.h hardware.h
	$(CC) $(BUILDFLAGS) -c -o fsd.o fsd.c

gcr.o: gcr.c gcr.h pll.h
	$(CC) $(BUILDFLAGS) -c -o gcr.o gcr.c

hardware.o: hardware.c hardware.h pins.h
	$(CC) $(BUILDFLAGS) -c -o hardware.o hardware.c

hfe.o: hfe.c hardware.h hfe.h
	$(CC) $(BUILDFLAGS) -c -o hfe.o hfe.c

jsmn.o: jsmn.c jsmn.h
	$(CC) $(BUILDFLAGS) -c -o jsmn.o jsmn.c

lzhuf.o: lzhuf.c lzhuf.h
	$(CC) $(BUILDFLAGS) -c -o lzhuf.o lzhuf.c

mfm.o: mfm.c crc.h diskstore.h hardware.h mfm.h mod.h pll.h
	$(CC) $(BUILDFLAGS) -c -o mfm.o mfm.c

mod.o: mod.c amigamfm.h fm.h mfm.h hardware.h
	$(CC) $(BUILDFLAGS) -c -o mod.o mod.c

pll.o: pll.c pll.h
	$(CC) $(BUILDFLAGS) -c -o pll.o pll.c

rfi.o: rfi.c hardware.h jsmn.h rfi.h
	$(CC) $(BUILDFLAGS) -c -o rfi.o rfi.c

scp.o: scp.c hardware.h mod.h scp.h
	$(CC) $(BUILDFLAGS) -c -o scp.o scp.c

teledisk.o: teledisk.c diskstore.h hardware.h teledisk.h
	$(CC) $(BUILDFLAGS) -c -o teledisk.o teledisk.c

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
	rm -f checkfsd
	rm -f checkhfe
	rm -f checkscp
	rm -f checktd0
	rm -f bbcfdc-nopi
