#ifndef _FSD_H_
#define _FSD_H_

#include <stdint.h>

#define FSD_UNFORMATTED 0x00
#define FSD_UNREADABLE 0x00
#define FSD_READABLE 0xff

// OSWORD &7F result byte
#define FSD_ERR_NONE 0x00
#define FSD_ERR_NOSECTOR 0x18
#define FSD_ERR_DELETED 0x20
#define FSD_ERR_BADIDCRC 0x0c
#define FSD_ERR_BADDATACRC 0x0e

// Error codes not returned by OSWORD &7F but indicate a length mismatch
#define FSD_ERR_CRC_OK_128 0xe0
#define FSD_ERR_CRC_OK_256 0xe1
#define FSD_ERR_CRC_OK_512 0xe2

#define FSD_CREATORID 0x0a

extern void fsd_write(FILE *fsdfile, const unsigned char tracks, const char *title, const uint8_t sides, const int sidetoread);

#endif
