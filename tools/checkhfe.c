#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hfe.h"

struct hfe_header hfeheader;
struct hfe_track curtrack;
int isv3=0;

int hfe_processheader(FILE *hfefile)
{
  bzero(&hfeheader, sizeof(hfeheader));
  if (fread(&hfeheader, sizeof(hfeheader), 1, hfefile)==0)
    return 1;

  if (strncmp((char *)&hfeheader.HEADERSIGNATURE, HFE_MAGIC1, strlen(HFE_MAGIC1))!=0)
  {
    if (strncmp((char *)&hfeheader.HEADERSIGNATURE, HFE_MAGIC3, strlen(HFE_MAGIC3))!=0)
    {
      printf("Not an HFE file\n");
      return 1;
    }
    else
    {
      isv3=1;

      printf("HFE v3 file\n");
    }
  }
  else
    printf("HFE v1 file\n");

  printf("Format revision : %d \n", hfeheader.formatrevision);
  printf("Tracks : %d \n", hfeheader.number_of_track);
  printf("Sides : %d \n", hfeheader.number_of_side);
  printf("Track encoding : %d ", hfeheader.track_encoding);
  switch (hfeheader.track_encoding)
  {
    case 0: printf("ISO IBM MFM"); break;
    case 1: printf("AMIGA MFM"); break;
    case 2: printf("ISO IBM FM"); break;
    case 3: printf("EMU FM"); break;
    default: printf("UNKNOWN"); break;
  }
  printf("\n");
  printf("Bit rate : %d k bit/s \n", hfeheader.bitRate);
  printf("RPM : %d \n", hfeheader.floppyRPM);
  printf("Interface mode : %d ", hfeheader.floppyinterfacemode);
  switch (hfeheader.floppyinterfacemode)
  {
    case 0: printf("IBM PC DD"); break;
    case 1: printf("IBM PC HD"); break;
    case 2: printf("ATARI ST DD"); break;
    case 3: printf("ATARI ST HD"); break;
    case 4: printf("AMIGA DD"); break;
    case 5: printf("AMIGA HD"); break;
    case 6: printf("CPC DD"); break;
    case 7: printf("GENERIC SHUGART DD"); break;
    case 8: printf("IBM PC ED"); break;
    case 9: printf("MSX2 DD"); break;
    case 10: printf("C64 DD"); break;
    case 11: printf("EMU SHUGART"); break;
    case 12: printf("S950 DD"); break;
    case 13: printf("S950 HD"); break;
    case 0xfe: printf("DISABLE"); break;
    default: printf("UNKNOWN"); break;
  }
  printf("\n");

  if (isv3)
    printf("Write protected : %d \n", hfeheader.write_protected);

  printf("Track list offset : %d \n", hfeheader.track_list_offset);
  printf("Write allowed : %s \n", (hfeheader.write_allowed==HFE_TRUE)?"Yes":"No");
  printf("Stepping : %s \n", (hfeheader.single_step==HFE_TRUE)?"Single":"Double");
  if (hfeheader.track0s0_altencoding!=HFE_TRUE)
  {
    printf("Track 0 S 0 alt encoding : %d \n", hfeheader.track0s0_altencoding);
    printf("Track 0 S 0 encoding : %d \n", hfeheader.track0s0_encoding);
  }
  if (hfeheader.track0s1_altencoding!=HFE_TRUE)
  {
    printf("Track 0 S 1 alt encoding : %d \n", hfeheader.track0s1_altencoding);
    printf("Track 0 S 1 encoding : %d \n", hfeheader.track0s1_encoding);
  }

  return 0;
}

int main(int argc, char **argv)
{
  FILE *fp;
  uint8_t track;

  if (argc!=2)
  {
    printf("Specify .hfe on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  if (hfe_processheader(fp)!=0)
  {
    fclose(fp);
    return 1;
  }

  // Process tracks
  for (track=0; track<hfeheader.number_of_track; track++)
  {
    fseek(fp, (hfeheader.track_list_offset*HFE_BLOCKSIZE)+(track*(sizeof(curtrack))), SEEK_SET);
    if (fread(&curtrack, sizeof(curtrack), 1, fp)==0)
      return 1;

    printf("  Track %.2d @ block %d, %d bytes\n", track, curtrack.offset, curtrack.track_len);

  }

  fclose(fp);

  return 0;
}
