#ifndef _ATARIST_H_
#define _ATARIST_H_

#include "diskstore.h"

#define ATARIST_SECTORSIZE 512
#define ATARIST_TRACKS 80

#define ATARIST_UNKNOWN -1

#define ATARIST_MINCLUSTER 2
#define ATARIST_FLOPPYRESSEC 1
#define ATARIST_FLOPPYNUMFATS 2

#define ATARIST_DIRENTRYLEN   32
#define ATARIST_DIRENTRYEND   0x00
#define ATARIST_DIRENTRYE5    0x05
#define ATARIST_DIRENTRYALIAS 0x2e
#define ATARIST_DIRPADDING    0x20
#define ATARIST_DIRENTRYDEL   0xe5

#define ATARIST_EPOCHYEAR 1980

#define ATARIST_ATTRIB_READONLY 0x01
#define ATARIST_ATTRIB_HIDDEN   0x02
#define ATARIST_ATTRIB_SYSTEM   0x04
#define ATARIST_ATTRIB_VOLUME   0x08
#define ATARIST_ATTRIB_DIR      0x10
#define ATARIST_ATTRIB_NEWMOD   0x20

#define ATARIST_CHECKSUMVALUE   0x1234

/*
  https://www-user.tu-chemnitz.de/~heha/viewchm.php/basteln/PC/usbfloppy/floppy.chm/
  https://info-coach.fr/atari/software/FD-Soft.php
*/

#pragma pack(push,1)

struct atarist_bpb
{
  uint16_t bps; // Bytes/sector (usually 512)
  uint8_t spc; // Sectors/cluster (must be a power of 2, usually 2)
  uint16_t ressec; // Reserved sectors (usually 1 for floppies)
  uint8_t nfats; // Number of FATs (usually 2)
  uint16_t ndirs; // Max number of filename entries in root directory
  uint16_t nsects; // Sectors on disk (including reserved)
  uint8_t media; // Media descriptor (0xf8 on HDD, unused on floppy)
  uint16_t spf; // Sectors/FAT
  uint16_t spt; // Sectors/track (usually 9)
  uint16_t nheads; // Number of heads (1=Single sided, 2=Double sided)
  uint16_t nhid; // Number of hidden sectors (not used)
};

struct atarist_boot
{
  uint16_t execflag; // Loaded into cmdload
  uint16_t ldmode; // Load mode (0 = file in fname loaded {usually TOS.IMG}, !=0 = load sectors from ssect for sectcnt
  uint16_t ssect; // Logical sector from which to boot (only if ldmode != 0)
  uint16_t sectcnt; // Number of sectors to load for boot  (only if ldmode != 0)
  uint32_t ldaaddr; // Memory address where boot program will be loaded
  uint32_t fatbuf; // Memory address where the FAT and catalog sectors must be loaded
  uint8_t fname[11]; // Name of image file (in 8.3 format, only whem ldmode = 0)
  uint8_t reserved; // Reserved
  uint8_t bootit[452]; // Boot program code
};

struct atarist_bootsector
{
  uint16_t bra; // 680x0 BRA.S (single relative) instruction to the bootstrap (if executable)
  uint8_t oem[6]; // Filler, or TOS places "Loader" here
  uint8_t serial[3]; // Low 24-bits are unique disk serial number

  struct atarist_bpb bpb; // BIOS parameter block

  struct atarist_boot boot; // Data for bootable disks

  uint16_t checksum; // Entire bootsector summed with this should be 0x1234 (if bootable)
};

struct atarist_direntry
{
  uint8_t fname[8]; // Name of file/directory (space padded)
  uint8_t fext[3]; // Name extension (space padded)
  uint8_t attrib; // Attributes 1=readonly, 2=hidden, 4=system, 8=volume label, 10=directory
  uint8_t reserved[10];
  uint16_t ftime; // Time of last update
  uint16_t fdate; // Date of last update
  uint16_t scluster; // Starting cluser (index into the FAT)
  uint32_t fsize; // Size of file
};

#pragma pack(pop)

extern int atarist_validate();
extern void atarist_showinfo(const int debug);

#endif
