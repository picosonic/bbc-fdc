#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "a2r.h"

struct a2r_header a2rheader;
int is525=0; // Is the capture from a 5.25" disk

int a2r_processheader(FILE *fp)
{
  fread(&a2rheader, 1, sizeof(a2rheader), fp);

  if (strncmp((char *)&a2rheader.id, A2R_MAGIC, strlen(A2R_MAGIC))!=0)
    return 1;

  if (a2rheader.ff!=0xff)
    return 1;

  if ((a2rheader.lfcrlf[0]!=0x0a) || (a2rheader.lfcrlf[1]!=0x0d) || (a2rheader.lfcrlf[2]!=0x0a))
    return 1;

  return 0;
}

int a2r_processmeta(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  uint8_t *meta;

  meta=malloc(chunkheader->size);

  if (meta==NULL)
  {
    printf("Failed to allocate memory for META block\n");

    return 1;
  }

  if (fread(meta, 1, chunkheader->size, fp)==chunkheader->size)
  {
    uint32_t i;

    printf("  ");
    for (i=0; i<chunkheader->size; i++)
    {
      printf("%c", meta[i]);

      if (meta[i]=='\n')
        printf("  ");
    }
    printf("\n");

    free(meta);

    return 0;
  }

  free(meta);

  return 1;
}

int a2r_processstream(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  struct a2r_strm stream;

  if (fread(&stream, 1, sizeof(stream), fp)<=0)
    return 1;

  if (is525)
    printf("  Location: %.2f\n", (float)stream.location/4);
  else
    printf("  Location: %d\n", stream.location);

  printf("  Capture type: %d (%s)\n", stream.type, (stream.type==1)?"Timing":(stream.type==2)?"Bits":(stream.type==3)?"Extended timing":"Unknown");
  printf("  Data length: %d bytes\n", stream.size);
  printf("  Loop point: %d\n", stream.loop);

  fseek(fp, chunkheader->size-sizeof(stream), SEEK_CUR);

  return 0;
}

int a2r_processinfo(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  struct a2r_info *info;
  uint16_t i;

  info=malloc(chunkheader->size);

  if (info==NULL)
  {
    printf("Failed to allocate memory for INFO block\n");

    return 1;
  }

  fread(info, 1, chunkheader->size, fp);

  printf("  Version: %d\n", info->version);

  printf("  Creator: ");
  for (i=0; i<sizeof(info->creator); i++)
    printf("%c", info->creator[i]);
  printf("\n");

  printf("  Disk type: %d (%s)\n", info->disktype, info->disktype==1?"5.25\"":"3.5\"");
  if (info->disktype==1)
    is525=1;

  printf("  Protection: Write%s\n", info->writeprotected==1?"able":" protected");

  printf("  Sync: %s\n", info->synchronised==1?"Cross track sync":"None");

  free(info);

  return 0;
}

int a2r_processchunk(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  uint16_t i;

  printf("Chunk '");
  for (i=0; i<4; i++)
    printf("%c", chunkheader->id[i]);
    
  printf("', length %d\n", chunkheader->size);

  if (strncmp((char *)&chunkheader->id, A2R_CHUNK_INFO, 4)==0)
  {
    if (a2r_processinfo(chunkheader, fp)!=0)
      return 1;
  }
  else
  if (strncmp((char *)&chunkheader->id, A2R_CHUNK_META, 4)==0)
  {
    if (a2r_processmeta(chunkheader, fp)!=0)
      return 1;
  }
  else
  if (strncmp((char *)&chunkheader->id, A2R_CHUNK_STRM, 4)==0)
  {
    if (a2r_processstream(chunkheader, fp)!=0)
      return 1;
  }
  else
    fseek(fp, chunkheader->size, SEEK_CUR);

  return 0;
}

int main(int argc, char **argv)
{
  FILE *fp;

  if (argc!=2)
  {
    printf("Specify .a2r on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  if (a2r_processheader(fp)!=0)
  {
    fclose(fp);
    return 1;
  }

  printf("A2R file\n");

  // Process chunks
  while (!feof(fp))
  {
    struct a2r_chunkheader chunkheader;

    if (fread(&chunkheader, 1, sizeof(chunkheader), fp)<=0)
      break;

    if (a2r_processchunk(&chunkheader, fp)!=0)
    {
      // Something went wrong
      fclose(fp);

      return 1;
    }
  }

  fclose(fp);

  return 0;
}
