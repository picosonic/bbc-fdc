#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "applegcr.h"
#include "hardware.h"
#include "woz.h"
#include "crc32.h"

struct woz_header wozheader;
int woz_is525=0; // Is the capture from a 5.25" disk in SS 40t 0.25 step
uint8_t woz_trackmap[WOZ_MAXTRACKS]; // Index for track data within TRKS chunk

long woz_readtrack(FILE *wozfile, const int track, const int side, char* buf, const uint32_t buflen)
{
  uint16_t toffset;

  // Clear out buffer incase we don't find the right track
  bzero(buf, buflen);

  // If this is a 5.25" image then only process requests for side 0
  if ((woz_is525) && (side!=0))
    return 0;

  // Determine where data for this track should be
  if (woz_is525)
  {
    toffset=track*4;
  }
  else
  {
    toffset=track+(side*80);
  }

  // Make sure it's in range
  if (toffset>=WOZ_MAXTRACKS)
    return 1;

  // Make sure it's not a blank track
  if (woz_trackmap[toffset]==WOZ_NOTRACK)
    return 0;

  if (wozheader.id[3]=='1')
  {
    struct woz_trks1 trks;
    uint32_t i;
    uint8_t j;
    uint32_t bufpos;
    uint8_t outb;
    uint8_t outblen;

    hw_samplerate=(USINSECOND/APPLEGCR_BITCELL)*2;

    fseek(wozfile, (woz_trackmap[toffset]*sizeof(trks))+256, SEEK_SET);
    if (fread(&trks, 1, sizeof(trks), wozfile)<sizeof(trks))
      return -1;

    // Process flux buffer
    bufpos=0; outb=0;
    for (i=0; i<trks.bytesused; i++)
    {
      uint8_t b;

      b=trks.bitstream[i];

      for (j=0; j<BITSPERBYTE; j++)
      {
        if ((b&0x80)==0)
          outb=(outb<<1);
        else
          outb=(outb<<1)|1;

        outb=(outb<<1);

        outblen+=2;

        b<<=1;

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
  else
    return -1;

  return 1;
}

int woz_processinfo(struct woz_chunkheader *chunkheader, FILE *wozfile)
{
  struct woz_info *info;
  info=malloc(chunkheader->chunksize);

  if (info==NULL)
    return 1;

  if (fread(info, 1, chunkheader->chunksize, wozfile)<=0)
  {
    free(info);

    return 1;
  }

  if (info->disktype==1)
    woz_is525=1;

  free(info);

  return 0;
}

int woz_processtmap(struct woz_chunkheader *chunkheader, FILE *wozfile)
{
  if (chunkheader->chunksize!=WOZ_MAXTRACKS)
    return 1;

  if (fread(woz_trackmap, 1, sizeof(woz_trackmap), wozfile)<sizeof(woz_trackmap))
    return 1;

  return 0;
}

int woz_readheader(FILE *wozfile)
{
  long filepos;
  unsigned char buf;
  uint32_t woz_calccrc=0;

  if (wozfile==NULL) return -1;

  if (fread(&wozheader, 1, sizeof(wozheader), wozfile)<=0)
    return -1;

  if (strncmp((char *)&wozheader.id, WOZ_MAGIC1, strlen(WOZ_MAGIC1))!=0)
  {
    if (strncmp((char *)&wozheader.id, WOZ_MAGIC2, strlen(WOZ_MAGIC2))!=0)
      return -1;
  }

  if (wozheader.ff!=0xff)
    return -1;

  if ((wozheader.lfcrlf[0]!=0x0a) || (wozheader.lfcrlf[1]!=0x0d) || (wozheader.lfcrlf[2]!=0x0a))
    return -1;

  // Check CRC32
  filepos=ftell(wozfile);

  while (!feof(wozfile))
  {
    fread(&buf, 1, 1, wozfile);

    if (!feof(wozfile))
      woz_calccrc=CRC32_CalcStream(woz_calccrc, &buf, 1);
  }

  if (wozheader.crc!=woz_calccrc)
    return -1;

  // Looks ok so far, now look for and process INFO chunk
  fseek(wozfile, filepos, SEEK_SET);

  while (!feof(wozfile))
  {
    struct woz_chunkheader chunkheader;

    if (fread(&chunkheader, 1, sizeof(chunkheader), wozfile)<=0)
      return -1;

    if (strncmp((char *)&chunkheader.id, WOZ_CHUNK_INFO, 4)==0)
    {
      if (woz_processinfo(&chunkheader, wozfile)!=0)
        return -1;
      else
        break;
    }
    else
      fseek(wozfile, chunkheader.chunksize, SEEK_CUR);
  }

  // Still ok, now look for and process TMAP chunk
  fseek(wozfile, filepos, SEEK_SET);

  while (!feof(wozfile))
  {
    struct woz_chunkheader chunkheader;

    if (fread(&chunkheader, 1, sizeof(chunkheader), wozfile)<=0)
      return -1;

    if (strncmp((char *)&chunkheader.id, WOZ_CHUNK_TMAP, 4)==0)
    {
      if (woz_processtmap(&chunkheader, wozfile)!=0)
        return -1;
      else
        break;
    }
    else
      fseek(wozfile, chunkheader.chunksize, SEEK_CUR);
  }

  return 0;
}
