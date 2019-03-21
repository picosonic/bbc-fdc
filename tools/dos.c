#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

#include "diskstore.h"
#include "dos.h"

int dos_debug=0;

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
    if (clusters<DOS_FAT12MAXCLUSTER)
      return DOS_FAT12;
    else
      return DOS_FAT16;
  }
  else
    return DOS_FAT32; // clusters >=65525
}

// Absolute disk offset from cluster id
unsigned long dos_clustertoabsolute(const unsigned long clusterid, const unsigned long sectorspercluster, const unsigned long bytespersector, const unsigned long dataregion)
{
  return (dataregion+(((clusterid-DOS_MINCLUSTER)*sectorspercluster)*bytespersector));
}

void dos_readdir(const int level, const unsigned long offset, const unsigned int entries, const unsigned long sectorspercluster, const unsigned long bytespersector, const unsigned long dataregion, const unsigned long parent, unsigned int disktracks)
{
  struct dos_direntry de;
  unsigned int i;
  int j;
  char shortname[8+1+3+1];
  unsigned char shortlen;

  diskstore_absoluteseek(offset, INTERLEAVED, disktracks);

  for (i=0; i<entries; i++)
  {
    diskstore_absoluteread((char *)&de, sizeof(de), INTERLEAVED, disktracks);

    // Check for end of directory
    if (de.shortname[0]==DOS_DIRENTRYEND)
      break;

    for (j=0; j<level; j++)
      printf("  ");

    shortlen=0;
    for (j=0; j<8; j++)
      shortname[shortlen++]=de.shortname[j];

    // Check for deleted files, replace first character with '?'
    if (((unsigned char)shortname[0]==DOS_DIRENTRYDEL) || ((unsigned char)shortname[0]==DOS_DIRENTRYPREDEL))
      shortname[0]='?';

    while ((shortlen>0) && (shortname[shortlen-1]==' '))
      shortlen--;

    if (de.shortextension[0]!=' ')
    {
      shortname[shortlen++]='.';
      for (j=0; j<3; j++)
        shortname[shortlen++]=de.shortextension[j];
    }

    while ((shortlen>0) && (shortname[shortlen-1]==' '))
      shortlen--;

    shortname[shortlen]=0;

    printf("%s%*s", shortname, 12-strlen(shortname), "");

    printf(" %.2x ", de.fileattribs);
    if (0!=(de.fileattribs&DOS_ATTRIB_READONLY))
      printf("R");
    else
      printf("W");

    if (0!=(de.fileattribs&DOS_ATTRIB_HIDDEN))
      printf("H");
    else
      printf("-");

    if (0!=(de.fileattribs&DOS_ATTRIB_SYSTEM))
      printf("S");
    else
      printf("-");

    if (0!=(de.fileattribs&DOS_ATTRIB_VOLUMELABEL))
      printf("V");
    else
      printf("-");

    if (0!=(de.fileattribs&DOS_ATTRIB_DIRECTORY))
      printf("D");
    else
      printf("F");

    if (0!=(de.fileattribs&DOS_ATTRIB_ARCHIVE))
      printf("A");
    else
      printf("-");

    printf(" %.2x", de.userattribs);

    printf(" %.2d:%.2d:%.2d %.2d/%.2d/%d", (de.modifytime&0xf800)>>11, (de.modifytime&0x7e0)>>5, (de.modifytime&0x1f)*2, de.modifydate&0x1f, (de.modifydate&0x1e0)>>5, ((de.modifydate&0xfe00)>>9)+1980);
    printf(" @ 0x%.4x -> %lx", de.startcluster, dos_clustertoabsolute(de.startcluster, sectorspercluster, bytespersector, dataregion));
    printf(" %u bytes\n", de.filesize);

    if (0!=(de.fileattribs&DOS_ATTRIB_DIRECTORY))
    {
      unsigned long subdir=dos_clustertoabsolute(de.startcluster, sectorspercluster, bytespersector, dataregion);

      // Don't recurse into "." and ".."
      if ((subdir!=parent) && (subdir!=offset))
      {
        unsigned long curdiskoffs=diskstore_absoffset;
        dos_readdir(level+1, subdir, entries, sectorspercluster, bytespersector, dataregion, offset, disktracks);

        diskstore_absoluteseek(curdiskoffs, INTERLEAVED, disktracks);
      }
    }
  }
}

void dos_readfat(const unsigned long offset, const unsigned long length, const unsigned char fatformat, const unsigned int disktracks)
{
  char *wholefat;
  unsigned long i;
  unsigned long cluster;
  unsigned long clusterid;

  wholefat=malloc(length);
  if (wholefat==NULL) return;

  diskstore_absoluteseek(offset, INTERLEAVED, disktracks);
  diskstore_absoluteread(wholefat, length, INTERLEAVED, disktracks);

  clusterid=0;

  if (fatformat==DOS_FAT16)
  {
    for (i=0; i<length; i+=2)
    {
      cluster=(wholefat[i]<<8)|wholefat[i+1];

      if (dos_debug)
        printf("[%lx]=%.4lx ", clusterid++, cluster);
    }
  }
  else
  if (fatformat==DOS_FAT12)
  {
    for (i=0; (i+3)<length; i+=3)
    {
      cluster=((unsigned char)wholefat[i]|(((unsigned char)wholefat[i+1]&0x0f)<<8))&0xfff;
      if (dos_debug)
        printf("[%lx]=%.3lx ", clusterid, cluster);

      clusterid++;

      cluster=(((((unsigned char)wholefat[i+1]&0xf0)>>4)|((unsigned char)wholefat[i+2]<<4)))&0xfff;
      if (dos_debug)
        printf("[%lx]=%.3lx ", clusterid, cluster);

      clusterid++;
    }
  }

  if (dos_debug)
    printf("\n");

  free(wholefat);
}

void dos_showinfo(const unsigned int disktracks, const unsigned int debug)
{
  Disk_Sector *sector1;
  struct dos_biosparams *biosparams;
  struct dos_extendedbiosparams *exbiosparams;
  unsigned long rootdir;
  unsigned long dataregion;
  unsigned char fatformat;
  int i;

  dos_debug=debug;

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
  printf("Sectors on volume (large): %u\n", biosparams->largesectors);

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

  fatformat=dos_fatformat(sector1);
  switch (fatformat)
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
  if (dos_debug)
  {
    for (i=0; i<biosparams->fatcopies; i++)
      printf("FAT%d @ 0x%x\n", i+1, (biosparams->reservedsectors+(biosparams->sectorsperfat*i))*biosparams->bytespersector);
  }

  // Read first FAT
  dos_readfat(biosparams->reservedsectors*biosparams->bytespersector, biosparams->sectorsperfat*biosparams->bytespersector, fatformat, disktracks);

  rootdir=(biosparams->reservedsectors+(biosparams->sectorsperfat*biosparams->fatcopies))*biosparams->bytespersector;

  if (dos_debug)
    printf("Root directory @ 0x%lx\n", rootdir);

  dataregion=(biosparams->reservedsectors+(biosparams->sectorsperfat*biosparams->fatcopies)+((biosparams->rootentries*DOS_DIRENTRYLEN)/biosparams->bytespersector))*biosparams->bytespersector, biosparams->smallsectors*biosparams->bytespersector;

  if (dos_debug)
    printf("Data region @ 0x%lx .. 0x%lx\n", dataregion, (biosparams->smallsectors*biosparams->bytespersector)-dataregion);

  printf("\n");

  // Do recursive directory listing
  dos_readdir(0, rootdir, biosparams->rootentries, biosparams->sectorspercluster, biosparams->bytespersector, dataregion, 0, disktracks);

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

void dos_gettitle(char *title, const int titlelen)
{
  Disk_Sector *sector1;
  struct dos_extendedbiosparams *exbiosparams;

  // Search for sector
  sector1=diskstore_findhybridsector(0, 0, 1);

  if (sector1==NULL)
    return;

  if (sector1->data==NULL)
    return;

  // Check sector is 512 bytes in length
  if (sector1->datasize!=DOS_SECTORSIZE)
    return;

  // BIOS parameter block
  exbiosparams=(struct dos_extendedbiosparams *)&sector1->data[DOS_OFFSETEBPB];

  // Use the volume serial as the title
  if (titlelen>=10)
    sprintf(title, "%.4x-%.4x", exbiosparams->volumeserial>>16, exbiosparams->volumeserial&0xffff);
}
