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
  uint8_t signature[2]; // File identification, "TD"=normal, "td"=compressed
  uint8_t sequence; // Volume sequence number, starting at 0
  uint8_t checkseq; // Common to all files in a sequence
  uint8_t version; // In decimal * 10, e.g. v1.5 is 15 in decimal
  uint8_t datarate; // Source density of drive
  uint8_t drivetype; // BIOS drive type, int 13/AH=08
  uint8_t stepping; // Disk track density vs drive track density, 0=1:1, 1=1:2, 2=2:1
  uint8_t dosflag; // When DOS disk processed, 0=all sectors imaged, >0=only allocated sectors saved
  uint8_t sides; // Number of sides disk media has, 1=single-sided, 2=double-sided
  uint16_t crc; // of first 10 bytes of header, polynomial 0xa097, input preset 0
};

struct comment_s
{
  uint16_t crc; // of whole comment block and comment data excluding this crc
  uint16_t datalen; // length of comment data
  uint8_t year; // Year since 1900
  uint8_t month; // 0=January, 11=December
  uint8_t day; // 1..31
  uint8_t hour; // 24hr, 00..23
  uint8_t minute; // 00..59
  uint8_t second; // 00..59
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
  uint8_t size; // logical sector data size, usually 0..6 (for 128 bytes to 8192 bytes)
  uint8_t flags; // capture process "syndrome" flags
  uint8_t crc; // lower byte of 16-bit crc for preceding 5 bytes, or unpacked data block (when used)
};

struct data_s
{
  uint16_t blocksize; // Size of sector data when unpacked
  uint8_t encoding; // 0=Raw, 1=Repeating pattern, 2=RLE
};

struct datarepeat_s
{
  uint16_t repcount; // Number of times to repeat sequence, usually (sector size / 2)
  uint16_t repdata; // Two bytes to repeat to regenerate sector data
};

#pragma pack(pop)

extern void td0_write(FILE *td0file, const unsigned char tracks, const char *title);

#endif
