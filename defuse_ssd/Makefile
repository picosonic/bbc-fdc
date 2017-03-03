Binary = defuse_ssd
OFiles = defuse_ssd.o
HFiles = defuse_ssd.h

all:    $(Binary)
        
defuse_ssd.o:         defuse_ssd.c $(HFiles)
	gcc -Wall -c -O3 -D_FILE_OFFSET_BITS=64 -o defuse_ssd.o defuse_ssd.c


$(Binary):      $(OFiles) $(HFiles)
	gcc -Wall $(OFiles) `pkg-config fuse --cflags --libs` -o $(Binary)

.PHONY: clean

clean:
	rm -f $(Binary) *.o

