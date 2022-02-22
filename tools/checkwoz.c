#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "woz.h"
#include "crc32.h"

struct woz_header wozheader;
int is525=0; // Is the capture from a 5.25" disk in SS 40t 0.25 step
uint8_t trackmap[WOZ_MAXTRACKS]; // Index for track data within TRKS chunk

int woz_processheader(FILE *fp)
{
  long filepos;
  unsigned char buf;
  uint32_t woz_calccrc=0;

  if (fread(&wozheader, sizeof(wozheader), 1, fp)==0)
    return 1;

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

int woz_processinfo(struct woz_chunkheader *chunkheader, FILE *fp)
{
  struct woz_info *info;
  int i;
  int systems=0;

  info=malloc(chunkheader->chunksize);
  if (info==NULL)
  {
    printf("Failed to allocate memory for INFO block\n");

    return 1;
  }

  if (fread(info, chunkheader->chunksize, 1, fp)==0)
  {
    free(info);
    return 1;
  }

  printf("  INFO version: %d\n", info->version);

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

  printf("  Write protected : %d\n", info->writeprotected);

  printf("  Sync: %s\n", info->synchronised==1?"Cross track sync":"None");

  printf("  Cleaned : %s\n", info->cleaned==1?"MC3470 fake bits removed":"No");

  printf("  Creator : '");
  for (i=0; i<32; i++)
  {
    printf("%c", info->creator[i]);
  }
  printf("'\n");

  // Process extra version 2 fields
  if (info->version>=2)
  {
    printf("  Disk sides : %d\n", info->sides);

    printf("  Boot sector format : %d (", info->bootsectorformat);
    switch (info->bootsectorformat)
    {
      case 0:
        printf("Unknown");
        break;

      case 1:
        printf("Contains boot sector for 16-sector");
        break;

      case 2:
        printf("Contains boot sector for 13-sector");
        break;

      case 3:
        printf("Contains boot sector for both 13 and 16-sector");
        break;

      default:
        printf("Unknown");
        break;
    }
    printf(")\n");

    printf("  Optimal bit timing : %d/8 = %d uS/bit\n", info->timing, (info->timing)/8); // standard 5.25 is 32 (4us), 3.5 is 16 (2us)

    printf("  Compatible hardware : 0x%.4x (", info->compatibility);
    if (info->compatibility & 0x1) printf("Apple ][");
    if (info->compatibility & 0x2) printf("%sApple ][ Plus", (systems++)>0?", ":"");
    if (info->compatibility & 0x4) printf("%sApple //e (unenhanced)", (systems++)>0?", ":"");
    if (info->compatibility & 0x8) printf("%sApple //c", (systems++)>0?", ":"");
    if (info->compatibility & 0x10) printf("%sApple //e (enhanced)", (systems++)>0?", ":"");
    if (info->compatibility & 0x20) printf("%sApple IIgs", (systems++)>0?", ":"");
    if (info->compatibility & 0x40) printf("%sApple //c Plus", (systems++)>0?", ":"");
    if (info->compatibility & 0x80) printf("%sApple ///", (systems++)>0?", ":"");
    if (info->compatibility & 0x100) printf("%sApple /// Plus", (systems++)>0?", ":"");
    printf(")\n");

    if (info->minimumram!=0)
      printf("  Required RAM : %dk\n", info->minimumram);

    printf("  Largest track : %d x 512 byte blocks\n", info->largesttrack);
  }

  if (info->version>=3)
  {
    printf("  FLUX block : %d\n", info->fluxblock);
    printf("  Largest FLUX track : %d\n", info->largestfluxtrack);
  }

  free(info);

  return 0;
}

int woz_processtmap(struct woz_chunkheader *chunkheader, FILE *fp)
{
  int i;

  if (chunkheader->chunksize!=WOZ_MAXTRACKS)
    return 1;

  if (fread(trackmap, sizeof(trackmap), 1, fp)==0)
    return 1;

  for (i=0; i<WOZ_MAXTRACKS; i++)
  {
    if (trackmap[i]!=WOZ_NOTRACK)
    {
      if (is525) // From INFO chunk
        printf("  [%d] Track %.2f @ %.2x\n", i, (double)i/4, trackmap[i]);
      else
        printf("  [%d] Side %d Track %d @ %.2x\n", i, i/80, i%80, trackmap[i]);
    }
  }

  return 0;
}

int woz_processtrks(struct woz_chunkheader *chunkheader, FILE *fp)
{
  long filepos;

  filepos=ftell(fp);

  if (wozheader.id[3]=='1')
  {
    struct woz_trks1 trks;
    int i;

    for (i=0; i<WOZ_MAXTRACKS; i++)
    {
      if (trackmap[i]!=WOZ_NOTRACK)
      {
        if (is525)
          printf("  Bitstream for Track %.2f\n", (double)i/4);
        else
          printf("  Bistream for Side %d Track %d\n", i/80, i%80);

        fseek(fp, filepos+(trackmap[i]*sizeof(trks)), SEEK_SET);

        if (fread(&trks, sizeof(trks), 1, fp)==0)
          return 1;

        printf("    Bytes used : %d\n", trks.bytesused);
        printf("    Bit count : %d\n", trks.bitcount);
        printf("    Splice point : %d\n", trks.splicepoint);
        printf("    Splice nibble : %d\n", trks.splicenibble);
        printf("    Splice bit count : %d\n", trks.splicebitcount);
      }
    }

    // Move on to next chunk
    fseek(fp, filepos, SEEK_SET);
    fseek(fp, chunkheader->chunksize, SEEK_CUR);
  }
  else
  if (wozheader.id[3]=='2')
  {
    struct woz_trks2 trks;
    int i;

    for (i=0; i<WOZ_MAXTRACKS; i++)
    {
      if (trackmap[i]!=WOZ_NOTRACK)
      {
        if (is525)
          printf("  Bitstream for Track %.2f\n", (double)i/4);
        else
          printf("  Bistream for Side %d Track %d\n", i/80, i%80);

        fseek(fp, filepos+(trackmap[i]*sizeof(trks)), SEEK_SET);

        if (fread(&trks, sizeof(trks), 1, fp)==0)
          return 1;

        printf("    Starting block : %d%s\n", trks.startingblock, ((trks.startingblock<3)?" (INVALID)":""));
        printf("    Block count : %d\n", trks.blockcount);
        printf("    Bit count : %d\n", trks.bitcount);
      }
    }

    // Move on to next chunk
    fseek(fp, filepos, SEEK_SET);
    fseek(fp, chunkheader->chunksize, SEEK_CUR);
  }
  else
    return 1;

  return 0;
}

int woz_processmeta(struct woz_chunkheader *chunkheader, FILE *fp)
{
  uint8_t *meta;
  int retcode=1;

  meta=malloc(chunkheader->chunksize);

  if (meta==NULL)
  {
    printf("Failed to allocate memory for META block\n");

    return retcode;
  }

  if (fread(meta, 1, chunkheader->chunksize, fp)==chunkheader->chunksize)
  {
    uint32_t i;

    printf("  ");
    for (i=0; i<chunkheader->chunksize; i++)
    {
      printf("%c", meta[i]);

      if (meta[i]=='\n')
        printf("  ");
    }
    printf("\n");

    retcode=0;
  }

  free(meta);

  return retcode;
}

int woz_processchunk(struct woz_chunkheader *chunkheader, FILE *fp)
{
  uint16_t i;

  printf("Chunk '");
  for (i=0; i<4; i++)
    printf("%c", chunkheader->id[i]);
    
  printf("', length %d\n", chunkheader->chunksize);

  if (strncmp((char *)&chunkheader->id, WOZ_CHUNK_INFO, 4)==0)
  {
    if (woz_processinfo(chunkheader, fp)!=0)
      return 1;
  }
  else
  if (strncmp((char *)&chunkheader->id, WOZ_CHUNK_TMAP, 4)==0)
  {
    if (woz_processtmap(chunkheader, fp)!=0)
      return 1;
  }
  else
  if (strncmp((char *)&chunkheader->id, WOZ_CHUNK_TRKS, 4)==0)
  {
    if (woz_processtrks(chunkheader, fp)!=0)
      return 1;
  }
  else
  if (strncmp((char *)&chunkheader->id, WOZ_CHUNK_META, 4)==0)
  {
    if (woz_processmeta(chunkheader, fp)!=0)
      return 1;
  }
  else
    fseek(fp, chunkheader->chunksize, SEEK_CUR);
  
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
    return 3;
  }

  printf("WOZ revision %c file\n", wozheader.id[3]);
  printf("Checksum %.8x\n", wozheader.crc);

  // Process chunks
  while (!feof(fp))
  {
    struct woz_chunkheader chunkheader;

    if (fread(&chunkheader, sizeof(chunkheader), 1, fp)==0)
      break;

    if (woz_processchunk(&chunkheader, fp)!=0)
    {
      // Something went wrong
      fclose(fp);

      return 4;
    }
  }

  fclose(fp);

  return 0;
}
