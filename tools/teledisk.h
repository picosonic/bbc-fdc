#ifndef _TELEDISK_H_
#define _TELEDISK_H_

#include <stdint.h>

#define TELEDISK_POLYNOMIAL 0xa097

#define TELEDISK_LAST_TRACK 0xff

// Sector flags
#define TELEDISK_FLAGS_REPEAT      0x01
#define TELEDISK_FLAGS_CRCERROR    0x02
#define TELEDISK_FLAGS_DELDATA     0x04
#define TELEDISK_FLAGS_UNALLOCATED 0x10
#define TELEDISK_FLAGS_NODATA      0x20
#define TELEDISK_FLAGS_NOID        0x40

#pragma pack(push,1)

struct header_s
{
  uint8_t signature[2];
  uint8_t sequence; // Volume sequence number, starting at 0
  uint8_t checkseq;
  uint8_t version;
  uint8_t datarate; // Density
  uint8_t drivetype; // BIOS drive type, int 13/AH=08
  uint8_t stepping;
  uint8_t dosflag;
  uint8_t sides;
  uint16_t crc; // of first 10 bytes of header, polynomial 0xa097, input preset 0
};

struct comment_s
{
  uint16_t crc; // of whole comment block and comment data excluding this crc
  uint16_t datalen; // length of comment data
  uint8_t year; // Year since 1900
  uint8_t month; // 0=January, 11=December
  uint8_t day; // 1..31
  uint8_t hour; // 24hr
  uint8_t minute;
  uint8_t second;
};

struct track_s
{
  uint8_t sectors; // sectors found on this track, 0xff means no more tracks
  uint8_t track; // physical track, 0 based
  uint8_t head; // physical head, 0 or 1
  uint8_t crc; // lower byte of 16-bit crc for preceding 3 bytes
};

struct sector_s
{
  uint8_t track; // logical track
  uint8_t head; // logical head
  uint8_t sector; // logical sector number
  uint8_t size; // sector data size, 0..6 (for 128 bytes to 8192 bytes)
  uint8_t flags; // capture process flags
  uint8_t crc; // lower byte of 16-bit crc for preceding 5 bytes, or unpacked data block (when used)
};

struct data_s
{
  uint16_t blocksize;
  uint8_t encoding; // 0=Raw, 1=Repeating pattern, 2=RLE
};

struct datarepeat_s
{
  uint16_t repcount;
  uint16_t repdata;
};

#pragma pack(pop)

extern void td0_write(FILE *td0file, const unsigned char tracks, const char *title);

#endif
