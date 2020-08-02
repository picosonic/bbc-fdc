#ifndef _SCP_H_
#define _SCP_H_

#include <stdio.h>

#define SCP_MAGIC "SCP"
#define SCP_VERSION 0x22

#define SCP_TRACK "TRK"

#define SCP_FLAGS_INDEX 0x01
#define SCP_FLAGS_96TPI 0x02
#define SCP_FLAGS_360RPM 0x04
#define SCP_FLAGS_NORMALISED 0x08
#define SCP_FLAGS_RW 0x10
#define SCP_FLAGS_FOOTER 0x20

extern void scp_writeheader(FILE *scpfile, const unsigned int rotations, const unsigned int starttrack, const unsigned int endtrack, const float rpm, const unsigned int sides);

extern void scp_writetrack(FILE *scpfile, const int track, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const unsigned int rotations);

extern void scp_finalise(FILE *scpfile, const unsigned int endtrack);

#endif
