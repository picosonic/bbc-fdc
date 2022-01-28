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
#include "atarist.h"

int atarist_debug=0;

// Absolute disk offset from cluster id
unsigned long atarist_clustertoabsolute(const unsigned long clusterid, const unsigned long sectorspercluster, const unsigned long bytespersector, const unsigned long dataregion)
{
  return (dataregion+(((clusterid-ATARIST_MINCLUSTER)*sectorspercluster)*bytespersector));
}

void atarist_readdir(const int level, const unsigned long offset, const unsigned int entries, const unsigned long sectorspercluster, const unsigned long bytespersector, const unsigned long dataregion, const unsigned long parent, unsigned int disktracks)
{
  struct atarist_direntry de;
  unsigned int i;
  unsigned int e;
  int j;

  diskstore_absoluteseek(offset, INTERLEAVED, 80);

  // Loop through entries - TODO add sanity checks / entry subdirectories
  for (e=0; e<entries; e++)
  {
    if (diskstore_absoluteread((char *)&de, sizeof(de), INTERLEAVED, 80)<sizeof(de))
      return;

    // Check for end of directory
    if (de.fname[0]==ATARIST_DIRENTRYEND)
      break;

    // Check if filesize exceeds disk size
    // TODO

    // Indent
    for (j=0; j<level; j++) printf("  ");

    // Extract name
    printf("'");
    for (i=0; i<8; i++)
    {
      if (de.fname[i]==ATARIST_DIRPADDING) break;

      if (i==0)
      {
        switch (de.fname[i])
        {
          case ATARIST_DIRENTRYE5: // Encoded 0xe5
            printf("%c", 0xe5);
            break;

          case ATARIST_DIRENTRYDEL: // Deleted file
            printf("?");
            break;

          case ATARIST_DIRENTRYALIAS: // . or ..
            printf("%c", de.fname[i]);
            break;

          default:
            printf("%c", de.fname[i]);
            break;
        }
      }
      else
        printf("%c", de.fname[i]);
    }
    if (de.fext[0]!=ATARIST_DIRPADDING)
      printf(".");

    for (i=0; i<3; i++)
    {
      if (de.fext[i]==ATARIST_DIRPADDING) break;
      printf("%c", de.fext[i]);
    }
    printf("'");

    // Extract date/time
    printf("  %.2d/%.2d/%d", de.fdate&0x1f, (de.fdate&0x1e0)>>5, ATARIST_EPOCHYEAR+((de.fdate&0xfe00)>>9));
    printf("  %.2d:%.2d:%.2d", (de.ftime&0xf800)>>11, (de.ftime&0x7e0)>>5, (de.ftime&0x1f)*2);

    // Extract attributes
    printf("  %c%c%c%c%c%c", de.attrib&ATARIST_ATTRIB_READONLY?'r':'w', de.attrib&ATARIST_ATTRIB_HIDDEN?'h':'-', de.attrib&ATARIST_ATTRIB_SYSTEM?'s':'-', de.attrib&ATARIST_ATTRIB_VOLUME?'v':'-', de.attrib&ATARIST_ATTRIB_DIR?'d':'f', de.attrib&ATARIST_ATTRIB_NEWMOD?'n':'-');

    printf("  (%d)", de.scluster);
    if ((de.attrib&ATARIST_ATTRIB_DIR)!=0)
      printf("  <dir>\n");
    else
      printf("  %d bytes\n", de.fsize);

    // Recurse into subdirectories
    if ((de.attrib&ATARIST_ATTRIB_DIR)!=0)
    {
      unsigned long subdir=atarist_clustertoabsolute(de.scluster, sectorspercluster, bytespersector, dataregion);

      // Don't recurse into "." and ".."
      if ((subdir!=parent) && (subdir!=offset))
      {
        unsigned long curdiskoffs=diskstore_absoffset;

        atarist_readdir(level+1, subdir, entries, sectorspercluster, bytespersector, dataregion, offset, disktracks);

        diskstore_absoluteseek(curdiskoffs, INTERLEAVED, disktracks);
      }
    }
  }
}

void atarist_showinfo(const int debug)
{
  Disk_Sector *sector1;

  atarist_debug=debug;

  // Search for boot sectors
  sector1=diskstore_findhybridsector(0, 0, 1);

  // Check we have the boot sector
  if (sector1==NULL)
    return;

  // Check we have data for the boot sector
  if (sector1->data==NULL)
    return;

  // Check sector is 512 bytes long
  if (sector1->datasize==ATARIST_SECTORSIZE)
  {
    struct atarist_bootsector *bootsector;
    int i;
    uint32_t serial;
    uint16_t cxsum;
    uint16_t rootsector;
    uint16_t dataregion;
    unsigned long offset;

    bootsector=(struct atarist_bootsector *)sector1->data;

    if (atarist_debug)
    {
      printf("BRA : %.2x %.2x\n", bootsector->bra&0xff, (bootsector->bra&0xff00) >> 8);

      printf("OEM : ");
      for (i=0; i<6; i++)
        printf("%c", bootsector->oem[i]);
      printf("\n");

      serial=bootsector->serial[2];
      serial=(serial<<8)|bootsector->serial[1];
      serial=(serial<<8)|bootsector->serial[0];
      printf("SERIAL : %d (%.3x)\n", serial, serial);

      // BPB
      printf("Bytes/sector : %d\n", bootsector->bpb.bps);
      printf("Sectors/cluster : %d\n", bootsector->bpb.spc);
      printf("Reserved sectors : %d\n", bootsector->bpb.ressec);
      printf("FATs : %d\n", bootsector->bpb.nfats);
      printf("Max root entries : %d\n", bootsector->bpb.ndirs);
      printf("Sectors : %d\n", bootsector->bpb.nsects);
      printf("Media : %.2x\n", bootsector->bpb.media);
      printf("Sectors/FAT : %d\n", bootsector->bpb.spf);
      printf("Sectors/track : %d\n", bootsector->bpb.spt);
      printf("Heads : %d\n", bootsector->bpb.nheads);
      printf("Hidden sectors: %d\n", bootsector->bpb.nhid);

      // Boot block
      printf("Exec flag : %.4x\n", bootsector->boot.execflag);
      printf("Load mode : %.4x\n", bootsector->boot.ldmode);

      if (bootsector->boot.ldmode!=0)
      {
        printf("Boot location start sector : %d\n", bootsector->boot.ssect);
        printf("Boot location sector count : %d\n", bootsector->boot.sectcnt);
      }

      printf("Load address : %.8x\n", bootsector->boot.ldaaddr);
      printf("FAT buffer address : %.8x\n", bootsector->boot.fatbuf);

      if (bootsector->boot.ldmode==0)
      {
        printf("BOOT file : ");
        for (i=0; i<11; i++)
          printf("%c", bootsector->boot.fname[i]);
        printf("\n");
      }

      printf("Reserved : %d\n", bootsector->boot.reserved);
    }

    cxsum=0;
    for (i=0; i<((ATARIST_SECTORSIZE-2)/2); i++)
    {
      cxsum+=((sector1->data[i*2])<<8);
      cxsum+=(sector1->data[(i*2)+1]);
    }

    cxsum=0x1234-cxsum;

    if (atarist_debug)
      printf("CHECKSUM : %.4x (%.4x)\n", be16toh(bootsector->checksum), cxsum);

    // Locate root directory
    rootsector=bootsector->bpb.ressec+(bootsector->bpb.spf*bootsector->bpb.nfats);
    printf("Root sector : %d\n", rootsector);

    // Calculate dataregion absolute offset
    dataregion=(bootsector->bpb.ressec+(bootsector->bpb.spf*bootsector->bpb.nfats)+((bootsector->bpb.ndirs*ATARIST_DIRENTRYLEN)/bootsector->bpb.bps))*bootsector->bpb.bps;

    // Catalogue disk from root directory
    offset=bootsector->bpb.bps*rootsector;
    atarist_readdir(0, offset, bootsector->bpb.ndirs, bootsector->bpb.spc, bootsector->bpb.bps, dataregion, 0, 80);
  }
}

int atarist_validate()
{
  int format;

  Disk_Sector *sector1;

  // Search for boot sectors
  sector1=diskstore_findhybridsector(0, 0, 1);

  format=ATARIST_UNKNOWN;

  // Check we have the boot sector
  if (sector1==NULL)
    return format;

  // Check we have data for the boot sector
  if (sector1->data==NULL)
    return format;

  // Check sector is 512 bytes long
  if (sector1->datasize==ATARIST_SECTORSIZE)
  {
    struct atarist_bootsector *bootsector;
    int i;
    uint16_t cxsum;

    bootsector=(struct atarist_bootsector *)sector1->data;

    // Validate bytes/sector
    if (bootsector->bpb.bps!=ATARIST_SECTORSIZE) return format;

    // Validate sectors/cluster is a power of 2
    if ((bootsector->bpb.spc!=2) && (bootsector->bpb.spc!=4) && (bootsector->bpb.spc!=8)) return format;

    // Validate reserved sectors
    if (bootsector->bpb.ressec!=1) return format;

    // Validate num FATs
    if (bootsector->bpb.nfats!=2) return format;

    // Validate heads value
    if ((bootsector->bpb.nheads!=1) && (bootsector->bpb.nheads!=2)) return format;

    // Validate checksum (may only be correct if disk is bootable)
    cxsum=0;
    for (i=0; i<((ATARIST_SECTORSIZE-2)/2); i++)
    {
      cxsum+=((sector1->data[i*2])<<8);
      cxsum+=(sector1->data[(i*2)+1]);
    }

    cxsum=0x1234-cxsum;

    if (be16toh(bootsector->checksum)!=cxsum) return format;

    return 1;
  }

  return format;
}
