#ifndef _DOS_H_
#define _DOS_H_

#define DOS_SECTORSIZE 512

// For FAT cluster id ranges
#define DOS_MINCLUSTER 2
#define DOS_FAT12MAXCLUSTER 4085
#define DOS_FAT16MAXCLUSTER 65525
#define DOS_FAT32MAXCLUSTER 268435445

// For FAT partition boot sector
#define DOS_UNDOCDIRECTJMP 0x69
#define DOS_SHORTJMP 0xeb
#define DOS_NEARJMP 0xe9
#define DOS_NOP 0x90
#define DOS_EOSM 0x55aa

// Detectable FAT types
#define DOS_UNKNOWN 0
#define DOS_FAT12 12
#define DOS_FAT16 16
#define DOS_FAT32 32

// Sector offsets
#define DOS_OFFSETBPB 0x0b
#define DOS_OFFSETEBPB 0x24
#define DOS_OFFSETFAT32EBPB 0x24

// Directory entries
#define DOS_DIRENTRYLEN 32

// DOS file attributes
#define DOS_ATTRIB_READONLY 0x01
#define DOS_ATTRIB_HIDDEN 0x02
#define DOS_ATTRIB_SYSTEM 0x04
#define DOS_ATTRIB_VOLUMELABEL 0x08
#define DOS_ATTRIB_DIRECTORY 0x10
#define DOS_ATTRIB_ARCHIVE 0x20
#define DOS_ATTRIB_DEVICE 0x40

#pragma pack(1)

// From Revolutionary guide to assembly language, Wrox Press, ISBN 1-874416-12-5
//  with changes from various sources including Wikipedia
struct dos_biosparams
{
  // DOS 2.0 BPB
  uint16_t bytespersector; // The size of a sector in bytes
  uint8_t sectorspercluster; // Number of sectors in a cluster
  uint16_t reservedsectors; // Number of sectors before first FAT
  uint8_t fatcopies; // Number of FAT copies, usually 2
  uint16_t rootentries; // Number of filenames in the root folder
  uint16_t smallsectors; // Number of sectors on volume (if < 65535, else 0)
  uint8_t mediatype; // Code for media type/descriptor
  uint16_t sectorsperfat; // Number of sectors per FAT

  // DOS 3.0 BPB
  uint16_t sectorspertrack; // Number of sectors per track
  uint16_t heads; // Number of surfaces/heads
  uint16_t hiddensectors_lo; // Number of hidden sectors (low word)

  // DOS 3.2 BPB
  uint16_t hiddensectors_hi; // Number of hidden sectors (high word)

  // DOS 3.31 BPP
  uint32_t largesectors; // Number of sectors on volume (if > 65535, else 0)
};

// FAT12/FAT16 EBPB
struct dos_extendedbiosparams
{
  uint8_t physicaldiskid; // BIOS physical disk number, floppies start at 0x00, hdd start at 0x80
  uint8_t currenthead; // Current head (not used by FAT)
  uint8_t signature; // Disk signature, WindowsNT requires this to be 0x28/0x29
  uint32_t volumeserial; // Almost unique serial number created at format, often from month/day combined with seconds/hundreths for high word, and year with hours/minutes for low word
  uint8_t volumelabel[11]; // Used to store volume label, this is now a special file in FAT, not available if signature is 0x28
  uint8_t systemid[8]; // System ID, depends on format, not available if signature is 0x28
};

// FAT32 EBPB - not used on floppy disks
struct dos_fat32extendedbiosparams
{
  uint32_t sectorsperfat;
  uint16_t drivedescription;
  uint16_t version;
  uint16_t rootcluster;
  uint16_t logicalfsinfosector;
  uint16_t logicalfirstfat32bootsector;
  uint8_t reserved29[12];

  uint8_t cf24; // FAT12/16 - BIOS physical disk number
  uint8_t cf25; // FAT12/16 - Current head
  uint8_t cf26; // FAT12/16 - Disk signature
  uint32_t cf27; // FAT12/16 - Volume ID
  uint8_t cf2b[11]; // FAT12/16 - Volume label
  uint8_t cf36[8]; // FAT12/16 - File system ID
};

// FAT directory entry
struct dos_direntry
{
  uint8_t shortname[8]; // Short filename padded with spaces
  uint8_t shortextension[3]; // Short extension padded with spaces
  uint8_t fileattribs; // File attributes bitfield
  uint8_t userattribs; // User attributes
  uint8_t createtimems; // Create time fine resolution (10ms units)
  uint16_t createtime; // Create time, rounded down to the nearest 2 seconds
  uint16_t createdate; // Create date (01/01/1980 .. 31/12/2107)
  uint16_t accessdate; // Last access date, range as above

  union
  {
    uint16_t startclusterhi; // FAT32 - high word of start cluster
    uint16_t accessrights; // FAT12/FAT16 - extended access rights
  };

  uint16_t modifytime; // Last modified time, rounded down to the nearest 2 seconds
  uint16_t modifydate; // Last modified date, range as above
  uint16_t startcluster; // Start of file cluster in FAT12/FAT16
  uint32_t filesize; // File size in bytes, volume label/directories are 0
};

extern void dos_showinfo(const unsigned int disktracks, const unsigned int debug);
extern int dos_validate();

#endif
