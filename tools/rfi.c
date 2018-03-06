#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>

#include "rfi.h"

void rfi_writeheader(FILE *rfifile, const int tracks, const int sides, const long rate, const unsigned char writeable)
{
  struct tm tim;
  struct timeval tv;

  if (rfifile==NULL) return;

  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tim);

  fprintf(rfifile, "%s", "RFI");

  fprintf(rfifile, "{date:\"%02d/%02d/%d\",time:\"%02d:%02d:%02d\",tracks:%d,sides:%d,rate:%ld,writeable:%d}", tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900, tim.tm_hour, tim.tm_min, tim.tm_sec, tracks, sides, rate, writeable);
}

void rfi_writetack(FILE *rfifile, const int track, const int side, const float rpm, const char *encoding, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  if (rfifile==NULL) return;

  fprintf(rfifile, "{track:%d,side:%d,rpm:%.2f,", track, side, rpm);

  if (strstr(encoding, "raw")!=NULL)
  {
    fprintf(rfifile, "enc:\"%s\",len:%ld}", encoding, rawdatalength);
    fwrite(rawtrackdata, 1, rawdatalength, rfifile);
  }
  else
  {
    // Don't write any track data for unknown encodings
    fprintf(rfifile, "enc:\"unknown\",len:0}");
  }
}
