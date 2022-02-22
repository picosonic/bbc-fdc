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
//
//////////////////////////////////////
//
// A2R v3.0 (Sep 16, 2021)
//
// RWCP
// 
// Raw captures similar to STRM format but includes a capture resolution (Number of picoseconds per tick), defaults to 125,000 picoseconds (125 nanoseconds).
//
// SLVD
//
// Flux streams which have been 'solved', no extraneous fluxes, single rotation, same format as timing.
// Any missing tracks indicates empty/unformatted.

#define A2R_MAGIC2 "A2R2"
#define A2R_MAGIC3 "A2R3"

// Chunk ids
#define A2R_CHUNK_INFO "INFO"
#define A2R_CHUNK_STRM "STRM"
#define A2R_CHUNK_META "META"
#define A2R_CHUNK_RWCP "RWCP"
#define A2R_CHUNK_SLVD "SLVD"

// Timings
#define A2R_TICKS 125
#define A2R_SAMPLE_RATE (NSINSECOND/A2R_TICKS)

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
  uint8_t disktype; // 1=5.25" SS 40t 0.25 step, 2=3.5" DS 80t Apple CLV, 3=5.25" DS 80t, 4=5.25" DS 40t, 5=3.5" DS 80t, 6=8" DS
  uint8_t writeprotected; // 1=Floppy is write protected
  uint8_t synchronised; // 1=Cross track sync was used during imaging

  // From v3
  uint8_t hardsectors; // 0=Soft sectored, 1+=Number of hard sectors
};

struct a2r_strm
{
  uint8_t location; // Track where capture happened (type 1 - 5.25" in quarter tracks, others in (track<<1)+side format), 0xff indicates end of STRM
  uint8_t type; // Capture type, 1=Timing, 2=Bits, 3=xtiming
  uint32_t size; // Size of data in bytes
  uint32_t loop; // Duration until sync sensor was triggered at loop point
};

struct a2r_rwcp
{
  uint8_t version; // RWCP revision (currently 1)
  uint32_t resolution; // Picoseconds per tick for flux and index timing (default 125,000 i.e. 125 nanoseconds)
  uint8_t reserved[11]; // Reserved for future use, all zeroes
};

struct a2r_rwcp_strm
{
  uint8_t mark; // "C"=Capture, "X"=End of captures
  uint8_t type; // Capture type, 1=Timing, 2=Bits, 3=xtiming
  uint16_t location; // Track where capture happened (type 1 - 5.25" in quarter tracks, others in (track<<1)+side format)
  uint8_t indexcount; // The count of index signals in the index array

  // Following this ..
  //   an array of uint32_t index positions (in ticks)
  //   capture data size uint32_t
  //   capture data
};

struct a2r_slvd
{
  uint8_t version; // SLVD chunk revision (currently 2)
  uint32_t resolution; // Picoseconds per tick for flux and index timing (default 125,000 i.e. 125 nanoseconds)
  uint8_t reserved[11]; // Reserved for future use, all zeroes
};

struct a2r_slvd_track
{
  uint8_t mark; // "T"=Track, "X"=End of tracks
  uint16_t location; // Track where capture happened (type 1 - 5.25" in quarter tracks, others in (track<<1)+side format)
  uint8_t mirrordistout; // Mirror distance outward, typically only drive type 1, 0=Not mirrored
  uint8_t mirrordistin; // Mirror distance inward
  uint8_t reserved[6]; // Reserved for future use, all zeroes
  uint8_t indexcount; // The count of index signals in the index array

  // Following this ..
  //   an array of uint32_t index positions (in ticks)
  //   flux data size uint32_t
  //   flux data
};

#pragma pack(pop)

extern long a2r_readtrack(FILE *a2rfile, const int track, const int side, unsigned char *buf, const uint32_t buflen);

extern int a2r_readheader(FILE *a2rfile);

#endif
