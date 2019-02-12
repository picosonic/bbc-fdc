#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "crc.h"
#include "lzhuf.h"
#include "teledisk.h"

#define LZ_UNCOMPRESSED (1024*1024*3)

FILE *fp;

struct header_s header;
struct comment_s comment;
struct track_s track;
struct sector_s sector;
struct data_s data;

int compressed=0;
uint8_t *block=NULL;
size_t blocksize=0;
size_t blockpointer=0;

size_t blockread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  if (compressed==1)
  {
    size_t totalread=(nmemb*size);

    if ((blockpointer+totalread)<blocksize)
    {
      memcpy(ptr, &block[blockpointer], totalread);
      blockpointer+=totalread;

      return totalread;
    }
    else
    {
      size_t avail;

      avail=(blocksize-blockpointer);
      if (avail>0)
      {
        memcpy(ptr, &block[blockpointer], avail);
        blockpointer+=avail;
      }

      return avail;
    }
  }
  else
    return fread(ptr, size, nmemb, stream);
}

int blockseek(FILE *stream, long offset, int whence)
{
  if (compressed==1)
  {
    blockpointer=offset;
    return 0;
  }
  else
    return fseek(stream, offset, whence);
}

long blocktell(FILE *stream)
{
  if (compressed==1)
    return blockpointer;
  else
    return ftell(stream);
}

int main(int argc, char **argv)
{
  size_t numread;
  int i, j, k;
  uint8_t buffer[8192];
  uint16_t crcvalue;

  if (argc!=2)
  {
    printf("Specify .td0 on command line\n");
    return 1;
  }

  fp=fopen(argv[1], "rb");
  if (fp==NULL)
  {
    printf("Unable to open file\n");
    return 2;
  }

  printf("Opened '%s'\n", argv[1]);

  numread=blockread(&header, 1, sizeof(header), fp);

  if (numread<sizeof(header))
  {
    printf("Unable to read header\n");
    fclose(fp);
    return 3;
  }

  // Check CRC before processing
  if (header.crc!=calc_crc_stream((unsigned char *)&header, 10, 0x0000, TELEDISK_POLYNOMIAL))
  {
      printf("Not a valid TeleDisk file\n");
      fclose(fp);
      return 4;
  }

  printf("Signature: %c%c\n", header.signature[0], header.signature[1]);

  if (strncmp((char *)header.signature, "td", 2)!=0)
  {
    if (strncmp((char *)header.signature, "TD", 2)!=0)
    {
      printf("Not a valid TeleDisk file\n");
      fclose(fp);
      return 4;
    }
  }
  else
    compressed=1;

  printf("Volume sequence: %d\n", header.sequence);
  printf("Check sequence: %d\n", header.checkseq);
  printf("Version: %d.%d\n", header.version/10, header.version%10);
  printf("Data rate: ");
  switch (header.datarate&0x03)
  {
    case 0x00: printf("250kbps %s\n", header.datarate>127?"FM":"MFM"); break;
    case 0x01: printf("300kbps %s\n", header.datarate>127?"FM":"MFM"); break;
    case 0x02: printf("500kbps %s\n", header.datarate>127?"FM":"MFM"); break;
    default: printf("Unknown %d\n", header.datarate&0x03); break;
  }

  printf("Drive type: ");
  switch (header.drivetype)
  {
    case 0x01: printf("360K\n"); break;
    case 0x02: printf("1.2M\n"); break;
    case 0x03: printf("720K\n"); break;
    case 0x04: printf("1.44M\n"); break;
    case 0x05: printf("2.88M\n"); break;
    case 0x06: printf("2.88M\n"); break;
    case 0x10: printf("ATAPI removable media device\n"); break;
    default: printf("Unknown %d\n", header.drivetype); break;
  }

  printf("Stepping: ");
  switch (header.stepping&0x03)
  {
    case 0: printf("Single-Step\n"); break;
    case 1: printf("Double-step\n"); break;
    case 2: printf("Even-only step (96 tpi disk in 48 tpi drive)\n"); break;
    default: printf("Unknown\n"); break;
  }

  printf("DOS allocation flag: 0x%.2x (%s)\n", header.dosflag, header.dosflag==0?"normal":"skip unallocated sectors");
  printf("Sides: %d\n", header.sides<2?1:2);
  printf("CRC: 0x%.4x (0x%.4x)\n", header.crc, calc_crc_stream((unsigned char *)&header, 10, 0x0000, TELEDISK_POLYNOMIAL));

  // If compression used, then everything below here is compressed
  if (compressed==1)
  {
    long clen; // Compressed data length
    struct stat fs; // File status
    uint8_t *lzdata; // Compressed data

    lz_Init();

    // Determine how much of the file is left
    fstat(fileno(fp), &fs);
    clen=fs.st_size-ftell(fp)+4;

    // Allocate memory, and read remainder of file
    lzdata=malloc(clen);
    if (lzdata!=NULL)
    {
      // Add fake length
      lzdata[0]=lzdata[1]=lzdata[2]=lzdata[3]=0xff;
      fread(&lzdata[4], clen, 1, fp);

      // Determine decompressed size, and allocate block memory
      block=malloc(LZ_UNCOMPRESSED);

      if (block!=NULL)
      {
        // Decompress data from lzdata to block
        blocksize=lz_Decode(lzdata, clen, block, LZ_UNCOMPRESSED);

        printf("Decompressed to %d bytes\n", blocksize);
      }
      else
      {
        printf("Unable to allocate memory for decompression\n");
        fclose(fp);
        free(lzdata);

        return 9;
      }

      free(lzdata);
    }
  }

  if ((header.stepping&0x80)!=0)
  {
    numread=blockread(&comment, 1, sizeof(comment), fp);

    if (numread<sizeof(comment))
    {
      printf("Unable to read comment\n");
      if (block!=NULL)
      {
        free(block);
        block=NULL;
      }

      fclose(fp);
      return 5;
    }

    printf("\nComment block read\n");
    printf("CRC: 0x%.4x\n", comment.crc);
    printf("Data length: %d\n", comment.datalen);
    printf("Captured : %.2d:%.2d:%.2d on %.2d/%.2d/%d\n", comment.hour, comment.minute, comment.second, comment.day, comment.month+1, comment.year+1900);

    crcvalue=calc_crc_stream((unsigned char *)&comment.datalen, 8, 0x0000, TELEDISK_POLYNOMIAL);

    if (comment.datalen>0)
    {
      char *cdata;
      int actuallen=comment.datalen;

      cdata=malloc(comment.datalen);
      if (cdata!=NULL)
      {
        printf("Comment data: '");
        numread=blockread(cdata, 1, comment.datalen, fp);
        if (numread==comment.datalen)
        {
          for (i=actuallen-1; i>0; i--)
            if (cdata[i]!=0)
            {
              actuallen=i+1;
              break;
            }

          for (i=0; i<actuallen; i++)
            printf("%c", cdata[i]==0?'\n':cdata[i]);
        }
        printf("'\n");
        crcvalue=calc_crc_stream((unsigned char *)cdata, comment.datalen, crcvalue, TELEDISK_POLYNOMIAL);
        free(cdata);
      }
    }
    printf("Calculated CRC: 0x%.4x\n", crcvalue);

    // Validate comment CRC
    if (comment.crc!=crcvalue)
    {
      printf("Invalid comment CRC\n");
      if (block!=NULL)
      {
        free(block);
        block=NULL;
      }

      fclose(fp);
      return 5;
    }
  }

  do
  {
    numread=blockread(&track, 1, sizeof(track), fp);
    if (numread<sizeof(track))
    {
      printf("Unable to read track header\n");
      if (block!=NULL)
      {
        free(block);
        block=NULL;
      }

      fclose(fp);
      return 6;
    }

    if (track.sectors==TELEDISK_LAST_TRACK)
    {
      if (block!=NULL)
      {
        free(block);
        block=NULL;
      }

      fclose(fp);
      return 0;
    }

    // Validate track header CRC
    if (track.crc!=(calc_crc_stream((unsigned char *)&track, 3, 0x0000, TELEDISK_POLYNOMIAL)&0xff))
    {
      printf("Invalid track CRC\n");
      if (block!=NULL)
      {
        free(block);
        block=NULL;
      }

      fclose(fp);
      return 6;
    }

    printf("--  TRACK ");
    printf("C%d ", track.track);
    printf("H%d ", track.head);
    printf("[%d] ", track.sectors);
    printf("CRC:0x%.2x (0x%.2x) --\n", track.crc, calc_crc_stream((unsigned char *)&track, 3, 0x0000, TELEDISK_POLYNOMIAL)&0xff); // lower byte of 16-bit crc for preceding 3 bytes

    if (track.sectors>0)
    {
      for (i=0; i<track.sectors; i++)
      {
        numread=blockread(&sector, 1, sizeof(sector), fp);
        if (numread<sizeof(sector))
        {
          printf("Unable to read sector header\n");
          if (block!=NULL)
          {
            free(block);
            block=NULL;
          }

          fclose(fp);
          return 7;
        }

        printf("  -- SECTOR ");
        printf("C%d ", sector.track);
        printf("H%d ", sector.head);
        printf("S%d ", sector.sector);
        printf("R%d (%d bytes) ", sector.size, 128<<sector.size);
        printf("flags:0x%.2x ", sector.flags);
        printf("CRC:0x%.2x --\n", sector.crc);

        crcvalue=calc_crc_stream((unsigned char *)&sector, 5, 0, TELEDISK_POLYNOMIAL);

        if (((sector.flags&TELEDISK_FLAGS_UNALLOCATED)==0) && ((sector.flags&TELEDISK_FLAGS_NODATA)==0))
        {
          numread=blockread(&data, 1, sizeof(data), fp);
          if (numread<sizeof(data))
          {
            printf("Unable to read sector data header\n");
            if (block!=NULL)
            {
              free(block);
              block=NULL;
            }

            fclose(fp);
            return 8;
          }

          printf("    DATA %d bytes, encoding %d\n", data.blocksize-1, data.encoding);
          switch (data.encoding)
          {
            struct datarepeat_s repblock;
            unsigned char rle_c, rle_r, rle_n;
            uint16_t rle_l;
            int32_t output;

            case 0: // Raw as-is
              blockread(&buffer, 1, data.blocksize-1, fp);
              crcvalue=calc_crc_stream((unsigned char *)&buffer, data.blocksize-1, 0, TELEDISK_POLYNOMIAL);
              printf("    [%d]", data.blocksize-1);
              for (j=0; j<(data.blocksize-1); j++)
                printf("%c", ((buffer[j]>=' ')&&(buffer[j]<='~'))?buffer[j]:'.');
              printf("\n");
              break;

            case 1: // Repeated 2-byte pattern
              blockread(&repblock, 1, sizeof(repblock), fp);

              printf("      @ %lx [%d x 0x%.4x]\n", blocktell(fp), repblock.repcount, repblock.repdata);
              printf("    [%d]", repblock.repcount*2);
              for (j=0; j<repblock.repcount; j++)
              {
                crcvalue=calc_crc_stream((unsigned char *)&repblock.repdata, sizeof(repblock.repdata), j==0?0:crcvalue, TELEDISK_POLYNOMIAL);
                printf("%c%c", (((repblock.repdata&0xff00)>>8>=' ')&&((repblock.repdata&0xff00)>>8<='~'))?(repblock.repdata&0xff00)>>8:'.', (((repblock.repdata&0xff)>=' ')&&((repblock.repdata&0xff)<='~'))?(repblock.repdata&0xff):'.');
              }
              printf("\n");
              break;

            case 2: // RLE
              output=0;
              printf("    [%d]", (128<<sector.size));

              while (output<(128<<sector.size))
              {
                blockread(&rle_c, 1, 1, fp);

                if (rle_c==0)
                {
                  // Literal block
                  blockread(&rle_n, 1, 1, fp); // Length of as-is data

                  blockread(&buffer, 1, rle_n, fp);
                  crcvalue=calc_crc_stream((unsigned char *)&buffer, rle_n, output==0?0:crcvalue, TELEDISK_POLYNOMIAL);
                  for (j=0; j<rle_n; j++)
                  {
                    printf("%c", ((buffer[j]>=' ')&&(buffer[j]<='~'))?buffer[j]:'.');
                    output++;

                    if (output>=(128<<sector.size)) break;
                  }
                }
                else
                {
                  // Repeating block
                  rle_l=rle_c*2; // Determine repeating block length
                  blockread(&rle_r, 1, 1, fp); // Repeat count
                  blockread(&buffer, 1, rle_l, fp);

                  for (k=0; k<rle_r; k++)
                  {
                    crcvalue=calc_crc_stream((unsigned char *)&buffer, rle_l, output==0?0:crcvalue, TELEDISK_POLYNOMIAL);
                    for (j=0; j<rle_l; j++)
                    {
                      printf("%c", ((buffer[j]>=' ')&&(buffer[j]<='~'))?buffer[j]:'.');
                      output++;

                      if (output>=(128<<sector.size)) break;
                    }
                  }
                }
              }
              printf("\n");
              break;

            default:
              blockseek(fp, data.blocksize-1, SEEK_CUR);
              break;
          }
        }

        printf("Calculated CRC:0x%.2x\n", crcvalue&0xff);

        if ((crcvalue&0xff)!=sector.crc)
        {
          if (block!=NULL)
          {
            free(block);
            block=NULL;
          }

          fclose(fp);

          return 10;
        }
      }
    }
  } while (track.sectors!=TELEDISK_LAST_TRACK);

  if (block!=NULL)
  {
    free(block);
    block=NULL;
  }

  fclose(fp);

  return 0;
}
