#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include <stdint.h>

// For disk/drive status
#define NODRIVE 0
#define NODISK 1
#define HAVEDISK 2

// Drive geometry
#define MAXHEADS 2
#define MAXTRACKS 80

extern int hw_currenttrack;
extern int hw_currenthead;

// Initialisation
extern int hw_init();

// Drive control
extern unsigned char hw_detectdisk();
extern void hw_driveselect();
extern void hw_startmotor();
extern void hw_stopmotor();

// Track seeking
extern int hw_attrackzero();
extern void hw_seektotrackzero();
extern void hw_sideselect(const int side);

// Signaling and data sampling
extern int hw_writeprotected();
extern void hw_samplerawtrackdata(char* buf, uint32_t len);

// Clean up
extern void hw_done();

#endif
