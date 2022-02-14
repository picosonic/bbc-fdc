#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "woz.h"
#include "crc32.h"

struct woz_header wozheader;
uint32_t woz_calccrc;

int woz_processheader(FILE *fp)
{
  long filepos;
  unsigned char buf;

  fread(&wozheader, 1, sizeof(wozheader), fp);

  if (strncmp((char *)&wozheader.id, WOZ_MAGIC1, strlen(WOZ_MAGIC1))!=0)
  {
    if (strncmp((char *)&wozheader.id, WOZ_MAGIC2, strlen(WOZ_MAGIC2))!=0)
      return 1;
  }

  if (wozheader.ff!=0xff)
    return 1;

  if ((wozheader.lfcrlf[0]!=0x0a) || (wozheader.lfcrlf[1]!=0x0d) || (wozheader.lfcrlf[2]!=0x0a))
    return 1;

  // Check CRC32
  woz_calccrc=0;
  filepos=ftell(fp);

  while (!feof(fp))
  {
    fread(&buf, 1, 1, fp);

    if (!feof(fp))
      woz_calccrc=CRC32_CalcStream(woz_calccrc, &buf, 1);
  }

  fseek(fp, filepos, SEEK_SET);

  if (wozheader.crc!=woz_calccrc)
    return 1;

  return 0;
}

int woz_processchunk(struct woz_chunkheader *chunkheader, FILE *fp)
{
  uint16_t i;

  printf("Chunk '");
  for (i=0; i<4; i++)
    printf("%c", chunkheader->id[i]);
    
  printf("', length %d\n", chunkheader->size);

//  if (strncmp((char *)&chunkheader->id, WOZ_CHUNK_INFO, 4)==0)
//  {
//    if (woz_processinfo(chunkheader, fp)!=0)
//      return 1;
//  }
//  else
    fseek(fp, chunkheader->size, SEEK_CUR);
  
  return 0;
}

int main(int argc, char **argv)
{
  FILE *fp;

  if (argc!=2)
  {
    printf("Specify .woz on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  if (woz_processheader(fp)!=0)
  {
    fclose(fp);
    return 1;
  }

  printf("WOZ revision %c file\n", wozheader.id[3]);
  printf("Checksum %.8x\n", wozheader.crc);

  // Process chunks
  while (!feof(fp))
  {
    struct woz_chunkheader chunkheader;

    if (fread(&chunkheader, 1, sizeof(chunkheader), fp)<=0)
      break;

    if (woz_processchunk(&chunkheader, fp)!=0)
    {
      // Something went wrong
      fclose(fp);

      return 1;
    }
  }

  fclose(fp);

  return 0;
}
