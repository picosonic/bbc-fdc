#include <stdio.h>

#include "crc.h"
#include "hardware.h"
#include "diskstore.h"
#include "mod.h"
#include "mfm.h"
#include "pll.h"

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

// MFM timings
float mfm_defaultwindow;
float mfm_bucket01, mfm_bucket001, mfm_bucket0001;

struct PLL *mfm_pll;

int mfm_debug=0;

// Validate clock bits against data bits
void mfm_validateclock(const unsigned char clock, const unsigned char data)
{
  (void) clock;
  (void) data;

  // TODO
}

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void mfm_addbit(const unsigned char bit, const unsigned long datapos)
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
    clock=mod_getclock(mfm_datacells);

    // Extract data byte
    data=mod_getdata(mfm_datacells);

    switch (mfm_state)
    {
      case MFM_SYNC:
        if (mfm_datacells==0x5224)
        {
          if (mfm_debug)
            fprintf(stderr, "[%lx] ==MFM IAM SYNC 5224==\n", datapos);

          mfm_bits=16; // Keep looking for sync (preventing overflow)
        }
        else
        if (mfm_datacells==0x4489)
        {
          if (mfm_debug)
            fprintf(stderr, "[%lx] ==MFM IDAM/DAM SYNC 4489==\n", datapos);

          mfm_bits=0;
          mfm_bitlen=0; // Clear output buffer

          mfm_state=MFM_MARK; // Move on to look for MFM address mark
        }
        else
          mfm_bits=16; // Keep looking for sync (preventing overflow)
        break;

      case MFM_MARK:
        switch (data)
        {
          case MFM_BLOCKADDR: // fe - IDAM
          case MFM_ALTBLOCKADDR: // ff - Alternative IDAM
          case M2FM_BLOCKADDR: // 0e - Intel M2FM IDAM
          case M2FM_HPBLOCKADDR: // 70 - HP M2FM IDAM
            if (mfm_debug)
              fprintf(stderr, "[%lx] MFM ID Address Mark %.2x\n", datapos, data);

            mfm_bits=0;
            mfm_blocktype=data;

            mfm_bitlen=0;
            mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p1);
            mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p2);
            mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p3);
            mfm_bitstream[mfm_bitlen++]=data;

            mfm_blocksize=3+1+4+2;

            // Clear IDAM cache incase previous was good and this one is bad
            mfm_idamtrack=-1;
            mfm_idamhead=-1;
            mfm_idamsector=-1;
            mfm_idamlength=-1;

            mfm_idpos=datapos;
            mfm_state=MFM_ADDR;
            break;

          case MFM_BLOCKDATA: // fb - DAM
          case MFM_ALTBLOCKDATA: // fa - Alternative DAM
          case MFM_RX02BLOCKDATA: // fd - RX02 M2FM DAM
          case M2FM_BLOCKDATA: // 0b - Intel M2FM DAM
          case M2FM_HPBLOCKDATA: // 50 - HP M2FM DAM
            if (mfm_debug)
              fprintf(stderr, "[%lx] MFM Data Address Mark %.2x\n", datapos, data);

            // Don't process if don't have a valid preceding IDAM
            if ((mfm_idamtrack!=-1) && (mfm_idamhead!=-1) && (mfm_idamsector!=-1) && (mfm_idamlength!=-1))
            {
              mfm_bits=0;
              mfm_blocktype=data;

              mfm_bitlen=0;
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p1);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p2);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p3);
              mfm_bitstream[mfm_bitlen++]=data;

              mfm_blockpos=datapos;
              mfm_state=MFM_DATA;
            }
            else
            {
              mfm_blocktype=MFM_BLOCKNULL;
              mfm_bitlen=0;
              mfm_state=MFM_SYNC;
            }
            break;

          case MFM_BLOCKDELDATA: // f8 - DDAM
          case MFM_ALTBLOCKDELDATA: // f9 - Alternative DDAM
          case M2FM_BLOCKDELDATA: // 08 - Intel M2FM DDAM
            if (mfm_debug)
              fprintf(stderr, "[%lx] MFM Deleted Data Address Mark %.2x\n", datapos, data);

            // Don't process if don't have a valid preceding IDAM
            if ((mfm_idamtrack!=-1) && (mfm_idamhead!=-1) && (mfm_idamsector!=-1) && (mfm_idamlength!=-1))
            {
              mfm_bits=0;
              mfm_blocktype=data;

              mfm_bitlen=0;
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p1);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p2);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p3);
              mfm_bitstream[mfm_bitlen++]=data;

              mfm_blockpos=datapos;
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
            break;
        }
        mfm_bits=0;
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
            fprintf(stderr, "[%lx] MFM Track %.02d ", datapos, mfm_bitstream[4]);
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
          else
          {
            // IDAM failed CRC, ignore following data block (for now)
            mfm_idpos=0;
            mfm_idamtrack=-1;
            mfm_idamhead=-1;
            mfm_idamsector=-1;
            mfm_idamlength=-1;
          }

          mfm_state=MFM_SYNC;
        }
        break;

      case MFM_DATA:
        // Validate clock bits against this data byte
        if (mfm_debug)
          mfm_validateclock(data, clock);

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
            fprintf(stderr, "[%lx] MFM DATA block %.2x ", datapos, mfm_blocktype);
            fprintf(stderr, "CRC %.2x%.2x ", mfm_bitstream[mfm_bitlen-2], mfm_bitstream[mfm_bitlen-2]);

            if (dataCRC==GOODDATA)
              fprintf(stderr, "OK\n");
            else
              fprintf(stderr, "BAD (%.4x)\n", mfm_datablockcrc);
          }

          if (dataCRC==GOODDATA)
          {
            if (diskstore_addsector(MODMFM, hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idpos, mfm_idblockcrc, mfm_blockpos, mfm_blocktype, mfm_blocksize-3-1-2, &mfm_bitstream[4], mfm_datablockcrc)==1)
            {
              if (mfm_debug)
                fprintf(stderr, "** MFM new sector T%d H%d - C%d H%d R%d N%d - IDCRC %.4x DATACRC %.4x **\n", hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idblockcrc, mfm_datablockcrc);
            }
          }

          // Require subsequent data blocks to have a valid ID block first
          mfm_idpos=0;
          mfm_idamtrack=-1;
          mfm_idamhead=-1;
          mfm_idamsector=-1;
          mfm_idamlength=-1;

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

void mfm_addsample(const unsigned long samples, const unsigned long datapos, const int usepll)
{
  if (usepll)
  {
    PLL_addsample(mfm_pll, samples, datapos);

    return;
  }

  // Does number of samples fit within "01" bucket ..
  if (samples<=mfm_bucket01)
  {
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "001" bucket ..
  if (samples<=mfm_bucket001)
  {
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "0001" bucket ..
  if (samples<=mfm_bucket0001)
  {
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
  else
  {
    // TODO This shouldn't happen in MFM encoding
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
}

void mfm_init(const int debug, const char density)
{
  float bitcell=MFM_BITCELLDD;
  float diff;

  mfm_debug=debug;

  if ((density&MOD_DENSITYMFMED)!=0)
    bitcell=MFM_BITCELLED;

  if ((density&MOD_DENSITYMFMHD)!=0)
    bitcell=MFM_BITCELLHD;

  // Adjust bitcell for RPM
  bitcell=(bitcell/(float)HW_DEFAULTRPM)*hw_rpm;

  // Determine number of samples between "1" pulses (default window)
  mfm_defaultwindow=((float)hw_samplerate/(float)USINSECOND)*bitcell;

  if (mfm_pll!=NULL)
    PLL_reset(mfm_pll, mfm_defaultwindow);
  else
    mfm_pll=PLL_create(mfm_defaultwindow, mfm_addbit);

  // From default window, determine ideal sample times for assigning bits "01", "001" or "0001"
  mfm_bucket01=mfm_defaultwindow;
  mfm_bucket001=(mfm_defaultwindow/2)*3;
  mfm_bucket0001=(mfm_defaultwindow/2)*4;

  // Increase bucket sizes to halfway between peaks
  diff=mfm_bucket001-mfm_bucket01;
  mfm_bucket01+=(diff/2);
  mfm_bucket001+=(diff/2);
  mfm_bucket0001+=(diff/2);

  // Set up MFM parser
  mfm_state=MFM_SYNC;
  mfm_datacells=0;
  mfm_bits=0;

  mfm_idpos=0;
  mfm_blockpos=0;

  mfm_blocktype=MFM_BLOCKNULL;
  mfm_blocksize=0;

  mfm_idblockcrc=0;
  mfm_datablockcrc=0;
  mfm_bitstreamcrc=0;

  mfm_bitlen=0;

  // Initialise previous data cache
  mfm_p1=0;
  mfm_p2=0;
  mfm_p3=0;

  // Initialise last found sector IDAM to invalid
  mfm_idamtrack=-1;
  mfm_idamhead=-1;
  mfm_idamsector=-1;
  mfm_idamlength=-1;

  // Initialise last known good sector IDAM to invalid
  mfm_lasttrack=-1;
  mfm_lasthead=-1;
  mfm_lastsector=-1;
  mfm_lastlength=-1;
}
