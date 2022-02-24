#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "hardware.h"
#include "hfe.h"

struct hfe_header hfeheader;
int hfe_isv3=0;
uint32_t hfe_bitrate;

int hfe_readheader(FILE *hfefile)
{
  if (hfefile==NULL) return -1;

  if (fread(&hfeheader, sizeof(hfeheader), 1, hfefile)==0) return -1;

  if (strncmp((char *)&hfeheader.HEADERSIGNATURE, HFE_MAGIC1, strlen(HFE_MAGIC1))!=0)
  {
    if (strncmp((char *)&hfeheader.HEADERSIGNATURE, HFE_MAGIC3, strlen(HFE_MAGIC3))!=0)
      return -1;
    else
      hfe_isv3=1;
  }

  hfe_bitrate=hfeheader.bitRate;

  if (hfeheader.floppyRPM!=0)
    hw_rpm=hfeheader.floppyRPM;

  return 0;
}

uint8_t hfe_byteflip(uint8_t val)
{
  uint8_t ret = 0;

  if (val & 0x80) ret |= 0x01;
  if (val & 0x40) ret |= 0x02;
  if (val & 0x20) ret |= 0x04;
  if (val & 0x10) ret |= 0x08;
  if (val & 0x08) ret |= 0x10;
  if (val & 0x04) ret |= 0x20;
  if (val & 0x02) ret |= 0x40;
  if (val & 0x01) ret |= 0x80;

  return ret;
}

void hfe_gettrackdata(FILE *hfefile, struct hfe_track *curtrack, const int side, unsigned char *buf, const uint32_t buflen)
{
  uint8_t data[HFE_BLOCKSIZE];
  uint8_t fluxdata;
  uint16_t dataread=0;
  uint16_t pos;

  int skipbits=0;
  int skipbits_len=0;
  int setbitrate=0;
  uint32_t num_bits=8;
  uint32_t bitrate=hfeheader.bitRate;

  uint32_t bitgap=0;
  int i;
  uint8_t b;

  double scalar=(double)(bitrate*1000)/(double)hw_samplerate;

  uint32_t bufpos=0;
  uint8_t outb=0;
  uint8_t outblen=0;

  bzero(buf, buflen);

  if (side>1) return;
  if (feof(hfefile)) return;

  fseek(hfefile, curtrack->offset*HFE_BLOCKSIZE, SEEK_SET);

  while ((dataread<curtrack->track_len) && (!feof(hfefile)))
  {
    int numread;

    numread=fread(&data, 1, sizeof(data), hfefile);
    if (numread==0) return;

    // Convert from HFE timings to flux
    for (pos=0; pos<(HFE_BLOCKSIZE/2); pos++)
    {
      fluxdata=hfe_byteflip(data[(side*(HFE_BLOCKSIZE/2))+pos]);

      // When read v3 files, process opcodes
      if (hfe_isv3==1)
      {
        if (setbitrate)
        {
          setbitrate=0;
          bitrate=((float)HFE_FLOPPY_EMU_FREQ/((float)fluxdata*2)/1000);
          scalar=(double)(bitrate*1000)/(double)hw_samplerate;
          continue;
        }
        else
        if (skipbits)
        {
          skipbits=0;
          skipbits_len=fluxdata;
          continue;
        }
        else
        if (skipbits_len>0)
        {
          num_bits=skipbits_len;
          skipbits_len=0;
          continue;
        }
        else
        if ((fluxdata&HFE_OP_MASK)==HFE_OP_MASK)
        {
          switch (fluxdata)
          {
            case HFE_OP_NOP:
              continue;
              break;

            case HFE_OP_IDX:
              continue;
              break;

            case HFE_OP_BITRATE:
              setbitrate=1;
              continue;
              break;

            case HFE_OP_SKIP:
              skipbits=1;
              continue;
              break;

            case HFE_OP_RAND:
              continue;
              break;

            default:
              continue;
              break;
          }
        }
      }

      for (i=0; i<BITSPERBYTE; i++)
      {
        b=(fluxdata&(1<<(7-i))); // Get next bit from flux data
        bitgap++;

        if (b!=0)
        {
          // bitgap = number of 2uS samples since last flux

          bitgap=(((double)bitgap/scalar))/HFE_US_PER_SAMPLE;

          while (bitgap>0)
          {
            bitgap--;

            outb=(outb<<1)|((bitgap==0)?1:0);
            outblen++;

            if (outblen==BITSPERBYTE)
            {
              if (bufpos<buflen)
                buf[bufpos++]=outb;

              outb=0;
              outblen=0;
            }
          }
        }
      }
    }

    dataread+=numread;
  }
}

long hfe_readtrack(FILE *hfefile, const int track, const int side, unsigned char *buf, const uint32_t buflen)
{
  struct hfe_track curtrack;

  if (hfefile==NULL) return 0;

  // Ensure requested track is in range
  if ((track<0) || (track>=hfeheader.number_of_track)) return 0;

  // Seek to the offset for this track
  fseek(hfefile, (hfeheader.track_list_offset*HFE_BLOCKSIZE)+(track*(sizeof(curtrack))), SEEK_SET);
  if (fread(&curtrack, sizeof(curtrack), 1, hfefile)==0)
    return 0;

  // Fetch flux data
  hfe_gettrackdata(hfefile, &curtrack, side, buf, buflen);

  return 0;
}
