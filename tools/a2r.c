#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "hardware.h"
#include "a2r.h"

struct a2r_header a2rheader;
int a2r_is525=0; // Is the capture from a 5.25" disk in SS 40t 0.25 step

void a2r_processtiming(struct a2r_strm *stream, FILE *a2rfile, unsigned char *buf, const uint32_t buflen)
{
  uint8_t *buff;
  uint32_t bitgap;

  hw_samplerate=A2R_SAMPLE_RATE;

  buff=malloc(stream->size);

  if (buff!=NULL)
  {
    uint32_t i;
    uint32_t bufpos;
    uint8_t outb;
    uint8_t outblen;

    if (fread(buff, stream->size, 1, a2rfile)==0)
    {
      free(buff);

      return;
    }

    // Process timings buffer
    outblen=0; bufpos=0;
    for (i=0; i<stream->size; i++)
    {
      bitgap=buff[i];

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
//      printf("%d %.2fuS = %d\n", buff[i], (float)(buff[i])/8, bitgap);
    }

    free(buff);
  }
  else
    fseek(a2rfile, stream->size, SEEK_CUR);
}

int a2r_processstream(struct a2r_chunkheader *chunkheader, FILE *a2rfile, const int track, const int side, unsigned char *buf, const uint32_t buflen)
{
  struct a2r_strm stream;
  uint32_t done;

  done=0;

  // Loop through all available stream data
  while (done<chunkheader->size)
  {
    if (fread(&stream, sizeof(stream), 1, a2rfile)==0)
      return 1;

    // Detect end of stream data
    if (stream.location==0xff)
    {
      fseek(a2rfile, 1-sizeof(stream), SEEK_CUR);
      return 0;
    }

    // Check if this is the track/side we want
    if (((a2r_is525==1) && (stream.location==track) && (side==0)) ||
        (((stream.location>>1)==track) && ((stream.location&1)==side)))
    {
      switch (stream.type)
      {
        case 1:
          a2r_processtiming(&stream, a2rfile, buf, buflen);
          break;

        default:
          // Skip over the data
          fseek(a2rfile, stream.size, SEEK_CUR);
          break;
      }
    }
    else
      fseek(a2rfile, stream.size, SEEK_CUR); // Not the right track/side

    done+=(sizeof(stream)+stream.size);
  }

  return 0;
}

long a2r_readtrack(FILE *a2rfile, const int track, const int side, unsigned char *buf, const uint32_t buflen)
{
  // set RPM if available to hw_rpm
  // set resolution/bitrate if available

  // Clear out buffer incase we don't find the right track
  bzero(buf, buflen);

  // Seek to start of chunks
  fseek(a2rfile, sizeof(a2rheader), SEEK_SET);

  // Loop through chunks
  while (!feof(a2rfile))
  {
    struct a2r_chunkheader chunkheader;

    if (fread(&chunkheader, sizeof(chunkheader), 1, a2rfile)==0)
      return -1;

    if (strncmp((char *)&chunkheader.id, A2R_CHUNK_STRM, 4)==0)
    {
      if (a2r_processstream(&chunkheader, a2rfile, track, side, buf, buflen)!=0)
        return -1;
      else
        return 0;
    }
    else
      fseek(a2rfile, chunkheader.size, SEEK_CUR);
  }

  return 0;
}

int a2r_processinfo(struct a2r_chunkheader *chunkheader, FILE *a2rfile)
{
  struct a2r_info *info;
  info=malloc(chunkheader->size);

  if (info==NULL)
    return 1;

  if (fread(info, chunkheader->size, 1, a2rfile)==0)
  {
    free(info);

    return 1;
  }

  if (info->disktype==1)
    a2r_is525=1;

  free(info);

  return 0;
}

int a2r_readheader(FILE *a2rfile)
{
  if (a2rfile==NULL) return -1;

  if (fread(&a2rheader, sizeof(a2rheader), 1, a2rfile)==0)
    return -1;

  if (strncmp((char *)&a2rheader.id, A2R_MAGIC2, strlen(A2R_MAGIC2))!=0)
  {
    if (strncmp((char *)&a2rheader.id, A2R_MAGIC3, strlen(A2R_MAGIC3))!=0)
      return -1;
  }

  if (a2rheader.ff!=0xff)
    return -1;

  if ((a2rheader.lfcrlf[0]!=0x0a) || (a2rheader.lfcrlf[1]!=0x0d) || (a2rheader.lfcrlf[2]!=0x0a))
    return -1;

  // Looks ok so far, now look for and process INFO chunk
  while (!feof(a2rfile))
  {
    struct a2r_chunkheader chunkheader;

    if (fread(&chunkheader, sizeof(chunkheader), 1, a2rfile)==0)
      return -1;

    if (strncmp((char *)&chunkheader.id, A2R_CHUNK_INFO, 4)==0)
    {
      if (a2r_processinfo(&chunkheader, a2rfile)!=0)
        return -1;
      else
        return 0;
    }
    else
      fseek(a2rfile, chunkheader.size, SEEK_CUR);
  }

  return 0;
}
