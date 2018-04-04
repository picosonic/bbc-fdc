#include <stdlib.h>
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

int adfs_validate(Disk_Sector *sector0, Disk_Sector *sector1)
{
  int format;
  unsigned char sniff[1024];

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
    if (format==ADFS_UNKNOWN)
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
        }
      }
    }
  }

  return format;
}
