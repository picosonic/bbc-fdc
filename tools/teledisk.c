#include <stdio.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include "crc.h"
#include "teledisk.h"
#include "diskstore.h"

void td0_write(FILE *td0file, const unsigned char tracks, const char *title)
{
  struct header_s header;
  struct comment_s comment;
  struct track_s track;
  struct sector_s sector;
  struct data_s data;

  char commentdata[256];

  uint16_t crcvalue;

  struct tm tim;
  struct timeval tv;

  unsigned char curtrack, curhead, cursector;
  unsigned char numsectors, totalsectors;
  Disk_Sector *sec;

  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tim);

  // Write the header
  header.signature[0]='T';
  header.signature[1]='D';
  header.sequence=0;
  header.checkseq=74;
  header.version=21;
  header.datarate=0|(diskstore_countsectormod(MODFM)>0?(128|2):2); // TODO
  header.drivetype=4; // TODO
  header.stepping=0|0x80; // TODO
  header.dosflag=0;
  header.sides=diskstore_countsectors(diskstore_mintrack, 1)>0?2:1;
  header.crc=calc_crc_stream((unsigned char *)&header, 10, 0x0000, TELEDISK_POLYNOMIAL);
  fwrite(&header, 1, sizeof(header), td0file);

  // Write the comment/date
  bzero(commentdata, sizeof(commentdata));
  strcpy(commentdata, title);
  comment.datalen=strlen(title)+1;
  comment.hour=tim.tm_hour;
  comment.minute=tim.tm_min;
  comment.second=tim.tm_sec;
  comment.day=tim.tm_mday;
  comment.month=tim.tm_mon;
  comment.year=tim.tm_year;
  crcvalue=calc_crc_stream((unsigned char *)&comment.datalen, 8, 0x0000, TELEDISK_POLYNOMIAL);
  comment.crc=calc_crc_stream((unsigned char *)commentdata, comment.datalen, crcvalue, TELEDISK_POLYNOMIAL);
  fwrite(&comment, 1, sizeof(comment), td0file);
  fwrite(&commentdata, 1, comment.datalen, td0file);

  // Loop through the tracks
  for (curtrack=0; curtrack<tracks; curtrack++)
  {
    totalsectors=diskstore_countsectors(curtrack, 0)+diskstore_countsectors(curtrack, 1);
 
    if (totalsectors>0)
    {
      // Loop through the heads
      for (curhead=0; curhead<2; curhead++)
      {
        numsectors=diskstore_countsectors(curtrack, curhead);

        // Track header
        track.track=curtrack;
        track.head=curhead;
        track.sectors=numsectors;
        track.crc=calc_crc_stream((unsigned char *)&track, 3, 0x0000, TELEDISK_POLYNOMIAL)&0xff;
        fwrite(&track, 1, sizeof(track), td0file);

        if (numsectors>0)
        {
          // Loop through the sectors
          for (cursector=0; cursector<numsectors; cursector++)
          {
            sec=diskstore_findnthsector(curtrack, curhead, cursector);

            if (sec!=NULL)
            {
              // Sector header
              sector.track=sec->logical_track;
              sector.head=sec->logical_head;
              sector.sector=sec->logical_sector;
              sector.size=sec->logical_size;
              sector.flags=0; // TODO
              sector.crc=calc_crc_stream(sec->data, sec->datasize, 0x0000, TELEDISK_POLYNOMIAL)&0xff;
              fwrite(&sector, 1, sizeof(sector), td0file);

              // TODO Sector data
              data.blocksize=(128<<sector.size)+1;
              data.encoding=0; // TODO RAW for now
              fwrite(&data, 1, sizeof(data), td0file);
              fwrite(sec->data, 1, sec->datasize, td0file);
            }
            else
            {
              // TODO handle missing sector
            }
          }
        }
      }
    }
    else
    {
      // TODO track without any sectors
    }
  }

  bzero(&track, sizeof(track));
  track.sectors=TELEDISK_LAST_TRACK;
  track.crc=calc_crc_stream((unsigned char *)&track, 3, 0x0000, TELEDISK_POLYNOMIAL)&0xff;
  fwrite(&track, 1, sizeof(track), td0file);
}
