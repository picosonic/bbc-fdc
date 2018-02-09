#include <stdio.h>
#include <strings.h>

#include "fsd.h"
#include "diskstore.h"

void fsd_write(FILE *fsdfile, const unsigned char tracks)
{
  unsigned char buffer[10];
  unsigned char curtrack, cursector;
  unsigned char numsectors;
  Disk_Sector *sec;

  // Write the header
  fwrite("FSD", 1, 3, fsdfile);

  bzero(buffer, 10);
  fwrite(buffer, 1, 5, fsdfile);

  fwrite("NO TITLE", 1, 8, fsdfile);
  buffer[0]=0;
  fwrite(buffer, 1, 1, fsdfile);

  buffer[0]=tracks;
  fwrite(buffer, 1, 1, fsdfile);

  // Loop through tracks
  for (curtrack=0; curtrack<tracks; curtrack++)
  {
    numsectors=diskstore_countsectors(curtrack, 0);

    // Track header
    buffer[0]=curtrack;
    fwrite(buffer, 1, 1, fsdfile);

    if (numsectors>0)
    {
      buffer[0]=numsectors;
      fwrite(buffer, 1, 1, fsdfile);

      buffer[0]=FSD_READABLE;
      fwrite(buffer, 1, 1, fsdfile);

      for (cursector=0; cursector<numsectors; cursector++)
      {
        sec=diskstore_findnthsector(curtrack, 0, cursector);

        if (sec!=NULL)
        {
          // Sector header
          buffer[0]=sec->logical_track;
          buffer[1]=sec->logical_head;
          buffer[2]=sec->logical_sector;
          buffer[3]=sec->logical_size;

          buffer[4]=sec->logical_size; // TODO - allow this to be different

          if (sec->datatype==0xf8)
            buffer[5]=FSD_ERR_DELETED;
          else
            buffer[5]=FSD_ERR_NONE;

          fwrite(buffer, 1, 6, fsdfile);

          fwrite(sec->data, 1, sec->datasize, fsdfile);
        }
        else
        {
          // TODO handle missing sector
        }
      }
    }
    else
    {
      buffer[0]=FSD_UNFORMATTED;
      fwrite(buffer, 1, 1, fsdfile);
    }
  }
}
