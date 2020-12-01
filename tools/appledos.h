#ifndef _APPLEDOS_H_
#define _APPLEDOS_H_

#pragma pack(push,1)

#define APPLEDOS_UNKNOWN -1

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

#pragma pack(pop)

extern int appledos_validate();
extern void appledos_showinfo(const int debug);

#endif
