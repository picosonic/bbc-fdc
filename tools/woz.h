#ifndef _WOZ_H_
#define _WOZ_H_

#define WOZ_MAGIC1 "WOZ1"
#define WOZ_MAGIC2 "WOZ2"

#define WOZ_MAXTRACKS 160
#define WOZ_NOTRACK 0xff
#define WOZ_TRACKSIZE 6646

// Chunk ids
#define WOZ_CHUNK_INFO "INFO"
#define WOZ_CHUNK_TMAP "TMAP"
#define WOZ_CHUNK_TRKS "TRKS"
#define WOZ_CHUNK_META "META"
#define WOZ_CHUNK_WRIT "WRIT"

////////////////////////////////////////////////////////////////////////////////////////////
//
// WOZ format references
//
// Version 1.0.1 - May 20th, 2018
//   https://applesaucefdc.com/woz/reference1/
//
// Version 2.1 - June 16th, 2021
//   https://applesaucefdc.com/woz/reference2/
//
////////////////////////////////////////////////////////////////////////////////////////////

#pragma pack(push,1)

struct woz_header
{
  uint8_t id[4]; // "WOZ1" or "WOZ2"
  uint8_t ff; // 0xff to prevent 7-bit transmission
  uint8_t lfcrlf[3]; // LF CR LF (0x0a 0x0d 0x0a)
  uint32_t crc; // CRC32 of all the remaining data in the file
};

struct woz_chunkheader
{
  uint8_t id[4]; // 4 ASCII char for chunk id
  uint32_t chunksize; // Size of chunk in bytes
};

struct woz_info
{
  uint8_t version; // 1 or 2
  uint8_t disktype; // 1=5.25", 2=3.5"
  uint8_t writeprotected; // 1=Source was write protected
  uint8_t synchronised; // 1=Cross track sync was used during imaging
  uint8_t cleaned; // 1=MC3470 fake bits removed
  uint8_t creator[32]; // Name of software that created file (UTF8, no BOM, padded with spaces)


  // Version 2 fields

  uint8_t sides; // Number of disk sides, 5.25" usually 1, 3.5" can be 1 or 2
  uint8_t bootsectorformat; // Type of boot sector found on disk (only for 5.25" disks), 0=Unknown, 1=16-sector, 2=13-sector, 3=both
  uint8_t timing; // Optimal timing in 125ns increments (e.g. 8=1ms)
  uint16_t compatibility; // Bitfield, 0x0001=Apple ][, 0x0002=Apple ][ plus, 0x0004=Apple //e (unenhanced), 0x0008=Apple //c, 0x0010=Apple //e enhanced, 0x0020=Apple IIgs, 0x0040=Apple //c plus, 0x0080=Apple ///, 0x0100=Apple /// plus
  uint16_t minimumram; // Minimum RAM needed for software on disk in kilobytes, 0=Unknown
  uint16_t largesttrack; // Number of 512 byte blocks used by the largest track

  // Version 3 fields

  uint16_t fluxblock; // Block (512 bytes) where FLUX chunk resides relative to start of file, or 0
  uint16_t largestfluxtrack; // Number of (512 bytes) blocks used by largest flux track
};

// Version 1 track data, 160 of these
//   starting location = (tmap_value * 6656) + 256
struct woz_trks1
{
  uint8_t bitstream[WOZ_TRACKSIZE]; // Bitstream padded out to 6646 bytes
  uint16_t bytesused; // Used bytes within bitstream
  uint16_t bitcount; // Used bits within bitstream
  uint16_t splicepoint; // Index of first bit after track splice, no splice=0xffff
  uint8_t splicenibble; // Nibble value to use for splice
  uint8_t splicebitcount; // Bit count of splice nibble
  uint16_t reserved;
};

// Version 2 track data, 160 of these, then BITS data
struct woz_trks2
{
  uint16_t startingblock; // First block of BITS data (512 byte blocks), relative to start of file
  uint16_t blockcount; // Number of blocks for this BITS data
  uint32_t bitcount; // Number of used bits in bitstream
};

#pragma pack(pop)

#endif
