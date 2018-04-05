#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

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

  if (sectorsize==ADFS_16BITSECTORSIZE)
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
      memcpy(&oldmapbuff[ADFS_8BITSECTORSIZE], sector1->data, sector1->datasize);

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

void adfs_readdir(const char *folder, const int maptype, const int dirtype, const int offset, const unsigned int adfs_sectorsize)
{
  struct adfs_dirheader dh;
  int i;
  int entry;
  int entries;
  unsigned char attrib;
  int hasfiletype;
  Disk_Sector *dsector;
  int secpos;

  int c, h, r; // Cylinder, head, record

  printf("['%s' @%x MAP:%d DIR:%d]\n", folder, offset, maptype, dirtype);

  // Only support old maps for now
  if (maptype!=ADFS_OLDMAP)
    return;

  // TODO Work out CHR values from offset/sectorsize

  // TODO Find sector matching CHR
  if (offset==0x200)
    dsector=diskstore_findhybridsector(0, 0, 2);

  if (offset==0x400)
    dsector=diskstore_findhybridsector(0, 0, 1);

  if ((dsector==NULL) || (dsector->data==NULL))
    return;

  // TODO Read directory entry from sector

  secpos=0;
  memcpy(&dh, dsector->data, sizeof(dh));
  secpos+=sizeof(dh);

  // TODO Iterate through directory
  printf("StartMasSeq: %.2x\n", dh.startmasseq);
  printf("StartName: \"");
  for (i=0; i<4; i++)
  {
    int c=dh.startname[i];
    printf("%c", (c>=' ')&(c<='~')?c:'.');
  }
  printf("\"\n");

  if (dirtype==ADFS_OLDDIR)
    entries=ADFS_OLDDIR_ENTRIES;
  else
    entries=ADFS_NEWDIR_ENTRIES;

  for (entry=0; entry<entries; entry++)
  {
    struct adfs_direntry de;
    unsigned int filetype;
    char filename[ADFS_MAXPATHLEN];
    struct timeval tv;
    uint32_t indirectaddr;

    if ((secpos+sizeof(de))<dsector->datasize)
    {
      memcpy(&de, &dsector->data[secpos], sizeof(de));
      secpos+=sizeof(de);

      if (de.dirobname[0]==0) break;

      filename[0]=0;
      filetype=0;
      hasfiletype=0;
      printf("\nName: %s", folder);
      if (folder[0]!=0) printf("/");
      for (i=0; i<10; i++)
      {
        int c=(de.dirobname[i]&0x7f);
        if ((c==0) || (c==0x0d) || (c==0x0a)) break;
        printf("%c", (c>=' ')&(c<='~')?c:'.');

        filename[strlen(filename)+1]=0;
        filename[strlen(filename)]=c;
      }
      printf("\n");

      // Extract object attributes
      if (dirtype==ADFS_NEWDIR)
      {
        attrib=de.newdiratts;
      }
      else
      {
        attrib=0;

        if (de.dirobname[0]&0x80) attrib|=ADFS_OWNER_READ;
        if (de.dirobname[1]&0x80) attrib|=ADFS_OWNER_WRITE;
        if (de.dirobname[2]&0x80) attrib|=ADFS_LOCKED;
        if (de.dirobname[3]&0x80) attrib|=ADFS_DIRECTORY;
        if (de.dirobname[4]&0x80) attrib|=ADFS_EXECUTABLE;
        if (de.dirobname[5]&0x80) attrib|=ADFS_PUBLIC_READ;
        if (de.dirobname[6]&0x80) attrib|=ADFS_PUBLIC_WRITE;
      }

      // Check for object having a filetype+timestamp
      if ((de.dirload&0xfff00000) == 0xfff00000)
        hasfiletype=1;

      printf("Length: %ld\n", (unsigned long)de.dirlen);

      indirectaddr=((de.dirinddiscadd[2]<<16) | (de.dirinddiscadd[1]<<8) | de.dirinddiscadd[0]);

      if (maptype==ADFS_OLDMAP)
      {
        printf("Indirect disc offset address: %.6lx\n", (unsigned long)indirectaddr);
      }
      else
      {
      //  printf("Indirect disc address: Fragment %x (Sector %x) Offset %x\n", (indirectaddr&0x7fff00)>>8, fragstart[(indirectaddr&0x7fff00)>>8], indirectaddr&0xff);
      }

      if (hasfiletype==0)
      {
        printf("Load address: %.8lx\n", (unsigned long)de.dirload);
        printf("Exec address: %.8lx\n", (unsigned long)de.direxec);
      }
      else
      {
        // Get the epoch
        if (dirtype==ADFS_NEWDIR)
        {
          unsigned int hightime=(de.dirload&0xff);
          unsigned int lowtime=de.direxec;
          unsigned long long csec=(((unsigned long long)hightime<<32) | lowtime);

          filetype=((0x000fff00 & de.dirload)>>8);

          printf("File type: %.3x\n", filetype);
          if ((csec/100)>=ADFS_RISCUNIXTSDIFF)
          {
            struct tm *tim;

            csec=(csec/100)-ADFS_RISCUNIXTSDIFF;

            tv.tv_sec=csec;
            tv.tv_usec=0;

            tim = localtime(&tv.tv_sec);

            printf("TS: %.2d:%.2d:%.2d %.2d/%.2d/%d\n", tim->tm_hour, tim->tm_min, tim->tm_sec, tim->tm_mday, tim->tm_mon+1, tim->tm_year+1900);
          }
        }
      }

      // TODO Recurse into directories
      if (0!=(attrib&ADFS_DIRECTORY))
      {
        char newfolder[ADFS_MAXPATHLEN];

        sprintf(newfolder, "%s/%s", folder, filename);

/*
        if (maptype==ADFS_OLDMAP)
          readdir(newfolder, maptype, dirtype, indirectaddr*adfs_sectorsize, adfs_sectorsize);
        else
          readdir(newfolder, maptype, dirtype, fragstart[(indirectaddr&0x7fff00)>>8]*1024, adfs_sectorsize);
*/
      }

      // Attributes
      printf("Attributes: %.2x ", attrib);

      if ((0!=(attrib&ADFS_OWNER_READ)) ||
         (0!=(attrib&ADFS_EXECUTABLE)))
        printf("R");
      else
        printf("-");

      if (0==(attrib&ADFS_OWNER_WRITE))
        printf("-");
      else
        printf("W");

      if (0==(attrib&ADFS_LOCKED))
        printf("-");
      else
        printf("L");

      if (0==(attrib&ADFS_DIRECTORY))
        printf("F");
      else
        printf("D");

      if (0==(attrib&ADFS_PUBLIC_READ))
        printf("-");
      else
        printf("r");

      if (0==(attrib&ADFS_PUBLIC_WRITE))
        printf("-");
      else
        printf("w");

      printf("\n");

      if (dirtype==ADFS_OLDDIR)
        printf("OldDirObSeq: %.2x\n", de.olddirobseq);
    }
    else
    {
      printf("**Sector data breached at entry %d/%d**\n", entry, entries);
      break;
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
    unsigned char oldmapbuff[ADFS_8BITSECTORSIZE*2];
    struct adfs_oldmap *oldmap;
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

    bzero(oldmapbuff, sizeof(oldmapbuff));
    if (sector1->datasize==ADFS_8BITSECTORSIZE)
    {
      memcpy(oldmapbuff, sector0->data, ADFS_8BITSECTORSIZE);
      memcpy(&oldmapbuff[ADFS_8BITSECTORSIZE], sector1->data, sector1->datasize);
    }
    else
      memcpy(oldmapbuff, sector0->data, sizeof(oldmapbuff));

    oldmap=(struct adfs_oldmap *)&oldmapbuff[0];

    printf("FreeStart: ");
    for (i=0; i<ADFS_OLDMAPLEN; i++)
    {
      unsigned long c=adfs_readval(&oldmap->freestart[i*ADFS_OLDMAPENTRY], ADFS_OLDMAPENTRY);

      printf("%.3lx ", c);
    }
    printf("\n");

    printf("Disc name: \"");
    for (i=0; i<5; i++)
    {
      int c=oldmap->oldname0[i];
      if (c==0x00) break;
      printf("%c", (c>=' ')&(c<='~')?c:'.');
      c=oldmap->oldname1[i];
      if (c==0x00) break;
      printf("%c", (c>=' ')&(c<='~')?c:'.');
    }
    printf("\"\n");

    printf("Disc size in (256 byte) sectors: %ld\n", adfs_readval((unsigned char *)&oldmap->oldsize, ADFS_OLDMAPENTRY));
    printf("Check0: %.2x\n", oldmap->check0);
    printf("FreeLen: ");
    for (i=0; i<ADFS_OLDMAPLEN; i++)
    {
      unsigned long c=adfs_readval(&oldmap->freelen[i*ADFS_OLDMAPENTRY], ADFS_OLDMAPENTRY);

      printf("%.3lx ", c);
    }
    printf("\n");

    discid=oldmap->oldid;
    printf("Disc ID: %.4lx (%ld)\n", discid, discid);
    printf("Boot option: %.2x ", oldmap->oldboot);
    switch (oldmap->oldboot)
    {
      case 0: printf("No action\n"); break;
      case 1: printf("*Load boot file\n"); break;
      case 2: printf("*Run boot file\n"); break;
      case 3: printf("*Exec boot file\n"); break;
      default: printf("Unknown\n"); break;
    }
    printf("FreeEnd: %.2x\n", oldmap->freeend);
    printf("Check1: %.2x\n", oldmap->check1);

    // Do a directory listing, using root at start of 1st sector after 512 bytes of old map
    //   as per RiscOS PRM 2-200
    if (dir==ADFS_NEWDIR)
      adfs_readdir("", map, dir, ADFS_16BITSECTORSIZE, adfs_sectorsize);
    else
      adfs_readdir("", map, dir, ADFS_8BITSECTORSIZE*2, adfs_sectorsize);
  }
}

int adfs_validate()
{
  int format;
  unsigned char sniff[ADFS_16BITSECTORSIZE];
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  // Search for first two sectors
  sector0=diskstore_findhybridsector(0, 0, 0);
  sector1=diskstore_findhybridsector(0, 0, 1);

  format=ADFS_UNKNOWN;

  // Check we have both sectors
  if ((sector0==NULL) || (sector1==NULL))
    return format;

  // Check we have data for both sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return format;

  // Check both sectors are either 256 or 1024 bytes long
  if (((sector0->datasize==ADFS_8BITSECTORSIZE) && (sector1->datasize==ADFS_8BITSECTORSIZE)) ||
      ((sector0->datasize==ADFS_16BITSECTORSIZE) && (sector1->datasize==ADFS_16BITSECTORSIZE)))
  {
    struct adfs_oldmap *oldmap;

    // Copy at least 512 bytes of sector data to sniff buffer
    bzero(sniff, sizeof(sniff));

    memcpy(sniff, sector0->data, sector0->datasize);
    if (sector1->datasize==ADFS_8BITSECTORSIZE)
      memcpy(&sniff[ADFS_8BITSECTORSIZE], sector1->data, sector1->datasize);

    /////////////////////////////////////////////////////////////
    // Test for old map
    /////////////////////////////////////////////////////////////
    oldmap=(struct adfs_oldmap *)&sniff[0];
    // Check reserved byte is zero
    if (oldmap->reserved==0)
    {
      // Validate first checksum
      if (adfs_checksum(&sniff[0], ADFS_8BITSECTORSIZE)==oldmap->check0)
      {
        // Validate second checksum
        if (adfs_checksum(&sniff[ADFS_8BITSECTORSIZE], ADFS_8BITSECTORSIZE)==oldmap->check1)
        {
          unsigned long sec;
          int i;

          sec=0;
          // Validate OLD MAP
          for (i=0; i<ADFS_OLDMAPLEN; i++)
          {
            sec|=adfs_readval(&oldmap->freestart[i*ADFS_OLDMAPENTRY], ADFS_OLDMAPENTRY);
            sec|=adfs_readval(&oldmap->freelen[i*ADFS_OLDMAPENTRY], ADFS_OLDMAPENTRY);
          }

          // Make sure top 3 bits are never set for any FreeStart or FreeLen
          if ((sec&0xE0000000)==0)
          {
            // Make sure free space end pointer is a multiple of 3
            if (((oldmap->freeend/ADFS_OLDMAPENTRY)*ADFS_OLDMAPENTRY)==oldmap->freeend)
            {
              unsigned disksectors;

              // This looks like an old map disk so far, so check number of 256 byte allocation units
              disksectors=adfs_readval((unsigned char *)&oldmap->oldsize, ADFS_OLDMAPENTRY);

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

        if ((sectorsize==ADFS_16BITSECTORSIZE) && (sectorspertrack==5))
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
