#ifndef _MFM_H_
#define _MFM_H_

// Microseconds in a bitcell window for double density MFM
#define MFM_BITCELLDD 4
// Microseconds in a bitcell window for high density MFM
#define MFM_BITCELLHD 2
// Microseconds in a bitcell window for extra-high density MFM
#define MFM_BITCELLED 1

// MFM Block types
#define MFM_BLOCKNULL 0x00
#define MFM_BLOCKINDEX 0xfc
#define MFM_BLOCKADDR 0xfe
#define MFM_BLOCKDATA 0xfb
#define MFM_BLOCKDELDATA 0xf8
// Alternate MFM Block types
#define MFM_ALTBLOCKADDR 0xff
#define MFM_ALTBLOCKDELDATA 0xf9
#define MFM_ALTBLOCKDATA 0xfa
#define MFM_RX02BLOCKDATA 0xfd

// Intel M2FM Block types
#define M2FM_BLOCKINDEX 0x0c
#define M2FM_BLOCKADDR 0x0e
#define M2FM_BLOCKDATA 0x0b
#define M2FM_BLOCKDELDATA 0x08

// HP M2FM Block types
#define M2FM_HPBLOCKADDR 0x70
#define M2FM_HPBLOCKBADTRACK 0xf0
#define M2FM_HPBLOCKDATA 0x50
#define M2FM_HPBLOCKECC 0xd0

// Maximum supported sector size
#define MFM_BLOCKSIZE (16384+5)

// State machine
#define MFM_SYNC 1
#define MFM_MARK 2
#define MFM_ADDR 3
#define MFM_DATA 4

extern int mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength; 
extern int mfm_lasttrack, mfm_lasthead, mfm_lastsector, mfm_lastlength;

extern void mfm_addsample(const unsigned long samples, const unsigned long datapos);

extern void mfm_init(const int debug, const char density);

#endif
