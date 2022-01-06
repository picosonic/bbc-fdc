#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#include "fsd.h"
#include "hardware.h"
#include "diskstore.h"

/*
Header:
=======
Identifier: "FSD"  string literal 3 bytes
   Creator:        5 bytes; date of creation/author
     Title:        Character string (unlimited length; may contain any but null)
 Title_End: 0x00   byte literal 1 byte
    Num_Tr: xx     1 byte, number of tracks

For each track
==============
   track_num: 1 byte (physical track number starting from 0) ** PHYSICAL **
     num_sec: 1 byte (00 == unformated)

    readable: 1 byte (00 == unreadable, ff ==readable) **ONLY IF FORMATTED**

   If readable:
     For each sector
     ===============
           Track_ID: 1 byte **LOGICAL**
        Head_number: 1 byte
          Sector_ID: 1 byte **LOGICAL**
      reported_size: 1 byte (2^{7+x}; 0 ==>128, 1 ==> 256, 2==>512, 3==>1024 etc)
          real_size: 1 byte (2^{7+x})
         error_code: 1 byte (0==No Error, &20==Deleted Data; &0E = Data CRC Error)
               data: <real_size> bytes
   Note that error_code matches the OSWORD &7F result byte

   If track unreadable:
           Track_ID: 1 byte
        Head_number: 1 byte
          Sector_ID: 1 byte
      reported_size: 1 byte

Decoding of the "creator" 5 byte field:
=======================================
   (byte1 byte2 byte3 byte4 byte5)

       Date_DD = (byte1 AND &F8)/8
       Date_MM = (byte3 AND &0F)
     Date_YYYY = (byte1 AND &07)*256+byte2 ** ONLY SUPPORTS YEARS UP TO 2047 **
    Creator_ID = (byte3 AND &F0)/16
   Release_num = ((byte5 AND &C0)/64)*256 + byte4

Notes
=====
The &E1 error code represents a sector that is really &100 bytes long but declares itself as something longer, for example &200 bytes.  The FSD format stores the over-read data so both the reported and real sizes are &200 bytes, but the CRC is only correct for a read of &100 bytes.  The E0 and E2 codes work similarly.
*/

void fsd_write(FILE *fsdfile, const unsigned char tracks, const char *title, const uint8_t sides, const int sidetoread)
{
  unsigned char buffer[10];
  unsigned char curtrack, curhead, cursector;
  unsigned char numsectors;
  Disk_Sector *sec;

  struct tm tim;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tim);

  // Write the header
  fwrite("FSD", 1, 3, fsdfile);

  buffer[0]=(tim.tm_mday<<3)|(((tim.tm_year+1900)&0x700)>>8);
  buffer[1]=(tim.tm_year+1900)&0xff;
  buffer[2]=(FSD_CREATORID<<4)|((tim.tm_mon+1)&0x0f);
  buffer[3]=0;
  buffer[4]=0;
  fwrite(buffer, 1, 5, fsdfile);

  fprintf(fsdfile, "%s", title);
  buffer[0]=0;
  fwrite(buffer, 1, 1, fsdfile);

  buffer[0]=tracks;
  fwrite(buffer, 1, 1, fsdfile);

  // Loop through tracks
  for (curtrack=0; curtrack<tracks; curtrack++)
  {
    unsigned char totalsectors;

    totalsectors=diskstore_countsectors(curtrack*hw_stepping, sidetoread);

    if (sides==2)
      totalsectors+=diskstore_countsectors(curtrack*hw_stepping, 1-sidetoread);

    // Track header
    buffer[0]=curtrack;
    fwrite(buffer, 1, 1, fsdfile);

    if (totalsectors>0)
    {
      buffer[0]=totalsectors;
      fwrite(buffer, 1, 1, fsdfile);

      buffer[0]=FSD_READABLE;
      fwrite(buffer, 1, 1, fsdfile);

      for (curhead=0; curhead<sides; curhead++)
      {
        if (sides==1) curhead=sidetoread;

        numsectors=diskstore_countsectors(curtrack*hw_stepping, curhead);
        for (cursector=0; cursector<numsectors; cursector++)
        {
          sec=diskstore_findnthsector(curtrack*hw_stepping, curhead, cursector);

          if (sec!=NULL)
          {
            // Sector header
            buffer[0]=sec->logical_track;
            buffer[1]=sec->logical_head; // No provision for physical head
            buffer[2]=sec->logical_sector;
            buffer[3]=sec->logical_size;

            switch (sec->datasize)
            {
              case 128: buffer[4]=0; break;
              case 256: buffer[4]=1; break;
              case 512: buffer[4]=2; break;
              case 1024: buffer[4]=3; break;
              case 2048: buffer[4]=4; break;
              case 4096: buffer[4]=5; break;
              case 8192: buffer[4]=6; break;
              case 16384: buffer[4]=7; break;
              default: buffer[4]=sec->logical_size; break;
            }

            buffer[5]=FSD_ERR_NONE;

            // Flag if sector marked deleted
            if ((sec->datatype==0xf8) || (sec->datatype==0xf9))
              buffer[5]|=FSD_ERR_DELETED;

            fwrite(buffer, 1, 6, fsdfile);

            fwrite(sec->data, 1, sec->datasize, fsdfile);
          }
          else
          {
            // TODO handle missing sector - may be due to ID or DATA CRC errors
          }
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
