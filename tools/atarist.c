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
    struct atarist_direntry direntry;
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

    // Load root directory
    offset=bootsector->bpb.bps*rootsector;

    diskstore_absoluteseek(offset, INTERLEAVED, 80);

    // Loop through entries - TODO add sanity checks / entry subdirectories
    while (1)
    {
      if (diskstore_absoluteread((char *)&direntry, sizeof(direntry), INTERLEAVED, 80)<sizeof(direntry)) return;
      if (direntry.fname[0]==0x00) break;

      printf("  '");
      for (i=0; i<8; i++)
      {
        if (direntry.fname[i]==0x20) break;
        printf("%c", direntry.fname[i]);
      }
      if (direntry.fext[0]!=0x20)
        printf(".");

      for (i=0; i<3; i++)
      {
        if (direntry.fext[i]==0x20) break;
        printf("%c", direntry.fext[i]);
      }
      printf("'  ");

      printf("%0.2d/%0.2d/%d  ", direntry.fdate&0x1f, (direntry.fdate&0x1e0)>>5, 1980+((direntry.fdate&0xfe00)>>9));
      printf("%.02d:%.02d:%0.2d  ", (direntry.ftime&0xf800)>>11, (direntry.ftime&0x7e0)>>5, (direntry.ftime&0x1f)*2);

      printf("%c%c%c%c%c%c  ", direntry.attrib&0x01?'r':'w', direntry.attrib&0x02?'h':'-', direntry.attrib&0x04?'s':'-', direntry.attrib&0x08?'v':'-', direntry.attrib&0x10?'d':'f', direntry.attrib&0x20?'n':'-');

      printf("(%d)  ", direntry.scluster);
      printf("%d bytes\n", direntry.fsize);
    }
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
