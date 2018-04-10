#ifndef _DOS_H_
#define _DOS_H_

#define DOS_SECTORSIZE 512

// For FAT partition boot sector
#define DOS_UNDOCDIRECTJMP 0x69
#define DOS_SHORTJMP 0xeb
#define DOS_NEARJMP 0xe9
#define DOS_NOP 0x90
#define DOS_EOSM 0x55aa

#pragma pack(1)

// From Revolutionary guide to assembly language, Wrox Press, ISBN 1-874416-12-5
//  with changes from various sources including Wikipedia
struct dos_biosparams
{
  // DOS 2.0 BPB
  uint16_t bytespersector; // The size of a sector in bytes
  uint8_t sectorspercluster; // Number of sectors in a cluster
  uint16_t reservedsectors; // Number of sectors from partition boot sector to first FAT
  uint8_t fatcopies; // Number of FAT copies, usually 2
  uint16_t rootentries; // Number of filenames in the root folder
  uint16_t smallsectors; // Number of sectors on volume (if < 65535, else 0)
  uint8_t mediatype; // Code for media type
  uint16_t sectorsperfat; // Number of sectors per FAT

  // DOS 3.0 BPB
  uint16_t sectorspertrack; // Number of sectors per track
  uint16_t heads; // Number of heads
  uint16_t hiddensectors_lo; // Number of hidden sectors (low word)

  // DOS 3.2 BPB
  uint16_t hiddensectors_hi; // Number of hidden sectors (high word)

  // DOS 3.31 BPP
  uint32_t largesectors; // Number of sectors on volume (if > 65535, else 0)
};

// FAT12/FAT16 EBPB
struct dos_extendedbiosparams
{
  uint8_t physicaldiskid; // BIOS physical disk number, floppies start at 0x00
  uint8_t currenthead; // Current head (not used by FAT)
  uint8_t signature; // Disk signature, WindowsNT requires this to be 0x28/0x29
  uint32_t volumeserial; // Almost unique serial number created at format, often from month/day combined with seconds/hundreths for high word, and year with hours/minutes for low word
  uint8_t volumelabel[11]; // Used to store volume label, this is now a special file in FAT, not available if signature is 0x28
  uint8_t systemid[8]; // System ID, depends on format, not available if signature is 0x28
};

// FAT32 EBPB
struct dos_fat32extendedbiosparams
{
  uint32_t sectorsperfat;
  uint16_t drivedescription;
  uint16_t version;
  // TODO
};

extern void dos_showinfo();
extern int dos_validate();

#endif
