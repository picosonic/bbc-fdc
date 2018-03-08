#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>

#include "rfi.h"

// Write file metadata
void rfi_writeheader(FILE *rfifile, const int tracks, const int sides, const long rate, const unsigned char writeable)
{
  struct tm tim;
  struct timeval tv;

  if (rfifile==NULL) return;

  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tim);

  fprintf(rfifile, "%s", RFI_MAGIC);

  fprintf(rfifile, "{date:\"%02d/%02d/%d\",time:\"%02d:%02d:%02d\",tracks:%d,sides:%d,rate:%ld,writeable:%d}", tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900, tim.tm_hour, tim.tm_min, tim.tm_sec, tracks, sides, rate, writeable);
}

// RLE encode raw binary sample data
unsigned long rfi_rleencode(unsigned char *rlebuffer, const unsigned long maxrlelen, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  unsigned long rlelen=0;
  unsigned char c;
  char state=0;
  unsigned int i, j;
  int count=0;

  // Determine starting sample level
  state=(rawtrackdata[0]&0x80)>>7;

  // If not starting at zero, then record a 0 count
  if (state!=0)
    rlebuffer[rlelen++]=0;

  for (i=0; i<rawdatalength; i++)
  {
    c=rawtrackdata[i];

    // Assume sample same as previous (missed between SPI bytes)
    count++;

    // Check for sample counter overflow
    if (count>0xff)
    {
      // Check for RLE buffer overflow
      if ((rlelen+2)>=maxrlelen) return 0;

      rlebuffer[rlelen++]=0xff;
      rlebuffer[rlelen++]=0;
      count=0;
    }

    // Process each of the 8 sample bits looking for state change
    for (j=0; j<8; j++)
    {
      count++;

      if (count>0xff)
      {
        // Check for RLE buffer overflow
        if ((rlelen+2)>=maxrlelen) return 0;

        rlebuffer[rlelen++]=0xff;
        rlebuffer[rlelen++]=0;
        count=0;
      }

      if (((c&0x80)>>7)!=state)
      {
        state=1-state;

        // Check for RLE buffer overflow
        if ((rlelen+1)>=maxrlelen) return 0;

        rlebuffer[rlelen++]=count;
        count=0;
      }

      c=c<<1;
    }
  }

  return rlelen;
}

// Write track metadata and track sample data
void rfi_writetack(FILE *rfifile, const int track, const int side, const float rpm, const char *encoding, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  if (rfifile==NULL) return;

  fprintf(rfifile, "{track:%d,side:%d,rpm:%.2f,", track, side, rpm);

  if (strstr(encoding, "raw")!=NULL)
  {
    fprintf(rfifile, "enc:\"%s\",len:%ld}", encoding, rawdatalength);
    fwrite(rawtrackdata, 1, rawdatalength, rfifile);
  }
  else
  if (strstr(encoding, "rle")!=NULL)
  {
    unsigned long rledatalength;
    unsigned char *rledata;

    rledata=malloc(rawdatalength);

    if (rledata!=NULL)
    {
      rledatalength=rfi_rleencode(rledata, rawdatalength, rawtrackdata, rawdatalength);

      fprintf(rfifile, "enc:\"%s\",len:%ld}", encoding, rledatalength);
      fwrite(rledata, 1, rledatalength, rfifile);

      free(rledata);
    }
    else
    {
      fprintf(rfifile, "enc:\"unknown\",len:0}");
    }
  }
  else
  {
    // Don't write any track data for unknown encodings
    fprintf(rfifile, "enc:\"unknown\",len:0}");
  }
}
