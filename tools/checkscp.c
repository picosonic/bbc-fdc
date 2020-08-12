#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "scp.h"

struct scp_header header;
long scp_endofheader;

void scp_processheader(FILE *scpfile)
{
  bzero(&header, sizeof(header));
  fread(&header, 1, sizeof(header), scpfile);

  if (strncmp((char *)&header.magic, SCP_MAGIC, strlen(SCP_MAGIC))!=0)
  {
    printf("Not an SCP file\n");
  }

  printf("SCP magic detected\n");

  if ((header.flags & SCP_FLAGS_FOOTER)==0)
    printf("Version: %d.%d\n", header.version>>4, header.version&0x0f);

  printf("Disk type: %d %d\n", header.disktype>>4, header.disktype&0x0f);
  printf("Revolutions: %d\n", header.revolutions);
  printf("Tracks: %d to %d\n", header.starttrack, header.endtrack);

  printf("Flags: 0x%.2x [", header.flags);

  if ((header.flags & SCP_FLAGS_INDEX)!=0)
    printf(" Indexed");
  else
    printf(" NonIndexed");

  if ((header.flags & SCP_FLAGS_96TPI)!=0)
    printf(" 96tpi");
  else
    printf(" 48tpi");

  if ((header.flags & SCP_FLAGS_360RPM)!=0)
    printf(" 360rpm");
  else
    printf(" 300rpm");

  if ((header.flags & SCP_FLAGS_NORMALISED)!=0)
    printf(" Normalised");
  else
    printf(" Captured");

  if ((header.flags & SCP_FLAGS_RW)!=0)
    printf(" Read/Write");
  else
    printf(" Read_Only");

  if ((header.flags & SCP_FLAGS_FOOTER)!=0)
    printf(" Has_Footer");

  if ((header.flags & SCP_FLAGS_EXTENDED)!=0)
    printf(" Extended");

  printf(" ]\n");

  printf("Bitcell encoding: %d bits\n", header.bitcellencoding==0?16:header.bitcellencoding);
  printf("Heads: %d (", header.heads);
  switch (header.heads)
  {
    case 0:
      printf("Both");
      break;

    case 1:
      printf("Bottom");
      break;

    case 2:
      printf("Top");
      break;

    default:
      printf("Unknown");
      break;

  }
  printf(")\n");
  printf("Resolution: %dns\n", (header.resolution+1)*SCP_BASE_NS);
  printf("Checksum: 0x%.8x\n", header.checksum);
}

void scp_processtrack(FILE *scpfile, const unsigned char track)
{
}

int main(int argc, char **argv)
{
  unsigned char track;
  uint32_t checksum;
  uint8_t block[256];
  size_t blocklen;
  size_t i;
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

  scp_processheader(fp);

  scp_endofheader=ftell(fp);

  // validate checksum
  checksum=0;
  while (!feof(fp))
  {
    blocklen=fread(block, 1, sizeof(block), fp);
    if (blocklen>0)
      for (i=0; i<blocklen; i++)
        checksum+=block[i];
  }
  fseek(fp, scp_endofheader, SEEK_SET);
  printf("Calculated Checksum: 0x%.8x ", checksum);

  if (checksum!=header.checksum)
  {
    printf("(Mismatch) \n");
    fclose(fp);

    return 1;
  }
  else
    printf("(OK) \n");

  for (track=header.starttrack; track<header.endtrack; track++)
    scp_processtrack(fp, track);

  fclose(fp);

  return 0;
}
