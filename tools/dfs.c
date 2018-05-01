#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dfs.h"
#include "diskstore.h"

// Read nth DFS filename from catalogue
//   but don't add "$."
//   return the "Locked" state of the file
int dfs_getfilename(Disk_Sector *sector0, const int entry, char *filename)
{
  int i;
  int len;
  unsigned char fchar;
  int locked;

  len=0;

  locked=(sector0->data[(entry*8)+7] & 0x80)?1:0;

  fchar=sector0->data[(entry*8)+7] & 0x7f;

  if (fchar!='$')
  {
    filename[len++]=fchar;
    filename[len++]='.';
  }

  for (i=0; i<7; i++)
  {
    fchar=sector0->data[(entry*8)+i] & 0x7f;

    if (fchar==' ') break;
    filename[len++]=fchar;
  }

  filename[len++]=0;

  return locked;
}

// Return load address for nth entry in DFS catalogue
unsigned long dfs_getloadaddress(Disk_Sector *sector1, const int entry)
{
  unsigned long loadaddress;

  loadaddress=((((sector1->data[8+((entry-1)*8)+6]&0x0c)>>2)<<16) |
               ((sector1->data[8+((entry-1)*8)+1])<<8) |
               ((sector1->data[8+((entry-1)*8)])));

  if (loadaddress & 0x30000) loadaddress |= 0xFF0000;

  return loadaddress;
}

// Return execute address for nth entry in DFS catalogue
unsigned long dfs_getexecaddress(Disk_Sector *sector1, const int entry)
{
  unsigned long execaddress;

  execaddress=((((sector1->data[8+((entry-1)*8)+6]&0xc0)>>6)<<16) |
               ((sector1->data[8+((entry-1)*8)+3])<<8) |
               ((sector1->data[8+((entry-1)*8)+2])));

  if (execaddress & 0x30000) execaddress |= 0xFF0000;

  return execaddress;
}

// Return file length for nth entry in DFS catalogue
unsigned long dfs_getfilelength(Disk_Sector *sector1, const int entry)
{
  return ((((sector1->data[8+((entry-1)*8)+6]&0x30)>>4)<<16) |
          ((sector1->data[8+((entry-1)*8)+5])<<8) |
          ((sector1->data[8+((entry-1)*8)+4])));
}

// Return file starting sector for nth entry in DFS catalogue
unsigned long dfs_getstartsector(Disk_Sector *sector1, const int entry)
{
  return (((sector1->data[8+((entry-1)*8)+6]&0x03)<<8) |
          ((sector1->data[8+((entry-1)*8)+7])));
}

// Return the disk title
void dfs_gettitle(const int head, char *title, const int titlelen)
{
  int i, j;
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  // Search for sectors
  sector0=diskstore_findhybridsector(0, head, 0);
  sector1=diskstore_findhybridsector(0, head, 1);

  // Blank out title
  title[0]=0;

  // Check we have both DFS catalogue sectors
  if ((sector0==NULL) || (sector1==NULL))
    return;

  // Check we have both DFS catalogue sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return;

  // Check there is enough space in return string
  if (titlelen<13) return;

  j=0;

  for (i=0; i<8; i++)
  {
    if ((sector0->data[i]==0) || (sector0->data[i]==13)) return;
    title[j++]=sector0->data[i] & 0x7f;
    title[j]=0;
  }
  for (i=0; i<4; i++)
  {
    if ((sector1->data[i]==0) || (sector1->data[i]==13)) return;
    title[j++]=sector1->data[i] & 0x7f;
    title[j]=0;
  }
}

void dfs_showinfo(const int head)
{
  int i;
  int numfiles;
  int locked;
  unsigned char bootoption;
  size_t tracks, totalusage, totalsectors, totalsize, sectorusage;
  char filename[10];
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  // Search for sectors
  sector0=diskstore_findhybridsector(0, head, 0);
  sector1=diskstore_findhybridsector(0, head, 1);

  // Check we have both DFS catalogue sectors
  if ((sector0==NULL) || (sector1==NULL))
    return;

  // Check we have both DFS catalogue sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return;

  printf("Disk title : \"");
  for (i=0; i<8; i++)
  {
    if (sector0->data[i]==0) break;
    printf("%c", sector0->data[i]);
  }
  for (i=0; i<4; i++)
  {
    if (sector1->data[i]==0) break;
    printf("%c", sector1->data[i]);
  }
  printf("\"\n");

  totalsectors=(((sector1->data[6]&0x03)<<8) | (sector1->data[7]));
  tracks=totalsectors/DFS_SECTORSPERTRACK;
  totalsize=totalsectors*DFS_SECTORSIZE;
  printf("Disk size : %lu tracks (%u sectors, %lu bytes)\n", (unsigned long)tracks, (unsigned int)totalsectors, (unsigned long)totalsize);

  bootoption=(sector1->data[6]&0x30)>>4;
  printf("Boot option: %d ", bootoption);
  switch (bootoption)
  {
    case 0:
      printf("Nothing");
      break;

    case 1:
      printf("*LOAD !BOOT");
      break;

    case 2:
      printf("*RUN !BOOT");
      break;

    case 3:
      printf("*EXEC !BOOT");
      break;

    default:
      printf("Unknown");
      break;
  }
  printf("\n");

  totalusage=0; sectorusage=2;
  printf("Write operations made to disk : %.2x\n", sector1->data[4]); // Stored in BCD

  numfiles=sector1->data[5]/8;
  printf("Catalogue entries : %d\n", numfiles);

  for (i=1; ((i<=numfiles) && (i<DFS_MAXFILES)); i++)
  {
    locked=dfs_getfilename(sector0, i, filename);

    printf("%-9s", filename);

    printf(" %.6lx %.6lx %.6lx %.3lx", dfs_getloadaddress(sector1, i), dfs_getexecaddress(sector1, i), dfs_getfilelength(sector1, i), dfs_getstartsector(sector1, i));
    totalusage+=dfs_getfilelength(sector1, i);
    sectorusage+=(dfs_getfilelength(sector1, i)/DFS_SECTORSIZE);
    if (((dfs_getfilelength(sector1, i)/DFS_SECTORSIZE)*DFS_SECTORSIZE)!=dfs_getfilelength(sector1, i))
      sectorusage++;

    if (locked) printf(" L");
    printf("\n");
  }

  printf("Total disk usage : %lu bytes (%d%% of disk)\n", (unsigned long)totalusage, (totalusage*100)/(totalsize-(2*DFS_SECTORSIZE)));
  printf("Remaining catalogue space : %d files, %d unused disk sectors\n", DFS_MAXFILES-numfiles, (((sector1->data[6]&0x03)<<8) | (sector1->data[7])) - sectorusage);
}

// Test for valid DFS catalogue, checks from http://beebwiki.mdfs.net/Acorn_DFS_disc_format
int dfs_validcatalogue(const int head)
{
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  // Search for sectors
  sector0=diskstore_findhybridsector(0, head, 0);
  sector1=diskstore_findhybridsector(0, head, 1);

  // Check we have both DFS catalogue sectors
  if ((sector0==NULL) || (sector1==NULL))
    return 0;

  // Check we have both DFS catalogue sectors
  if ((sector0->data==NULL) || (sector1->data==NULL))
    return 0;

  // Check both sectors are 256 bytes in length
  if ((sector0->datasize!=DFS_SECTORSIZE) || (sector1->datasize!=DFS_SECTORSIZE))
    return 0;

  // Check cycle number is BCD
  if ((sector1->data[4]&0xf0)>0x90) return 0;
  if ((sector1->data[4]&0x0f)>0x09) return 0;

  // Check reserved bits
  if ((sector1->data[5]&0x07)!=0) return 0;
  if ((sector1->data[6]&0xc0)!=0) return 0;
  if ((sector1->data[6]&0x0c)!=0) return 0;

  // TODO check that the disk size is between 2 and 800 sectors (maybe also up to 1023)
  if ((((sector1->data[6]&0x03)<<8) | (sector1->data[7]))<1)
    return 0;

  // TODO check the title contains printable ASCII padded with NULs or spaces

  // TODO for each file

    // TODO filename contains one to seven valid characters (0x20..0x7e, except '.',':','"','#','*',' ') and be padded with spaces

    // TODO filename directory character (without attribute bit) must be a valid name character

    // TODO filename (including directory) must be unique

    // TODO file start sector should be more than 1 and less than disc size

    // TODO files may not overlap

    // TODO files may not overshoot end of the disk

  // ************ FURTHER

  // TODO disc size should be 400 or 800 sectors (for 40 or 80 tracks)

  return 1;
}
