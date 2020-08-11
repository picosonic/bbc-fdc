#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "scp.h"

struct scp_header header;

unsigned char scp_processheader(FILE *scpfile)
{
  fread(&header, 1, sizeof(header), scpfile);

  if (strncmp((char *)&header.magic, SCP_MAGIC, strlen(SCP_MAGIC))!=0)
  {
    printf("Not an SCP file\n");
    return 0;
  }

  printf("SCP magic detected\n");

  printf("Version: %d.%d\n", header.version>>4, header.version&0x0f);
  printf("Disk type: %d %d\n", header.disktype>>4, header.disktype&0x0f);
  printf("Revolutions: %d\n", header.revolutions);
  printf("Tracks: %d to %d\n", header.starttrack, header.endtrack);
  printf("Flags: 0x%.2x\n", header.flags);
  printf("Bitcell encoding: %d bits\n", header.bitcellencoding==0?16:header.bitcellencoding);
  printf("Heads: %d\n", header.heads);
  printf("Resolution: %dns\n", (header.resolution+1)*SCP_BASE_NS);
  printf("Checksum: 0x%.8x\n", header.checksum);

  printf("Tracks in SCP: %d\n", header.endtrack-header.starttrack);

  return 0;
}

void scp_processtrack(FILE *scpfile, const unsigned char track)
{
}

int main(int argc, char **argv)
{
  unsigned char numtracks;
  unsigned char track;
  FILE *fp;

  if (argc!=2)
  {
    printf("Specify .scp on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  numtracks=scp_processheader(fp);

  for (track=0; track<numtracks; track++)
    scp_processtrack(fp, track);

  fclose(fp);

  return 0;
}
