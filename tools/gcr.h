#ifndef _GCR_H_
#define _GCR_H_

// State machine
#define GCR_IDLE 0
#define GCR_ID 1
#define GCR_DATA 2

#define GCR_SECTORLEN 256

extern int gcr_idamtrack, gcr_idamsector;
extern int gcr_lasttrack, gcr_lastsector;

extern void gcr_addsample(const unsigned long samples, const unsigned long datapos);

extern void gcr_init(const int debug, const char density);

#endif
