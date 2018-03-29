#include <stdio.h>

#include "crc.h"
#include "diskstore.h"
#include "dfs.h"
#include "mod.h"
#include "fm.h"
#include "hardware.h"

int fm_state=FM_SYNC; // state machine
unsigned int fm_datacells=0; // 16 bit sliding buffer
int fm_bits=0; // Number of used bits within sliding buffer
unsigned int fm_p1, fm_p2, fm_p3=0;

// Most recent address mark
unsigned long fm_idpos, fm_blockpos;
int fm_idamtrack, fm_idamhead, fm_idamsector, fm_idamlength; // IDAM values
int fm_lasttrack, fm_lasthead, fm_lastsector, fm_lastlength;
unsigned char fm_blocktype;
unsigned int fm_blocksize;
unsigned int fm_idblockcrc, fm_datablockcrc, fm_bitstreamcrc;

// Output block data buffer, for a single sector
unsigned char fm_bitstream[FM_BLOCKSIZE];
unsigned int fm_bitlen=0;

// FM timings
float fm_defaultwindow;
float fm_bucket1, fm_bucket01;

int fm_debug=0;

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void fm_addbit(const unsigned char bit, const unsigned long datapos)
{
  unsigned char clock, data;
  unsigned char dataCRC; // EDC

  // Maintain previous 48 bits of data
  fm_p1=(fm_p1<<1)|((fm_p2&0x8000)>>15);
  fm_p2=(fm_p2<<1)|((fm_p3&0x8000)>>15);
  fm_p3=(fm_p3<<1)|((fm_datacells&0x8000)>>15);

  fm_datacells=((fm_datacells<<1)&0xffff);
  fm_datacells|=bit;
  fm_bits++;

  // Keep processing until we have 8 clock bits + 8 data bits
  if (fm_bits>=16)
  {
    // Extract clock byte, for data this should be 0xff
    clock=mod_getclock(fm_datacells);

    // Extract data byte
    data=mod_getdata(fm_datacells);

    switch (fm_state)
    {
      case FM_SYNC:
        // Detect standard FM address marks
        switch (fm_datacells)
        {
          case 0xf77a: // clock=d7 data=fc
            if (fm_debug)
              fprintf(stderr, "\n[%lx] Index Address Mark\n", datapos);
            fm_blocktype=data;
            fm_bitlen=0;
            fm_state=FM_SYNC;

            // Clear IDAM cache, although I've not seen IAM on Acorn DFS
            fm_idamtrack=-1;
            fm_idamhead=-1;
            fm_idamsector=-1;
            fm_idamlength=-1;
            break;

          case 0xf57e: // clock=c7 data=fe
            if (fm_debug)
              fprintf(stderr, "\n[%lx] ID Address Mark\n", datapos);
            fm_blocktype=data;
            fm_blocksize=6+1;
            fm_bitlen=0;
            fm_bitstream[fm_bitlen++]=data;
            fm_idpos=datapos;
            fm_state=FM_ADDR;

            // Clear IDAM cache incase previous was good and this one is bad
            fm_idamtrack=-1;
            fm_idamhead=-1;
            fm_idamsector=-1;
            fm_idamlength=-1;
            break;

          case 0xf56f: // clock=c7 data=fb
            if (fm_debug)
              fprintf(stderr, "\n[%lx] Data Address Mark, distance from ID %lx\n", datapos, datapos-fm_idpos);

            // Don't process if don't have a valid preceding IDAM
            if ((fm_idamtrack!=-1) && (fm_idamhead!=-1) && (fm_idamsector!=-1) && (fm_idamlength!=-1))
            {
              fm_blocktype=data;
              fm_bitlen=0;
              fm_bitstream[fm_bitlen++]=data;
              fm_blockpos=datapos;
              fm_state=FM_DATA;
            }
            else
            {
              fm_blocktype=FM_BLOCKNULL;
              fm_bitlen=0;
              fm_state=FM_SYNC;
            }
            break;

          case 0xf56a: // clock=c7 data=f8
            if (fm_debug)
              fprintf(stderr, "\n[%lx] Deleted Data Address Mark, distance from ID %lx\n", datapos, datapos-fm_idpos);

            // Don't process if don't have a valid preceding IDAM
            if ((fm_idamtrack!=-1) && (fm_idamhead!=-1) && (fm_idamsector!=-1) && (fm_idamlength!=-1))
            {
              fm_blocktype=data;
              fm_bitlen=0;
              fm_bitstream[fm_bitlen++]=data;
              fm_blockpos=datapos;
              fm_state=FM_DATA;
            }
            else
            {
              fm_blocktype=FM_BLOCKNULL;
              fm_bitlen=0;
              fm_state=FM_SYNC;
            }
            break;

          default:
            // No matching address marks
            break;
        }
        break;

      case FM_ADDR:
        // Keep reading until we have the whole block in fm_bitstream[]
        fm_bitstream[fm_bitlen++]=data;

        if (fm_bitlen==fm_blocksize)
        {
          fm_idblockcrc=calc_crc(&fm_bitstream[0], fm_bitlen-2);
          fm_bitstreamcrc=(((unsigned int)fm_bitstream[fm_bitlen-2]<<8)|fm_bitstream[fm_bitlen-1]);
          dataCRC=(fm_idblockcrc==fm_bitstreamcrc)?GOODDATA:BADDATA;

          if (fm_debug)
          {
            fprintf(stderr, "Track %d (%d) ", fm_bitstream[1], hw_currenttrack);
            fprintf(stderr, "Head %d (%d) ", fm_bitstream[2], hw_currenthead);
            fprintf(stderr, "Sector %d ", fm_bitstream[3]);
            fprintf(stderr, "Data size %d ", fm_bitstream[4]);
            fprintf(stderr, "CRC %.2x%.2x", fm_bitstream[5], fm_bitstream[6]);

            if (dataCRC==GOODDATA)
              fprintf(stderr, " OK\n");
            else
              fprintf(stderr, " BAD (%.4x)\n", fm_idblockcrc);
          }

          if (dataCRC==GOODDATA)
          {
            // Record IDAM values
            fm_idamtrack=fm_bitstream[1];
            fm_idamhead=fm_bitstream[2];
            fm_idamsector=fm_bitstream[3];
            fm_idamlength=fm_bitstream[4];

            // Record last known good IDAM values for this track
            fm_lasttrack=fm_idamtrack;
            fm_lasthead=fm_idamhead;
            fm_lastsector=fm_idamsector;
            fm_lastlength=fm_idamlength;

            // Sanitise data block length
            switch(fm_idamlength)
            {
              case 0x00: // 128
              case 0x01: // 256
              case 0x02: // 512
              case 0x03: // 1024
              case 0x04: // 2048
              case 0x05: // 4096
              case 0x06: // 8192
              case 0x07: // 16384
                fm_blocksize=(128<<fm_idamlength)+3;
                break;

              default:
                if (fm_debug)
                  fprintf(stderr, "Invalid record length %.2x\n", fm_idamlength);

                // Default to DFS standard sector size + (fm_blocktype + (2 x crc))
                fm_blocksize=DFS_SECTORSIZE+3;
                break;
            }
          }
          else
          {
            // IDAM failed CRC, ignore following data block (for now)
            fm_blocksize=0;

            // Clear IDAM cache
            fm_idamtrack=-1;
            fm_idamhead=-1;
            fm_idamsector=-1;
            fm_idamlength=-1;
          }

          fm_state=FM_SYNC;
          fm_blocktype=FM_BLOCKNULL;
        }
        break;

      case FM_DATA:
        // Keep reading until we have the whole block in fm_bitstream[]
        fm_bitstream[fm_bitlen++]=data;

        if (fm_bitlen==fm_blocksize)
        {
          // All the bytes for this "data" block have been read, so process them

          // Calculate CRC (EDC)
          fm_datablockcrc=calc_crc(&fm_bitstream[0], fm_bitlen-2);
          fm_bitstreamcrc=(((unsigned int)fm_bitstream[fm_bitlen-2]<<8)|fm_bitstream[fm_bitlen-1]);

          if (fm_debug)
            fprintf(stderr, "  %.2x CRC %.4x", fm_blocktype, fm_bitstreamcrc);

          dataCRC=(fm_datablockcrc==fm_bitstreamcrc)?GOODDATA:BADDATA;

          // Report and save if the CRC matches
          if (dataCRC==GOODDATA)
          {
            if (fm_debug)
              fprintf(stderr, " OK [%lx]\n", datapos);

            if (diskstore_addsector(MODFM, hw_currenttrack, hw_currenthead, fm_idamtrack, fm_idamhead, fm_idamsector, fm_idamlength, fm_idpos, fm_idblockcrc, fm_blockpos, fm_blocktype, fm_blocksize-3, &fm_bitstream[1], fm_datablockcrc)==1)
            {
              if (fm_debug)
                fprintf(stderr, "** FM new sector T%d H%d - C%d H%d R%d N%d - IDCRC %.4x DATACRC %.4x **\n", hw_currenttrack, hw_currenthead, fm_idamtrack, fm_idamhead, fm_idamsector, fm_idamlength, fm_idblockcrc, fm_datablockcrc);
            }
          }
          else
          {
            if (fm_debug)
              fprintf(stderr, " BAD (%.4x)\n", fm_datablockcrc);
          }

          // Require subsequent data blocks to have a valid ID block first
          fm_idamtrack=-1;
          fm_idamhead=-1;
          fm_idamsector=-1;
          fm_idamlength=-1;

          fm_idpos=0;

          fm_blocktype=FM_BLOCKNULL;
          fm_blocksize=0;
          fm_state=FM_SYNC;
        }
        break;

      default:
        // Unknown state, should never happen
        fm_blocktype=FM_BLOCKNULL;
        fm_blocksize=0;
        fm_state=FM_SYNC;
        break;
    }

    // If waiting for sync, then keep width at 16 bits and continue shifting/adding new bits
    if (fm_state==FM_SYNC)
      fm_bits=16;
    else
      fm_bits=0;
  }
}

void fm_addsample(const unsigned long samples, const unsigned long datapos)
{
  // Does number of samples fit within "1" bucket ..
  if (samples<=fm_bucket1)
  {
    fm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "01" bucket
  if (samples<=fm_bucket01)
  {
   fm_addbit(0, datapos);
   fm_addbit(1, datapos);
  }
  else
  {
    // TODO This shouldn't happen in single-density FM encoding
   fm_addbit(0, datapos);
   fm_addbit(0, datapos);
   fm_addbit(1, datapos);
  }
}

// Initialise the FM parser
void fm_init(const int debug, const char density)
{
  char bitcell=FM_BITCELL;

  fm_debug=debug;

  if ((density&MOD_DENSITYFMSD)==0)
  {
    // TODO cope with different densities of FM
  }

  // Determine number of samples between "1" pulses (default window)
  fm_defaultwindow=((float)hw_samplerate/(float)USINSECOND)*(float)bitcell;

  // From default window, determine bucket sizes for assigning bits "1" or "01"
  fm_bucket1=fm_defaultwindow+(fm_defaultwindow/2);
  fm_bucket01=(fm_defaultwindow*2)+(fm_defaultwindow/2);

  // Set up FM parser
  fm_state=FM_SYNC;
  fm_datacells=0;
  fm_bits=0;

  fm_idpos=0;
  fm_blockpos=0;

  fm_blocktype=FM_BLOCKNULL;
  fm_blocksize=0;

  fm_idblockcrc=0;
  fm_datablockcrc=0;
  fm_bitstreamcrc=0;

  fm_bitlen=0;

  // Initialise previous data cache
  fm_p1=0;
  fm_p2=0;
  fm_p3=0;

  // Initialise last found sector IDAM to invalid
  fm_idamtrack=-1;
  fm_idamhead=-1;
  fm_idamsector=-1;
  fm_idamlength=-1;

  // Initialise last known good sector IDAM to invalid
  fm_lasttrack=-1;
  fm_lasthead=-1;
  fm_lastsector=-1;
  fm_lastlength=-1;
}
