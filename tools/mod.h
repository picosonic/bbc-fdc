#ifndef _MOD_H_
#define _MOD_H_

#define MOD_HISTOGRAMSIZE 512
#define MOD_PEAKSIZE 5

extern int mod_peak[MOD_PEAKSIZE];
extern int mod_peaks;

unsigned char mod_getclock(const unsigned int datacells);
unsigned char mod_getdata(const unsigned int datacells);

extern void mod_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt);

extern void mod_init(const int debug);

#endif
