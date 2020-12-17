#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "hardware.h"
#include "diskstore.h"
#include "mod.h"
#include "mfm.h"
#include "amigamfm.h"
#include "pll.h"

int amigamfm_state=MFM_SYNC; // state machine
unsigned int amigamfm_datacells=0; // 16 bit sliding buffer
int amigamfm_bits=0; // Number of used bits within sliding buffer
unsigned int amigamfm_p1, amigamfm_p2, amigamfm_p3=0; // bit history

unsigned long amigamfm_blockpos;

// Output block data buffer, for a single sector
unsigned char amigamfm_bitstream[MFM_BLOCKSIZE];
unsigned int amigamfm_bitlen=0;

// MFM timings
float amigamfm_defaultwindow;
float amigamfm_bucket01, amigamfm_bucket001, amigamfm_bucket0001;

struct PLL *amigamfm_pll;

int amigamfm_debug=0;

// Validate clock bits
void amigamfm_validateclock(const unsigned char clock, const unsigned char data)
{
  (void) clock;
  (void) data;
}

// Extract a header long from MFM stream
unsigned long amigamfm_getlong(const unsigned int longpos, const unsigned int data_size)
{
  unsigned long retval=0;

  unsigned long odd;
  unsigned long even;

  odd=amigamfm_bitstream[longpos];
  odd=(odd<<8)|amigamfm_bitstream[longpos+1];
  odd=(odd<<8)|amigamfm_bitstream[longpos+2];
  odd=(odd<<8)|amigamfm_bitstream[longpos+3];

  even=amigamfm_bitstream[longpos+(data_size*4)];
  even=(even<<8)|amigamfm_bitstream[longpos+(data_size*4)+1];
  even=(even<<8)|amigamfm_bitstream[longpos+(data_size*4)+2];
  even=(even<<8)|amigamfm_bitstream[longpos+(data_size*4)+3];

  retval=(even & AMIGA_MFM_MASK) | ((odd & AMIGA_MFM_MASK) << 1);

  return retval;
}

// Calculate header checksum
unsigned long amigamfm_calchdrsum(const unsigned int longpos, const unsigned int data_size)
{
  unsigned long checksum=0;

  unsigned long odd;
  unsigned long even;

  unsigned int count;
  unsigned int longoffs;

  for (count=0; count<(data_size/4); count++)
  {
    longoffs=longpos+(count*4);

    odd=amigamfm_bitstream[longoffs+0];
    odd=(odd<<8)|amigamfm_bitstream[longoffs+1];
    odd=(odd<<8)|amigamfm_bitstream[longoffs+2];
    odd=(odd<<8)|amigamfm_bitstream[longoffs+3];

    even=amigamfm_bitstream[longoffs+(data_size)+0];
    even=(even<<8)|amigamfm_bitstream[longoffs+(data_size)+1];
    even=(even<<8)|amigamfm_bitstream[longoffs+(data_size)+2];
    even=(even<<8)|amigamfm_bitstream[longoffs+(data_size)+3];

    checksum^=odd;
    checksum^=even;
  }

  return (checksum & AMIGA_MFM_MASK);
}

// Extract a data byte from MFM stream
unsigned char amigamfm_getbyte(const unsigned int bytepos, const unsigned int data_size)
{
  unsigned char retval=0;

  unsigned char odd;
  unsigned char even;

  odd=amigamfm_bitstream[bytepos];

  even=amigamfm_bitstream[bytepos+(data_size)];

  retval=(even & 0x55) | ((odd & 0x55) << 1);

  return retval;
}

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void amigamfm_addbit(const unsigned char bit, const unsigned long datapos)
{
  unsigned char clock, data;

  // Maintain previous 48 bits of data
  amigamfm_p1=((amigamfm_p1<<1)|((amigamfm_p2&0x8000)>>15))&0xffff;
  amigamfm_p2=((amigamfm_p2<<1)|((amigamfm_p3&0x8000)>>15))&0xffff;
  amigamfm_p3=((amigamfm_p3<<1)|((amigamfm_datacells&0x8000)>>15))&0xffff;

  amigamfm_datacells=((amigamfm_datacells<<1)&0xffff);
  amigamfm_datacells|=bit;
  amigamfm_bits++;

  if (amigamfm_bits>=16)
  {
    // Extract clock byte
    clock=mod_getclock(amigamfm_datacells);

    // Extract data byte
    data=mod_getdata(amigamfm_datacells);

    switch (amigamfm_state)
    {
      case MFM_SYNC:
        if ((amigamfm_datacells==0x4489) &&
            (amigamfm_p3==0x4489) &&
            (amigamfm_p2==0xaaaa) &&
            ((amigamfm_p1&0x7fff)==0x2aaa)) // Should be 0xaaaa, but MFM encoding prior to 16th March 1990 had a bug
        {
          if (amigamfm_debug)
            fprintf(stderr, "[%lx] ==AMIGA MFM IDAM/DAM SYNC AAAA AAAA 4489 4489==\n", datapos);

          amigamfm_bits=0;
          amigamfm_bitlen=0; // Clear output buffer

          // Add sync to header buffer
          amigamfm_bitstream[amigamfm_bitlen++]=((amigamfm_p1&0xff00)>>8);
          amigamfm_bitstream[amigamfm_bitlen++]=(amigamfm_p1&0xff);
          amigamfm_bitstream[amigamfm_bitlen++]=((amigamfm_p2&0xff00)>>8);
          amigamfm_bitstream[amigamfm_bitlen++]=(amigamfm_p2&0xff);
          amigamfm_bitstream[amigamfm_bitlen++]=((amigamfm_p3&0xff00)>>8);
          amigamfm_bitstream[amigamfm_bitlen++]=(amigamfm_p3&0xff);

          amigamfm_bitstream[amigamfm_bitlen++]=((amigamfm_datacells&0xff00)>>8);
          amigamfm_bitstream[amigamfm_bitlen++]=(amigamfm_datacells&0xff);

          amigamfm_blockpos=datapos;

          amigamfm_state=MFM_ADDR; // Move on to read header
        }
        else
          amigamfm_bits=16; // Keep looking for sync (preventing overflow)
        break;

      case MFM_ADDR:
        amigamfm_validateclock(clock, data);

        if (amigamfm_bitlen<(AMIGA_SECTOR_SIZE))
        {
          amigamfm_bitstream[amigamfm_bitlen++]=((amigamfm_datacells&0xff00)>>8);
          amigamfm_bitstream[amigamfm_bitlen++]=(amigamfm_datacells&0xff);
          amigamfm_bits=0;
        }
        else
        {
          unsigned long info=amigamfm_getlong(AMIGA_INFO_OFFSET, 1);
          unsigned char format=((info&0xff000000)>>24);
          unsigned char track=((info&0x00ff0000)>>16);
          unsigned char head=track&0x01;
          unsigned char sector=((info&0x0000ff00)>>8);
          unsigned char sectors_to_end=(info&0xff);
          unsigned long hdrsum=amigamfm_getlong(AMIGA_HEADER_CXSUM_OFFSET, 1);
          unsigned long datasum=amigamfm_getlong(AMIGA_DATA_CXSUM_OFFSET, 1);
          unsigned long calchdrsum;
          unsigned long calcdatasum;

          // Split off head bit from track number
          track=track>>1;

          if (amigamfm_debug)
            fprintf(stderr, "INFO = %.8lx\n", info);

          if (format==0xff)
          {
            int bytepos;
            unsigned char sbyte;
            unsigned char hdrCRC;
            unsigned char dataCRC;

            calchdrsum=amigamfm_calchdrsum(AMIGA_INFO_OFFSET, 4);
            calchdrsum^=amigamfm_calchdrsum(AMIGA_SECTOR_LABEL_OFFSET, 16);

            calcdatasum=amigamfm_calchdrsum(AMIGA_DATA_OFFSET, AMIGA_DATASIZE);

            hdrCRC=(hdrsum==calchdrsum)?GOODDATA:BADDATA;
            dataCRC=(datasum==calcdatasum)?GOODDATA:BADDATA;

            if (amigamfm_debug)
            {
              fprintf(stderr, "Format : Amiga v1.0\n");

              fprintf(stderr, "Track:%d Head:%d Sector:%d Sectors_to_end:%d\n", track, head, sector, sectors_to_end);

              fprintf(stderr, "  Header checksum %.8lx (%.8lx) %s\n", hdrsum, calchdrsum, hdrCRC==GOODDATA?"OK":"BAD");
              fprintf(stderr, "  Data checksum %.8lx (%.8lx) %s\n", datasum, calcdatasum, dataCRC==GOODDATA?"OK":"BAD");
            }

            if ((hdrCRC==GOODDATA) && (dataCRC==GOODDATA))
            {
              unsigned char outbuff[MFM_BLOCKSIZE];

              // Record IDAM values
              mfm_idamtrack=track;
              mfm_idamhead=head;
              mfm_idamsector=sector;
              mfm_idamlength=2;

              // Record last known good IDAM values for this track
              mfm_lasttrack=mfm_idamtrack;
              mfm_lasthead=mfm_idamhead;
              mfm_lastsector=mfm_idamsector;
              mfm_lastlength=mfm_idamlength;

              // Extract the sector data
              for (bytepos=0; bytepos<AMIGA_DATASIZE; bytepos++)
              {
                sbyte=amigamfm_getbyte(AMIGA_DATA_OFFSET+bytepos, AMIGA_DATASIZE);
                outbuff[bytepos]=sbyte;
              }

              // Save the sector
              if (diskstore_addsector(MODMFM, hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, amigamfm_blockpos, 0, amigamfm_blockpos, 0, AMIGA_DATASIZE, &outbuff[0], 0)==1)
              {
                if (amigamfm_debug)
                  fprintf(stderr, "** AMIGA MFM new sector T%d H%d - C%d H%d R%d **\n", hw_currenttrack, hw_currenthead, track, head, sector);
              }
            }
          }
          else
          {
            if (amigamfm_debug)
              fprintf(stderr, "Unknown sector format %x\n", format);
          }

          amigamfm_state=MFM_SYNC;
        }
        break;

      default:
        // Unknown state, put it back to SYNC
        amigamfm_p1=0;
        amigamfm_p2=0;
        amigamfm_p3=0;
        amigamfm_bits=0;

        amigamfm_blockpos=0;

        amigamfm_state=MFM_SYNC;
        break;
    }
  }
}

void amigamfm_addsample(const unsigned long samples, const unsigned long datapos, const int usepll)
{
  if (usepll)
  {
    PLL_addsample(amigamfm_pll, samples, datapos);

    return;
  }

  // Does number of samples fit within "01" bucket ..
  if (samples<=amigamfm_bucket01)
  {
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "001" bucket ..
  if (samples<=amigamfm_bucket001)
  {
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "0001" bucket ..
  if (samples<=amigamfm_bucket0001)
  {
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(1, datapos);
  }
  else
  {
    // TODO This shouldn't happen in MFM encoding
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(0, datapos);
    amigamfm_addbit(1, datapos);
  }
}

void amigamfm_init(const int debug, const char density)
{
  float bitcell=MFM_BITCELLDD;
  float diff;

  amigamfm_debug=debug;

  if ((density&MOD_DENSITYMFMED)!=0)
    bitcell=MFM_BITCELLED;

  if ((density&MOD_DENSITYMFMHD)!=0)
    bitcell=MFM_BITCELLHD;

  // Adjust bitcell for RPM
  bitcell=(bitcell/(float)HW_DEFAULTRPM)*hw_rpm;

  // Determine number of samples between "1" pulses (default window)
  amigamfm_defaultwindow=((float)hw_samplerate/(float)USINSECOND)*bitcell;

  if (amigamfm_pll!=NULL)
    PLL_reset(amigamfm_pll, amigamfm_defaultwindow);
  else
    amigamfm_pll=PLL_create(amigamfm_defaultwindow, amigamfm_addbit);

  // From default window, determine ideal sample times for assigning bits "01", "001" or "0001"
  amigamfm_bucket01=amigamfm_defaultwindow;
  amigamfm_bucket001=(amigamfm_defaultwindow/2)*3;
  amigamfm_bucket0001=(amigamfm_defaultwindow/2)*4;

  // Increase bucket sizes to halfway between peaks
  diff=amigamfm_bucket001-amigamfm_bucket01;
  amigamfm_bucket01+=(diff/2);
  amigamfm_bucket001+=(diff/2);
  amigamfm_bucket0001+=(diff/2);

  // Set up MFM parser
  amigamfm_blockpos=0;
  amigamfm_state=MFM_SYNC;
  amigamfm_datacells=0;
  amigamfm_bits=0;

  amigamfm_bitlen=0;

  // Initialise previous data cache
  amigamfm_p1=0;
  amigamfm_p2=0;
  amigamfm_p3=0;
}

void amigamfm_decodedate(const uint32_t days, const uint32_t mins, const uint32_t ticks, struct tm *tim)
{
  struct timeval tv;

  tv.tv_sec=AMIGA_EPOCH+(days*(24*60*60))+(mins*60)+(ticks/50);
  tv.tv_usec=0;

  localtime_r(&tv.tv_sec, tim);
}

uint32_t amigamfm_readlong(const uint32_t offset, const uint8_t *data)
{
  return ((data[offset+0]<<24) | (data[offset+1]<<16) | (data[offset+2]<<8) | (data[offset+3]));
}

void amigamfm_gettitle(char *title, const int titlelen)
{
  int i;

  Disk_Sector *sector0;

  // Search for Root Block sector
  sector0=diskstore_findhybridsector(40, 0, 0);

  // Check we have sector
  if (sector0==NULL)
  {
    char tmpbuff[AMIGA_DATASIZE];

    // We may not have read it yet, so have a go
    diskstore_abstrack=40;
    diskstore_abshead=0;
    diskstore_abssector=0;
    diskstore_abssecoffs=0;

    diskstore_absoluteread(tmpbuff, AMIGA_DATASIZE, 0, 80);

    // Search again
    sector0=diskstore_findhybridsector(40, 0, 0);
  }

  // Check we have sector
  if (sector0==NULL)
    return;

  // Check we have data for sector
  if (sector0->data==NULL)
    return;

  // Check sector is the right length
  if (sector0->datasize==AMIGA_DATASIZE)
  {
    if (titlelen>sector0->data[AMIGA_DATASIZE-0x50])
    {
      for (i=0; i<sector0->data[AMIGA_DATASIZE-0x50]; i++)
        title[i]=sector0->data[(AMIGA_DATASIZE-0x4f)+i];

      title[i]=0;
    }
  }
}

// http://lclevy.free.fr/adflib/adf_info.html
void amigamfm_showinfo(const unsigned int disktracks, const int debug)
{
  int i;
  (void) debug;

  Disk_Sector *sector0;

  // Search for Root Block sector
  sector0=diskstore_findhybridsector(40, 0, 0);

  // Check we have sector
  if (sector0==NULL)
  {
    char tmpbuff[AMIGA_DATASIZE];

    // We may not have read it yet, so have a go
    diskstore_abstrack=40;
    diskstore_abshead=0;
    diskstore_abssector=0;
    diskstore_abssecoffs=0;

    diskstore_absoluteread(tmpbuff, AMIGA_DATASIZE, 0, disktracks);

    // Search again
    sector0=diskstore_findhybridsector(40, 0, 0);
  }

  // Check we have sector
  if (sector0==NULL)
    return;

  // Check we have data for sector
  if (sector0->data==NULL)
    return;

  // Check sector is the right length
  if (sector0->datasize==AMIGA_DATASIZE)
  {
    struct tm tim;

    printf("Rootblock\n");

    printf("Type : %d\n", amigamfm_readlong(0, sector0->data));
    printf("Hash table size : %d\n", amigamfm_readlong(0xc, sector0->data));
    printf("Checksum : %.8x\n", amigamfm_readlong(0x14, sector0->data));

    amigamfm_decodedate(amigamfm_readlong(AMIGA_DATASIZE-0x5c, sector0->data), amigamfm_readlong(AMIGA_DATASIZE-0x58, sector0->data), amigamfm_readlong(AMIGA_DATASIZE-0x54, sector0->data), &tim);
    printf("Root dir changed : %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

    amigamfm_decodedate(amigamfm_readlong(AMIGA_DATASIZE-0x28, sector0->data), amigamfm_readlong(AMIGA_DATASIZE-0x24, sector0->data), amigamfm_readlong(AMIGA_DATASIZE-0x20, sector0->data), &tim);
    printf("Volume changed : %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

    amigamfm_decodedate(amigamfm_readlong(AMIGA_DATASIZE-0x1c, sector0->data), amigamfm_readlong(AMIGA_DATASIZE-0x18, sector0->data), amigamfm_readlong(AMIGA_DATASIZE-0x14, sector0->data), &tim);
    printf("Filesystem creation : %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

    printf("Volume name : \"");
    for (i=0; i<sector0->data[AMIGA_DATASIZE-0x50]; i++)
      printf("%c", sector0->data[(AMIGA_DATASIZE-0x4f)+i]);
    printf("\"\n");

    printf("Extension : %d\n", amigamfm_readlong(AMIGA_DATASIZE-0x8, sector0->data));
    printf("Secondary type : %d\n", amigamfm_readlong(AMIGA_DATASIZE-0x4, sector0->data));

    printf("\n");
  }
}

uint32_t amigamfm_calcbootchecksum(const uint8_t *bootblock)
{
  uint32_t checksum;
  uint32_t presum;
  unsigned int i;

  checksum=0;

  for (i=0; i<AMIGA_BOOTBLOCKSIZE/4; i++)
  {
    presum=checksum;

    checksum+=((bootblock[i*4]<<24) | (bootblock[i*4+1]<<16) | (bootblock[i*4+2]<<8) | (bootblock[i*4+3]));

    if (checksum<presum)
      ++checksum;
  }

  return ~(checksum);
}

int amigamfm_validate()
{
  int format;
  uint8_t sniff[AMIGA_SECTOR_SIZE];
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  format=AMIGA_UNKNOWN;

  // Search for sectors
  sector0=diskstore_findhybridsector(0, 0, 0);
  sector1=diskstore_findhybridsector(0, 0, 1);

  // Check we have sectors
  if ((sector0==NULL) || (sector1==NULL))
    return format;

  // Check we have data for sectors
  if ((sector0->data==NULL) || (sector0->data==NULL))
    return format;

  // Check sector is 512 bytes long
  if ((sector0->datasize==AMIGA_DATASIZE) && (sector1->datasize==AMIGA_DATASIZE))
  {
    // Copy sector data to sniff buffer
    bzero(sniff, sizeof(sniff));

    memcpy(sniff, sector0->data, sector0->datasize);
    memcpy(&sniff[AMIGA_DATASIZE], sector1->data, sector1->datasize);

    // Test for "DOS"
    if ((sniff[0]=='D') &&
        (sniff[1]=='O') &&
        (sniff[2]=='S'))
    {
      uint32_t checksum;
      uint32_t rootblock;

      checksum=sniff[4];
      checksum=(checksum<<8)|sniff[5];
      checksum=(checksum<<8)|sniff[6];
      checksum=(checksum<<8)|sniff[7];

      rootblock=sniff[8];
      rootblock=(rootblock<<8)|sniff[9];
      rootblock=(rootblock<<8)|sniff[10];
      rootblock=(rootblock<<8)|sniff[11];

      if (rootblock==AMIGA_DD_ROOTBLOCK)
      {
        format=AMIGA_DOS_FORMAT;

        //if (amigamfm_debug)
        {
          printf("Amiga DOS found\n");

          if (sniff[3]&0x01)
            printf("FFS (AmigaDOS 2.04), ");
          else
            printf("OFS (AmigaDOS 1.2), ");

          if (sniff[3]&0x02)
            printf("INTL ONLY, ");
          else
            printf("NO_INTL ONLY, ");

          if (sniff[3]&0x04)
            printf("DIRC&INTL\n");
          else
            printf("NO_DIRC&INTL\n");

          // Clear old checksum
          memset(&sniff[4], 0, 4);

          printf("Checksum : %x, calculated %x\n", checksum, amigamfm_calcbootchecksum((uint8_t *)&sniff));
          printf("Rootblock : %d\n", rootblock);
        }
      }
    }
    else if ((sniff[0]=='P') &&
        (sniff[1]=='F') &&
        (sniff[2]=='S'))
    {
      format=AMIGA_PFS_FORMAT;

      printf("Amiga PFS (Professional File System) found\n");
    }
  }

  return format;
}
