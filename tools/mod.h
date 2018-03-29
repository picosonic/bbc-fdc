#ifndef _MOD_H_
#define _MOD_H_

#define MOD_HISTOGRAMSIZE 512
#define MOD_PEAKSIZE 5

#define MOD_DENSITYAUTO 0
#define MOD_DENSITYFMSD 1
#define MOD_DENSITYMFMDD 2
#define MOD_DENSITYMFMHD 4
#define MOD_DENSITYMFMED 8

extern int mod_peak[MOD_PEAKSIZE];
extern int mod_peaks;
extern char mod_density;

unsigned char mod_getclock(const unsigned int datacells);
unsigned char mod_getdata(const unsigned int datacells);

extern float mod_samplestoms(const long samples);

extern void mod_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt);

extern void mod_init(const int debug);

#endif
