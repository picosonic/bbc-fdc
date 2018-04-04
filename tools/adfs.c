#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "diskstore.h"
#include "adfs.h"

// Reverse log2
unsigned long rev_log2(const unsigned long x)
{
  return (1<<x);
}

// Read an (unaligned) value of length 1..4 bytes
unsigned long adfs_readval(unsigned char *p, int len)
{
  unsigned long val = 0;

  switch (len)
  {
    case 4:  val |= p[3] << 24;
    case 3:  val |= p[2] << 16;
    case 2:  val |= p[1] << 8;
    default: val |= p[0];
  }

  return val;
}

unsigned char adfs_checksum(const unsigned char *buff, const int sectorsize)
{
  unsigned char carry;
  unsigned int sum;
  int i;

  carry=0;

  // Don't include the checksum in the calculation
  i=sectorsize-1;

  if (sectorsize==1024)
    sum=0;
  else
    sum=255;

  do
  {
    sum=sum+buff[i-1]+carry;
    if (sum>255)
      carry=1;
    else
      carry=0;
    sum=sum&255;

    i--;
  } while (i>0);

  return (sum&255);
}

// New map zone check
unsigned char map_zone_valid_byte(void const * const map, const unsigned char log2_sector_size, unsigned int zone)
{
  unsigned char const * const map_base = map;
  unsigned int sum_vector0;
  unsigned int sum_vector1;
  unsigned int sum_vector2;
  unsigned int sum_vector3;
  unsigned int zone_start;
  unsigned int rover;

  // Sanitise sectorsize
  if ((log2_sector_size<8) || (log2_sector_size>10))
    return 0;

  sum_vector0 = 0;
  sum_vector1 = 0;
  sum_vector2 = 0;
  sum_vector3 = 0;
  zone_start = zone<<log2_sector_size;

  for (rover=((zone+1)<<log2_sector_size)-4; rover>zone_start; rover-=4)
  {
    sum_vector0 += map_base[rover+0] + (sum_vector3>>8);
    sum_vector3 &= 0xff;
    sum_vector1 += map_base[rover+1] + (sum_vector0>>8);
    sum_vector0 &= 0xff;
    sum_vector2 += map_base[rover+2] + (sum_vector1>>8);
    sum_vector1 &= 0xff;
    sum_vector3 += map_base[rover+3] + (sum_vector2>>8);
    sum_vector2 &= 0xff;
  }

  // Don't add the check byte when calculating its value
  sum_vector0 += (sum_vector3>>8);
  sum_vector1 += map_base[rover+1] + (sum_vector0>>8);
  sum_vector2 += map_base[rover+2] + (sum_vector1>>8);
  sum_vector3 += map_base[rover+3] + (sum_vector2>>8);

  return (unsigned char) ((sum_vector0^sum_vector1^sum_vector2^sum_vector3) & 0xff);
}

void adfs_gettitle(const int adfs_format, char *title, const int titlelen)
{
  int map, dir;
  unsigned int adfs_sectorsize;
  Disk_Sector *sector0;
  Disk_Sector *sector1;
  int i, j;

  // Blank out title
  title[0]=0;

  switch (adfs_format)
  {
    case ADFS_S:
    case ADFS_M:
    case ADFS_L:
      map=ADFS_OLDMAP;
      dir=ADFS_OLDDIR;
      adfs_sectorsize=ADFS_8BITSECTORSIZE;
      break;

    case ADFS_D:
      map=ADFS_OLDMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      break;

    case ADFS_E:
    case ADFS_F:
      map=ADFS_NEWMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      break;

    case ADFS_UNKNOWN:
    default:
      return;
  }

  // Search for sectors
  sector0=diskstore_findhybridsector(0, 0, 0);
  sector1=diskstore_findhybridsector(0, 0, 1);

  // Check we have both sectors
  if ((sector0==NULL) || (sector1==NULL))
    return;

  // Check we have data for both sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return;

  if (map==ADFS_OLDMAP)
  {
    unsigned char oldmapbuff[512];

    // Check there is enough space in return string
    if (titlelen<11) return;

    // Populate old map
    memcpy(oldmapbuff, sector0->data, sizeof(oldmapbuff));
    if (sector1->datasize==ADFS_8BITSECTORSIZE)
      memcpy(&oldmapbuff[256], sector1->data, sector1->datasize);

    j=0;

    for (i=0; i<5; i++)
    {
      int c;

      c=oldmapbuff[247+i];
      if (c==0x00) break;
      title[j++]=c & 0x7f;
      title[j]=0;

      c=oldmapbuff[ADFS_8BITSECTORSIZE+246+i];
      if (c==0x00) break;
      title[j++]=c & 0x7f;
      title[j]=0;
    }
  }
}

void adfs_showinfo(const int adfs_format)
{
  int map, dir;
  unsigned int adfs_sectorsize;
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  switch (adfs_format)
  {
    case ADFS_S:
    case ADFS_M:
    case ADFS_L:
      map=ADFS_OLDMAP;
      dir=ADFS_OLDDIR;
      adfs_sectorsize=ADFS_8BITSECTORSIZE;
      break;

    case ADFS_D:
      map=ADFS_OLDMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      break;

    case ADFS_E:
    case ADFS_F:
      map=ADFS_NEWMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      break;

    case ADFS_UNKNOWN:
    default:
      return;
  }

  if (map==ADFS_OLDMAP)
  {
    unsigned char oldmapbuff[512];
    unsigned long discid;
    int i;

    // Search for sectors
    sector0=diskstore_findhybridsector(0, 0, 0);
    sector1=diskstore_findhybridsector(0, 0, 1);

    // Check we have both sectors
    if ((sector0==NULL) || (sector1==NULL))
      return;

    // Check we have data for both sectors
    if ((sector0->data==NULL) || (sector1->data==NULL))
      return;

    memcpy(oldmapbuff, sector0->data, sizeof(oldmapbuff));
    if (sector1->datasize==ADFS_8BITSECTORSIZE)
      memcpy(&oldmapbuff[256], sector1->data, sector1->datasize);

    printf("FreeStart: ");
    for (i=0; i<ADFS_OLDMAPLEN; i++)
    {
      unsigned long c=adfs_readval(&oldmapbuff[i*3], 3);

      printf("%.3lx ", c);
    }
    printf("\n");

    printf("Disc name: \"");
    for (i=0; i<5; i++)
    {
      int c=oldmapbuff[247+i];
      if (c==0x00) break;
      printf("%c", (c>=' ')&(c<='~')?c:'.');
      c=oldmapbuff[256+246+i];
      if (c==0x00) break;
      printf("%c", (c>=' ')&(c<='~')?c:'.');
    }
    printf("\"\n");

    printf("Sectors on disc: %ld\n", adfs_readval(&oldmapbuff[252], 3));
    printf("Check0: %.2x\n", oldmapbuff[255]);
    printf("FreeLen: ");
    for (i=0; i<ADFS_OLDMAPLEN; i++)
    {
      unsigned long c=adfs_readval(&oldmapbuff[256+(i*3)], 3);

      printf("%.3lx ", c);
    }
    printf("\n");

    discid=adfs_readval(&oldmapbuff[507], 2);
    printf("Disc ID: %.4lx (%ld)\n", discid, discid);
    printf("Boot option: %.2x ", oldmapbuff[509]);
    switch (oldmapbuff[509])
    {
      case 0: printf("No action\n"); break;
      case 1: printf("*Load boot file\n"); break;
      case 2: printf("*Run boot file\n"); break;
      case 3: printf("*Exec boot file\n"); break;
      default: printf("Unknown\n"); break;
    }
    printf("FreeEnd: %.2x\n", oldmapbuff[510]);
    printf("Check1: %.2x\n", oldmapbuff[511]);

// TODO

  }
}

int adfs_validate()
{
  int format;
  unsigned char sniff[1024];
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  // Search for sectors
  sector0=diskstore_findhybridsector(0, 0, 0);
  sector1=diskstore_findhybridsector(0, 0, 1);

  format=ADFS_UNKNOWN;

  // Check we have both sectors
  if ((sector0==NULL) || (sector1==NULL))
    return format;

  // Check we have data for both sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return format;

  // Check for ADFS
  if (((sector0->datasize==ADFS_8BITSECTORSIZE) && (sector1->datasize==ADFS_8BITSECTORSIZE)) ||
      ((sector0->datasize==ADFS_16BITSECTORSIZE) && (sector1->datasize==ADFS_16BITSECTORSIZE)))
  {
    // Copy sector data to sniff buffer
    bzero(sniff, sizeof(sniff));

    memcpy(sniff, sector0->data, sector0->datasize);
    if (sector1->datasize==ADFS_8BITSECTORSIZE)
      memcpy(&sniff[256], sector1->data, sector1->datasize);

    /////////////////////////////////////////////////////////////
    // Test for old map
    /////////////////////////////////////////////////////////////

    // Check reserved byte is zero
    if (sniff[246]==0)
    {
      // Validate first checksum
      if (adfs_checksum(&sniff[0], ADFS_8BITSECTORSIZE)==sniff[ADFS_8BITSECTORSIZE-1])
      {
        // Validate second checksum
        if (adfs_checksum(&sniff[ADFS_8BITSECTORSIZE], ADFS_8BITSECTORSIZE)==sniff[(ADFS_8BITSECTORSIZE*2)-1])
        {
          unsigned long sec;
          int i;

          sec=0;
          // Validate OLD MAP
          for (i=0; i<ADFS_OLDMAPLEN; i++)
          {
            sec|=adfs_readval(&sniff[i*3], 3);
            sec|=adfs_readval(&sniff[ADFS_8BITSECTORSIZE+(i*3)], 3);
          }

          // Make sure top 3 bits are never set for any FreeStart or FreeLen
          if ((sec&0xE0000000)==0)
          {
            // Make sure free space end pointer is a multiple of 3
            if (((sniff[ADFS_8BITSECTORSIZE+254]/3)*3)==sniff[ADFS_8BITSECTORSIZE+254])
            {
              unsigned disksectors;

              // This looks like an old map disk so far, so check number of 256 byte allocation units
              disksectors=adfs_readval(&sniff[252], 3);

              // 5 * 4 * 80 * 2
              if (disksectors==3200)
                format=ADFS_D;

              // 16 * 80 * 2
              if (disksectors==2560)
                format=ADFS_L;

              // 16 * 80 * 1
              if (disksectors==1280)
                format=ADFS_M;

              // 16 * 40 * 1
              if (disksectors==640)
                format=ADFS_S;
            }
          }
        }
      }
    }

    /////////////////////////////////////////////////////////////
    // Test for new map
    /////////////////////////////////////////////////////////////
    if ((format==ADFS_UNKNOWN) && ((sector0->datasize==ADFS_16BITSECTORSIZE) && (sector1->datasize==ADFS_16BITSECTORSIZE)))
    {
      unsigned char zonecheck;
      unsigned long sectorsize;
      unsigned long sectorspertrack;

      // Validate NewMap ZoneCheck for zone 0
      zonecheck=map_zone_valid_byte(&sniff, sniff[4], 0);

      if (zonecheck==sniff[0])
      {
        sectorsize=rev_log2(sniff[4]);
        sectorspertrack=sniff[5];

        // TODO validate CrossCheck

        if ((sectorsize==1024) && (sectorspertrack==5))
        {
          format=ADFS_E;
        }
        else
        {
          // Check for ADFSF with a boot block, and hence discrecord at position 0xc00 + 0x1c0
          if (adfs_checksum(&sniff[0], ADFS_16BITSECTORSIZE)==sniff[ADFS_16BITSECTORSIZE-1])
          {
          }
        }
      }
    }
  }

  return format;
}
