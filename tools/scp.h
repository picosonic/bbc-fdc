#ifndef _SCP_H_
#define _SCP_H_

#include <stdio.h>

extern void scp_writeheader(FILE *scpfile);

extern void scp_writetrack(FILE *scpfile, const int track, const int side, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const unsigned int rotations);

#endif
