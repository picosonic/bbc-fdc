#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include <stdint.h>

// For disk/drive status
#define HW_NODRIVE 0
#define HW_NODISK 1
#define HW_HAVEDISK 2

// Drive geometry
#define HW_MAXHEADS 2
#define HW_MAXTRACKS 80

#define HW_NORMALSTEPPING 1
#define HW_DOUBLESTEPPING 2

extern int hw_currenttrack;
extern int hw_currenthead;

extern int hw_stepping;

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