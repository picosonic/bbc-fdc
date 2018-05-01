#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>
#include <stdint.h>

#include "diskstore.h"
#include "adfs.h"

int adfs_debug=0;

long *fragstart;

// Reverse log2
unsigned long rev_log2(const unsigned long x)
{
  return (unsigned long)(1<<x);
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

// New map zone check, from RiscOS PRM 2-206/207
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
    unsigned char oldmapbuff[ADFS_8BITSECTORSIZE*2];
    struct adfs_oldmap *oldmap;

    // Check there is enough space in return string
    if (titlelen<11) return;

    // Populate old map
    bzero(oldmapbuff, sizeof(oldmapbuff));
    if (sector1->datasize==ADFS_8BITSECTORSIZE)
    {
      memcpy(oldmapbuff, sector0->data, ADFS_8BITSECTORSIZE);
      memcpy(&oldmapbuff[ADFS_8BITSECTORSIZE], sector1->data, sector1->datasize);
    }
    else
      memcpy(oldmapbuff, sector0->data, sizeof(oldmapbuff));

    oldmap=(struct adfs_oldmap *)&oldmapbuff[0];

    j=0;

    for (i=0; i<5; i++)
    {
      int c=oldmap->oldname0[i];
      if (c==0x00) break;
      title[j++]=c & 0x7f;
      title[j]=0;

      c=oldmap->oldname1[i];
      if (c==0x00) break;
      title[j++]=c & 0x7f;
      title[j]=0;
    }
  }
}

char *adfs_filetype(const unsigned int filetype)
{
  switch (filetype)
  {
    case 0x695: return "GIF";
    case 0xa91: return "Zip";
    case 0xadf: return "PDF";
    case 0xae9: return "Alarm";
    case 0xaf1: return "Music";
    case 0xaff: return "DrawFile";
    case 0xb60: return "PNG";
    case 0xc85: return "JPEG";
    case 0xddc: return "Archive";
    case 0xdea: return "DXF";
    case 0xdec: return "DiscRec";
    case 0xf89: return "GZip";
    case 0xfae: return "Resource";
    case 0xfaf: return "HTML";
    case 0xfb0: return "Allocate";
    case 0xfc3: return "Patch";
    case 0xfc6: return "PrntDefn";
    case 0xfca: return "Squash";
    case 0xfcd: return "HardDisc";
    case 0xfce: return "FloppyDisc";
    case 0xfd1: return "BASICTxt";
    case 0xfe1: return "Make";
    case 0xfd6: return "TaskExec";
    case 0xfd7: return "TaskObey";
    case 0xfdb: return "TextCRLF";
    case 0xfea: return "Desktop";
    case 0xfeb: return "Obey";
    case 0xfec: return "Template"; 
    case 0xfed: return "Palette";
    case 0xff2: return "Config";
    case 0xff5: return "PoScript";
    case 0xff6: return "Font";
    case 0xff8: return "Absolute";
    case 0xff9: return "Sprite";
    case 0xffa: return "Module";
    case 0xffb: return "Basic";
    case 0xffc: return "Utility";
    case 0xffd: return "Data";
    case 0xffe: return "Command";
    case 0xfff: return "Text";
    default: return "";
  }
}

void adfs_readdir(const int level, const char *folder, const int maptype, const int dirtype, const unsigned long offset, const unsigned int adfs_sectorsize, const unsigned char sectorspertrack)
{
  struct adfs_dirheader dh;
  struct adfs_direntry de;
  int i;
  int entry;
  int entries;
  unsigned char attrib;
  int hasfiletype;

  diskstore_absoluteseek(offset, dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);
  diskstore_absoluteread((char *)&dh, sizeof(dh), dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);

  if (adfs_debug)
    printf("['%s' @0x%lx MAP:%d DIR:%d SECSIZE:%u SEC/TRACK:%d]\n", folder, offset, maptype, dirtype, adfs_sectorsize, sectorspertrack);

  // Iterate through directory
  if (adfs_debug)
  {
    printf("StartMasSeq: %.2x\n", dh.startmasseq);
    printf("StartName: \"");
    for (i=0; i<4; i++)
    {
      int c=dh.startname[i];
      printf("%c", ((c>=' ')&(c<='~'))?c:'.');
    }
    printf("\"\n");
  }

  if (dirtype==ADFS_OLDDIR)
    entries=ADFS_OLDDIR_ENTRIES;
  else
    entries=ADFS_NEWDIR_ENTRIES;

  for (entry=0; entry<entries; entry++)
  {
    unsigned int filetype;
    char filename[ADFS_MAXPATHLEN];
    struct timeval tv;
    uint32_t indirectaddr;

    diskstore_absoluteread((char *)&de, sizeof(de), dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);

    // Check for last entry, as per RiscOS PRM 2-211
    if (de.dirobname[0]==0) break;

    // Initialise this file entry
    filename[0]=0;
    filetype=0;
    hasfiletype=0;

    // Print spaces depending on directory depth
    for (i=0; i<level; i++)
       printf("  ");

    // Extract filename
    for (i=0; i<10; i++)
    {
      int c=(de.dirobname[i]&0x7f);
      if ((c==0) || (c==0x0d) || (c==0x0a)) break;

      filename[strlen(filename)+1]=0;
      filename[strlen(filename)]=c;
    }

    // Print filename padded to 10 characters
    printf("%s%*s", filename, 10-strlen(filename), "");

    // Extract object attributes
    if (dirtype==ADFS_NEWDIR)
    {
      attrib=de.newdiratts;
    }
    else
    {
      attrib=0;

      // Map old to new dir attributes
      if (de.dirobname[0]&0x80) attrib|=ADFS_OWNER_READ;
      if (de.dirobname[1]&0x80) attrib|=ADFS_OWNER_WRITE;
      if (de.dirobname[2]&0x80) attrib|=ADFS_LOCKED;
      if (de.dirobname[3]&0x80) attrib|=ADFS_DIRECTORY;
      if (de.dirobname[4]&0x80) attrib|=ADFS_EXECUTABLE;
      if (de.dirobname[5]&0x80) attrib|=ADFS_PUBLIC_READ;
      if (de.dirobname[6]&0x80) attrib|=ADFS_PUBLIC_WRITE;
    }

    // Attributes
    printf(" ");

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

    // Check for object having a filetype+timestamp, as per RiscOS PRM 2-16
    if ((de.dirload&0xfff00000) == 0xfff00000)
      hasfiletype=1;

    printf(" %10lu", (unsigned long)de.dirlen);

    indirectaddr=((de.dirinddiscadd[2]<<16) | (de.dirinddiscadd[1]<<8) | de.dirinddiscadd[0]);

    if (maptype==ADFS_OLDMAP)
    {
      printf(" %.6lx", (unsigned long)indirectaddr);
    }
    else
    {
      printf(" [Frag %x (Sec %lx) Offs %x]", (indirectaddr&0x7fff00)>>8, fragstart[(indirectaddr&0x7fff00)>>8], indirectaddr&0xff);
    }

    if (hasfiletype==0)
    {
      // Note : exec address should be >= load address and < (load address + file length), RiscOS PRM 2-16
      printf(" %.8lx", (unsigned long)de.dirload);
      printf(" %.8lx", (unsigned long)de.direxec);
    }
    else
    {
      // Get the epoch, as per RiscOS PRM 2-16
      if (dirtype==ADFS_NEWDIR)
      {
        unsigned int hightime=(de.dirload&0xff);
        unsigned int lowtime=de.direxec;
        unsigned long long csec=(((unsigned long long)hightime<<32) | lowtime);

        filetype=((0x000fff00 & de.dirload)>>8);

        if ((csec/100)>=ADFS_RISCUNIXTSDIFF)
        {
          struct tm tim;

          csec=(csec/100)-ADFS_RISCUNIXTSDIFF;

          tv.tv_sec=csec;
          tv.tv_usec=0;

          localtime_r(&tv.tv_sec, &tim);

          printf(" %.2d:%.2d:%.2d %.2d/%.2d/%d", tim.tm_hour, tim.tm_min, tim.tm_sec, tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900);
        }
        printf(" %.3x %s", filetype, adfs_filetype(filetype));
      }
    }

    printf("\n");

    // Recurse into directories
    if (0!=(attrib&ADFS_DIRECTORY))
    {
      unsigned long curdiskoffs;
      char newfolder[ADFS_MAXPATHLEN];

      sprintf(newfolder, "%s/%s", folder, filename);

      if (maptype==ADFS_OLDMAP)
      {
        curdiskoffs=diskstore_absoffset;

        adfs_readdir(level+1, newfolder, maptype, dirtype, indirectaddr*ADFS_8BITSECTORSIZE, adfs_sectorsize, sectorspertrack);

        diskstore_absoluteseek(curdiskoffs, dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);
      }
      else
      {
        curdiskoffs=diskstore_absoffset;

        adfs_readdir(level+1, newfolder, maptype, dirtype, fragstart[(indirectaddr&0x7fff00)>>8]*adfs_sectorsize, adfs_sectorsize, sectorspertrack);

        diskstore_absoluteseek(curdiskoffs, dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);
      }
    }

    if ((adfs_debug) && (dirtype==ADFS_OLDDIR))
      printf("OldDirObSeq: %.2x\n", de.olddirobseq);
  }

  // Skip over unused entries
  while ((entry+1)<entries)
  {
    diskstore_absoluteread((char *)&de, sizeof(de), dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);
    entry++;
  }

  // Process DirTail
  if (dirtype==ADFS_NEWDIR)
  {
    struct adfs_newdirtail ndt;

    diskstore_absoluteread((char *)&ndt, sizeof(ndt), dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);

    if (adfs_debug)
    {
      printf("NewDirLastMark: %.2x\n", ndt.lastmark);
      printf("NewDirParent: %.2x %.2x %.2x\n", ndt.parent[2], ndt.parent[1], ndt.parent[0]);
      printf("NewDirTitle: ");
      for (i=0; i<19; i++)
      {
        int c=(ndt.title[i]&0x7f);
        if ((c==0) || (c==0x0d) || (c==0x0a)) break;
        printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      }
      printf("\n");
      printf("NewDirName: ");
      for (i=0; i<10; i++)
      {
        int c=(ndt.name[i]&0x7f);
        if ((c==0) || (c==0x0d) || (c==0x0a)) break;
        printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      }
      printf("\n");
      printf("EndMasSeq: %.2x\n", ndt.endmasseq);
      printf("EndName: \"");
      for (i=0; i<4; i++)
      {
        int c=ndt.endname[i];
        printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      }
      printf("\"\n");
      printf("DirCheckByte: %.2x\n", ndt.checkbyte);
    }
  }
  else
  {
    struct adfs_olddirtail odt;

    diskstore_absoluteread((char *)&odt, sizeof(odt), dirtype==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, 80);

    if (adfs_debug)
    {
      printf("OldDirLastMark: %.2x\n", odt.lastmark);
      printf("OldDirName: ");
      for (i=0; i<10; i++)
      {
        int c=(odt.name[i]&0x7f);
        if ((c==0) || (c==0x0d) || (c==0x0a)) break;
        printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      }
      printf("\n");
      printf("OldDirParent: %.2x %.2x %.2x\n", odt.parent[2], odt.parent[1], odt.parent[0]);
      printf("OldDirTitle: ");
      for (i=0; i<19; i++)
      {
        int c=(odt.title[i]&0x7f);
        if ((c==0) || (c==0x0d) || (c==0x0a)) break;
        printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      }
      printf("\n");
      printf("EndMasSeq: %.2x\n", odt.endmasseq);
      printf("EndName: \"");
      for (i=0; i<4; i++)
      {
        int c=odt.endname[i];
        printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      }
      printf("\"\n");
      printf("DirCheckByte: %.2x\n", odt.checkbyte);
    }
  }
}

void adfs_dumpdiscrecord(struct adfs_discrecord *dr)
{
  int i;

  printf("\nADFS Disc Record\n");

  printf("Sector size in bytes : %lu\n", rev_log2(dr->log2secsize));
  printf("Sectors/track : %d\n", dr->secspertrack);
  printf("Heads: %d (%s)\n", dr->heads, dr->heads==1?"sequenced":dr->heads==2?"interleaved":"Unknown");

  printf("Density: %d ", dr->density);
  switch (dr->density)
  {
    case 0: printf("Hard disk\n"); break;
    case 1: printf("Single density (125Kbps FM)\n"); break;
    case 2: printf("Double density (250Kbps FM)\n"); break;
    case 3: printf("Double+ density (300Kbps FM)\n"); break;
    case 4: printf("Quad density (500Kbps FM)\n"); break;
    case 8: printf("Octal density (1000Kbps FM)\n"); break;
    default:
      printf("Unknown\n");
      break;
  }

  printf("ID field length of a map fragment in bits: %d\n", dr->idlen);
  if ((dr->idlen<(dr->log2secsize+3)) || (dr->idlen>19))
    printf("Invalid idlen size\n");

  printf("Bytes/map bit: 0x%.2x (%lu)\n", dr->log2bpmb, rev_log2(dr->log2bpmb));
  printf("Track to track skew: %d\n", dr->skew);
  printf("Boot option: %d ", dr->bootoption);
  switch (dr->bootoption)
  {
    case 0: printf("No action\n"); break;
    case 1: printf("*Load boot file\n"); break;
    case 2: printf("*Run boot file\n"); break;
    case 3: printf("*Exec boot file\n"); break;
    default: printf("Unknown\n"); break;
  }

  printf("Lowest sector: %d\n", dr->lowsector&0x3f);
  printf("Treat sides as %s\n", (dr->lowsector&0x40)==0?"interleaved":"sequence");
  printf("Disc is %d track\n", (dr->lowsector&0x80)==0?80:40);

  printf("Zones in map: %ld\n", (long)((dr->nzones_high<<8) | dr->nzones));
  printf("Non-allocation bits between zones: 0x%.4x\n", dr->zone_spare);
  printf("Root directory address: 0x%.8x\n", dr->root);
  printf("Disc size in bytes: %lu\n", (unsigned long)dr->disc_size);

  printf("Disc cycle ID: 0x%.4x\n", dr->disc_id);
  printf("Disc name: \"");
  for (i=0; i<10; i++)
  {
    int c=dr->disc_name[i];
    printf("%c", ((c>=' ')&(c<='~'))?c:'.');
  }
  printf("\"\n");

  printf("Disc filetype: 0x%.8x\n", dr->disc_type);
  printf("Share size: 0x%.2x\n", dr->log2sharesize);
  printf("Big flag: 0x%.2x\n", dr->big_flag);
  printf("Root size: 0x%.8x\n", dr->root_size);

  printf("\n");
}

void adfs_readnewmap(const unsigned char idlen, const unsigned int bytespermapbit, const unsigned char nzones, const unsigned long discsize, const unsigned long sectorsize, const unsigned long zonespare)
{
  unsigned int fragid;
  unsigned char fragbits;
  unsigned char mapdata;
  unsigned int zoneread;
  unsigned int zeroes;
  unsigned long pos;
  unsigned long start;
  unsigned char bit;
  unsigned int zone;
  unsigned int mapbytes;

  if (adfs_debug)
    printf("New Map @%lx (%d, %u, %d) %lu :\n", diskstore_absoffset, idlen, bytespermapbit, nzones, sectorsize);

  pos=0;

  fragid=0; fragbits=0;
  start=pos; zeroes=0; zone=0;
  zoneread=sectorsize-(sizeof(struct adfs_discrecord))-(zonespare/8);
  mapbytes=(discsize/bytespermapbit)/8;

  if (adfs_debug)
  {
    printf("Granularity: %d, map bytes %u\n", (sectorsize>bytespermapbit)?sectorsize:bytespermapbit, mapbytes);

    printf("Zone %u/%d @ %lx, %u allocation bytes\n", zone, nzones, diskstore_absoffset, zoneread);
  }

  fragstart=malloc(ADFS_MAXFRAG*sizeof(long));
  if (fragstart==NULL) return;

  do
  {
    diskstore_absoluteread((char *)&mapdata, 1, INTERLEAVED, 80);

    zoneread--;
    if (zoneread==0)
    {
      zone++;
      zoneread=sectorsize-(zonespare/8);

      // Skip bit between map data
      diskstore_absoluteseek(diskstore_absoffset+(zonespare/8), INTERLEAVED, 80);

      if (adfs_debug)
        printf("Zone %u @ %lx, %u allocation bytes\n", zone, diskstore_absoffset, zoneread);
    }

    for (bit=0; bit<8; bit++)
    {
      // Read fragment id
      if (fragbits<idlen)
      {
        fragid|=((mapdata&0x01)<<fragbits);
        fragbits++;
      }
      else
      {
        // Look for next "1" bit to indicate end of fragment
        if ((mapdata&0x01)==1)
        {
          // TODO fix this
          if (bytespermapbit==64) start/=2;

          fragstart[fragid]=start;

          if (adfs_debug)
          {
            printf("  (%.2x): ", fragid);
            printf("[%.2x = %d bytes] @ %lx\n", zeroes, (idlen+zeroes+1)*bytespermapbit, start);
          }

          fragid=0; fragbits=0;
          zeroes=0;

          start=pos+1;
        }
        else
          zeroes++;
      }
      mapdata=(mapdata>>1);
    }

    pos++;
  } while (pos<mapbytes);

  if (adfs_debug)
    printf("\n");
}

void adfs_showinfo(const int adfs_format, const unsigned int disktracks, const int debug)
{
  int map, dir;
  unsigned int adfs_sectorsize;
  unsigned char sectorspertrack;
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  adfs_debug=debug;

  fragstart=NULL;

  switch (adfs_format)
  {
    case ADFS_S:
    case ADFS_M:
    case ADFS_L:
      map=ADFS_OLDMAP;
      dir=ADFS_OLDDIR;
      adfs_sectorsize=ADFS_8BITSECTORSIZE;
      sectorspertrack=16;
      break;

    case ADFS_D:
      map=ADFS_OLDMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      sectorspertrack=5;
      break;

    case ADFS_E:
      map=ADFS_NEWMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      sectorspertrack=5;
      break;

    case ADFS_F:
      map=ADFS_NEWMAP;
      dir=ADFS_NEWDIR;
      adfs_sectorsize=ADFS_16BITSECTORSIZE;
      sectorspertrack=10;
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

    if (adfs_debug)
    {
      printf("FreeStart: ");
      for (i=0; i<ADFS_OLDMAPLEN; i++)
     {
        unsigned long c=adfs_readval(&oldmap->freestart[i*ADFS_OLDMAPENTRY], ADFS_OLDMAPENTRY);

        printf("%.3lx ", c);
      }
      printf("\n");
    }

    printf("Disc name: \"");
    for (i=0; i<5; i++)
    {
      int c=oldmap->oldname0[i];
      if (c==0x00) break;
      printf("%c", ((c>=' ')&(c<='~'))?c:'.');
      c=oldmap->oldname1[i];
      if (c==0x00) break;
      printf("%c", ((c>=' ')&(c<='~'))?c:'.');
    }
    printf("\"\n");

    printf("Disc size in (256 byte) sectors: %lu\n", adfs_readval((unsigned char *)&oldmap->oldsize, ADFS_OLDMAPENTRY));

    if (adfs_debug)
    {
      printf("Check0: %.2x\n", oldmap->check0);
      printf("FreeLen: ");
      for (i=0; i<ADFS_OLDMAPLEN; i++)
      {
        unsigned long c=adfs_readval(&oldmap->freelen[i*ADFS_OLDMAPENTRY], ADFS_OLDMAPENTRY);

        printf("%.3lx ", c);
      }
      printf("\n");
    }

    discid=oldmap->oldid;
    printf("Disc ID: %.4lx (%lu)\n", discid, discid);
    printf("Boot option: %.2x ", oldmap->oldboot);
    switch (oldmap->oldboot)
    {
      case 0: printf("No action\n"); break;
      case 1: printf("*Load boot file\n"); break;
      case 2: printf("*Run boot file\n"); break;
      case 3: printf("*Exec boot file\n"); break;
      default: printf("Unknown\n"); break;
    }

    if (adfs_debug)
    {
      printf("FreeEnd: %.2x\n", oldmap->freeend);
      printf("Check1: %.2x\n", oldmap->check1);
    }

    printf("\n");

    // Do a directory listing, using root at start of 1st sector after 512 bytes of old map
    //   as per RiscOS PRM 2-200
    if (dir==ADFS_NEWDIR)
      adfs_readdir(0, "", map, dir, ADFS_16BITSECTORSIZE, adfs_sectorsize, sectorspertrack);
    else
      adfs_readdir(0, "", map, dir, ADFS_8BITSECTORSIZE*2, adfs_sectorsize, sectorspertrack);
  }
  else
  {
    struct adfs_zoneheader zh;
    struct adfs_discrecord dr;
    long secoffset;

    // New MAP

    // If this is a format with a boot sector, look for that
    if (adfs_format==ADFS_F)
    {
      diskstore_absoluteseek(ADFS_BOOTBLOCKOFFSET+ADFS_BOOTDROFFSET, dir==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, disktracks);
      diskstore_absoluteread((char *)&dr, sizeof(dr), dir==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, disktracks);

      diskstore_absoluteseek((dr.disc_size/2)-(adfs_sectorsize*2), dir==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, disktracks);
    }
    else
      diskstore_absoluteseek(0, dir==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, disktracks);

    diskstore_absoluteread((char *)&zh, sizeof(zh), dir==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, disktracks);

    printf("ZoneCheck: %.2x\n", zh.zonecheck);
    printf("FreeLink: %.4x\n", zh.freelink);
    printf("CrossCheck: %.2x\n", zh.crosscheck);

    diskstore_absoluteread((char *)&dr, sizeof(dr), dir==ADFS_OLDDIR?SEQUENCED:INTERLEAVED, disktracks);

    adfs_dumpdiscrecord(&dr);

    adfs_readnewmap(dr.idlen, rev_log2(dr.log2bpmb), dr.nzones, dr.disc_size, rev_log2(dr.log2secsize), dr.zone_spare);

    if (fragstart==NULL) return;

    secoffset=dr.root&0xff;
    if (secoffset>1)
      secoffset-=1;
    else
      secoffset=0;

    adfs_readdir(0, "", map, dir, (fragstart[(dr.root&0x7fff00)>>8]+secoffset)*adfs_sectorsize, adfs_sectorsize, sectorspertrack);
  }

  if (fragstart!=NULL)
  {
    free(fragstart);
    fragstart=NULL;
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
      // Validate first checksum, RiscOS PRM 2-200/201
      if (adfs_checksum(&sniff[0], ADFS_8BITSECTORSIZE)==oldmap->check0)
      {
        // Validate second checksum, RiscOS PRM 2-200/201
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

      // Validate NewMap ZoneCheck for zone 0
      zonecheck=map_zone_valid_byte(&sniff, sniff[4], 0);

      if (zonecheck==sniff[0])
      {
        struct adfs_discrecord *dr;

        dr=(struct adfs_discrecord *)&sniff[4];
        adfs_dumpdiscrecord(dr);

        // TODO validate CrossCheck, as per RiscOS PRM 2-206

        if ((rev_log2(dr->log2secsize)==ADFS_16BITSECTORSIZE) && (dr->secspertrack==5))
        {
          format=ADFS_E;
          if (dr->root_size!=0) format++;
        }
      }

      // Check for ADFS with a boot block, and hence discrecord at position 0xc00 + 0x1c0
      //   as per RiscOS PRM 2-213
      if (format==ADFS_UNKNOWN)
      {
        // On a floppy disk with 1024 byte sectors (which all new map are), 0xc00 is at C0 H0 S3
        sector0=diskstore_findhybridsector(0, 0, 3);

        if ((sector0!=NULL) && (sector0->data!=NULL))
        {
          memcpy(sniff, sector0->data, sector0->datasize);

          // Validate boot block checksum, RiscOS PRM 2-215
          if (adfs_checksum(&sniff[0], (ADFS_8BITSECTORSIZE*2))==sniff[(ADFS_8BITSECTORSIZE*2)-1])
          {
            struct adfs_discrecord *dr;

            dr=(struct adfs_discrecord *)&sniff[ADFS_BOOTDROFFSET];
            adfs_dumpdiscrecord(dr);

            if ((rev_log2(dr->log2secsize)==ADFS_16BITSECTORSIZE) && (dr->secspertrack==10))
            {
              format=ADFS_F;
              if (dr->root_size!=0) format++;
            }

            if ((rev_log2(dr->log2secsize)==ADFS_16BITSECTORSIZE) && (dr->secspertrack==20))
              format=ADFS_G;
          }
        }
      }
    }
  }

  return format;
}
