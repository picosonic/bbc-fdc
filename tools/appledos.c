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
#include "applegcr.h"
#include "appledos.h"

void appledos_showinfo(const int debug)
{
  Disk_Sector *sector0;
  Disk_Sector *sector1;

  // First check sectors are in Apple format
  if (diskstore_countsectormod(MODAPPLEGCR)==0)
    return;

  // Search for VTOC sector
  sector0=diskstore_findhybridsector(17, 0, 0);

  // Check we have VTOC sector
  if (sector0==NULL)
    return;

  // Check we have data for VTOC sector
  if (sector0->data==NULL)
    return;

  // Check sector is the right length
  if (sector0->datasize==APPLEGCR_SECTORLEN)
  {
    struct appledos_vtoc *vtoc;
    int track;

    // Map VTOC to sector buffer
    vtoc=(struct appledos_vtoc *)&sector0->data[0];

    printf("First catalogue: T%d S%d\n", vtoc->firstcattrack, vtoc->firstcatsector);
    printf("Apple DOS release used: %d%s\n", vtoc->dosrelease, (vtoc->dosrelease==3)?" (3.3)":"");
    printf("Disk volume: %d\n", vtoc->diskvol); // 1 to 254
    printf("Track/Sector pairs: %d\n", vtoc->tracksectorpairs); // Should be 122 for 256 byte sectors
    printf("Last track with allocated sectors: %d\n", vtoc->lastallocatedtrack);
    printf("Track allocation direction: %d\n", vtoc->allocationdirection);
    printf("Tracks/Disk: %d\n", vtoc->tracksperdisk); // Normally 35
    printf("Sectors/Track: %d\n", vtoc->sectorspertrack); // 13 or 16
    printf("Bytes/Sector: %d\n", (vtoc->bytespersector[1]<<8)|vtoc->bytespersector[0]); // LO/HI

    // Loop through the allocation bitmaps (one per track)
    if ((debug) && (vtoc->tracksperdisk<=35))
    {
      struct appledos_alloc_bitmap *alloc;

      printf("\nFree sector bit map\n");
      for (track=0; track<vtoc->tracksperdisk; track++)
      {
        // Map VTOC to sector buffer
        alloc=(struct appledos_alloc_bitmap *)&sector0->data[0x38+(track*4)];

        // Bitmap bytes 2 and 3 are not used
        // If a bit is set in bitmap, then sector is not allocated
        // Bit order is byte 0=FEDCBA98, byte 1=76543210
        if ((alloc->bitmap[2]==0) && (alloc->bitmap[3]==0))
          printf("T%d %.2x %.2x\n", track, alloc->bitmap[0], alloc->bitmap[1]);
      }
    }

    // Search for catalog sector
    sector1=diskstore_findhybridsector(vtoc->firstcattrack, 0, vtoc->firstcatsector);

    // Check we have catalog sector
    if (sector1==NULL)
      return;

    // Check we have data for catalog sector
    if (sector1->data==NULL)
      return;

    // Check sector is the right length
    if (sector1->datasize==APPLEGCR_SECTORLEN)
    {
      struct appledos_catalog *cat;
      struct appledos_fileentry *fentry;
      int catno;
      int i;

      // Map catalog to sector buffer
      cat=(struct appledos_catalog *)&sector1->data[0];

      printf("Next catalogue: T%d S%d\n", cat->nextcattrack, cat->nextcatsector);

      // Loop through file entries
      for (catno=0; catno<7; catno++)
      {
        // Map file entry to sector buffer
        fentry=(struct appledos_fileentry *)&sector1->data[0x0b+(catno*0x23)];

        // Stop listing if sector out of range
        if (fentry->firstsectorlisttrack>(APPLEDOS_MAXTRACK+1))
          break;

        // Stop listing if file length is zero
        if (((fentry->filelen[1]<<8)|fentry->filelen[0])==0)
          break;

        printf("[T%.2d S%.2d] ", fentry->firstsectorlisttrack, fentry->firstsectorlistsector);
        printf("%s", (fentry->filetypeflags&0x80)?"L ":" ");

        printf("\"");
        for (i=0; i<30; i++)
          printf("%c", fentry->filename[i]&0x7f);
        printf("\" [%.2x] ", fentry->filetypeflags&0x7f);

        switch (fentry->filetypeflags&0x7f)
        {
          case 0x00: printf("TEXT"); break;
          case 0x01: printf("INTEGER BASIC"); break;
          case 0x02: printf("APPLESOFT BASIC"); break;
          case 0x04: printf("BINARY"); break;
          case 0x08: printf("S"); break;
          case 0x10: printf("RELOCATABLE"); break;
          case 0x20: printf("A"); break;
          case 0x40: printf("B"); break;

          default:
            break;
        }
        printf(", %d sectors", (fentry->filelen[1]<<8)|fentry->filelen[0]);

        printf("\n");
      }
    }
  }

  return;
}

int appledos_validate()
{
  int format;
  Disk_Sector *sector0;

  format=APPLEDOS_UNKNOWN;

  // First check sectors are in Apple format
  if (diskstore_countsectormod(MODAPPLEGCR)==0)
    return format;

  // Validate we have either 13 or 16 sectors/track
  if ((diskstore_maxsectorid!=12) && (diskstore_maxsectorid!=15))
    return format;

  // Search for VTOC sector
  sector0=diskstore_findhybridsector(17, 0, 0);

  // Check we have VTOC sector
  if (sector0==NULL)
    return format;

  // Check we have data for VTOC sector
  if (sector0->data==NULL)
    return format;

  // Check sector is the right length
  if (sector0->datasize==APPLEGCR_SECTORLEN)
  {
    struct appledos_vtoc *vtoc;
    int track;

    // Map VTOC to sector buffer
    vtoc=(struct appledos_vtoc *)&sector0->data[0];

    // Validate catalogue position
    if ((vtoc->firstcattrack>(APPLEDOS_MAXTRACK+1)) || (vtoc->firstcatsector>APPLEDOS_MAXSECTOR+1))
      return format;

    // TODO Validate AppleDOS version (seen so far 3 = 3.3)

    // Validate disk volume
    if ((vtoc->diskvol<1) || (vtoc->diskvol>254))
      return format;

    // Validate track/sector pairs, should be 122 for 256 byte sectors
    if (vtoc->tracksectorpairs!=122)
      return format;

    // Validate last track with allocated sectors
    if (vtoc->lastallocatedtrack>(APPLEDOS_MAXTRACK+1))
      return format;

    // TODO Validate track allocation direction (seen so far 1 = forward)

    // Validate tracks/disk
    if (vtoc->tracksperdisk>(APPLEDOS_MAXTRACK+1))
      return format;

    // Validate sectors/track
    if ((vtoc->sectorspertrack!=13) && (vtoc->sectorspertrack!=16))
      return format;

    // Validate bytes/sector
    if (((vtoc->bytespersector[1]<<8)|vtoc->bytespersector[0])!=APPLEGCR_SECTORLEN)
      return format;

    // Validate allocation bitmaps (one per track)
    if (vtoc->tracksperdisk<=35)
    {
      struct appledos_alloc_bitmap *alloc;

      for (track=0; track<vtoc->tracksperdisk; track++)
      {
        // Map VTOC to sector buffer
        alloc=(struct appledos_alloc_bitmap *)&sector0->data[0x38+(track*4)];

        // Validate unused bits within allocation bitmap are unset
        if ((alloc->bitmap[2]!=0) || (alloc->bitmap[3]!=0))
          return format;
      }

      // All still OK, so return as an AppleDOS disk
      return APPLEDOS_33;
    }
  }

  return format;
}
