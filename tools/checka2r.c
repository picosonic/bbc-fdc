#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "a2r.h"

struct a2r_header a2rheader;
int is525=0; // Is the capture from a 5.25" disk in SS 40t 0.25 step

int a2r_processheader(FILE *fp)
{
  if (fread(&a2rheader, sizeof(a2rheader), 1, fp)==0)
    return 1;

  if (strncmp((char *)&a2rheader.id, A2R_MAGIC2, strlen(A2R_MAGIC2))!=0)
  {
    if (strncmp((char *)&a2rheader.id, A2R_MAGIC3, strlen(A2R_MAGIC3))!=0)
      return 1;
  }

  if (a2rheader.ff!=0xff)
    return 1;

  if ((a2rheader.lfcrlf[0]!=0x0a) || (a2rheader.lfcrlf[1]!=0x0d) || (a2rheader.lfcrlf[2]!=0x0a))
    return 1;

  return 0;
}

int a2r_processmeta(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  uint8_t *meta;
  int retval=1;

  meta=malloc(chunkheader->size);

  if (meta==NULL)
  {
    printf("Failed to allocate memory for META block\n");

    return retval;
  }

  if (fread(meta, chunkheader->size, 1, fp)==1)
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

    retval=0;
  }

  free(meta);

  return retval;
}

int a2r_processstream(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  struct a2r_strm stream;
  uint32_t done;

  done=0;

  // Loop through all available stream data
  while (done<chunkheader->size)
  {
    if (fread(&stream, sizeof(stream), 1, fp)==0)
      return 1;

    // Detect end of stream data
    if (stream.location==0xff)
    {
      fseek(fp, 1-sizeof(stream), SEEK_CUR);
      break;
    }

    printf("  Track ");
    if (is525)
      printf("%.2f", (float)stream.location/4);
    else
      printf("%d side %d", (stream.location>>1), stream.location&1);

    printf(", type %d (%s)", stream.type, (stream.type==1)?"Timing":(stream.type==2)?"Bits":(stream.type==3)?"Extended timing":"Unknown");
    printf(", length %u bytes", stream.size);
    printf(", loop %u\n", stream.loop);

    // Skip over the actual data
    fseek(fp, stream.size, SEEK_CUR);

    done+=(sizeof(stream)+stream.size);
  }

  return 0;
}

int a2r_processrawcapture(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  struct a2r_rwcp rwcp;
  uint32_t done;

  if (fread(&rwcp, sizeof(rwcp), 1, fp)==0)
    return 1;

  printf("  Version: %d\n", rwcp.version);
  printf("  Resolution: %u picoseconds/tick\n", rwcp.resolution);

  done=sizeof(rwcp);

  // Loop through all available stream data
  while (done<chunkheader->size)
  {
    struct a2r_rwcp_strm stream;
    uint32_t capturesize;

    if (fread(&stream, sizeof(stream), 1, fp)==0)
      return 1;

    done+=sizeof(stream);

    // Detect end of stream data
    if (stream.mark=='X')
    {
      fseek(fp, 1-sizeof(stream), SEEK_CUR);
      break;
    }

    printf("  Track ");
    if (is525)
      printf("%.2f", (float)stream.location/4);
    else
      printf("%d side %d", (stream.location>>1), stream.location&1);

    printf(", type %d (%s)", stream.type, (stream.type==1)?"Timing":(stream.type==2)?"Bits":(stream.type==3)?"Extended timing":"Unknown");
    printf(", %d index", stream.indexcount);

    // Skip over index array
    fseek(fp, sizeof(uint32_t)*stream.indexcount, SEEK_CUR);
    done+=(sizeof(uint32_t)*stream.indexcount);

    if (fread(&capturesize, sizeof(capturesize), 1, fp)==0)
      return 1;

    done+=sizeof(capturesize);
    printf(", %u bytes\n", capturesize);

    // Skip over capture data
    fseek(fp, capturesize, SEEK_CUR);
    done+=capturesize;
  }
 
  return 0;
}

int a2r_processsolved(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  struct a2r_slvd slvd;
  uint32_t done;

  if (fread(&slvd, sizeof(slvd), 1, fp)==0)
    return 1;

  printf("  Version: %d\n", slvd.version);
  printf("  Resolution: %u picoseconds/tick\n", slvd.resolution);

  done=sizeof(slvd);

  // Loop through all available stream data
  while (done<chunkheader->size)
  {
    struct a2r_slvd_track stream;
    uint32_t capturesize;

    if (fread(&stream, sizeof(stream), 1, fp)==0)
      return 1;

    done+=sizeof(stream);

    // Detect end of stream data
    if (stream.mark=='X')
    {
      fseek(fp, 1-sizeof(stream), SEEK_CUR);
      break;
    }

    printf("  Track ");
    if (is525)
      printf("%.2f", (float)stream.location/4);
    else
      printf("%d side %d", (stream.location>>1), stream.location&1);

    printf(", %d mirror dist out", stream.mirrordistout);
    printf(", %d mirror dist in", stream.mirrordistin);
    printf(", %d index", stream.indexcount);

    // Skip over index array
    fseek(fp, sizeof(uint32_t)*stream.indexcount, SEEK_CUR);
    done+=(sizeof(uint32_t)*stream.indexcount);

    if (fread(&capturesize, sizeof(capturesize), 1, fp)==0)
      return 1;

    done+=sizeof(capturesize);
    printf(", %u bytes\n", capturesize);

    // Skip over capture data
    fseek(fp, capturesize, SEEK_CUR);
    done+=capturesize;
  }

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

  if (fread(info, chunkheader->size, 1, fp)==0)
  {
    free(info);
    return 1;
  }

  printf("  Version: %d\n", info->version);

  printf("  Creator: ");
  for (i=0; i<sizeof(info->creator); i++)
    printf("%c", info->creator[i]);
  printf("\n");

  printf("  Disk type: %d (", info->disktype);
  switch (info->disktype)
  {
    case 1:
      printf("5.25\" SS 40trk 0.25 step");
      break;

    case 2:
      printf("3.5\" DS 80trk Apple CLV");
      break;

    case 3:
      printf("5.25\" DS 80trk");
      break;

    case 4:
      printf("5.25\" DS 40trk");
      break;

    case 5:
      printf("3.5\" DS 80trk");
      break;

    case 6:
      printf("8\" DS");
      break;

    default:
      printf("Unknown");
      break;
  }
  printf(")\n");

  if (info->disktype==1)
    is525=1;

  printf("  Protection: Write%s\n", info->writeprotected==1?"able":" protected");

  printf("  Sync: %s\n", info->synchronised==1?"Cross track sync":"None");

  if (a2rheader.id[3]>='3')
  {
    printf("  Sector type: ");
    if (info->hardsectors==0)
      printf("Soft sectored\n");
    else
      printf("%d hard sectors\n", info->hardsectors);
  }

  free(info);

  return 0;
}

int a2r_processchunk(struct a2r_chunkheader *chunkheader, FILE *fp)
{
  uint16_t i;

  printf("Chunk '");
  for (i=0; i<4; i++)
    printf("%c", chunkheader->id[i]);
    
  printf("', length %u\n", chunkheader->size);

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
  if (strncmp((char *)&chunkheader->id, A2R_CHUNK_RWCP, 4)==0)
  {
    if (a2r_processrawcapture(chunkheader, fp)!=0)
      return 1;
  }
  else
  if (strncmp((char *)&chunkheader->id, A2R_CHUNK_SLVD, 4)==0)
  {
    if (a2r_processsolved(chunkheader, fp)!=0)
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
    return 3;
  }

  printf("A2R revision %c file\n", a2rheader.id[3]);

  // Process chunks
  while (!feof(fp))
  {
    struct a2r_chunkheader chunkheader;

    if (fread(&chunkheader, sizeof(chunkheader), 1, fp)==0)
      break;

    if (a2r_processchunk(&chunkheader, fp)!=0)
    {
      // Something went wrong
      fclose(fp);

      return 4;
    }
  }

  fclose(fp);

  return 0;
}
