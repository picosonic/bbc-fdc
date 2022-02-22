#ifndef _HFE_H_
#define _HFE_H_

// Copyright (C) 2006-2022 Jean-François DEL NERO
//   https://sourceforge.net/projects/hxcfloppyemu/

#define HFE_MAGIC1 "HXCPICFE"
#define HFE_MAGIC3 "HXCHFEV3"

#define HFE_TRUE 0xff
#define HFE_FALSE 0x00

#define HFE_BLOCKSIZE 512

// HFE v3 opcodes
#define HFE_OP_MASK    0xf0
#define HFE_OP_NOP     0xf0
#define HFE_OP_IDX     0xf1
#define HFE_OP_BITRATE 0xf2
#define HFE_OP_SKIP    0xf3
#define HFE_OP_RAND    0xf4

// Timings
#define HFE_FLOPPY_EMU_FREQ 36000000
#define HFE_US_PER_SAMPLE 2

#pragma pack(push,1)

struct hfe_header
{
  uint8_t  HEADERSIGNATURE[8]; // "HXCPICFE" or "HXCHFEV3"
  uint8_t  formatrevision; // Revision 0
  uint8_t  number_of_track; // Number of tracks
  uint8_t  number_of_side; // Number of valid sides
  uint8_t  track_encoding; // Used for write support
  uint16_t bitRate; // Bitrate in Kbit/s, e.g. 250 = 250000 bit/s (max 500)
  uint16_t floppyRPM; // Rotation per minute
  uint8_t  floppyinterfacemode; // Floppy interface mode
  uint8_t  write_protected; // Disk was write protected (only on v3)
  uint16_t track_list_offset; // Offset of the track list LUT in 512 byte blocks, 1=0x200
  uint8_t  write_allowed; // Floppy image write protected
  uint8_t  single_step; // 0xff : single step, 0x00 double step
  uint8_t  track0s0_altencoding; // 0x00 use alternate track encoding for track 0 side 0
  uint8_t  track0s0_encoding; // alternate track encoding for track 0 side 0
  uint8_t  track0s1_altencoding; // 0x00 use alternate track encoding for track 0 side 1
  uint8_t  track0s1_encoding; // alternate track encoding for track 0 side 1
};

struct hfe_track
{
  uint16_t offset; // Offset of the track data in 512 byte blocks, 2=0x400
  uint16_t track_len; // Length of track data in bytes
};

#pragma pack(pop)

extern struct hfe_header hfeheader;

extern long hfe_readtrack(FILE *hfefile, const int track, const int side, unsigned char *buf, const uint32_t buflen);

extern int hfe_readheader(FILE *hfefile);

#endif
