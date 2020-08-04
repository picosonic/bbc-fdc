#ifndef _SCP_H_
#define _SCP_H_

#define SCP_MAGIC "SCP"
#define SCP_VERSION 0x22

#define SCP_TRACK "TRK"

#define SCP_FLAGS_INDEX 0x01
#define SCP_FLAGS_96TPI 0x02
#define SCP_FLAGS_360RPM 0x04
#define SCP_FLAGS_NORMALISED 0x08
#define SCP_FLAGS_RW 0x10
#define SCP_FLAGS_FOOTER 0x20

#pragma pack(push,1)

struct scp_header
{
  uint8_t magic[3]; // "SCP"
  uint8_t version; // (nibbles major/minor)
  uint8_t disktype;
  uint8_t revolutions; // (1-5)
  uint8_t starttrack; // (0-167)
  uint8_t endtrack; // (0-167)
  uint8_t flags;
  uint8_t bitcellencoding;
  uint8_t heads; // sides
  uint8_t resolution; // 0=25ns, 1=50ns, 2=75ns, 3=100ns, etc.
  uint32_t checksum; // from next byte to EOF
};

#pragma pack(pop)

extern void scp_writeheader(FILE *scpfile, const uint8_t rotations, const uint8_t starttrack, const uint8_t endtrack, const float rpm, const uint8_t sides);

extern void scp_writetrack(FILE *scpfile, const int track, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const unsigned int rotations);

extern void scp_finalise(FILE *scpfile, const unsigned int endtrack);

#endif
