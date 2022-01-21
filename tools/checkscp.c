#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "scp.h"

struct scp_header header;
long scp_endofheader=0;
uint32_t *scp_trackoffsets=NULL;

int scp_processheader(FILE *scpfile)
{
  bzero(&header, sizeof(header));
  fread(&header, 1, sizeof(header), scpfile);

  if (strncmp((char *)&header.magic, SCP_MAGIC, strlen(SCP_MAGIC))!=0)
  {
    printf("Not an SCP file\n");
    return 1;
  }

  printf("SCP magic detected\n");

  if ((header.flags & SCP_FLAGS_FOOTER)==0)
    printf("Version: %d.%d\n", header.version>>4, header.version&0x0f);

  printf("Disk type: %d %d (", header.disktype>>4, header.disktype&0x0f);
  switch (header.disktype&0xf0)
  {
    case SCP_MAN_COMMODORE:
      printf("Commodore");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_C64:
          printf(" C64");
          break;

        case SCP_DISK_Amiga:
          printf(" Amiga");
          break;

        case SCP_DISK_AmigaHD:
          printf(" AmigaHD");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_ATARI:
      printf("Atari");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_AtariFMSS:
          printf(" FM SS");
          break;

        case SCP_DISK_AtariFMDS:
          printf(" FM DS");
          break;

        case SCP_DISK_AtariFMEx:
          printf(" FM Ex");
          break;

        case SCP_DISK_AtariSTSS:
          printf(" ST SS");
          break;

        case SCP_DISK_AtariSTDS:
          printf(" ST DS");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_APPLE:
      printf("Apple");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_AppleII:
          printf(" II");
          break;

        case SCP_DISK_AppleIIPro:
          printf(" II Pro");
          break;

        case SCP_DISK_Apple400K:
          printf(" 400K");
          break;

        case SCP_DISK_Apple800K:
          printf(" 800K");
          break;

        case SCP_DISK_Apple144:
          printf(" 144");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_PC:
      printf("PC");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_PC360K:
          printf(" 360K");
          break;

        case SCP_DISK_PC720K:
          printf(" 720K");
          break;

        case SCP_DISK_PC12M:
          printf(" 1.2Mb");
          break;

        case SCP_DISK_PC144M:
          printf(" 1.44Mb");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_TANDY:
      printf("Tandy");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_TRS80SSSD:
          printf(" TRS80 SSSD");
          break;

        case SCP_DISK_TRS80SSDD:
          printf(" TRS80 SSDD");
          break;

        case SCP_DISK_TRS80DSSD:
          printf(" TRS80 DSSD");
          break;

        case SCP_DISK_TRS80DSDD:
          printf(" TRS80 DSDD");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_TI:
      printf("TI");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_TI994A:
          printf(" 994A");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_ROLAND:
      printf("Roland");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_D20:
          printf(" D20");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_AMSTRAD:
      printf("Amstrad");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_CPC:
          printf(" CPC");
          break;

        default:
          break;
      }
      break;

    case SCP_MAN_OTHER:
      printf("Other");

      switch (header.disktype&0x0f)
      {
        case SCP_DISK_360:
          printf(" 360K");
          break;

        case SCP_DISK_12M:
          printf(" 1.2Mb");
          break;

        case SCP_DISK_Rsrvd1:
          printf(" Reserved 1");
          break;

        case SCP_DISK_Rsrvd2:
          printf(" Reserved 2");
          break;

        case SCP_DISK_720:
          printf(" 720K");
          break;

        case SCP_DISK_144M:
          printf(" 1.44Mb");
          break;

        default:
          break;
      }
      break;

    default:
      printf("Unknown");
      break;
  }

  printf(")\n");

  printf("Revolutions: %d\n", header.revolutions);
  printf("Tracks: %d to %d\n", header.starttrack, header.endtrack);

  printf("Flags: 0x%.2x [", header.flags);

  if ((header.flags & SCP_FLAGS_INDEX)!=0)
    printf(" Indexed");
  else
    printf(" NonIndexed");

  if ((header.flags & SCP_FLAGS_96TPI)!=0)
    printf(" 96tpi");
  else
    printf(" 48tpi");

  if ((header.flags & SCP_FLAGS_360RPM)!=0)
    printf(" 360rpm");
  else
    printf(" 300rpm");

  if ((header.flags & SCP_FLAGS_NORMALISED)!=0)
    printf(" Normalised");
  else
    printf(" Captured");

  if ((header.flags & SCP_FLAGS_RW)!=0)
    printf(" Read/Write");
  else
    printf(" Read_Only");

  if ((header.flags & SCP_FLAGS_FOOTER)!=0)
    printf(" Has_Footer");

  if ((header.flags & SCP_FLAGS_EXTENDED)!=0)
    printf(" Extended");

  if ((header.flags & SCP_FLAGS_CREATOR)!=0)
    printf(" 3rd_party");
  else
    printf(" SuperCard_Pro");

  printf(" ]\n");

  printf("Bitcell encoding: %d bits\n", header.bitcellencoding==0?16:header.bitcellencoding);
  printf("Heads: %d (", header.heads);
  switch (header.heads)
  {
    case 0:
      printf("Both");
      break;

    case 1:
      printf("Bottom");
      break;

    case 2:
      printf("Top");
      break;

    default:
      printf("Unknown");
      break;

  }
  printf(")\n");
  printf("Resolution: %dns\n", (header.resolution+1)*SCP_BASE_NS);
  printf("Checksum: 0x%.8x\n", header.checksum);

  return 0;
}

void scp_processtimings(FILE *scpfile, const long dataoffset, const uint32_t datalen)
{
  long trackpos;
  uint32_t sample;
  uint16_t bitcell;
  long ticks;

  // Remember where file pointer was
  trackpos=ftell(scpfile);

  // Seek to start of data
  fseek(scpfile, dataoffset, SEEK_SET);

  for (sample=0; sample<datalen; sample++)
  {
    fread(&bitcell, 1, sizeof(bitcell), scpfile);

    // Swap byte order (if required)
    bitcell=be16toh(bitcell);

    printf("  %.4x = %.3fus\n", bitcell, ((double)bitcell*((header.resolution+1)*SCP_BASE_NS))/1000);
    for (ticks=0; ticks<(bitcell/2); ticks++)
      printf("#");
    printf("\n");
  }

  // Restore file pointer
  fseek(scpfile, trackpos, SEEK_SET);
}

void scp_processtrack(FILE *scpfile, const unsigned char track, const uint8_t revolutions)
{
  struct scp_tdh thdr;
  struct scp_timings timings;

  // Don't process empty tracks
  if (scp_trackoffsets[track]==0) return;

  // Seek to data for this track
  fseek(scpfile, scp_trackoffsets[track], SEEK_SET);

  // Verify track header
  fread(&thdr, 1, sizeof(thdr), scpfile);
  if (strncmp((char *)&thdr.magic, SCP_TRACK, strlen(SCP_TRACK))==0)
  {
    uint8_t i;

    printf("Track %d.%d -> %d.%d\n", track/2, track%2, thdr.track/2, thdr.track%2);

    for (i=0; i<revolutions; i++)
    {
      fread(&timings, 1, sizeof(timings), scpfile);
      printf("  %d:%x (%.2f ms) / %u len / %u offs\n", i, timings.indextime, ((double)timings.indextime*SCP_BASE_NS)/1000000, timings.tracklen, timings.dataoffset);

      // Process the track timings
//      scp_processtimings(scpfile, scp_trackoffsets[track]+timings.dataoffset, timings.tracklen);
    }
  }
}

int main(int argc, char **argv)
{
  uint32_t checksum;
  uint8_t block[256];
  size_t i;
  FILE *fp;

  if (argc!=2)
  {
    printf("Specify .scp on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  if (scp_processheader(fp)!=0)
  {
    fclose(fp);
    return 1;
  }

  scp_endofheader=ftell(fp);

  // validate checksum
  checksum=0;
  while (!feof(fp))
  {
    size_t blocklen;

    blocklen=fread(block, 1, sizeof(block), fp);
    if (blocklen>0)
      for (i=0; i<blocklen; i++)
        checksum+=block[i];
  }
  fseek(fp, scp_endofheader, SEEK_SET);
  printf("Calculated Checksum: 0x%.8x ", checksum);

  if (checksum!=header.checksum)
  {
    printf("(Mismatch) \n");
    fclose(fp);

    return 1;
  }
  else
    printf("(OK) \n");

  // Skip over extended data if used
  if ((header.flags & SCP_FLAGS_EXTENDED)!=0)
    fseek(fp, 0x80, SEEK_SET);

  // Allocate memory to store track data offsets
  scp_trackoffsets=malloc(sizeof(uint32_t) * SCP_MAXTRACKS);
  if (scp_trackoffsets!=NULL)
  {
    unsigned char track;

    fread(scp_trackoffsets, 1, (header.endtrack-header.starttrack+1)*sizeof(uint32_t), fp);

    for (track=header.starttrack; track<header.endtrack; track++)
      scp_processtrack(fp, track, header.revolutions);

    // Look for ASCII timestamp
    //   After all track data, if first byte is 0x30 to 0x5F
    //   Otherwise this is the start of extension footer (when indicated in header flags)

    free(scp_trackoffsets);
  }

  fclose(fp);

  return 0;
}
