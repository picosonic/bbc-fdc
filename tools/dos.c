#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

#include "diskstore.h"
#include "dos.h"

// Determine FAT type, all DOS floppies should be FAT12 (since they are less than 16Mb capacity)
int dos_fatformat(Disk_Sector *sector1)
{
  struct dos_biosparams *biosparams;
  unsigned long rootdirsectors, datasectors, fatsectors, clusters;

  if (sector1==NULL)
    return DOS_UNKNOWN;

  if (sector1->data==NULL)
    return DOS_UNKNOWN;

  // Check sector is 512 bytes in length
  if (sector1->datasize!=DOS_SECTORSIZE)
    return DOS_UNKNOWN;

  biosparams=(struct dos_biosparams *)&sector1->data[DOS_OFFSETBPB];

  rootdirsectors=((biosparams->rootentries*DOS_DIRENTRYLEN)+(biosparams->bytespersector-1))/biosparams->bytespersector;

  if (biosparams->smallsectors!=0)
    datasectors=biosparams->smallsectors;
  else
    datasectors=biosparams->largesectors;

  // Check for FAT32
  if (biosparams->sectorsperfat!=0)
  {
    // Either FAT12 or FAT16
    fatsectors=biosparams->sectorsperfat;
  }
  else
  {
    struct dos_fat32extendedbiosparams *fat32ebpb;

    // FAT32
    fat32ebpb=(struct dos_fat32extendedbiosparams *)&sector1->data[DOS_OFFSETFAT32EBPB];
    fatsectors=fat32ebpb->sectorsperfat;
  }

  datasectors-=(biosparams->reservedsectors+(biosparams->fatcopies*fatsectors)+rootdirsectors);

  clusters=datasectors/biosparams->sectorspercluster;

  if (biosparams->sectorsperfat!=0)
  {
    if (clusters<4085)
      return DOS_FAT12;
    else
      return DOS_FAT16;
  }
  else
    return DOS_FAT32; // clusters >=65525
}

void dos_showinfo()
{
  Disk_Sector *sector1;
  struct dos_biosparams *biosparams;
  struct dos_extendedbiosparams *exbiosparams;
  unsigned long rootdir;
  int i;

  // Search for sector
  sector1=diskstore_findhybridsector(0, 0, 1);

  if (sector1==NULL)
    return;

  if (sector1->data==NULL)
    return;

  // Check sector is 512 bytes in length
  if (sector1->datasize!=DOS_SECTORSIZE)
    return;

  // OEM name
  printf("OEM Name: '");
  for (i=0; i<8; i++)
    printf("%c", sector1->data[0x03+i]);
  printf("'\n");

  // BIOS parameter block
  biosparams=(struct dos_biosparams *)&sector1->data[DOS_OFFSETBPB];
  exbiosparams=(struct dos_extendedbiosparams *)&sector1->data[DOS_OFFSETEBPB];

  printf("Bytes/Sector: %d\n", biosparams->bytespersector);
  printf("Sectors/Cluster: %d\n", biosparams->sectorspercluster);
  printf("Reserved sectors: %d\n", biosparams->reservedsectors);
  printf("FAT copies: %d\n", biosparams->fatcopies);
  printf("Root entries: %d\n", biosparams->rootentries);
  printf("Sectors on volume (small): %d\n", biosparams->smallsectors);
  printf("Media type: %.2x", biosparams->mediatype);
  switch (biosparams->mediatype)
  {
    case 0xf0:
      printf(" [2.88MB 3.5-inch, 2-sided, 36-sector or 1.44MB 3.5-inch, 2-sided, 18-sector]");
      break;

    case 0xf8:
      printf(" [Hard disk]");
      break;

    case 0xf9:
      printf(" [720K 3.5-inch, 2-sided, 9-sector or 1.2 MB 5.25-inch, 2-sided, 15-sector]");
      break;

    case 0xfa:
      printf(" [320K 5.25-inch or 3.5-inch, 1-sided, 8-sector]");
      break;

    case 0xfb:
      printf(" [640K 5.25-inch or 3.5-inch, 2-sided, 8-sector]");
      break;

    case 0xfc:
      printf(" [180K 5.25-inch, 1-sided, 9-sector]");
      break;

    case 0xfd:
      printf(" [360K 5.25-inch, 2-sided, 9-sector or 500K 8-inch, 2-sided, single-density]");
      break;

    case 0xfe:
      printf(" [160K 5.25-inch, 1-sided, 8-sector or 250K 8-inch, 1-sided, single-density or 1.2 MB 8-inch, 2-sided, double-density]");
      break;

    case 0xff:
      printf(" [320K 5.25-inch, 2-sided, 8-sector]");
      break;

    default:
      break;
  }
  printf("\n");
  printf("Sectors/FAT: %d\n", biosparams->sectorsperfat);
  printf("Sectors/Track: %d\n", biosparams->sectorspertrack);
  printf("Heads: %d\n", biosparams->heads);
  printf("Hidden sectors: %d\n", (biosparams->hiddensectors_hi<<16)|biosparams->hiddensectors_lo);
  printf("Sectors on volume (large): %d\n", biosparams->largesectors);

  // BIOS extended parameter block
  printf("Physical drive number: %d\n", exbiosparams->physicaldiskid);
  printf("Current head: %d\n", exbiosparams->currenthead);
  printf("Signature: %.2x\n", exbiosparams->signature);
  printf("Volume ID serial: %.4x-%.4x\n", exbiosparams->volumeserial>>16, exbiosparams->volumeserial&0xffff);

  printf("Volume label: '");
  for (i=0; i<11; i++)
  {
    int c=exbiosparams->volumelabel[i];
    printf("%c", ((c>=' ')&&(c<='~'))?c:'.');
  }
  printf("'\n");

  printf("System ID: '");
  for (i=0; i<8; i++)
  {
    int c=exbiosparams->systemid[i];
    printf("%c", ((c>=' ')&&(c<='~'))?c:'.');
  }
  printf("'\n");

  switch (dos_fatformat(sector1))
  {
    case DOS_FAT12:
      printf("FAT12\n");
      break;

    case DOS_FAT16:
      printf("FAT16\n");
      break;

    case DOS_FAT32:
      printf("FAT32\n");
      break;

    default:
      printf("Unknown FAT type\n\n");
      return;
  }

  // Show disk offsets
  for (i=0; i<biosparams->fatcopies; i++)
    printf("FAT%d @ 0x%x\n", i+1, (biosparams->reservedsectors+(biosparams->sectorsperfat*i))*biosparams->bytespersector);

  rootdir=(biosparams->reservedsectors+(biosparams->sectorsperfat*biosparams->fatcopies))*biosparams->bytespersector;
  printf("Root directory @ 0x%lx\n", rootdir);

  printf("Data region @ 0x%x .. 0x%x\n", (biosparams->reservedsectors+(biosparams->sectorsperfat*biosparams->fatcopies)+((biosparams->rootentries*DOS_DIRENTRYLEN)/biosparams->bytespersector))*biosparams->bytespersector, biosparams->smallsectors*biosparams->bytespersector);

  printf("\n");
}

int dos_validate()
{
  Disk_Sector *sector1;
  struct dos_biosparams *biosparams;
  unsigned long tmpval;

  // Search for sector
  sector1=diskstore_findhybridsector(0, 0, 1);

  if (sector1==NULL)
    return DOS_UNKNOWN;

  if (sector1->data==NULL)
    return DOS_UNKNOWN;

  // Check sector is 512 bytes in length
  if (sector1->datasize!=DOS_SECTORSIZE)
    return DOS_UNKNOWN;

  // Check for jump instruction
  if (!(((sector1->data[0]==DOS_SHORTJMP) && (sector1->data[2]==DOS_NOP)) ||
      (sector1->data[0]==DOS_NEARJMP) ||
      (sector1->data[0]==DOS_UNDOCDIRECTJMP)))
    return DOS_UNKNOWN;

  // Check for end of sector marker
  tmpval=sector1->data[DOS_SECTORSIZE-2];
  tmpval=(tmpval<<8)|sector1->data[DOS_SECTORSIZE-1];
  if (tmpval!=DOS_EOSM)
    return DOS_UNKNOWN;

  // Check some values in BPB
  biosparams=(struct dos_biosparams *)&sector1->data[DOS_OFFSETBPB];

  // Sector size should be right
  if (biosparams->bytespersector!=DOS_SECTORSIZE)
    return DOS_UNKNOWN;

  // Should be either 1 or 2 FAT copies
  if ((biosparams->fatcopies<1) || (biosparams->fatcopies>2))
    return DOS_UNKNOWN;

  // Sectors per cluster should be within valid range
  switch (biosparams->sectorspercluster)
  {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
      break;

    default:
      return DOS_UNKNOWN;
  }

  // Should be at least 1 reserved sector
  if (biosparams->reservedsectors==0)
    return DOS_UNKNOWN;

  // Should be multiple of 16 entries in root folder
  if (((biosparams->rootentries/16)*16)!=biosparams->rootentries)
    return DOS_UNKNOWN;

  return dos_fatformat(sector1);
}
