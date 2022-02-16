#include <stdio.h>
#include <stdlib.h>

#include "fsd.h"

unsigned char fsd_processheader(FILE *fsdfile)
{
  char magic[3];
  unsigned char creator[5];
  char titlechar;
  unsigned char numtracks;

  if (fread(magic, 3, 1, fsdfile)==0)
    return 0;

  if ((magic[0]!='F') || (magic[1]!='S') || (magic[2]!='D'))
  {
    printf("Not an FSD file\n");
    return 0;
  }

  printf("FSD magic detected\n");

  if (fread(creator, 5, 1, fsdfile)==0)
    return 0;

  printf("Created: %.2d/%.2d/%d by %d release %d unused %.2x\n", (creator[0]&0xf8)>>3, creator[2]&0x0f, ((creator[0]&0x07)<<8)|creator[1], (creator[2]&0xf0)>>4, (((creator[4]&0xc0)>>6)<<8)|creator[3], creator[4]&0x3f);

  printf("Title: \"");

  if (fread(&titlechar, 1, 1, fsdfile)==0)
    return 0;

  while ((titlechar!=0) && (!feof(fsdfile)))
  {
    printf("%c", titlechar);

    if (fread(&titlechar, 1, 1, fsdfile)==0)
      return 0;
  }

  printf("\"\n");

  if (fread(&numtracks, 1, 1, fsdfile)==0)
    return 0;

  printf("Tracks in FSD: %d\n", numtracks);

  return numtracks;
}

void fsd_processsector(FILE *fsdfile, const unsigned char readability)
{
  unsigned char sectorheader[6];

  if (fread(sectorheader, 4, 1, fsdfile)==0)
    return;

  printf("    C%d H%d S%d N%d", sectorheader[0], sectorheader[1], sectorheader[2], sectorheader[3]);

  if (readability!=0)
  {
    unsigned char databyte;
    int datasize, i;

    if (fread(&sectorheader[4], 2, 1, fsdfile)==0)
      return;

    printf(" (%d) code %.2x\n", sectorheader[4], sectorheader[5]);

    datasize=128<<sectorheader[4];

    printf("      [%d]", datasize);
    for (i=0; i<datasize; i++)
    {
      if (fread(&databyte, 1, 1, fsdfile)==0)
        return;

      printf("%c", ((databyte>=' ')&&(databyte<='~'))?databyte:'.');
    }
    printf("\n");
  }
  else
    printf("\n");
}

void fsd_processtrack(FILE *fsdfile, const unsigned char track)
{
  unsigned char trackheader[3];
  int sector;

  printf("\n  Track : ");

  if (fread(trackheader, 2, 1, fsdfile)==0)
    return;

  printf("%d (%d)", trackheader[0], track);

  if (trackheader[1]!=FSD_UNFORMATTED)
  {
    if (fread(&trackheader[2], 1, 1, fsdfile)==0)
      return;

    printf("(%.2x %sreadable)\n", trackheader[2], trackheader[2]==0?"un":"");
  }
  else
    printf("\n");

  printf("  Sectors: ");
  if (trackheader[1]==0)
    printf("Unformatted\n");
  else
    printf("%d\n", trackheader[1]);

  for (sector=0; sector<trackheader[1]; sector++)
    fsd_processsector(fsdfile, trackheader[2]);
}

int main(int argc, char **argv)
{
  unsigned char numtracks;
  unsigned char track;
  FILE *fp;

  if (argc!=2)
  {
    printf("Specify .fsd on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  numtracks=fsd_processheader(fp);

  for (track=0; track<numtracks; track++)
  {
    if (feof(fp)) break;

    fsd_processtrack(fp, track);
  }

  fclose(fp);

  return 0;
}
