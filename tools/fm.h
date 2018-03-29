#ifndef _FM_H_
#define _FM_H_

// Microseconds in a bitcell window for single-density FM
#define FM_BITCELL 4

// FM Block types
#define FM_BLOCKNULL 0x00
#define FM_BLOCKINDEX 0xfc
#define FM_BLOCKADDR 0xfe
#define FM_BLOCKDATA 0xfb
#define FM_BLOCKDELDATA 0xf8

// Maximum supported sector size
#define FM_BLOCKSIZE (16384+5)

// State machine
#define FM_SYNC 1
#define FM_ADDR 2
#define FM_DATA 3

extern int fm_idamtrack, fm_idamhead, fm_idamsector, fm_idamlength; 
extern int fm_lasttrack, fm_lasthead, fm_lastsector, fm_lastlength;

extern void fm_addsample(const unsigned long samples, const unsigned long datapos);

extern void fm_init(const int debug, const char density);

#endif
