#ifndef _A2R_H_
#define _A2R_H_

// A2R v2.0.1 (Dec 2, 2018)
// All data is stored little-endian
//
// STRM
//
// For timing/xtiming, data is stored as a stream of unsigned bytes (aligned to index), each byte representing a single flux transition, and is the number of "ticks" since the last flux transition. Ticks are 125 nanoseconds, so 4uS is 32 ticks. 0xff means keep adding a non-0xff is seen. Timing is 250ms, xtiming is 450ms.
//
// For bits, data is a 16384 byte bitstream. This is a series of bits recorded from drive and normalised to 4uS intervals. High bit first, low bit last. Estimated loop point is number of ticks from start of capture (used to determine track length). Deprecated in Applesauce v1.0.3.
//
// META
//
// Tab-delimited UTF8 list of key value pairs. All rows end with a linefeed (0x0a), multiple values are pipe-separated. Empty values allowed for standard keys. Keys are case sensitive. No duplicate keys.
//
// Standard metadata keys
// Key               Purpose
// title             Name/Title
// subtitle          Subtitle
// publisher         Publisher
// developer         Developer
// copyright         Copyright date
// version           Version number
// language          Language
// requires_ram      RAM requirements
// requires_machine  Which computer does this run on
// notes             Additional notes
// side              Physical disk side (as "Disk #, Side [A|B]")
// side_name         Name of the disk side
// contributor       Name of the person who imaged the disk
// image_date        ISO8601 date of imaging (YYYY-MM-DDTHH:MM:SS.HHHZ)

#define A2R_MAGIC "A2R2"

// Chunk ids
#define A2R_CHUNK_INFO "INFO"
#define A2R_CHUNK_STRM "STRM"
#define A2R_CHUNK_META "META"

#pragma pack(push,1)

struct a2r_header
{
  uint8_t id[4]; // "A2R2"
  uint8_t ff; // 0xff to prevent 7-bit transmission
  uint8_t lfcrlf[3]; // LF CR LF (0x0a 0x0d 0x0a)
};

struct a2r_chunkheader
{
  uint8_t id[4]; // 4 ASCII char for chunk id
  uint32_t size; // Size of chunk in bytes
};

struct a2r_info
{
  uint8_t version; // INFO chunk revision (currently 1)
  uint8_t creator[32]; // Name of software which created the file, UTF8, no BOM, padded with spaces
  uint8_t disktype; // 1=5.25", 2=3.5"
  uint8_t writeprotected; // 1=Floppy is write protected
  uint8_t synchronised; // 1=Cross track sync was used during imaging
};

struct a2r_strm
{
  uint8_t location; // Track where capture happened (5.25" in quarter tracks, 3.5" in (track<<1)+side format), 0xff indicates end of STRM
  uint8_t type; // Capture type, 1=Timing, 2=Bits, 3=xtiming
  uint32_t size; // Size of data in bytes
  uint32_t loop; // Duration until sync sensor was triggered at loop point
};

#pragma pack(pop)

#endif
