#include <stdio.h>
#include <stdlib.h>

#include "hardware.h"
#include "diskstore.h"
#include "gcr.h"

// GCR for C64
//
// 250k bits/sec (on tracks 31 to 35)
// 35 tracks, numbered 1 to 34
// 683 sectors of 256 bytes
// BAM (Block Availability Map) on track 18, sector 0
// File entries on track 18, sectors 1 to 19

/*
Data  GCR
----  -----
0000  01010
0001  01011
0010  10010
0011  10011
0100  01110
0101  01111
0110  10110
0111  10111

1000  01001
1001  11001
1010  11010
1011  11011
1100  01101
1101  11101
1110  11110
1111  10101
*/

/*
TRACK  BITRATE  BYTES/TRACK  SECTORS/TRACK  PERCENTAGE
 1-17  307682   7692         21              100%
18-24  285714   7143         19             92.8%
25-30  266667   6667         18             86.6%
31-35  250000   6250         17             81.2%
*/

/*
ID

5x   sync 0xff (NOT GCR)
0x00 header 0x08
0x01 cxsum (EOR 0x02..0x05)
0x02 sector
0x03 track
0x04 format id #2
0x05 format id #1
0x06 OFF 0x0f
0x07 OFF 0x0f
8x   fillbytes 0x55 (NOT GCR)
*/

/*
DATA

5x   sync 0xff (NOT GCR)
0x00 header 0x07
0x01..0xnn data (256 bytes)
0xnn+1 cxsum (EOR the 256 data bytes)
0xnn+2 0x00
0xnn+3 0x00
min 4x fillbytes 0x55 (NOT GCR)
*/

unsigned long gcr_bucket1=63;
unsigned long gcr_bucket01=99;

unsigned char gcr_gcrbuffer[1024*1024];
int gcr_gcrlen=0;

unsigned char gcr_bytebuffer[1024*1024];
int gcr_bytelen=0;

int gcr_state=GCR_IDLE; // state machine
unsigned int gcr_datacells; // 16 bit sliding buffer
int gcr_bits=0; // Number of used bits within sliding buffer

// Most recent address mark
unsigned long gcr_idpos, gcr_blockpos;
int gcr_idamtrack, gcr_idamsector; // IDAM values
int gcr_lasttrack, gcr_lastsector; // last known good IDAM values
unsigned int gcr_idblockcrc, gcr_datablockcrc;

int gcr_debug=0;

// Add a 5 bit gcr code to the gcr buffer
void gcr_addgcr(unsigned char gcr)
{
  gcr_gcrbuffer[gcr_gcrlen++]=gcr;
}

// Decode a 5-bit gcr code to 4 bit binary nibble
unsigned char gcr_gcrtonibble(unsigned char gcr)
{
  switch (gcr)
  {
    case 0x0a: return 0;
    case 0x0b: return 1;
    case 0x12: return 2;
    case 0x13: return 3;
    case 0x0e: return 4;
    case 0x0f: return 5;
    case 0x16: return 6;
    case 0x17: return 7;
    case 0x09: return 8;
    case 0x19: return 9;
    case 0x1a: return 10;
    case 0x1b: return 11;
    case 0x0d: return 12;
    case 0x1d: return 13;
    case 0x1e: return 14;
    case 0x15: return 15;
  }

  // Reset on error
  gcr_state=GCR_IDLE;
  gcr_datacells=0;
  gcr_gcrlen=0;
  gcr_bytelen=0;
  gcr_bits=0;

  return 0xff;
}

// Perform an exclusive-or checksum on data
unsigned char eorsum(int start, int end)
{
  unsigned char retval=0;
  int i;

  for (i=start; i<=end; i++)
    retval=retval^gcr_bytebuffer[i];

  return retval;
}

// Decode and process a gcr encoded block
void gcr_decodegcr()
{
  int i, j;
  unsigned char enc;

  unsigned char byteval;
  int n=0;

  unsigned char eorcalc=0;

  unsigned char gcrcode=0;
  int codelen=0;

  for (i=0; i<gcr_gcrlen; i++)
  {
    enc=gcr_gcrbuffer[i];

    for (j=0; j<8; j++)
    {
      gcrcode=(gcrcode<<1)|((enc&0x80)>>7);
      enc=(enc<<1);

      codelen++;

      if (codelen==5)
      {
        unsigned char nibble=gcr_gcrtonibble(gcrcode);

        // Stop processing on GCR error
        if (nibble==0xff)
        {
//          printf("GCR error\n");
          return;
        }

        if (n==0)
        {
          byteval=nibble;
          n++;
        }
        else
        {
          byteval=(byteval<<4)|nibble;

          gcr_bytebuffer[gcr_bytelen++]=byteval;

          n=0;
        }

        codelen=0;
        gcrcode=0;
      }

    }
  }

  // Dump
/*
  if (gcr_debug)
  {
    for (i=0; i<gcr_bytelen; i++)
      fprintf(stderr, "%.2x ", gcr_bytebuffer[i]);

    fprintf(stderr, "\n");
  }
*/

  if (gcr_bytebuffer[0]==0x08)
  {
    eorcalc=eorsum(2, 5);

    // Check the checksum matches before processing
    if (gcr_bytebuffer[1]==eorcalc)
    {
      if (gcr_debug)
      {
        printf("\n  Header : %.2x", gcr_bytebuffer[0]);
        printf("  Checksum : %.2x", gcr_bytebuffer[1]);
        printf("  Sector : %.2d", gcr_bytebuffer[2]);
        printf("  Track : %.2d", gcr_bytebuffer[3]);
        printf("  ID2 : %.2x", gcr_bytebuffer[4]);
        printf("  ID1 : %.2x", gcr_bytebuffer[5]);
        printf("  OF : %.2x", gcr_bytebuffer[6]);
        printf("  OF : %.2x", gcr_bytebuffer[7]);

        printf("  [OK]\n");
      }

      gcr_idamtrack=gcr_bytebuffer[3];
      gcr_idamsector=gcr_bytebuffer[2];

      // Record last known good IDAM values for this track
      gcr_lasttrack=gcr_idamtrack;
      gcr_lastsector=gcr_idamsector;

      gcr_idblockcrc=gcr_bytebuffer[1];
    }
    else
    {
      if (gcr_debug)
        printf("\n** INVALID ID EORSUM [%.2x] (%.2x)\n", gcr_bytebuffer[1], eorcalc);
    }
  }
  else
  if (gcr_bytebuffer[0]==0x07)
  {
    eorcalc=eorsum(1, GCR_SECTORLEN);

    // Check the checksum matches before processing
    if (gcr_bytebuffer[GCR_SECTORLEN+1]==eorcalc)
    {
      if (gcr_debug)
      {
        printf("\nDATA EORSUM OK\n");
        printf("*** GCR good sector");
        if ((gcr_idamtrack!=-1) && (gcr_idamsector!=-1))
          printf(" T%d S%d", gcr_idamtrack, gcr_idamsector);

        printf(" ***\n");
      }

      gcr_datablockcrc=gcr_bytebuffer[GCR_SECTORLEN+1];

      if ((gcr_idamtrack!=-1) && (gcr_idamsector!=-1))
      {
        diskstore_addsector(MODGCR, hw_currenttrack, hw_currenthead, gcr_idamtrack, hw_currenthead, gcr_idamsector, 1, gcr_idpos, gcr_idblockcrc, gcr_blockpos, gcr_bytebuffer[0], GCR_SECTORLEN, &gcr_bytebuffer[1], gcr_datablockcrc);
      }
      else
      {
        if (gcr_debug)
        {
          printf("\n** VALID DATA BUT INVALID ID");
          if ((gcr_lasttrack!=-1) && (gcr_lastsector!=-1))
            printf(", last found ID was T%d S%d", gcr_lasttrack, gcr_lastsector);

          printf(" **\n");
        }
      }
    }
    else
    {
      if (gcr_debug)
      {
        printf("\n** INVALID DATA EORSUM [%.2x] (%.2x)", gcr_bytebuffer[GCR_SECTORLEN+1], eorcalc);
        if ((gcr_idamtrack!=-1) && (gcr_idamsector!=-1))
          printf(", possibly for T%d S%d", gcr_idamtrack, gcr_idamsector);

        printf(" **\n");
      }
    }

    // Clear IDAM cache
    gcr_idamtrack=-1;
    gcr_idamsector=-1;
  }

  gcr_bytelen=0;
}

void gcr_addbit(const unsigned char bit, const unsigned long datapos)
{
  gcr_datacells=((gcr_datacells<<1)&0xffff);
  gcr_datacells|=bit;
  gcr_bits++;

  switch (gcr_state)
  {
    case GCR_IDLE:
      if (gcr_bits>=16)
      {
        if (gcr_datacells==0xff52) // ID
        {
          if (gcr_debug)
            fprintf(stderr, "GCR ID @ 0x%lx\n", datapos);

          gcr_gcrlen=0;
          gcr_addgcr(gcr_datacells & 0xff);

          gcr_state=GCR_ID;

          gcr_datacells=0;
          gcr_bits=0;

          // Clear IDAM cache incase previous was good and this one is bad
          gcr_idamtrack=-1;
          gcr_idamsector=-1;
        }
        else
        if (gcr_datacells==0xff55) // DATA
        {
          if (gcr_debug)
            fprintf(stderr, "GCR DATA @ 0x%lx\n", datapos);

          gcr_gcrlen=0;
          gcr_addgcr(gcr_datacells & 0xff);

          gcr_state=GCR_DATA;

          gcr_datacells=0;
          gcr_bits=0;
        }
      }
      break;

    case GCR_ID:
      if (gcr_bits>=8)
      {
        gcr_addgcr(gcr_datacells & 0xff);

        gcr_bits=0;
        gcr_datacells=0;

        // Check for 10 encoded gcr
        if (gcr_gcrlen==10)
        {
          gcr_decodegcr();

          gcr_state=GCR_IDLE;
        }
      }
      break;

    case GCR_DATA:
      if (gcr_bits>=8)
      {
        gcr_addgcr(gcr_datacells & 0xff);

        gcr_bits=0;
        gcr_datacells=0;

        // Check for 325 encoded gcr
        if (gcr_gcrlen==325)
        {
          gcr_decodegcr();

          gcr_state=GCR_IDLE;
        }
      }
      break;

    default:
      gcr_state=GCR_IDLE;
      break;
  }
}

void gcr_addsample(const unsigned long samples, const unsigned long datapos)
{
  if (hw_currenttrack<=(17*2))
  {
    gcr_bucket1=63;
    gcr_bucket01=99;
  }
  else
  if (hw_currenttrack<=(24*2))
  {
    gcr_bucket1=66;
    gcr_bucket01=106;
  }
  else
  if (hw_currenttrack<=(30*2))
  {
    gcr_bucket1=71;
    gcr_bucket01=114;
  }
  else
  {
    gcr_bucket1=77;
    gcr_bucket01=122;
  }

  if (samples<=gcr_bucket1)
  {
    gcr_addbit(1, datapos);
  }
  else
  if (samples<=gcr_bucket01)
  {
    gcr_addbit(0, datapos);
    gcr_addbit(1, datapos);
  }
  else
  {
    gcr_addbit(0, datapos);
    gcr_addbit(0, datapos);
    gcr_addbit(1, datapos);
  }
}

void gcr_init(const int debug, const char density)
{
  gcr_debug=debug;

  gcr_state=GCR_IDLE;
  gcr_bits=0;
  gcr_datacells=0;

  gcr_idpos=0;
  gcr_blockpos=0;

  gcr_idamtrack=-1;
  gcr_idamsector=-1;
  gcr_lasttrack=-1;
  gcr_lastsector=-1;
}
