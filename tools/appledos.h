#ifndef _APPLEDOS_H_
#define _APPLEDOS_H_

#pragma pack(push,1)

#define APPLEDOS_UNKNOWN -1
#define APPLEDOS_32 3
#define APPLEDOS_33 3

#define APPLEDOS_MAXTRACK 34
#define APPLEDOS_MAXSECTOR 15

// From "Beneath Apple DOS" - Don Worth and Pieter Lechner, May 1982

// VTOC - Volume table of contents
struct appledos_vtoc
{
  uint8_t unused00;
  uint8_t firstcattrack; // Track number of first catalog sector
  uint8_t firstcatsector; // Sector number of first catalog sector
  uint8_t dosrelease; // Release number of DOS used to INIT this diskette
  uint8_t unused04[2];
  uint8_t diskvol; // Diskette volume number (1-254)
  uint8_t unused07[32];
  uint8_t tracksectorpairs; // Maximum number of track/sector pairs which will fit in one file track/sector list sector (122 for 256 byte sectors)
  uint8_t unused28[8];
  uint8_t lastallocatedtrack; // Last track where sectors were allocated
  uint8_t allocationdirection; // Direction of track allocation (+1 or -1)
  uint8_t unused32[2];
  uint8_t tracksperdisk; // Number of tracks per diskette (normally 35)
  uint8_t sectorspertrack; // Number of sectors per track (13 or 16)
  uint8_t bytespersector[2]; // Number of bytes per sector (LO/HI format)
};

// VTOC allocation bitmap
struct appledos_alloc_bitmap
{
  uint8_t bitmap[4]; // Bitmap of free sectors in track n
};

// Catalog sector format
struct appledos_catalog
{
  uint8_t unused00;
  uint8_t nextcattrack; // Track number of next catalog sector (usually 17)
  uint8_t nextcatsector; // Sector number of next catalog sector
  uint8_t unused03[8];
};

// File descriptive entry
struct appledos_fileentry
{
  uint8_t firstsectorlisttrack; // Track of first track/sector list sector (if it's a deleted file 0xFF, with original track moved to byte 0x20 | if it's 0x00 never used)
  uint8_t firstsectorlistsector; // Sector of first track/sector list sector
  uint8_t filetypeflags; // File type and flags (If top bit is set then file is locked)
                         // 00 - TEXT file
                         // 01 - INTEGER BASIC file
                         // 02 - APPLESOFT BASIC file
                         // 04 - BINARY file
                         // 08 - S type file
                         // 10 - RELOCATABLE object module file
                         // 20 - A type file
                         // 40 - B type file
  uint8_t filename[30]; // 30 characters
  uint8_t filelen[2]; // Length of file in sectors (LO/HI format)
};

// Track/Sector list
struct appledos_tracksector
{
  uint8_t unused00;
  uint8_t nextsectorlisttrack; // Track number of next T/S list sector (or zero if no more used)
  uint8_t nextsectorlistsector; // Sector number of next T/S list sector (if present)
  uint8_t unused03[2];
  uint8_t sectoroffset[2]; // Sector offset in file of the first sector described by this list
  uint8_t unused07[5];
};

// T/S pair
struct appledos_ts
{
  uint8_t track; // Track if this data sector (or zero)
  uint8_t sector; // Sector of this data sector (or zero)
};

#pragma pack(pop)

extern int appledos_validate();
extern void appledos_showinfo(const int debug);

#endif
