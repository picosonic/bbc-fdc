#ifndef _DOS_H_
#define _DOS_H_

#define DOS_SECTORSIZE 512

// For FAT partition boot sector
#define DOS_JUMP 0xeb3c90
#define DOS_JUMP2 0xeb3e90
#define DOS_EOSM 0x55aa

#pragma pack(1)

struct dos_biosparams
{
  uint16_t bytespersector; // The size of a sector in bytes
  uint8_t sectorspercluster; // Number of sectors in a cluster
  uint16_t reservedsectors; // Number of sectors from partition boot sector to first FAT
  uint8_t fatcopies; // Number of FAT copies, usually 2
  uint16_t rootentries; // Number of filenames in the root folder
  uint16_t smallsectors; // Number of sectors on volume (if < 65535, else 0)
  uint8_t mediatype; // Code for media type
  uint16_t sectorsperfat; // Number of sectors per FAT
  uint16_t sectorspertrack; // Number of sectors per track
  uint16_t heads; // Number of heads
  uint32_t hiddensectors; // Same as relative sector field in partition table
  uint32_t largesectors; // Number of sectors on volume (if > 65535, else 0)

  // Extended BIOS params follow
  uint8_t physicaldiskid; // BIOS physical disk number, floppies start at 0x00
  uint8_t currenthead; // Current head (not used by FAT)
  uint8_t signature; // Disk signature, WindowsNT requires this to be 0x28/0x29
  uint32_t volumeserial; // Almost unique serial number created at format
  uint8_t volumelabel[11]; // Used to store volume label, this is now a special file in FAT
  uint8_t systemid[8]; // System ID, depends on format
};


extern void dos_showinfo();
extern int dos_validate();

#endif
