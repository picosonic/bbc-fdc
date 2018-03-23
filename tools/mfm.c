#include <stdio.h>

#include "crc.h"
#include "hardware.h"
#include "diskstore.h"
#include "mod.h"
#include "mfm.h"

int mfm_state=MFM_SYNC; // state machine
unsigned int mfm_datacells=0; // 16 bit sliding buffer
int mfm_bits=0; // Number of used bits within sliding buffer
unsigned int mfm_p1, mfm_p2, mfm_p3=0; // bit history

// Most recent address mark
unsigned long mfm_idpos, mfm_blockpos;
int mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength; // IDAM values
int mfm_lasttrack, mfm_lasthead, mfm_lastsector, mfm_lastlength;
unsigned char mfm_blocktype;
unsigned int mfm_blocksize;
unsigned int mfm_idblockcrc, mfm_datablockcrc, mfm_bitstreamcrc;

// Output block data buffer, for a single sector
unsigned char mfm_bitstream[MFM_BLOCKSIZE];
unsigned int mfm_bitlen=0;

// Processing position within the sample buffer
unsigned long mfm_datapos=0;

int mfm_debug=0;

unsigned char mfm_getclock(const unsigned int datacells)
{
  unsigned char clock;

  clock=((datacells&0x8000)>>8);
  clock|=((datacells&0x2000)>>7);
  clock|=((datacells&0x0800)>>6);
  clock|=((datacells&0x0200)>>5);
  clock|=((datacells&0x0080)>>4);
  clock|=((datacells&0x0020)>>3);
  clock|=((datacells&0x0008)>>2);
  clock|=((datacells&0x0002)>>1);

  return clock;
}

unsigned char mfm_getdata(const unsigned int datacells)
{
  unsigned char data;

  data=((datacells&0x4000)>>7);
  data|=((datacells&0x1000)>>6);
  data|=((datacells&0x0400)>>5);
  data|=((datacells&0x0100)>>4);
  data|=((datacells&0x0040)>>3);
  data|=((datacells&0x0010)>>2);
  data|=((datacells&0x0004)>>1);
  data|=((datacells&0x0001)>>0);

  return data;
}

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void mfm_addbit(const unsigned char bit)
{
  unsigned char clock, data;
  unsigned char dataCRC; // EDC

  // Maintain previous 48 bits of data
  mfm_p1=((mfm_p1<<1)|((mfm_p2&0x8000)>>15))&0xffff;
  mfm_p2=((mfm_p2<<1)|((mfm_p3&0x8000)>>15))&0xffff;
  mfm_p3=((mfm_p3<<1)|((mfm_datacells&0x8000)>>15))&0xffff;

  mfm_datacells=((mfm_datacells<<1)&0xffff);
  mfm_datacells|=bit;
  mfm_bits++;

  if (mfm_bits>=16)
  {
    // Extract clock byte
    clock=mfm_getclock(mfm_datacells);

    // Extract data byte
    data=mfm_getdata(mfm_datacells);

    switch (mfm_state)
    {
      case MFM_SYNC:
        if (mfm_datacells==0x4489) // possibly also 0x5224 ?
        {
          if (mfm_debug)
            fprintf(stderr, "[%lx] ==MFM SYNC==\n", mfm_datapos);

          mfm_bits=0;
          mfm_bitlen=0; // Clear output buffer

          mfm_state=MFM_MARK; // Move on to look for MFM address mark
        }
        else
          mfm_bits=16; // Keep looking for sync (preventing overflow)
        break;

      case MFM_MARK:
        switch (mfm_datacells)
        {
          case 0x5554: // fe
            if (mfm_debug)
              fprintf(stderr, "[%lx] ID Address Mark\n", mfm_datapos);

            mfm_bits=0;
            mfm_datacells=0;
            mfm_blocktype=data;

            mfm_bitlen=0;
            mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p1);
            mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p2);
            mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p3);
            mfm_bitstream[mfm_bitlen++]=data;

            mfm_blocksize=3+1+4+2;

            // Clear IDAM cache incase previous was good and this one is bad
            mfm_idamtrack=-1;
            mfm_idamhead=-1;
            mfm_idamsector=-1;
            mfm_idamlength=-1;

            mfm_state=MFM_ADDR;
            break;

          case 0x5545: // fb
            if (mfm_debug)
              fprintf(stderr, "[%lx] Data Address Mark\n", mfm_datapos);

            // Don't process if don't have a valid preceding IDAM
            if ((mfm_idamtrack!=-1) && (mfm_idamhead!=-1) && (mfm_idamsector!=-1) && (mfm_idamlength!=-1))
            {
              mfm_bits=0;
              mfm_datacells=0;
              mfm_blocktype=data;

              mfm_bitlen=0;
              mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p1);
              mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p2);
              mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p3);
              mfm_bitstream[mfm_bitlen++]=data;

              mfm_state=MFM_DATA;
            }
            else
            {
              mfm_blocktype=MFM_BLOCKNULL;
              mfm_bitlen=0;
              mfm_state=MFM_SYNC;
            }
            break;

          case 0x554a: // f8
            if (mfm_debug)
              fprintf(stderr, "[%lx] Deleted Data Address Mark\n", mfm_datapos);

            // Don't process if don't have a valid preceding IDAM
            if ((mfm_idamtrack!=-1) && (mfm_idamhead!=-1) && (mfm_idamsector!=-1) && (mfm_idamlength!=-1))
            {
              mfm_bits=0;
              mfm_datacells=0;
              mfm_blocktype=data;

              mfm_bitlen=0;
              mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p1);
              mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p2);
              mfm_bitstream[mfm_bitlen++]=mfm_getdata(mfm_p3);
              mfm_bitstream[mfm_bitlen++]=data;

              mfm_state=MFM_DATA;
            }
            else
            {
              mfm_blocktype=MFM_BLOCKNULL;
              mfm_bitlen=0;
              mfm_state=MFM_SYNC;
            }
            break;

          default:
            mfm_bits=16; // Keep looking for MFM address mark (preventing overflow)
            break;
        }
        break;

      case MFM_ADDR:
        if (mfm_bitlen<mfm_blocksize)
        {
          mfm_bitstream[mfm_bitlen++]=data;
          mfm_bits=0;
        }
        else
        {
          mfm_idblockcrc=calc_crc(&mfm_bitstream[0], mfm_bitlen-2);
          mfm_bitstreamcrc=(((unsigned int)mfm_bitstream[mfm_bitlen-2]<<8)|mfm_bitstream[mfm_bitlen-1]);
          dataCRC=(mfm_idblockcrc==mfm_bitstreamcrc)?GOODDATA:BADDATA;

          if (mfm_debug)
          {
            fprintf(stderr, "Track %.02d ", mfm_bitstream[4]);
            fprintf(stderr, "Head %d ", mfm_bitstream[5]);
            fprintf(stderr, "Sector %.02d ", mfm_bitstream[6]);
            fprintf(stderr, "Data size %d ", mfm_bitstream[7]);
            fprintf(stderr, "CRC %.2x%.2x ", mfm_bitstream[mfm_bitlen-2], mfm_bitstream[mfm_bitlen-1]);

            if (dataCRC==GOODDATA)
              fprintf(stderr, "OK\n");
            else
              fprintf(stderr, "BAD (%.4x)\n", mfm_idblockcrc);
          }

          if (dataCRC==GOODDATA)
          {
            // Record IDAM values
            mfm_idamtrack=mfm_bitstream[4];
            mfm_idamhead=mfm_bitstream[5];
            mfm_idamsector=mfm_bitstream[6];
            mfm_idamlength=mfm_bitstream[7];

            // Record last known good IDAM values for this track
            mfm_lasttrack=mfm_idamtrack;
            mfm_lasthead=mfm_idamhead;
            mfm_lastsector=mfm_idamsector;
            mfm_lastlength=mfm_idamlength;

            // Sanitise data block length
            switch(mfm_idamlength)
            {
              case 0x00: // 128
              case 0x01: // 256
              case 0x02: // 512
              case 0x03: // 1024
              case 0x04: // 2048
              case 0x05: // 4096
              case 0x06: // 8192
              case 0x07: // 16384
                mfm_blocksize=3+1+(128<<mfm_idamlength)+2;
                break;

              default:
                if (mfm_debug)
                  fprintf(stderr, "Invalid record length %.2x\n", mfm_idamlength);

                mfm_state=MFM_SYNC;
                break;
            }
          }

          mfm_state=MFM_SYNC;
        }
        break;

      case MFM_DATA:
        if (mfm_bitlen<mfm_blocksize)
        {
          mfm_bitstream[mfm_bitlen++]=data;
          mfm_bits=0;
        }
        else
        {
          mfm_datablockcrc=calc_crc(&mfm_bitstream[0], mfm_bitlen-2);
          mfm_bitstreamcrc=(((unsigned int)mfm_bitstream[mfm_bitlen-2]<<8)|mfm_bitstream[mfm_bitlen-1]);
          dataCRC=(mfm_datablockcrc==mfm_bitstreamcrc)?GOODDATA:BADDATA;

          if (mfm_debug)
          {
            fprintf(stderr, "DATA block ");
            fprintf(stderr, "CRC %.2x%.2x ", mfm_bitstream[mfm_bitlen-2], mfm_bitstream[mfm_bitlen-2]);

            if (dataCRC==GOODDATA)
              fprintf(stderr, "OK\n");
            else
              fprintf(stderr, "BAD (%.4x)\n", mfm_datablockcrc);
          }

          if (dataCRC==GOODDATA)
          {
            if (diskstore_addsector(MODMFM, hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idblockcrc, mfm_blocktype, mfm_blocksize-3-1-2, &mfm_bitstream[4], mfm_datablockcrc)==1)
            {
              if (mfm_debug)
                fprintf(stderr, "** MFM new sector T%d H%d - C%d H%d R%d N%d - IDCRC %.4x DATACRC %.4x **\n", hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idblockcrc, mfm_datablockcrc);
            }
          }

          mfm_state=MFM_SYNC;
        }
        break;

      default:
        // Unknown state, put it back to SYNC
        mfm_p1=0;
        mfm_p2=0;
        mfm_p3=0;
        mfm_bits=0;

        mfm_state=MFM_SYNC;
        break;
    }
  }
}

void mfm_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt)
{
  int j;
  char level,bi=0;
  unsigned char c, clock, data;
  int count;

  mfm_state=MFM_SYNC;

  level=(sampledata[0]&0x80)>>7;
  bi=level;
  count=0;
  mfm_datacells=0;

  // Initialise last sector IDAM to invalid
  mfm_idamtrack=-1;
  mfm_idamhead=-1;
  mfm_idamsector=-1;
  mfm_idamlength=-1;

  // Initialise last known good IDAM to invalid
  mfm_lasttrack=-1;
  mfm_lasthead=-1;
  mfm_lastsector=-1;
  mfm_lastlength=-1;

  mfm_blocksize=0;
  mfm_blocktype=MFM_BLOCKNULL;
  mfm_idpos=0;

  for (mfm_datapos=0; mfm_datapos<samplesize; mfm_datapos++)
  {
    c=sampledata[mfm_datapos];

    // Fill in missing sample between SPI bytes
//    count++;

    for (j=0; j<BITSPERBYTE; j++)
    {
      bi=((c&0x80)>>7);

      count++;

      if (bi!=level)
      {
        level=1-level;

        // Look for rising edge
        if (level==1)
        {
          // DD
          if ((count>34) && (count<54))
          {
            mfm_addbit(0);
            mfm_addbit(1);
          }
          else
          if ((count>56) && (count<76))
          {
            mfm_addbit(0);
            mfm_addbit(0);
            mfm_addbit(1);
          }
          else
          if ((count>80) && (count<96))
          {
            mfm_addbit(0);
            mfm_addbit(0);
            mfm_addbit(0);
            mfm_addbit(1);
          }

/*
          // HD
          if ((count>15) && (count<29)) // 22
          {
            mfm_addbit(0);
            mfm_addbit(1);
          }
          else
          if ((count>29) && (count<43)) // 35
          {
            mfm_addbit(0);
            mfm_addbit(0);
            mfm_addbit(1);
          }
          else
          if ((count>43) && (count<57)) // 47
          {
            mfm_addbit(0);
            mfm_addbit(0);
            mfm_addbit(0);
            mfm_addbit(1);
          }
*/
          count=0;
        }
      }

      c=c<<1;
    }
  }
}

void mfm_init(const int debug)
{
  mfm_debug=debug;

  mfm_state=MFM_SYNC;
  mfm_datacells=0;
  mfm_bits=0;

  mfm_idamtrack=-1;
  mfm_idamhead=-1;
  mfm_idamsector=-1;
  mfm_idamlength=-1;

  mfm_lasttrack=-1;
  mfm_lasthead=-1;
  mfm_lastsector=-1;
  mfm_lastlength=-1;

  mfm_blocktype=MFM_BLOCKNULL;
  mfm_blocksize=0;

  mfm_idblockcrc=0;
  mfm_datablockcrc=0;
  mfm_bitstreamcrc=0;

  mfm_bitlen=0;

  mfm_datapos=0;

  mfm_p1=0;
  mfm_p2=0;
  mfm_p3=0;
}
