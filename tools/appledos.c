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
}

int appledos_validate()
{
  int format;
  Disk_Sector *sector0;

  // Search for VTOC sector
  sector0=diskstore_findhybridsector(17, 0, 0);

  format=APPLEDOS_UNKNOWN;

  // Check we have VTOC sector
  if (sector0==NULL)
    return format;

  // Check we have data for VTOC sector
  if (sector0->data==NULL)
    return format;

  // Check sector is the right length
  if (sector0->datasize==APPLEGCR_SECTORLEN)
  {
    unsigned char sniff[APPLEGCR_SECTORLEN];
    struct appledos_vtoc *vtoc;
    int track;

    // Copy VTOC to sniff buffer
    memcpy(sniff, sector0->data, sector0->datasize);

    // Map VTOC to sniff buffer
    vtoc=(struct appledos_vtoc *)&sniff[0];

    printf("First catalogue: T%d S%d\n", vtoc->firstcattrack, vtoc->firstcatsector);
    printf("Apple DOS release used: %d\n", vtoc->dosrelease);
    printf("Disk volume: %d\n", vtoc->diskvol); // 1 to 254
    printf("Track/Sector pairs: %d\n", vtoc->tracksectorpairs); // Should be 122 for 256 byte sectors
    printf("Last track with allocated sectors: %d\n", vtoc->lastallocatedtrack);
    printf("Track allocation direction: %d\n", vtoc->allocationdirection);
    printf("Tracks/Disk: %d\n", vtoc->tracksperdisk); // Normally 35
    printf("Sectors/Track: %d\n", vtoc->sectorspertrack); // 13 or 16
    printf("Bytes/Sector: %d\n", (vtoc->bytespersector[1]<<8)|vtoc->bytespersector[0]); // LO/HI

    // Loop through the allocation bitmaps (one per track)
    if (vtoc->tracksperdisk<=35)
    {
      struct appledos_alloc_bitmap *alloc;

      for (track=0; track<vtoc->tracksperdisk; track++)
      {
        // Map VTOC to sniff buffer
        alloc=(struct appledos_vtoc *)&sniff[0x38+(track*4)];

        // Bitmap bytes 2 and 3 are not used
        // If a bit is set in bitmap, then sector is not allocated
        // Bit order is byte 0=FEDCBA98, byte 1=76543210
        if ((alloc->bitmap[2]==0) && (alloc->bitmap[3]==0))
          printf("T%d %.2x %.2x\n", track, alloc->bitmap[0], alloc->bitmap[1]);
      }
    }
  }

  return format;
}
