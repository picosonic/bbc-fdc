#include <stdio.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include "crc.h"
#include "teledisk.h"
#include "diskstore.h"

int td0_checkrepeats(uint8_t *buf, uint16_t buflen)
{
  uint16_t i;
  uint8_t c1, c2;

  c1=buf[0];
  c2=buf[1];

  // First check for same character repeated
  for (i=1; i<buflen; i++)
    if (buf[i]!=c1) break;

  if (i>=buflen)
    return 1;

  // Next check for pair of characters repeated
  for (i=2; i<buflen; i+=2)
    if ((buf[i]!=c1) || (buf[i+1]!=c2)) break;

  if (i>=buflen)
    return 2;

  return 0;
}

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
              struct datarepeat_s repblock;

              // Sector header
              sector.track=sec->logical_track;
              sector.head=sec->logical_head;
              sector.sector=sec->logical_sector;
              sector.size=sec->logical_size;
              sector.flags=0|(((sec->datatype==0xf8) || (sec->datatype==0xf9)?TELEDISK_FLAGS_DELDATA:0)); // TODO
              sector.crc=calc_crc_stream(sec->data, sec->datasize, 0x0000, TELEDISK_POLYNOMIAL)&0xff;
              fwrite(&sector, 1, sizeof(sector), td0file);

              // Sector data
              data.blocksize=(128<<sector.size)+1;
              data.encoding=0; // Default to RAW

              if (td0_checkrepeats(sec->data, sec->datasize)>0)
              {
                data.encoding=1;
                repblock.repcount=(sec->datasize/2);
                repblock.repdata=(sec->data[0]<<8)|sec->data[1];
              }

              // TODO Check for RLE

              fwrite(&data, 1, sizeof(data), td0file);

              if (data.encoding==1)
                fwrite(&repblock, 1, sizeof(repblock), td0file);
              else
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
