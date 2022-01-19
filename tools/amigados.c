#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "diskstore.h"
#include "amigamfm.h"
#include "amigados.h"

uint32_t amigados_rootblock=0;

int amigados_debug=0;

void amigados_decodedate(const uint32_t days, const uint32_t mins, const uint32_t ticks, struct tm *tim)
{
  struct timeval tv;

  tv.tv_sec=AMIGADOS_EPOCH+(days*(24*60*60))+(mins*60)+(ticks/50);
  tv.tv_usec=0;

  localtime_r(&tv.tv_sec, tim);
}

uint32_t amigados_readlong(const uint32_t offset, const uint8_t *data)
{
  return ((data[offset+0]<<24) | (data[offset+1]<<16) | (data[offset+2]<<8) | (data[offset+3]));
}

uint16_t amigados_readshort(const uint32_t offset, const uint8_t *data)
{
  return ((data[offset+0]<<8) | (data[offset+1]));
}

void amigados_gettitle(const unsigned int disktracks, char *title, const int titlelen)
{
  uint8_t tmpbuff[AMIGA_DATASIZE];

  if (amigados_rootblock==0) return;

  // Absolute seek and read
  diskstore_absoluteseek(amigados_rootblock*AMIGA_DATASIZE, INTERLEAVED, disktracks);

  if (diskstore_absoluteread((char *)tmpbuff, AMIGA_DATASIZE, INTERLEAVED, disktracks)<AMIGA_DATASIZE)
    return;

  if (titlelen>tmpbuff[AMIGA_DATASIZE-0x50])
  {
    int i;

    for (i=0; i<tmpbuff[AMIGA_DATASIZE-0x50]; i++)
      title[i]=tmpbuff[(AMIGA_DATASIZE-0x4f)+i];

    title[i]=0;
  }
}

void amigados_readfsentry(const unsigned int level, const unsigned int disktracks, const uint32_t fsblock)
{
  uint32_t i;
  uint32_t prot;
  uint8_t fsbuff[AMIGA_DATASIZE];
  struct tm tim;

  // Absolute seek and read
  diskstore_absoluteseek(fsblock*AMIGA_DATASIZE, INTERLEAVED, disktracks);

  if (diskstore_absoluteread((char *)fsbuff, AMIGA_DATASIZE, INTERLEAVED, disktracks)<AMIGA_DATASIZE)
    return;

  // Check type
  if (amigados_readlong(0, fsbuff)!=2)
    return;

  // Check self pointer
  if (amigados_readlong(4, fsbuff)!=fsblock)
    return;

  for (i=0; i<level; i++)
    printf("  ");

  // Filename
  printf("  ");
  for (i=0; i<fsbuff[AMIGA_DATASIZE-0x50]; i++)
    printf("%c", fsbuff[(AMIGA_DATASIZE-0x4f)+i]);

  for (; i<31; i++)
    printf(" ");

  // Check for Dir or File
  if (amigados_readlong(AMIGA_DATASIZE-4, fsbuff)==AMIGADOS_DIR)
  {
    printf("    Dir");
  }
  else
  {
    // File size
    printf(" %6u", amigados_readlong(AMIGA_DATASIZE-0xbc, fsbuff));
  }

  // Protection
  prot=amigados_readlong(AMIGA_DATASIZE-0xc0, fsbuff);
  printf(" ");

  if (prot&0x800) printf("r"); else printf("-");
  if (prot&0x400) printf("w"); else printf("-");
  if (prot&0x200) printf("e"); else printf("-");
  if (prot&0x100) printf("d"); else printf("-");

  if (prot&0x8) printf("-"); else printf("r");
  if (prot&0x4) printf("-"); else printf("w");
  if (prot&0x2) printf("-"); else printf("e");
  if (prot&0x1) printf("-"); else printf("d");

  // Last change date
  amigados_decodedate(amigados_readlong(AMIGA_DATASIZE-0x5c, fsbuff), amigados_readlong(AMIGA_DATASIZE-0x58, fsbuff), amigados_readlong(AMIGA_DATASIZE-0x54, fsbuff), &tim);
  printf(" %.2d:%.2d:%.2d %.2d/%.2d/%d", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

  // Optional comment
  if (amigados_readlong(AMIGA_DATASIZE-0xb8, fsbuff)>0)
  {
    printf("  (");
    for (i=0; i<fsbuff[AMIGA_DATASIZE-0xb8]; i++)
      printf("%c", fsbuff[(AMIGA_DATASIZE-0xb7)+i]);
    printf(")");
  }

  printf("\n");

  // If this is a directory process child entries
  for (i=0; i<((AMIGA_DATASIZE/4)-56); i++)
  {
    uint32_t fsdblock;

    fsdblock=amigados_readlong(0x18+(i*4), fsbuff);

    if (fsdblock!=0)
    {
      if (amigados_debug)
        printf("  HT[%u] %.8x\n", i, fsdblock);

      amigados_readfsentry(level+1, disktracks, fsdblock);
    }
  }

  // Check for fs entries which share the same hash by following hash chain
  if (amigados_readlong(AMIGA_DATASIZE-0x10, fsbuff)!=0)
    amigados_readfsentry(level, disktracks, amigados_readlong(AMIGA_DATASIZE-0x10, fsbuff));
}

// http://lclevy.free.fr/adflib/adf_info.html
void amigados_showinfo(const unsigned int disktracks, const int debug)
{
  uint32_t i;
  uint8_t tmpbuff[AMIGA_DATASIZE];
  struct tm tim;
  (void) debug;

  if (amigados_rootblock==0) return;

  printf("Rootblock @ %u\n", amigados_rootblock);

  // Absolute seek and read
  diskstore_absoluteseek(amigados_rootblock*AMIGA_DATASIZE, INTERLEAVED, disktracks);

  if (diskstore_absoluteread((char *)tmpbuff, AMIGA_DATASIZE, INTERLEAVED, disktracks)<AMIGA_DATASIZE)
    return;

  printf("Rootblock\n");

  printf("Type : %u\n", amigados_readlong(0, tmpbuff));
  printf("Hash table size : %u\n", amigados_readlong(0xc, tmpbuff));
  printf("Checksum : %.8x\n", amigados_readlong(0x14, tmpbuff));

  printf("\n");

  for (i=0; i<amigados_readlong(0xc, tmpbuff); i++)
  {
    uint32_t fsblock;

    fsblock=amigados_readlong(0x18+(i*4), tmpbuff);

    if (fsblock!=0)
    {
      if (amigados_debug)
        printf("  HT[%u] %.8x\n", i, fsblock);

      amigados_readfsentry(0, disktracks, fsblock);
    }
  }

  printf("\n");

  amigados_decodedate(amigados_readlong(AMIGA_DATASIZE-0x5c, tmpbuff), amigados_readlong(AMIGA_DATASIZE-0x58, tmpbuff), amigados_readlong(AMIGA_DATASIZE-0x54, tmpbuff), &tim);
  printf("Root dir changed : %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

  amigados_decodedate(amigados_readlong(AMIGA_DATASIZE-0x28, tmpbuff), amigados_readlong(AMIGA_DATASIZE-0x24, tmpbuff), amigados_readlong(AMIGA_DATASIZE-0x20, tmpbuff), &tim);
  printf("Volume changed : %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

  amigados_decodedate(amigados_readlong(AMIGA_DATASIZE-0x1c, tmpbuff), amigados_readlong(AMIGA_DATASIZE-0x18, tmpbuff), amigados_readlong(AMIGA_DATASIZE-0x14, tmpbuff), &tim);
  printf("Filesystem creation : %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);

  printf("Volume name : \"");
  for (i=0; i<tmpbuff[AMIGA_DATASIZE-0x50]; i++)
    printf("%c", tmpbuff[(AMIGA_DATASIZE-0x4f)+i]);
  printf("\"\n");

  printf("Extension : %u\n", amigados_readlong(AMIGA_DATASIZE-0x8, tmpbuff));
  printf("Secondary type : %u\n", amigados_readlong(AMIGA_DATASIZE-0x4, tmpbuff));

  printf("\n");
}

uint32_t amigados_calcbootchecksum(const uint8_t *bootblock)
{
  uint32_t checksum;
  unsigned int i;

  checksum=0;

  for (i=0; i<AMIGADOS_BOOTBLOCKSIZE/4; i++)
  {
    uint32_t presum;

    presum=checksum;

    checksum+=((bootblock[i*4]<<24) | (bootblock[i*4+1]<<16) | (bootblock[i*4+2]<<8) | (bootblock[i*4+3]));

    if (checksum<presum)
      ++checksum;
  }

  return ~(checksum);
}

int amigados_validate()
{
  int format;
  uint8_t sniff[AMIGA_SECTOR_SIZE];
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  format=AMIGADOS_UNKNOWN;

  // Search for sectors
  sector0=diskstore_findhybridsector(0, 0, 0);
  sector1=diskstore_findhybridsector(0, 0, 1);

  // Check we have sectors
  if ((sector0==NULL) || (sector1==NULL))
    return format;

  // Check we have data for sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return format;

  // Check sector is 512 bytes long
  if ((sector0->datasize==AMIGA_DATASIZE) && (sector1->datasize==AMIGA_DATASIZE))
  {
    // Copy sector data to sniff buffer
    bzero(sniff, sizeof(sniff));

    memcpy(sniff, sector0->data, sector0->datasize);
    memcpy(&sniff[AMIGA_DATASIZE], sector1->data, sector1->datasize);

    // Test for "DOS"
    if ((sniff[0]=='D') &&
        (sniff[1]=='O') &&
        (sniff[2]=='S'))
    {
      uint32_t checksum;

      checksum=sniff[4];
      checksum=(checksum<<8)|sniff[5];
      checksum=(checksum<<8)|sniff[6];
      checksum=(checksum<<8)|sniff[7];

      amigados_rootblock=sniff[8];
      amigados_rootblock=(amigados_rootblock<<8)|sniff[9];
      amigados_rootblock=(amigados_rootblock<<8)|sniff[10];
      amigados_rootblock=(amigados_rootblock<<8)|sniff[11];

      if (amigados_rootblock==AMIGADOS_DD_ROOTBLOCK)
      {
        format=AMIGADOS_DOS_FORMAT;

        if (amigados_debug)
        {
          printf("Amiga DOS found\n");

          if (sniff[3]&0x01)
            printf("FFS (AmigaDOS 2.04), ");
          else
            printf("OFS (AmigaDOS 1.2), ");

          if (sniff[3]&0x02)
            printf("INTL ONLY, ");
          else
            printf("NO_INTL ONLY, ");

          if (sniff[3]&0x04)
            printf("DIRC&INTL\n");
          else
            printf("NO_DIRC&INTL\n");

          // Clear old checksum
          memset(&sniff[4], 0, 4);

          printf("Checksum : %x, calculated %x\n", checksum, amigados_calcbootchecksum((uint8_t *)&sniff));
          printf("Rootblock : %u\n", amigados_rootblock);
        }
      }
    }
    else if ((sniff[0]=='P') &&
        (sniff[1]=='F') &&
        (sniff[2]=='S'))
    {
      format=AMIGADOS_PFS_FORMAT;

      printf("Amiga PFS (Professional File System) found\n");
    }
  }

  return format;
}

void amigados_init(const int debug)
{
  amigados_debug=debug;
  amigados_rootblock=0;
}
