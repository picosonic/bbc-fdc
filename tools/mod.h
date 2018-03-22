#ifndef _MOD_H_
#define _MOD_H_

#define MOD_HISTOGRAMSIZE 512

extern void mod_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt);

extern void mod_init(const int debug);

#endif
