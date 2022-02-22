#ifndef _SCP_H_
#define _SCP_H_

#define SCP_MAGIC "SCP"
#define SCP_VERSION 0x22

#define SCP_TRACK "TRK"

#define SCP_MAXTRACKS 168

#define SCP_BASE_NS 25

#define SCP_EXTFOOTER_MAGIC "FPCS"

// Flags
#define SCP_FLAGS_INDEX 0x01
#define SCP_FLAGS_96TPI 0x02
#define SCP_FLAGS_360RPM 0x04
#define SCP_FLAGS_NORMALISED 0x08
#define SCP_FLAGS_RW 0x10
#define SCP_FLAGS_FOOTER 0x20
#define SCP_FLAGS_EXTENDED 0x40
#define SCP_FLAGS_CREATOR 0x80

// Disk types - manufacturer
#define SCP_MAN_COMMODORE 0x00
#define SCP_MAN_ATARI 0x10
#define SCP_MAN_APPLE 0x20
#define SCP_MAN_PC 0x30
#define SCP_MAN_TANDY 0x40
#define SCP_MAN_TI 0x50
#define SCP_MAN_ROLAND 0x60
#define SCP_MAN_AMSTRAD 0x70
#define SCP_MAN_OTHER 0x80

// CBM DISK TYPES
#define SCP_DISK_C64 0x00
#define SCP_DISK_Amiga 0x04
#define SCP_DISK_AmigaHD 0x08

// ATARI DISK TYPES
#define SCP_DISK_AtariFMSS 0x00
#define SCP_DISK_AtariFMDS 0x01
#define SCP_DISK_AtariFMEx 0x02
#define SCP_DISK_AtariSTSS 0x04
#define SCP_DISK_AtariSTDS 0x05

// APPLE DISK TYPES
#define SCP_DISK_AppleII 0x00
#define SCP_DISK_AppleIIPro 0x01
#define SCP_DISK_Apple400K 0x04
#define SCP_DISK_Apple800K 0x05
#define SCP_DISK_Apple144 0x06

// PC DISK TYPES
#define SCP_DISK_PC360K 0x00
#define SCP_DISK_PC720K 0x01
#define SCP_DISK_PC12M 0x02
#define SCP_DISK_PC144M 0x03

// TANDY DISK TYPES
#define SCP_DISK_TRS80SSSD 0x00
#define SCP_DISK_TRS80SSDD 0x01
#define SCP_DISK_TRS80DSSD 0x02
#define SCP_DISK_TRS80DSDD 0x03

// TI DISK TYPES
#define SCP_DISK_TI994A 0x00

// ROLAND DISK TYPES
#define SCP_DISK_D20 0x00

// AMSTRAD DISK TYPES
#define SCP_DISK_CPC 0x00

// OTHER DISK TYPES
#define SCP_DISK_360 0x00
#define SCP_DISK_12M 0x01
#define SCP_DISK_Rsrvd1 0x02
#define SCP_DISK_Rsrvd2 0x03
#define SCP_DISK_720 0x04
#define SCP_DISK_144M 0x05

#pragma pack(push,1)

// SCP file header
struct scp_header
{
  uint8_t magic[3]; // "SCP"
  uint8_t version; // (nibbles major/minor)
  uint8_t disktype; // (upper nibble = manufacturer, lower nibble = machine)
  uint8_t revolutions; // (1-5)
  uint8_t starttrack; // (0-167)
  uint8_t endtrack; // (0-167)
  uint8_t flags; // bitfield
  uint8_t bitcellencoding; // 0=16 bits, >0=number of bits used
  uint8_t heads; // 0=both sides, 1=side 0 only, 2=side 1 only
  uint8_t resolution; // 0=25ns, 1=50ns, 2=75ns, 3=100ns, 4=125ns, etc.
  uint32_t checksum; // data added together from next byte to EOF
};

// Extensions data block
struct scp_extensions
{
  uint8_t extdata[0x70]; // Information on the extended mode variables is forthcoming
};

// Track data header
struct scp_tdh
{
  uint8_t magic[3]; // "TRK"
  uint8_t track; // track number
};

// Per-revolution timings
struct scp_timings
{
  uint32_t indextime; // time in ns/25ns for one revolution
  uint32_t tracklen; // number of bitcells for this track
  uint32_t dataoffset; // offset to flux data from start of TDH
};

// Extension footer
struct scp_extfooter
{
  uint32_t drivemanufacturer; // Drive manufacturer string offset
  uint32_t drivemodel; // Drive model string offset
  uint32_t driveserial; // Drive serial number string offset
  uint32_t creator; // Creator string offset
  uint32_t application; // Application name string offset
  uint32_t comments; // Comments string offset
  uint8_t creationtimestamp[8]; // Image creation timestamp
  uint8_t modificationtimestamp[8]; // Image modification timestamp
  uint8_t appver; // Application version (nibbles major/minor)
  uint8_t scphwver; // SCP hardware version (nibbles major/minor)
  uint8_t scpfwver; // SCP firmware version (nibbles major/minor)
  uint8_t imgrevision; // Image format revision (nibbles major/minor)
  uint8_t fpcs[4]; // "FPCS"
};

#pragma pack(pop)

extern struct scp_header scpheader;
extern uint32_t *scp_trackoffsets;

extern long scp_readtrack(FILE * scpfile, const int track, const int side, unsigned char *buf, const uint32_t buflen);

extern int scp_readheader(FILE *scpfile);

extern void scp_writeheader(FILE *scpfile, const uint8_t rotations, const uint8_t starttrack, const uint8_t endtrack, const float rpm, const uint8_t sides, const int sidetoread);

extern void scp_writetrack(FILE *scpfile, const uint8_t track, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const uint8_t rotations, const float rpm);

extern void scp_finalise(FILE *scpfile, const uint8_t endtrack);

#endif
