#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "scp.h"

unsigned char scp_processheader(FILE *scpfile)
{
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
