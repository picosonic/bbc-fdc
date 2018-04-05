#ifndef _ADFS_H_
#define _ADFS_H_

#include "diskstore.h"

#define ADFS_8BITSECTORSIZE 256
#define ADFS_16BITSECTORSIZE 1024

#define ADFS_OLDMAP 0
#define ADFS_NEWMAP 1

#define ADFS_OLDDIR 0
#define ADFS_NEWDIR 1

// ADFS formats
#define ADFS_UNKNOWN -1
#define ADFS_S 0
#define ADFS_M 1
#define ADFS_L 2
#define ADFS_D 3
#define ADFS_E 4
#define ADFS_F 5
#define ADFS_EPLUS 6
#define ADFS_FPLUS 7
#define ADFS_G 8

#define ADFS_MAXPATHLEN 256

/*
From RiscOS PRM, 2-197, with G format from RiscOS sources

Logical layout
--------------
Format Map Zones Dir Boot
L      Old --    Old No
D      Old --    New No
E      New  1    New No
F      New  4    New Yes
G      New  7    New ???

All ADFS formats are MFM encoded

Physical layout
---------------
Format Density Sectors/Track Bytes/Sector Storage Heads         Tracks Sides
S      Double  16            256          160k    1 Sequenced   40     1
M      Double  16            256          320k    1 Sequenced   80     1
L      Double  16            256          640K    1 Sequenced   80     2
D      Double   5            1024         800K    2 Interleaved 80     2
E      Double   5            1024         800K    2 Interleaved 80     2
F      Quad    10            1024         1.6M    2 Interleaved 80     2
G      Octal   20            1024         3.2M    2 Interleaved 80     2

DiscRecord attribute defaults
-----------------------------
Format IDlen Bytes/MapBit Skew ZoneSpare RootDir
L      0     0            0    0         0x200
D      0     0            0    0         0x400
E      15    7            1    0x520     0x203
E+     15    7            1    0x520     0x301
F      15    6            1    1600      0x209
F+     15    6            1    1600      0x33801
G      15    6            1    800       0x20f
*/

/*
From Wikipedia and other sources

ADFS S/M/L mostly for 8bit. Arthur/RiscOS maintained L format compatibility.

Arthur added D format with 77 entries per directory, up from 47.
D/E have per-file 12-bit "type" attribute where load/exec were stored in L format.

RiscOS added E and F format for double density and high density respectively.
These introduced "new map" to support fragmentation.

RiscOS 4 added E+ and F+ which allowed for long filenames (255 characters) and more than 77 files (approx 88,000) per directory. The root directory moved. Directories are no longer fixed-size 2048 byte entities, but cannot span more than one disc zone. These use BigMaps.

There is also a G format which allows 77 directory entries, new map and 3200K capacity using 20 sectors/track at Octal density.
*/

/*
OldDir attributes

Bit Meaning
0 The file is readable by you
1 The file is writable by you
2 Undefined
3 The object is locked for you
4 The file is readable by others
5 The file is writable by others
6 Undefined
7 The object is locked for others

In ADFS, bits 4-7 are always identical to bits 0-3. In calls which write the attributes 
of an object, all bits except 0, 1 and 3 are ignored. If the object is a directory, bits 0 and 1 are also ignored. Note that 'others' in the above context means other users of, say, the Econet filing system.
*/

// NewDir attributes, from RiscOS PRM 2-210
#define ADFS_OWNER_READ         (1 << 0)
#define ADFS_OWNER_WRITE        (1 << 1)
#define ADFS_LOCKED             (1 << 2)
#define ADFS_DIRECTORY          (1 << 3)
#define ADFS_EXECUTABLE         (1 << 4)
#define ADFS_PUBLIC_READ        (1 << 5)
#define ADFS_PUBLIC_WRITE       (1 << 6)

// Number of entries and entry size within OldMap
#define ADFS_OLDMAPLEN 82
#define ADFS_OLDMAPENTRY 3

// Difference between RiscOS epoch and UNIX epoch, i.e. seconds between 1st Jan 1900 and 1st Jan 1970
#define ADFS_RISCUNIXTSDIFF 2208988800LL

#define ADFS_OLDDIR_BLOCKSIZE 1280
#define ADFS_OLDDIR_ENTRIES 47
#define ADFS_NEWDIR_BLOCKSIZE 2048
#define ADFS_NEWDIR_ENTRIES 77
#define ADFS_DIR_ENTRYSIZE 26

#pragma pack(1)

struct adfs_oldmap
{
  uint8_t freestart[ADFS_OLDMAPLEN*ADFS_OLDMAPENTRY]; // Table of freespace start sectors
  uint8_t reserved; // Reserved - must be zero
  uint8_t oldname0[5]; // Half disc name (interleaved with oldname1)
  uint8_t oldsize[3]; // Disc size in (256 byte) sectors
  uint8_t check0; // Checksum on first 256 bytes

  uint8_t freelen[ADFS_OLDMAPLEN*ADFS_OLDMAPENTRY];
  uint8_t oldname1[5]; // Half disc name (interleaved with oldname0)
  uint16_t oldid; // Disc ID to identify when disc has been modified
  uint8_t oldboot; // Boot option (as in *OPT 4,n)
  uint8_t freeend; // Pointer to end of free space list
  uint8_t check1; // Checksum on second 256 bytes
};

struct adfs_dirheader
{
  uint8_t startmasseq; // update sequence number to check dir start with dir end
  uint8_t startname[4]; // 'Hugo' or 'Nick'
};

struct adfs_direntry
{
  uint8_t dirobname[10]; // name of object
  uint32_t dirload; // load address of object
  uint32_t direxec; // exec address of object
  uint32_t dirlen; // length of object
  uint8_t dirinddiscadd[3]; // indirect disc address of object

  union
  {
    uint8_t olddirobseq; // object sequence number
    uint8_t newdiratts; // object attributes
  };
};

struct adfs_olddirtail
{
  uint8_t lastmark; // 0 to indicate end of entries
  uint8_t name[10]; // directory name
  uint8_t parent[3]; // indirect disc address of parent directory
  uint8_t title[19]; // directory title
  uint8_t unused[14]; // reserved, must be zero
  uint8_t endmasseq; // to match with startmasseq (in header), else "broken directory"
  uint8_t endname[4]; // to match with startname (in header)
  uint8_t checkbyte; // check byte on directory
};

struct adfs_newdirtail
{
  uint8_t lastmark; // 0 to indicate end of entries
  uint8_t unused[2]; // reserved, must be zero
  uint8_t parent[3]; // indirect disc address of parent directory
  uint8_t title[19]; // directory title
  uint8_t name[10]; // directory name
  uint8_t endmasseq; // to match with startmasseq (in header), else "broken directory"
  uint8_t endname[4]; // to match with startname (in header)
  uint8_t checkbyte; // check byte on directory
};

extern void adfs_gettitle(const int adfs_format, char *title, const int titlelen);
extern void adfs_showinfo(const int adfs_format);
extern int adfs_validate();

#endif
