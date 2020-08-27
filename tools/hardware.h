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

// For RPM calculation
#define SECONDSINMINUTE 60

// Microseconds in a second
#define USINSECOND 1000000

// Nanoseconds in a second
#define NSINSECOND 1000000000

// Nanoseconds in a microsecond
#define NSINUS 1000

// For buffer calculations
#define BITSPERBYTE 8
#define HW_DEFAULTRPM 300
#define HW_ROTATIONSPERSEC (HW_DEFAULTRPM/SECONDSINMINUTE)

// For SPI clock dividers
#define HW_SPIDIV1024 1024
#define HW_SPIDIV512 512 
#define HW_SPIDIV256 256
#define HW_SPIDIV128 128
#define HW_SPIDIV64 64
#define HW_SPIDIV32 32
#define HW_SPIDIV16 16
#define HW_SPIDIV8 8
#define HW_SPIDIV4 4

#define HW_250MHZ 250000000
#define HW_400MHZ 400000000
#define HW_500MHZ 500000000

extern unsigned int hw_maxtracks;
extern unsigned int hw_currenttrack;
extern unsigned int hw_currenthead;
extern unsigned long hw_samplerate;
extern float hw_rpm;

extern int hw_stepping;

// Initialisation
#ifdef NOPI
extern int hw_init(const char *rawfile, const int spiclockdivider);
#else
extern int hw_init(const int spiclockdivider);
#endif

// Drive control
extern unsigned char hw_detectdisk();
extern void hw_driveselect();
extern void hw_startmotor();
extern void hw_stopmotor();

// Track seeking
extern int hw_attrackzero();
extern void hw_seektotrackzero();
extern void hw_seektotrack(const int track);
extern void hw_sideselect(const int side);
extern void hw_setmaxtracks(const int maxtracks);
extern void hw_seekin();
extern void hw_seekout();

// Signaling and data sampling
extern void hw_waitforindex();
extern int hw_writeprotected();
extern void hw_samplerawtrackdata(char* buf, uint32_t len);
extern void hw_sleep(const unsigned int seconds);
extern float hw_measurerpm();
extern void hw_fixspisamples(char *inbuf, long inlen, char *outbuf, long outlen);

// Clean up
extern void hw_done();

#endif
