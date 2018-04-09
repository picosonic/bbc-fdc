#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

#include "diskstore.h"
#include "dos.h"

void dos_showinfo()
{
  Disk_Sector *sector1;
  struct dos_biosparams *biosparams;
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
  biosparams=(struct dos_biosparams *)&sector1->data[0x0b];
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
  printf("Hidden sectors: %d\n", biosparams->hiddensectors);
  printf("Sectors on volume (large): %d\n", biosparams->largesectors);

  // BIOS extended parameter block
  printf("Physical disk: %d\n", biosparams->physicaldiskid);
  printf("Current head: %d\n", biosparams->currenthead);
  printf("Signature: %.2x\n", biosparams->signature);
  printf("Volume serial: %.8x\n", biosparams->volumeserial);

  printf("Volume label: '");
  for (i=0; i<11; i++)
  {
    int c=biosparams->volumelabel[i];
    printf("%c", ((c>=' ')&&(c<='~'))?c:'.');
  }
  printf("'\n");

  printf("System ID: '");
  for (i=0; i<8; i++)
  {
    int c=biosparams->systemid[i];
    printf("%c", ((c>=' ')&&(c<='~'))?c:'.');
  }
  printf("'\n");

  printf("\n");
}

int dos_validate()
{
  Disk_Sector *sector1;
  unsigned long tmpval;

  // Search for sector
  sector1=diskstore_findhybridsector(0, 0, 1);

  if (sector1==NULL)
    return 0;

  if (sector1->data==NULL)
    return 0;

  // Check sector is 512 bytes in length
  if (sector1->datasize!=DOS_SECTORSIZE)
    return 0;

  // Check for jump instruction
  tmpval=sector1->data[0];
  tmpval=(tmpval<<8)+sector1->data[1];
  tmpval=(tmpval<<8)+sector1->data[2];
  if ((tmpval!=DOS_JUMP) && (tmpval!=DOS_JUMP2))
    return 0;

  // Check for end of sector marker
  tmpval=sector1->data[0x1fe];
  tmpval=(tmpval<<8)+sector1->data[0x1ff];
  if (tmpval!=DOS_EOSM)
    return 0;

  return 1;
}
