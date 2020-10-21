#ifndef _APPLEGCR_H_
#define _APPLEGCR_H_

// State machine
#define APPLEGCR_IDLE 0
#define APPLEGCR_ID 1
#define APPLEGCR_DATA 2

// GCR encoding types
#define APPLEGCR_DATA_53 410
#define APPLEGCR_DATA_62 342

#define APPLEGCR_SECTORLEN 256

// Ideal bitcell width at 300 RPM
#define APPLEGCR_BITCELL 4

extern int applegcr_idamtrack, applegcr_idamsector;
extern int applegcr_lasttrack, applegcr_lastsector;

extern void applegcr_addsample(const unsigned long samples, const unsigned long datapos, const int usepll);

extern void applegcr_init(const int debug, const char density);

#endif
