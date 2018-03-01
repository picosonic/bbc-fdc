#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "hardware.h"
#include "diskstore.h"
#include "dfs.h"
#include "fsd.h"

// SPI read buffer size
#define SPIBUFFSIZE (1024*1024)

// Microseconds in a bitcell window for single-density FM
#define BITCELL 4

// SPI sample rate in Hz
#define SAMPLERATE 12500000

// Microseconds in a second
#define USINSECOND 1000000

// Disk bitstream block size
#define BLOCKSIZE (16384+5)

// For sector status
#define NODATA 0
#define BADDATA 1
#define GOODDATA 2

// For type of capture
#define DISKNONE 0
#define DISKCAT 1
#define DISKIMG 2
#define DISKRAW 3

// For type of output
#define IMAGENONE 0
#define IMAGERAW 1
#define IMAGESSD 2
#define IMAGEDSD 3
#define IMAGEFSD 4

#define RETRIES 5

// State machine
#define SYNC 1
#define ADDR 2
#define DATA 3

// FM Block types
#define BLOCKNULL 0x00
#define BLOCKINDEX 0xfc
#define BLOCKADDR 0xfe
#define BLOCKDATA 0xfb
#define BLOCKDELDATA 0xf8

int debug=0;
int sides=1; // Default to single sided
int disktracks;
int drivetracks;
int capturetype=DISKNONE; // Default to no output
int outputtype=IMAGENONE; // Default to no image

int state=SYNC;

unsigned char *spibuffer;
unsigned int datacells;
int bits=0;
int info=0;

// Most recent address mark
unsigned long idpos, blockpos;
int idamtrack, idamhead, idamsector, idamlength;
int lasttrack, lasthead, lastsector, lastlength;
unsigned char blocktype;
unsigned int blocksize;
unsigned int idblockcrc, datablockcrc, bitstreamcrc;

// Block data buffer
unsigned char bitstream[BLOCKSIZE];
unsigned int bitlen=0;

// Processing position within the SPI buffer
unsigned long datapos=0;

// File handles
FILE *diskimage=NULL;
FILE *rawdata=NULL;

// CCITT CRC16 (Floppy Disk Data)
unsigned int calc_crc(unsigned char *data, int datalen)
{
  unsigned int crc=0xffff;
  int i, j;

  for (i=0; i<datalen; i++)
  {
    crc ^= data[i] << 8;
    for (j=0; j<8; j++)
      crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
  }

  return (crc & 0xffff);
}

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void addbit(unsigned char bit)
{
  unsigned char clock, data;
  unsigned char dataCRC;

  datacells=((datacells<<1)&0xffff);
  datacells|=bit;
  bits++;

  // Keep processing until we have 8 clock bits + 8 data bits
  if (bits>=16)
  {
    // Extract clock byte
    clock=((datacells&0x8000)>>8);
    clock|=((datacells&0x2000)>>7);
    clock|=((datacells&0x0800)>>6);
    clock|=((datacells&0x0200)>>5);
    clock|=((datacells&0x0080)>>4);
    clock|=((datacells&0x0020)>>3);
    clock|=((datacells&0x0008)>>2);
    clock|=((datacells&0x0002)>>1);

    // Extract data byte
    data=((datacells&0x4000)>>7);
    data|=((datacells&0x1000)>>6);
    data|=((datacells&0x0400)>>5);
    data|=((datacells&0x0100)>>4);
    data|=((datacells&0x0040)>>3);
    data|=((datacells&0x0010)>>2);
    data|=((datacells&0x0004)>>1);
    data|=((datacells&0x0001)>>0);

    switch (state)
    {
      case SYNC:
        // Detect standard FM address marks
        switch (datacells)
        {
          case 0xf77a: // d7 fc
            if (debug)
              printf("\n[%lx] Index Address Mark\n", datapos);
            blocktype=data;
            bitlen=0;
            state=SYNC;

            // Clear IDAM cache, although I've not seen IAM on Acorn DFS
            idamtrack=-1;
            idamhead=-1;
            idamsector=-1;
            idamlength=-1;
            break;

          case 0xf57e: // c7 fe
            if (debug)
              printf("\n[%lx] ID Address Mark\n", datapos);
            blocktype=data;
            blocksize=6+1;
            bitlen=0;
            bitstream[bitlen++]=data;
            idpos=datapos;
            state=ADDR;

            // Clear IDAM cache incase previous was good and this one is bad
            idamtrack=-1;
            idamhead=-1;
            idamsector=-1;
            idamlength=-1;
            break;

          case 0xf56f: // c7 fb
            if (debug)
              printf("\n[%lx] Data Address Mark, distance from ID %lx\n", datapos, datapos-idpos);

            // Don't process if don't have a valid preceding IDAM
            if ((idamtrack!=-1) && (idamhead!=-1) && (idamsector!=-1) && (idamlength!=-1))
            {
              blocktype=data;
              bitlen=0;
              bitstream[bitlen++]=data;
              blockpos=datapos;
              state=DATA;
            }
            else
            {
              blocktype=BLOCKNULL;
              bitlen=0;
              state=SYNC;
            }
            break;

          case 0xf56a: // c7 f8
            if (debug)
              printf("\n[%lx] Deleted Data Address Mark, distance from ID %lx\n", datapos, datapos-idpos);

            // Don't process if don't have a valid preceding IDAM
            if ((idamtrack!=-1) && (idamhead!=-1) && (idamsector!=-1) && (idamlength!=-1))
            {
              blocktype=data;
              bitlen=0;
              bitstream[bitlen++]=data;
              blockpos=datapos;
              state=DATA;
            }
            else
            {
              blocktype=BLOCKNULL;
              bitlen=0;
              state=SYNC;
            }
            break;

          default:
            // No matching address marks
            break;
        }
        break;

      case ADDR:
        // Keep reading until we have the whole block in bitsteam[]
        bitstream[bitlen++]=data;

        if (bitlen==blocksize)
        {
          idblockcrc=calc_crc(&bitstream[0], bitlen-2);
          bitstreamcrc=(((unsigned int)bitstream[bitlen-2]<<8)|bitstream[bitlen-1]);
          dataCRC=(idblockcrc==bitstreamcrc)?GOODDATA:BADDATA;

          if (debug)
          {
            printf("Track %d (%d) ", bitstream[1], hw_currenttrack);
            printf("Head %d (%d) ", bitstream[2], hw_currenthead);
            printf("Sector %d ", bitstream[3]);
            printf("Data size %d ", bitstream[4]);
            printf("CRC %.2x%.2x", bitstream[5], bitstream[6]);

            if (dataCRC==GOODDATA)
              printf(" OK\n");
            else
              printf(" BAD (%.4x)\n", idblockcrc);
          }

          if (dataCRC==GOODDATA)
          {
            // Record IDAM values
            idamtrack=bitstream[1];
            idamhead=bitstream[2];
            idamsector=bitstream[3];
            idamlength=bitstream[4];

            // Record last known good IDAM values for this track
            lasttrack=idamtrack;
            lasthead=idamhead;
            lastsector=idamsector;
            lastlength=idamlength;

            // Sanitise data block length
            switch(idamlength)
            {
              case 0x00: // 128
              case 0x01: // 256
              case 0x02: // 512
              case 0x03: // 1024
              case 0x04: // 2048
              case 0x05: // 4096
              case 0x06: // 8192
              case 0x07: // 16384
                blocksize=(128<<idamlength)+3;
                break;

              default:
                if (debug)
                  printf("Invalid record length %.2x\n", idamlength);

                // Default to DFS standard sector size + (blocktype + (2 x crc))
                blocksize=DFS_SECTORSIZE+3;
                break;
            }
          }
          else
          {
            // IDAM failed CRC, ignore following data block (for now)
            blocksize=0;

            // Clear IDAM cache
            idamtrack=-1;
            idamhead=-1;
            idamsector=-1;
            idamlength=-1;
          }

          state=SYNC;
          blocktype=BLOCKNULL;
        }
        break;

      case DATA:
        // Keep reading until we have the whole block in bitsteam[]
        bitstream[bitlen++]=data;

        if (bitlen==blocksize)
        {
          // All the bytes for this "data" block have been read, so process them

          // Calculate CRC
          datablockcrc=calc_crc(&bitstream[0], bitlen-2);
          bitstreamcrc=(((unsigned int)bitstream[bitlen-2]<<8)|bitstream[bitlen-1]);

          if (debug)
            printf("  %.2x CRC %.4x", blocktype, bitstreamcrc);

          dataCRC=(datablockcrc==bitstreamcrc)?GOODDATA:BADDATA;

          // Report and save if the CRC matches
          if (dataCRC==GOODDATA)
          {
            if (debug)
              printf(" OK [%lx]\n", datapos);

            diskstore_addsector(hw_currenttrack, hw_currenthead, idamtrack, idamhead, idamsector, idamlength, idblockcrc, blocktype, blocksize-3, &bitstream[1], datablockcrc);
          }
          else
          {
            if (debug)
              printf(" BAD (%.4x)\n", datablockcrc);
          }

          // Do a catalogue if we haven't already and sector 00 and 01 have been read correctly for this side
          if ((info==0) && (diskstore_findhybridsector(0, hw_currenthead, 0)!=NULL) && (diskstore_findhybridsector(0, hw_currenthead, 1)!=NULL))
          {
            printf("\nSide : %d\n", hw_currenthead);
            dfs_showinfo(diskstore_findhybridsector(0, hw_currenthead, 0), diskstore_findhybridsector(0, hw_currenthead, 1));
            info++;
            printf("\n");
          }

          // Require subsequent data blocks to have a valid ID block first
          idamtrack=-1;
          idamhead=-1;
          idamsector=-1;
          idamlength=-1;

          idpos=0;

          blocktype=BLOCKNULL;
          blocksize=0;
          state=SYNC;
        }
        break;

      default:
        // Unknown state, should never happen
        blocktype=BLOCKNULL;
        blocksize=0;
        state=SYNC;
        break;
    }

    // If waiting for sync, then keep width at 16 bits and continue shifting/adding new bits
    if (state==SYNC)
      bits=16;
    else
      bits=0;
  }
}

void process(int attempt)
{
  int j,k, pos;
  char level,bi=0;
  unsigned char c, clock, data;
  int count;
  unsigned long avg[50];
  int bitwidth=0;
  float defaultwindow;
  int bucket1, bucket01;

  state=SYNC;

  defaultwindow=((float)BITCELL/((float)1/((float)SAMPLERATE/(float)USINSECOND)));
  bucket1=defaultwindow+(defaultwindow/2);
  bucket01=(defaultwindow*2)+(defaultwindow/2);

  level=(spibuffer[0]&0x80)>>7;
  bi=level;
  count=0;
  datacells=0;

  // Initialise last sector IDAM to invalid
  idamtrack=-1;
  idamhead=-1;
  idamsector=-1;
  idamlength=-1;

  // Initialise last known good IDAM to invalid
  lasttrack=-1;
  lasthead=-1;
  lastsector=-1;
  lastlength=-1;

  blocksize=0;
  blocktype=BLOCKNULL;
  idpos=0;

  for (datapos=0;datapos<SPIBUFFSIZE; datapos++)
  {
    c=spibuffer[datapos];

    // Fill in missing sample between SPI bytes
    count++;

    for (j=0; j<8; j++)
    {
      bi=((c&0x80)>>7);

      count++;

      if (bi!=level)
      {
        level=1-level;

        // Look for rising edge
        if (level==1)
        {
          if (count<bucket1)
          {
            addbit(1);
          }
          else
          if (count<bucket01)
          {
            addbit(0);
            addbit(1);
          }
          else
          {
            // This shouldn't happen in single-density FM encoding
            addbit(0);
            addbit(0);
            addbit(1);
          }

          // Reset sample counter
          count=0;
        }
      }

      c=c<<1;
    }
  }
}

// Stop the motor and tidy up upon exit
void exitFunction()
{
  printf("Exit function\n");
  hw_done();

  if (spibuffer!=NULL)
  {
    free(spibuffer);
    spibuffer=NULL;
  }
}

// Handle signals by stopping motor and tidying up
void sig_handler(const int sig)
{
  if (sig==SIGSEGV)
    printf("SEG FAULT\n");

  hw_done();
  exit(0);
}

void showargs(const char *exename)
{
  fprintf(stderr, "%s - Floppy disk raw flux capture and processor\n\n", exename);
  fprintf(stderr, "Syntax : ");
#ifdef NOPI
  fprintf(stderr, "[-i inputfile] ");
#endif
  fprintf(stderr, "[[-c] | [-o outputfile]] [-v]\n");
}

int main(int argc,char **argv)
{
  int argn=0;
  unsigned int i, j;
  unsigned char retry, side, drivestatus;

  // Check we have some arguments
  if (argc==1)
  {
    showargs(argv[0]);
    return 1;
  }

  // Process command line arguments
  while (argn<argc)
  {
    if (strcmp(argv[argn], "-v")==0)
    {
      debug=1;
    }
    else
    if (strcmp(argv[argn], "-c")==0)
    {
      capturetype=DISKCAT;
    }
    else
    if ((strcmp(argv[argn], "-o")==0) && ((argn+1)<argc))
    {
      ++argn;

      if (strstr(argv[argn], ".ssd")!=NULL)
      {
        sides=1;

        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGESSD;
        }
        else
          printf("Unable to save ssd image\n");
      }
      else
      if (strstr(argv[argn], ".dsd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEDSD;
        }
        else
          printf("Unable to save dsd image\n");
      }
      else
      if (strstr(argv[argn], ".fsd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEFSD;
        }
        else
          printf("Unable to save fsd image\n");
      }
      else
      if (strstr(argv[argn], ".raw")!=NULL)
      {
        rawdata=fopen(argv[argn], "w+");
        if (rawdata==NULL)
        {
          capturetype=DISKRAW;
          outputtype=IMAGERAW;
        }
        else
          printf("Unable to save rawdata\n");
      }
    }
#ifdef NOPI
    else
    if ((strcmp(argv[argn], "-i")==0) && ((argn+1)<argc))
    {
      ++argn;

      if (!hw_init(argv[argn]))
      {
        fprintf(stderr, "Failed virtual hardware init\n");
        return 4;
      }
    }
#endif

    ++argn;
  }

  // Make sure we have something to do
  if (capturetype==DISKNONE)
  {
    showargs(argv[0]);
    return 1;
  }

  diskstore_init();

#ifndef NOPI
  if (geteuid() != 0)
  {
    fprintf(stderr,"Must be run as root\n");
    return 2;
  }
#endif

  // Allocate memory for SPI buffer
  spibuffer=malloc(SPIBUFFSIZE);
  if (spibuffer==NULL)
  {
    fprintf(stderr, "\n");
    return 3;
  }

  printf("Start\n");

#ifndef NOPI
  if (!hw_init())
  {
    fprintf(stderr, "Failed hardware init\n");
    return 4;
  }
#endif

  // Install signal handlers to make sure motor is stopped
  atexit(exitFunction);
  signal(SIGINT, sig_handler); // Ctrl-C
  signal(SIGSEGV, sig_handler); // Seg fault
  signal(SIGTERM, sig_handler); // Termination request

  drivestatus=hw_detectdisk();

  if (drivestatus==HW_NODRIVE)
  {
    fprintf(stderr, "Failed to detect drive\n");
    return 5;
  }

  if (drivestatus==HW_NODISK)
  {
    fprintf(stderr, "Failed to detect disk in drive\n");
    return 6;
  }

  // Select drive, depending on jumper
  hw_driveselect();

  // Start MOTOR
  hw_startmotor();

  // Wait for motor to get up to speed
  hw_sleep(1);

  // Determine if head is at track 00
  if (hw_attrackzero())
    printf("Starting at track zero\n");
  else
    printf("Starting not at track zero\n");

  // Determine if disk in drive is write protected
  if (hw_writeprotected())
    printf("Disk is write-protected\n");
  else
    printf("Disk is writeable\n");

  // Start off assuming an 80 track disk in 80 track drive
  disktracks=HW_MAXTRACKS;
  drivetracks=HW_MAXTRACKS;

  // Try to determine what type of disk is in what type of drive

  // Seek to track 2
  hw_seektotrack(2);

  // Select upper side
  hw_sideselect(0);

  // Wait for a bit after seek to allow drive speed to settle
  hw_sleep(1);

  // Sample track
  hw_waitforindex();
  hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);
  process(99);
  // Check readability
  if ((lasttrack==-1) || (lasthead==-1) || (lastsector==-1) || (lastlength==-1))
  {
    printf("No valid FM sector IDs found\n");
  }
  else
  {
    unsigned char othertrack=lasttrack;
    unsigned char otherhead=lasthead;
    unsigned char othersector=lastsector;
    unsigned char otherlength=lastlength;

    // Select lower side
    hw_sideselect(1);

    // Wait for a bit after head switch to allow drive to settle
    hw_sleep(1);

    // Sample track
    hw_waitforindex();
    hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);
    process(99);
    // Check readability
    if ((lasttrack==-1) || (lasthead==-1) || (lastsector==-1) || (lastlength==-1))
    {
      // Only upper side was readable
        printf("Single-sided disk detected\n");
    }
    else
    {
      // Only mark as double-sided when not using single-sided output
      if ((outputtype!=IMAGEFSD) && (outputtype!=IMAGESSD))
      {
        // Both sides readable
        sides=2;
      }

      // If IDAM shows same head, then double-sided separate
      if (lasthead==otherhead)
        printf("Double-sided with separate sides disk detected\n");
      else
        printf("Double-sided disk detected\n");
    }

    // If IDAM cylinder shows 2 then correct stepping
    if (othertrack==2)
    {
      printf("Correct drive stepping for this disk\n");
    }
    else
    {
      // If IDAM cylinder shows 1 then 40 track in 80 track drive
      if (othertrack==1)
      {
        printf("40 track disk detected in 80 track drive\n");

        // Enable double stepping
        hw_stepping=HW_DOUBLESTEPPING;

        disktracks=40;
        drivetracks=80;
      }

      // If IDAM cylinder shows 4 then 80 track in 40 track drive
      if (othertrack==4)
      {
        printf("80 track disk detected in 40 track drive\n*** Unable to image this disk in this drive ***\n");

        disktracks=80;
        drivetracks=40;
      }
    }
  }

  // Start at track 0
  hw_seektotrackzero();

  // Loop through the tracks
  for (i=0; i<drivetracks; i++)
  {
    hw_seektotrack(i);

    // Process all available disk sides (heads)
    for (side=0; side<sides; side++)
    {
      // Request a directory listing for this side of the disk
      if (i==0) info=0;

      // Select the correct side
      hw_sideselect(side);

      // Retry the capture if any sectors are missing
      for (retry=0; retry<RETRIES; retry++)
      {
        // Wait for a bit after seek/head select to allow drive speed to settle
        hw_sleep(1);

        if (retry==0)
          printf("Sampling data for track %.2X head %.2x\n", i, side);

        // Wait for index rising edge prior to sampling to align as much as possible with index hole
        // Values seen on a scope are 200ms between pulses of 4.28ms width
        hw_waitforindex();

        // Sampling data
        hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);

        // Process the raw sample data to extract FM encoded data
        if (capturetype!=DISKRAW)
        {
          process(retry);

          // Determine if we have successfully read the whole track
          if ((diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 0)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 1)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 2)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 3)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 4)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 5)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 6)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 7)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 8)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 9)!=NULL))
            break;

          printf("Retry attempt %d, sectors ", retry+1);
          for (j=0; j<DFS_SECTORSPERTRACK; j++)
            if (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, j)==NULL) printf("%.2d ", j);
          printf("\n");
        }
        else
          break; // No retries in RAW mode
      } // retry loop

      if (capturetype!=DISKRAW)
      {
        // If we're on side 1 track 1 and no second catalogue found, then assume single sided
        if ((side==1) && (i==1))
        {
          if (info==0)
          {
            sides=1;

            printf("Single sided disk\n");
          }
          else
            printf("Double sided disk\n");
        }

        if (retry>=RETRIES)
          printf("I/O error reading head %d track %d\n", hw_currenthead, i);
      }
      else
      {
        // Write the raw sample data if required
        if (rawdata!=NULL)
          fwrite(spibuffer, 1, SPIBUFFSIZE, rawdata);
      }
    } // side loop

    // If we're only doing a catalogue, then don't read any more tracks
    if (capturetype==DISKCAT)
      break;

    // If this is an 80 track disk in a 40 track drive, then don't go any further
    if ((drivetracks==40) && (disktracks==80))
      break;
  } // track loop

  // Return the disk head to track 0 following disk imaging
  hw_seektotrackzero();

  printf("Finished\n");

  // Stop the drive motor
  hw_stopmotor();

  // Write the data to disk image file (if required)
  if (diskimage!=NULL)
  {
    if (outputtype==IMAGEFSD)
    {
      fsd_write(diskimage, disktracks);
    }
    else
    {
      Disk_Sector *sec;
      unsigned char blanksector[DFS_SECTORSIZE];

      // Prepare a blank sector when no sector is found in store
      bzero(blanksector, sizeof(blanksector));

      for (i=0; ((i<HW_MAXTRACKS) && (i<disktracks)); i++)
      {
        for (j=0; j<DFS_SECTORSPERTRACK; j++)
        {
          // Write
          sec=diskstore_findhybridsector(i, 0, j);

          if ((sec!=NULL) && (sec->data!=NULL))
            fwrite(sec->data, 1, DFS_SECTORSIZE, diskimage);
          else
            fwrite(blanksector, 1, DFS_SECTORSIZE, diskimage);
        }

        // Write DSD interlaced as per BeebEm
        if (sides==2)
        {
          for (j=0; j<DFS_SECTORSPERTRACK; j++)
          {
            sec=diskstore_findhybridsector(i, 1, j);

            if ((sec!=NULL) && (sec->data!=NULL))
              fwrite(sec->data, 1, DFS_SECTORSIZE, diskimage);
            else
              fwrite(blanksector, 1, DFS_SECTORSIZE, diskimage);
          }
        }
      }
    }
  }

  // Close disk image files (if open)
  if (diskimage!=NULL) fclose(diskimage);
  if (rawdata!=NULL) fclose(rawdata);

  // Free memory allocated to SPI buffer
  if (spibuffer!=NULL)
  {
    free(spibuffer);
    spibuffer=NULL;
  }

  // Dump a list of valid sectors
  if (debug)
    diskstore_dumpsectorlist(DFS_MAXTRACKS);

  return 0;
}
