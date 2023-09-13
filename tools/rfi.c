#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>

#include "hardware.h"
#include "rfi.h"
#include "jsmn.h"

char *rfi_headerstring = NULL;
unsigned int rfi_headerlen = 0;

// Header metadata
int rfi_tracks = 0;
int rfi_sides = 0;
long rfi_rate = 0;
unsigned char rfi_writeable = 0;

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

int rfi_readheader(FILE *rfifile)
{
  unsigned char buff[4];

  if (rfifile==NULL) return -1;

  // Check for rfi magic
  fseek(rfifile, 0, SEEK_SET);
  bzero(buff, sizeof(buff));

  if (fread(buff, 3, 1, rfifile)==0) return -1;
  if (strcmp((char *)buff, RFI_MAGIC)!=0) return -1;

  // Check for "{"
  if (fread(buff, 1, 1, rfifile)==0) return -1;
  if (buff[0]!='{') return -1;

  // Determine header JSON size by looking for "}"
  while (!feof(rfifile))
  {
    if (fread(buff, 1, 1, rfifile)==0) return -1;

    if (buff[0]=='}')
    {
      jsmn_parser parser;
      int numtokens;

      rfi_headerlen=ftell(rfifile)-3;

      rfi_headerstring=malloc(rfi_headerlen+1);
      if (rfi_headerstring==NULL)
        return -1;

      fseek(rfifile, 3, SEEK_SET);
      if (fread(rfi_headerstring, rfi_headerlen, 1, rfifile)==0)
      {
        free(rfi_headerstring);
        return -1;
      }
      rfi_headerstring[rfi_headerlen]=0;

      jsmn_init(&parser);

      // Quick check for validity and to count the tokens
      numtokens=jsmn_parse(&parser, rfi_headerstring, rfi_headerlen, NULL, 0);

      if (numtokens>0)
      {
        int i;
        jsmntok_t *tokens;

        tokens=malloc(numtokens*sizeof(jsmntok_t));

        if (tokens==0)
        {
          free(rfi_headerstring);
          return -1;
        }

        jsmn_init(&parser);
        numtokens=jsmn_parse(&parser, rfi_headerstring, rfi_headerlen, tokens, numtokens);

        for (i=0; i<numtokens; i++)
        {
          if ((tokens[i].type==JSMN_PRIMITIVE) && (tokens[i].size==1) && ((i+1)<=numtokens))
          {
            char rfic;

            if (strncmp(&rfi_headerstring[tokens[i].start], "tracks", tokens[i].end-tokens[i].start)==0)
            {
              rfic=rfi_headerstring[tokens[i+1].end];
              rfi_headerstring[tokens[i+1].end]=0;

              sscanf(&rfi_headerstring[tokens[i+1].start], "%3d", &rfi_tracks);

              rfi_headerstring[tokens[i+1].end]=rfic;
            }
            else
            if (strncmp(&rfi_headerstring[tokens[i].start], "sides", tokens[i].end-tokens[i].start)==0)
            {
              rfic=rfi_headerstring[tokens[i+1].end];
              rfi_headerstring[tokens[i+1].end]=0;

              sscanf(&rfi_headerstring[tokens[i+1].start], "%1d", &rfi_sides);

              rfi_headerstring[tokens[i+1].end]=rfic;
            }
            else
            if (strncmp(&rfi_headerstring[tokens[i].start], "rate", tokens[i].end-tokens[i].start)==0)
            {
              rfic=rfi_headerstring[tokens[i+1].end];
              rfi_headerstring[tokens[i+1].end]=0;

              if (sscanf(&rfi_headerstring[tokens[i+1].start], "%10ld", &rfi_rate)==1)
                hw_samplerate=rfi_rate;

              rfi_headerstring[tokens[i+1].end]=rfic;
            }
            else
            if (strncmp(&rfi_headerstring[tokens[i].start], "writeable", tokens[i].end-tokens[i].start)==0)
            {
              rfic=rfi_headerstring[tokens[i+1].end];
              rfi_headerstring[tokens[i+1].end]=0;

              sscanf(&rfi_headerstring[tokens[i+1].start], "%1c", &rfi_writeable);

              if (rfi_writeable=='1')
                rfi_writeable=1;
              else
                rfi_writeable=0;

              rfi_headerstring[tokens[i+1].end]=rfic;
            }
          }
        }

        free(tokens);
        free(rfi_headerstring);

        if (rfi_tracks>0)
          return 0;
        else
          return -1;
      }

      free(rfi_headerstring);
    }
  }

  // Got to end of file
  return -1;
}

// RLE encode raw binary sample data
unsigned long rfi_rleencode(unsigned char *rlebuffer, const unsigned long maxrlelen, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  unsigned long rlelen=0;
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
    unsigned char c;

    c=rawtrackdata[i];

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
void rfi_writetrack(FILE *rfifile, const int track, const int side, const float rpm, const char *encoding, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  if (rfifile==NULL) return;

  fprintf(rfifile, "{track:%d,side:%d,rpm:%.2f,", track, side, rpm);

  if (strstr(encoding, "raw")!=NULL)
  {
    fprintf(rfifile, "enc:\"%s\",len:%lu}", encoding, rawdatalength);
    fwrite(rawtrackdata, 1, rawdatalength, rfifile);
  }
  else
  if (strstr(encoding, "rle")!=NULL)
  {
    unsigned char *rledata;

    rledata=malloc(rawdatalength);

    if (rledata!=NULL)
    {
      unsigned long rledatalength;

      rledatalength=rfi_rleencode(rledata, rawdatalength, rawtrackdata, rawdatalength);

      fprintf(rfifile, "enc:\"%s\",len:%lu}", encoding, rledatalength);
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

long rfi_readtrack(FILE *rfifile, const int track, const int side, unsigned char *buf, const uint32_t buflen)
{
  int rfi_track = -1;
  int rfi_side = -1;
  float rfi_rpm = -1;
  char rfi_trackencoding[10];
  unsigned long rfi_trackdatalen = 0;

  char metabuffer[1024];

  if (rfifile==NULL) return 0;

  // Make sure we have valid file JSON metadata
  if (rfi_headerlen==0) return 0;

  // Seek past file JSON metadata
  if (fseek(rfifile, rfi_headerlen+3, SEEK_SET)!=0)
    return 0;

  while (!feof(rfifile))
  {
    jsmn_parser parser;
    int numtokens;
    long metapos;
    int i;

    // Initialise track metadata
    rfi_track=-1;
    rfi_side=-1;
    rfi_rpm=-1;
    rfi_trackencoding[0]=0;
    rfi_trackdatalen=0;

    // Read track metadata
    metapos=ftell(rfifile);

    if (metapos==-1)
      return 0;

    if (fread(metabuffer, sizeof(metabuffer), 1, rfifile)!=1)
      return 0;

    if (fseek(rfifile, metapos, SEEK_SET)!=0)
      return 0;

    for (i=0; i<(int)sizeof(metabuffer); i++)
    {
      if ((metabuffer[i]=='}') && ((i+1)<(int)sizeof(metabuffer)))
      {
        metabuffer[i+1]=0;
        break;
      }
    }

    // Quick check for validty and to count the tokens
    jsmn_init(&parser);
    numtokens=jsmn_parse(&parser, metabuffer, sizeof(metabuffer), NULL, 0);

    if (numtokens>0)
    {
      jsmntok_t *tokens;

      tokens=malloc(numtokens*sizeof(jsmntok_t));

      if (tokens==NULL) return 0;

      jsmn_init(&parser);
      numtokens=jsmn_parse(&parser, metabuffer, sizeof(metabuffer), tokens, numtokens);

      // Move file pointer to first byte after track header
      if (fseek(rfifile, tokens[0].end, SEEK_CUR)!=0)
      {
        free(tokens);
        return 0;
      }

      for (i=0; i<numtokens; i++)
      {
        if ((tokens[i].type==JSMN_PRIMITIVE) && (tokens[i].size==1) && ((i+1)<=numtokens))
        {
          char rfic;

          if (strncmp(&metabuffer[tokens[i].start], "enc", tokens[i].end-tokens[i].start)==0)
          {
            rfic=metabuffer[tokens[i+1].end];
            metabuffer[tokens[i+1].end]=0;

            if (strlen(&metabuffer[tokens[i+1].start])<sizeof(rfi_trackencoding))
              strcpy(rfi_trackencoding, &metabuffer[tokens[i+1].start]);

            metabuffer[tokens[i+1].end]=rfic;
          }
          else
          if (strncmp(&metabuffer[tokens[i].start], "track", tokens[i].end-tokens[i].start)==0)
          {
            rfic=metabuffer[tokens[i+1].end];
            metabuffer[tokens[i+1].end]=0;

            sscanf(&metabuffer[tokens[i+1].start], "%3d", &rfi_track);

            metabuffer[tokens[i+1].end]=rfic;
          }
          else
          if (strncmp(&metabuffer[tokens[i].start], "side", tokens[i].end-tokens[i].start)==0)
          {
            rfic=metabuffer[tokens[i+1].end];
            metabuffer[tokens[i+1].end]=0;

            sscanf(&metabuffer[tokens[i+1].start], "%1d", &rfi_side);

            metabuffer[tokens[i+1].end]=rfic;
          }
          else
          if (strncmp(&metabuffer[tokens[i].start], "len", tokens[i].end-tokens[i].start)==0)
          {
            rfic=metabuffer[tokens[i+1].end];
            metabuffer[tokens[i+1].end]=0;

            sscanf(&metabuffer[tokens[i+1].start], "%8lu", &rfi_trackdatalen);

            metabuffer[tokens[i+1].end]=rfic;
          }
          else
          if (strncmp(&metabuffer[tokens[i].start], "rpm", tokens[i].end-tokens[i].start)==0)
          {
            rfic=metabuffer[tokens[i+1].end];
            metabuffer[tokens[i+1].end]=0;

            sscanf(&metabuffer[tokens[i+1].start], "%f", &rfi_rpm);

            metabuffer[tokens[i+1].end]=rfic;
          }
        }
      }

      free(tokens);
      tokens=NULL;

      // Is this the track we want?
      if ((rfi_track==track) && (rfi_side==side) && (rfi_trackencoding[0]!=0) && (rfi_trackdatalen!=0))
      {
        // Don't adjust RPM when override used
        if (hw_forcedrpm==0.0)
        {
          if (rfi_rpm!=-1)
            hw_rpm=rfi_rpm;
          else
            hw_rpm=HW_DEFAULTRPM;
        }

        if (strstr(rfi_trackencoding, "raw")!=NULL)
        {
          if (rfi_trackdatalen<=buflen)
            return fread(buf, rfi_trackdatalen, 1, rfifile);
          else
            return fread(buf, buflen, 1, rfifile);
        }
        else
        if (strstr(rfi_trackencoding, "rle")!=NULL)
        {
          unsigned char b, blen, s;
          long rlen=0;
          char *rlebuff;

          rlebuff=malloc(rfi_trackdatalen);

          if (rlebuff==NULL) return 0;

          blen=0; s=0; b=0;
          if (fread(rlebuff, rfi_trackdatalen, 1, rfifile)==0)
          {
            free(rlebuff);
            return 0;
          }

          for (i=0; (unsigned int)i<rfi_trackdatalen; i++)
          {
            unsigned char c;

            // Extract next RLE value
            c=rlebuff[i];

            while (c>0)
            {
              b=(b<<1)|s;
              blen++;

              if (blen==8)
              {
                buf[rlen++]=b;

                // Check for unpacking overflow
                if (rlen>=buflen)
                {
                  free(rlebuff);
                  return rlen;
                }

                b=0;
                blen=0;
              }

              c--;
            }

            // Switch states
            s=1-s;
          }

          free(rlebuff);

          return rlen;
        }
      }
      else
      {
        // If the currently read track is more than the one we want, then the track isn't here
        if (rfi_track>track) return 0;

        // Skip this track, it's not the one we want
        if (fseek(rfifile, rfi_trackdatalen, SEEK_CUR)!=0)
          return 0;
      }
    }
    else // No tokens found, give up further processing
      return 0;
  }

  return 0;
}

