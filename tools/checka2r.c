#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "a2r.h"

struct a2r_header a2rheader;

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
    uint16_t i;

    if (fread(&chunkheader, 1, sizeof(chunkheader), fp)<=0)
      break;

    printf("Chunk '");
    for (i=0; i<4; i++)
      printf("%c", chunkheader.id[i]);
    
    printf("', length %d\n", chunkheader.size);

    if (strncmp((char *)&chunkheader.id, A2R_CHUNK_INFO, 4)==0)
    {
      struct a2r_info *info;

      info=malloc(chunkheader.size);

      if (info==NULL)
      {
        printf("Failed to allocate memory for INFO block\n");
        fclose(fp);
        return 1;
      }

      fread(info, 1, chunkheader.size, fp);

      printf("  Version: %d\n", info->version);

      printf("  Creator: ");
      for (i=0; i<sizeof(info->creator); i++)
        printf("%c", info->creator[i]);
      printf("\n");

      printf("  Disk type: %d (%s)\n", info->disktype, info->disktype==1?"5.25\"":"3.5\"");

      printf("  Protection: Write%s\n", info->writeprotected==1?"able":" protected");

      printf("  Sync: %s\n", info->synchronised==1?"Cross track sync":"None");

      free(info);
    }
    else
      fseek(fp, chunkheader.size, SEEK_CUR);
  }

  fclose(fp);

  return 0;
}
