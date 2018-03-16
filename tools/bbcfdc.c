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
#include "rfi.h"
#include "fm.h"

// SPI read buffer size
#define SPIBUFFSIZE (1024*1024)

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

// Capture retries when not in raw mode
#define RETRIES 5

int debug=0;
int sides=1; // Default to single sided
int disktracks;
int drivetracks;
int capturetype=DISKNONE; // Default to no output
int outputtype=IMAGENONE; // Default to no image

unsigned char *spibuffer;
int info=0;

// Processing position within the SPI buffer
unsigned long datapos=0;

// File handles
FILE *diskimage=NULL;
FILE *rawdata=NULL;

// Used for reversing bit order within a byte
static unsigned char revlookup[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};
unsigned char reverse(unsigned char n)
{
   // Reverse the top and bottom nibble then swap them.
   return (revlookup[n&0x0f]<<4) | revlookup[n>>4];
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
  fprintf(stderr, "[-i input_rfi_file] ");
#endif
  fprintf(stderr, "[[-c] | [-o output_file]] [-r retries] [-s] [-v]\n");
}

int main(int argc,char **argv)
{
  int argn=0;
  unsigned int i, j;
  unsigned char retry, retries, side, drivestatus;
  int sortsectors=0;

  // Check we have some arguments
  if (argc==1)
  {
    showargs(argv[0]);
    return 1;
  }

  retries=RETRIES;

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
    if (strcmp(argv[argn], "-s")==0)
    {
      sortsectors=1;
    }
    else
    if ((strcmp(argv[argn], "-r")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      if (sscanf(argv[argn], "%3d", &retval)==1)
        retries=retval;
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
      if (strstr(argv[argn], ".rfi")!=NULL)
      {
        rawdata=fopen(argv[argn], "w+");
        if (rawdata!=NULL)
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

      if (!hw_init(argv[argn], HW_SPIDIV32))
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

  fm_init(debug);

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
  if (!hw_init(HW_SPIDIV32))
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
  fm_process(spibuffer, SPIBUFFSIZE, 99);
  // Check readability
  if ((fm_lasttrack==-1) || (fm_lasthead==-1) || (fm_lastsector==-1) || (fm_lastlength==-1))
  {
    printf("No valid FM sector IDs found\n");
  }
  else
  {
    unsigned char othertrack=fm_lasttrack;
    unsigned char otherhead=fm_lasthead;
    unsigned char othersector=fm_lastsector;
    unsigned char otherlength=fm_lastlength;

    // Select lower side
    hw_sideselect(1);

    // Wait for a bit after head switch to allow drive to settle
    hw_sleep(1);

    // Sample track
    hw_waitforindex();
    hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);
    fm_process(spibuffer, SPIBUFFSIZE, 99);
    // Check readability
    if ((fm_lasttrack==-1) || (fm_lasthead==-1) || (fm_lastsector==-1) || (fm_lastlength==-1))
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
      if (fm_lasthead==otherhead)
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

  // Write RFI header when doing raw capture
  if (capturetype==DISKRAW)
    rfi_writeheader(rawdata, drivetracks, sides, hw_samplerate, hw_writeprotected());

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
      for (retry=0; retry<retries; retry++)
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
          fm_process(spibuffer, SPIBUFFSIZE, retry);

#ifdef NOPI
          // No point in retrying when not using real hardware
          break;
#endif

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
        // Check if catalogue has been done
        if (info==0)
        {
          Disk_Sector *cat1;
          Disk_Sector *cat2;

          // Search for catalogue sectors
          cat1=diskstore_findhybridsector(0, hw_currenthead, 0);
          cat2=diskstore_findhybridsector(0, hw_currenthead, 1);

          // If they were found and they appear to be DFS catalogue then do a catalogue
          if ((cat1!=NULL) && (cat2!=NULL) && (dfs_validcatalogue(cat1, cat2)))
          {
            printf("\nSide : %d\n", hw_currenthead);
            dfs_showinfo(diskstore_findhybridsector(0, hw_currenthead, 0), diskstore_findhybridsector(0, hw_currenthead, 1));
            info++;
            printf("\n");
          }
        }

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

        if (retry>=retries)
          printf("I/O error reading head %d track %d\n", hw_currenthead, i);
      }
      else
      {
        // Write the raw sample data if required
        if (rawdata!=NULL)
          rfi_writetack(rawdata, i, side, hw_measurerpm(), "rle", spibuffer, SPIBUFFSIZE);
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

  // Check if sectors have been requested to be sorted
  if (sortsectors)
    diskstore_sortsectors();

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
