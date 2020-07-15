#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "diskstore.h"
#include "applegcr.h"

int applegcr_state=APPLEGCR_IDLE; // state machine
uint32_t applegcr_datacells; // 32 bit sliding buffer
int applegcr_bits=0; // Number of used bits within sliding buffer

unsigned int applegcr_datamode;
const uint8_t applegcr_gcr53encodemap[]=
{
  0xab, 0xad, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xba, // 0x00
  0xbb, 0xbd, 0xbe, 0xbf, 0xd6, 0xd7, 0xda, 0xdb,
  0xdd, 0xde, 0xdf, 0xea, 0xeb, 0xed, 0xee, 0xef, // 0x10
  0xf5, 0xf6, 0xf7, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
};
const uint8_t applegcr_gcr62encodemap[]=
{
  0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6, // 0x00
  0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
  0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc, // 0x10
  0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
  0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, // 0x20
  0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, // 0x30
  0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
unsigned char applegcr_gcr53decodemap[0x100];
unsigned char applegcr_gcr62decodemap[0x100];

const uint8_t applegcr_bit_reverse[] = {0, 2, 1, 3};

unsigned char applegcr_bytebuff[1024];
unsigned int applegcr_bytelen;

unsigned char applegcr_decodebuff[1024];

int applegcr_debug=0;

void applegcr_buildgcrdecodemaps()
{
  unsigned int i;

  memset(applegcr_gcr53decodemap, 0x00, sizeof(applegcr_gcr53decodemap));
  for (i=0; i<sizeof(applegcr_gcr53encodemap); i++)
    applegcr_gcr53decodemap[applegcr_gcr53encodemap[i]]=i;

  memset(applegcr_gcr62decodemap, 0x00, sizeof(applegcr_gcr62decodemap));
  for (i=0; i<sizeof(applegcr_gcr62encodemap); i++)
    applegcr_gcr62decodemap[applegcr_gcr62encodemap[i]]=i;
}

// Odd-Even encoded (this is basically like standard FM)
unsigned char applegcr_decode4and4(unsigned char b1, unsigned char b2)
{
  return (((b1<<1)|1)&b2);
}

// EOR all the (decoded) payload bytes
unsigned char applegcr_calc_eor(unsigned char *buff, unsigned int len)
{
  unsigned char result;
  unsigned char decoded;
  int i;

  result=0;
  for (i=0; i<(len/2); i++)
  {
    decoded=applegcr_decode4and4(buff[i*2], buff[(i*2)+1]);
    result=result^decoded;
  }

  return result;
}

void applegcr_addbit(const unsigned char bit, const unsigned long datapos)
{
  applegcr_datacells=(applegcr_datacells<<1)|bit;
  applegcr_bits++;

  switch (applegcr_state)
  {
    case APPLEGCR_IDLE:
      if (applegcr_bits>=24)
      {
        switch (applegcr_datacells&0xffffff)
        {
          case 0xd5aab5: // Address field / DOS 3.2
            if (applegcr_debug)
              fprintf(stderr, "Found a D5 AA B5, DOS 3.2 (5/3) ID\n");

            applegcr_datamode=APPLEGCR_DATA_53;
            applegcr_state=APPLEGCR_ID;
            applegcr_bytelen=0; applegcr_bits=0;
            break;

          case 0xd5aa96: // Address field / DOS 3.3
            if (applegcr_debug)
              fprintf(stderr, "Found a D5 AA 96, DOS 3.3 (6/2) ID\n");

            applegcr_datamode=APPLEGCR_DATA_62;
            applegcr_state=APPLEGCR_ID;
            applegcr_bytelen=0; applegcr_bits=0;
            break;

          case 0xd5aaad: // Data field / 342+1 bytes encoded as 6 and 2
            if (applegcr_debug)
              fprintf(stderr, "Found a D5 AA AD, DATA\n");

            // applegcr_state=APPLEGCR_DATA;
            applegcr_bytelen=0; applegcr_bits=0;
            break;

          case 0xdeaaeb: // Epilogue
            if (applegcr_debug)
              fprintf(stderr, "Found a DE AA EB, EPILOGUE\n");
            break;

          default:
            break;
        }
      }
      break;

    case APPLEGCR_ID:
      // D5 AA B5 or D5 AA 96
      // VOL VOL
      // TRK TRK
      // SCT SCT
      // SUM SUM
      // DE AA EB

      if (applegcr_bits==8)
      {
        applegcr_bytebuff[applegcr_bytelen++]=applegcr_datacells&0xff;
        applegcr_bits=0;
      }

      if (applegcr_bytelen>=11)
      {
        if (applegcr_debug)
        {
          fprintf(stderr, " Vol : %d", applegcr_decode4and4(applegcr_bytebuff[0], applegcr_bytebuff[1])); // Defaults to 254
          fprintf(stderr, " Trk : %d", applegcr_decode4and4(applegcr_bytebuff[2], applegcr_bytebuff[3]));
          fprintf(stderr, " Sct : %d", applegcr_decode4and4(applegcr_bytebuff[4], applegcr_bytebuff[5]));
          fprintf(stderr, " Sum : %d", applegcr_decode4and4(applegcr_bytebuff[6], applegcr_bytebuff[7]));
          fprintf(stderr, " EOR : %d\n", applegcr_calc_eor(&applegcr_bytebuff[0], 6));
        }

        applegcr_bits=0;
        applegcr_state=APPLEGCR_IDLE;
      }
      break;

    default:
      break;
  }

  // Limit bits used to 32
  if (applegcr_bits>=32)
    applegcr_bits=32;
}

void applegcr_addsample(const unsigned long samples, const unsigned long datapos)
{
  // 50,100,150
  //   4us, 8us and 12us
  //   1, 01, 001

  if (samples>125)
    applegcr_addbit(0, datapos);

  if (samples>75)
    applegcr_addbit(0, datapos);

  applegcr_addbit(1, datapos);
}

void applegcr_init(const int debug, const char density)
{
  applegcr_debug=debug;

  applegcr_state=APPLEGCR_IDLE;

  applegcr_buildgcrdecodemaps();
}
