#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "diskstore.h"
#include "applegcr.h"
#include "pll.h"

// GCR for Apple II
//
// Apple DOS 3 (29 Jun '78) / 3.1 (20 Jul '78) / 3.2 (16 Feb '79) / 3.2.1 (31 Jul '79)
//   Single side, soft sectored
//   35 tracks, numbered 0 to 34, at 48tpi
//   13 sectors, numbered 0 to 12
//   256 bytes/sector
//   Total sectors/disk 455 (usable 403)
//   Total size 116,480 bytes (113.75k)
//   GCR 5/3
//
//
// Apple DOS 3.3 (25 Aug '80) / Apple Pascal (Aug '79)
// ProDOS (Oct '83) / Apple Fortran / CP/M
//   Single side, soft sectored
//   35 tracks, numbered 0 to 34, at 48tpi
//   16 sectors, numbered 0 to 15
//   256 bytes/sector
//   Total sectors/disk 560 (usable 496)
//   Total size 143,360 bytes (140k)
//   GCR 6/2
//
//
// Sector interleaving / skewing
//
// Physical    DOS3.3     PASCAL     CP/M
//    0          0          0         0
//    1          7          8         B
//    2          E          1         6
//    3          6          9         1
//    4          D          2         C
//    5          5          A         7
//    6          C          3         2
//    7          4          B         D
//    8          B          4         8
//    9          3          C         3
//    A          A          5         E
//    B          2          D         9
//    C          9          6         4
//    D          1          E         F
//    E          8          7         A
//    F          F          F         5

int applegcr_state=APPLEGCR_IDLE; // state machine
uint32_t applegcr_datacells; // 32 bit sliding buffer
int applegcr_bits=0; // Number of used bits within sliding buffer
float applegcr_defaultwindow; // Number of samples in window
float applegcr_threshold01; // Number of samples for an 01
float applegcr_threshold001; // Number of samples for an 001

// Most recent address mark
unsigned long applegcr_idpos, applegcr_blockpos;
int applegcr_idamtrack, applegcr_idamsector; // IDAM values
int applegcr_lasttrack, applegcr_lastsector; // last known good IDAM values
unsigned int applegcr_idblockcrc, applegcr_datablockcrc;

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

struct PLL *applegcr_pll;

int applegcr_debug=0;

void applegcr_buildgcrdecodemaps()
{
  unsigned int i;

  bzero(applegcr_gcr53decodemap, sizeof(applegcr_gcr53decodemap));
  for (i=0; i<sizeof(applegcr_gcr53encodemap); i++)
    applegcr_gcr53decodemap[applegcr_gcr53encodemap[i]]=i;

  bzero(applegcr_gcr62decodemap, sizeof(applegcr_gcr62decodemap));
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
  unsigned int i;

  result=0;
  for (i=0; i<(len/2); i++)
  {
    decoded=applegcr_decode4and4(buff[i*2], buff[(i*2)+1]);
    result=result^decoded;
  }

  return result;
}

// Process data block stored using 6 data bits, 2 extra bits per byte format
//
// * each of the source bytes is cut into two parts: its highest 6 bits and its lowest two;
// * the first 86 bytes of the encoded sector are used to keep the lowest two bits of all bytes;
// * the remaining portions of six bits fill the final 256 on-disk bytes of the sector;
// * an exclusive OR checksum is used, but to reduce decoding time it is applied within the six-bit data
void applegcr_process_data62()
{
  int i;
  unsigned char buff[512];
  unsigned char value;
  unsigned char cx;

  bzero(buff, sizeof(buff));
  bzero(applegcr_decodebuff, sizeof(applegcr_decodebuff));

  // Convert 342+1 disk bytes into 342+1 6-bit GCR
  for (i=0; i<(342+1); i++)
    applegcr_decodebuff[i]=applegcr_gcr62decodemap[applegcr_bytebuff[i]];

  // XOR 342+1 GCR bytes to undo checksum process
  for (i=0; i<(342+1); i++)
  {
    if (i==0)
      applegcr_decodebuff[i]^=0;
    else
      applegcr_decodebuff[i]^=applegcr_decodebuff[i-1];
  }

  cx=applegcr_decodebuff[342];

  if (cx==0)
  {
    // Recombine bits
    for (i=0; i<86; i++)
    {
      value=applegcr_decodebuff[i];

      if (i<84)
        buff[i+172]|=applegcr_bit_reverse[(value>>4) & 0x3];

      buff[i+86]|=applegcr_bit_reverse[(value>>2) & 0x3];
      buff[i]|=applegcr_bit_reverse[(value>>0) & 0x3];
    }

    for (i=86; i<(342+1); i++)
      buff[i-86]|=(applegcr_decodebuff[i]<<2);

    // Check we have an ID
    if ((applegcr_idamtrack!=-1) && (applegcr_idamsector!=-1))
    {
      diskstore_addsector(MODAPPLEGCR, hw_currenttrack, hw_currenthead, applegcr_idamtrack, hw_currenthead, applegcr_idamsector, 1, applegcr_idpos, applegcr_idblockcrc, applegcr_blockpos, applegcr_datamode, APPLEGCR_SECTORLEN, &buff[0], applegcr_decodebuff[342]);
    }
    else
    {
      if (applegcr_debug)
      {
        fprintf(stderr, "** VALID DATA BUT INVALID ID");
        if ((applegcr_lasttrack!=-1) && (applegcr_lastsector!=-1))
          fprintf(stderr, ", last found ID was T%d S%d", applegcr_lasttrack, applegcr_lastsector);

        fprintf(stderr, " **\n");
      }
    }
  }
  else
  {
    if (applegcr_debug)
    {
      fprintf(stderr, "** INVALID DATA EORSUM [%.2x] (%.2x)", applegcr_decodebuff[341], applegcr_decodebuff[342]);
      if ((applegcr_idamtrack!=-1) && (applegcr_idamsector!=-1))
        fprintf(stderr, ", possibly for T%d S%d", applegcr_idamtrack, applegcr_idamsector);

      fprintf(stderr, " **\n");
    }
  }

  // Clear IDAM cache
  applegcr_idamtrack=-1;
  applegcr_idamsector=-1;
}

// Process data block stored using 5 data bits, 3 extra bits per byte format
void applegcr_process_data53()
{
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
              fprintf(stderr, "[%lx] Found a [%.2X] D5 AA B5, DOS 3.2 (5/3) ID\n", datapos, (applegcr_datacells&0xff000000)>>24);

            applegcr_datamode=APPLEGCR_DATA_53;
            applegcr_state=APPLEGCR_ID;
            applegcr_bytelen=0; applegcr_bits=0;

            applegcr_idpos=datapos;

            // Clear IDAM cache
            applegcr_idamtrack=-1;
            applegcr_idamsector=-1;
            break;

          case 0xd5aa96: // Address field / DOS 3.3
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a [%.2X] D5 AA 96, DOS 3.3 (6/2) ID\n", datapos, (applegcr_datacells&0xff000000)>>24);

            applegcr_datamode=APPLEGCR_DATA_62;
            applegcr_state=APPLEGCR_ID;
            applegcr_bytelen=0; applegcr_bits=0;

            applegcr_idpos=datapos;

            // Clear IDAM cache
            applegcr_idamtrack=-1;
            applegcr_idamsector=-1;
            break;

          case 0xd5aaad: // Data field / 342+1 bytes encoded as 6 and 2
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a [%.2X] D5 AA AD, DATA\n", datapos, (applegcr_datacells&0xff000000)>>24);

            applegcr_state=APPLEGCR_DATA;
            applegcr_bytelen=0; applegcr_bits=0;

            applegcr_blockpos=datapos;
            break;

          case 0xdeaaeb: // Epilogue
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a DE AA EB, EPILOGUE\n", datapos);
            break;

          case 0xd4aab7: // Address field / 13 sector / non-standard
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a [%.2X] D4 AA B7, non-standard ID\n", datapos, (applegcr_datacells&0xff000000)>>24);
            break;

          case 0xd4aa96: // Address field / 16 sector / non-standard
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a [%.2X] D4 AA 96, non-standard ID\n", datapos, (applegcr_datacells&0xff000000)>>24);
            break;

          case 0xd5bbcf: // Data field non-standard
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a [%.2X] D5 BB CF, non-standard DATA\n", datapos, (applegcr_datacells&0xff000000)>>24);
            break;

          case 0xdaaaeb: // Epilogue non-standard
            if (applegcr_debug)
              fprintf(stderr, "[%lx] Found a DA AA EB, non-standard EPILOGUE\n", datapos);
            break;

          default:
            break;
        }
      }
      break;

    case APPLEGCR_ID:
      // D5 AA B5 or D5 AA 96 - Prologue
      // VOL VOL - Volume
      // TRK TRK - Track
      // SCT SCT - Sector
      // SUM SUM - Checksum (XOR of previous 6 bytes comprising volume/track/sector)
      // DE AA EB - Epilogue

      if (applegcr_bits==8)
      {
        applegcr_bytebuff[applegcr_bytelen++]=applegcr_datacells&0xff;
        applegcr_bits=0;
      }

      if (applegcr_bytelen>=8)
      {
        if (applegcr_debug)
        {
          fprintf(stderr, " Vol : %d", applegcr_decode4and4(applegcr_bytebuff[0], applegcr_bytebuff[1])); // Defaults to 254
          fprintf(stderr, " Trk : %d", applegcr_decode4and4(applegcr_bytebuff[2], applegcr_bytebuff[3]));
          fprintf(stderr, " Sct : %d", applegcr_decode4and4(applegcr_bytebuff[4], applegcr_bytebuff[5]));
          fprintf(stderr, " Sum : %d", applegcr_decode4and4(applegcr_bytebuff[6], applegcr_bytebuff[7]));
          fprintf(stderr, " EOR : %d\n", applegcr_calc_eor(&applegcr_bytebuff[0], 6));
        }

        if (applegcr_decode4and4(applegcr_bytebuff[6], applegcr_bytebuff[7]) == applegcr_calc_eor(&applegcr_bytebuff[0], 6))
        {
          applegcr_idamtrack=applegcr_decode4and4(applegcr_bytebuff[2], applegcr_bytebuff[3]);
          applegcr_idamsector=applegcr_decode4and4(applegcr_bytebuff[4], applegcr_bytebuff[5]);

          // Record last known good IDAM values for this track
          applegcr_lasttrack=applegcr_idamtrack;
          applegcr_lastsector=applegcr_idamsector;

          applegcr_idblockcrc=applegcr_decode4and4(applegcr_bytebuff[6], applegcr_bytebuff[7]);
        }
        else
        {
          // IDAM failed CRC, ignore following data block (for now)
          applegcr_idpos=0;
          applegcr_idamtrack=-1;
          applegcr_idamsector=-1;
        }

        applegcr_bits=0;
        applegcr_state=APPLEGCR_IDLE;
      }
      break;

    case APPLEGCR_DATA:
      // DOS 3.2
      //
      // D5 AA AD - Prologue
      // 410 bytes, coded 5/8 - 256 bytes of data
      // SUM - Checksum (XOR)
      // DE AA EB - Epilogue

      // DOS 3.3
      //
      // D5 AA AD - Prologue
      // 342 bytes, coded 6/8 - 256 bytes of data
      // SUM - Checksum (XOR)
      // DE AA EB - Epilogue

      if (applegcr_bits==8)
      {
        applegcr_bytebuff[applegcr_bytelen++]=applegcr_datacells&0xff;
        applegcr_bits=0;
      }

      if (applegcr_bytelen>=(applegcr_datamode+1))
      {
        if (applegcr_debug)
          fprintf(stderr, "Processing data block [%d]\n", applegcr_datamode);

        if (applegcr_datamode==APPLEGCR_DATA_62)
          applegcr_process_data62();
        else
          applegcr_process_data53();

        // Require subsequent data blocks to have a valid ID block first
        applegcr_idpos=0;
        applegcr_idamtrack=-1;
        applegcr_idamsector=-1;

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

void applegcr_addsample(const unsigned long samples, const unsigned long datapos, const int usepll)
{
  // 50,100,150
  //   4us, 8us and 12us
  //   1, 01, 001

  if (usepll)
  {
    PLL_addsample(applegcr_pll, samples, datapos);

    return;
  }

  if (samples>applegcr_threshold001)
    applegcr_addbit(0, datapos);

  if (samples>applegcr_threshold01)
    applegcr_addbit(0, datapos);

  applegcr_addbit(1, datapos);
}

void applegcr_init(const int debug, const char density)
{
  float bitcell=APPLEGCR_BITCELL;
  (void) density;

  applegcr_debug=debug;

  // Adjust bitcell for RPM
  bitcell=(bitcell/(float)HW_DEFAULTRPM)*hw_rpm;

  applegcr_defaultwindow=((float)hw_samplerate/(float)USINSECOND)*bitcell;
  applegcr_threshold01=applegcr_defaultwindow*1.5;
  applegcr_threshold001=applegcr_defaultwindow*2.5;

  if (applegcr_pll!=NULL)
    PLL_reset(applegcr_pll, applegcr_defaultwindow);
  else
    applegcr_pll=PLL_create(applegcr_defaultwindow, applegcr_addbit);

  // Set up Apple GCR parser
  applegcr_state=APPLEGCR_IDLE;

  applegcr_buildgcrdecodemaps();

  applegcr_idpos=0;
  applegcr_blockpos=0;

  // Initialise last found sector IDAM to invalid
  applegcr_idamtrack=-1;
  applegcr_idamsector=-1;

  // Initialise last known good sector IDAM to invalid
  applegcr_lasttrack=-1;
  applegcr_lastsector=-1;
}
