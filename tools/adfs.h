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

*/

/*

ADFS S/M/L mostly for 8bit. Arthur/RiscOS maintained L format compatibility.

Arthur added D format with 77 entries per directory, up from 47.
D/E have per-file 12-bit "type" attribute where load/exec were stored in L.

RiscOS added E and F format for double density and high density respectively.
These introduced "new map" to support fragmentation.

RiscOS 4 added E+ and F+ which allowed for long filenames (255 characters) and more than 77 files (approx 88,000) per directory. The root directory moved. Directories are no longer fixed-size 2048 byte entities, but cannot span more than one disc zone.

There is also a G format which allows 77 directory entries, new map and 3200K capacity using 20 sectors/track at Octal density.

*/

#define ADFS_OWNER_READ         (1 << 0)
#define ADFS_OWNER_WRITE        (1 << 1)
#define ADFS_LOCKED             (1 << 2)
#define ADFS_DIRECTORY          (1 << 3)
#define ADFS_EXECUTABLE         (1 << 4)
#define ADFS_PUBLIC_READ        (1 << 5)
#define ADFS_PUBLIC_WRITE       (1 << 6)

#define ADFS_OLDMAPLEN 82

/*
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

#define ADFS_RISCUNIXTSDIFF 2208988800LL

#define ADFS_OLDDIR_BLOCKSIZE 1280
#define ADFS_OLDDIR_ENTRIES 47
#define ADFS_NEWDIR_BLOCKSIZE 2048
#define ADFS_NEWDIR_ENTRIES 77
#define ADFS_DIR_ENTRYSIZE 26

extern int adfs_validate(Disk_Sector *sector0, Disk_Sector *sector1);

#endif
