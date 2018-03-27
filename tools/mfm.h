#ifndef _MFM_H_
#define _MFM_H_

// Microseconds in a bitcell window for double-density MFM
#define MFM_BITCELLDD 2
// Microseconds in a bitcell window for high-density MFM
#define MFM_BITCELLHD 1

// MFM Block types
#define MFM_BLOCKNULL 0x00

// Maximum supported sector size
#define MFM_BLOCKSIZE (16384+5)

// State machine
#define MFM_SYNC 1
#define MFM_MARK 2
#define MFM_ADDR 3
#define MFM_DATA 4

extern int mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength; 
extern int mfm_lasttrack, mfm_lasthead, mfm_lastsector, mfm_lastlength;

extern void mfm_process(const unsigned char *sampledata, const unsigned long samplesize, const long bitcell, const int attempt);

extern void mfm_init(const int debug);

#endif
