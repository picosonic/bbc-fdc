#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "hardware.h"
#include "diskstore.h"
#include "dfi.h"
#include "scp.h"
#include "adfs.h"
#include "amigamfm.h"
#include "applegcr.h"
#include "dfs.h"
#include "dos.h"
#include "fsd.h"
#include "teledisk.h"
#include "rfi.h"
#include "mod.h"
#include "fm.h"
#include "mfm.h"
#include "gcr.h"
#include "pll.h"

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
#define IMAGEDFI 5
#define IMAGEIMG 6
#define IMAGETD0 7
#define IMAGESCP 8
#define IMAGESDD 9
#define IMAGEDDD 10

// Used for values which can be overriden
#define AUTODETECT -1

// Capture retries when not in raw mode
#define RETRIES 5

// Number of rotations to cature per track
#define ROTATIONS 3

int debug=0;
int summary=0;
int catalogue=0;
int sides=AUTODETECT;
int disktracks=AUTODETECT;
int drivetracks=AUTODETECT;
int capturetype=DISKNONE; // Default to no output
int outputtype=IMAGENONE; // Default to no image

unsigned char *samplebuffer=NULL;
unsigned char *flippybuffer=NULL;
unsigned long samplebuffsize;
int flippy=0;
int info=0;
int layout=0;
int sidetoread=AUTODETECT;
int usepll=0;

// Processing position within the SPI buffer
unsigned long datapos=0;

// File handles
FILE *diskimage=NULL;
FILE *rawdata=NULL;
FILE *csvhandle=NULL;

int sectorspertrack=AUTODETECT;
int totalsectors=0;

// Used for reversing bit order within a byte
static unsigned char revlookup[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};
unsigned char reverse(unsigned char n)
{
   // Reverse the top and bottom nibble then swap them.
   return (revlookup[n&0x0f]<<4) | revlookup[n>>4];
}

// Used for flipping the bits in a raw sample buffer
void fillflippybuffer(const unsigned char *rawdata, const unsigned long rawlen)
{
  if (flippybuffer==NULL)
    flippybuffer=malloc(rawlen);

  if (flippybuffer!=NULL)
  {
    unsigned long em;

    for (em=0; em<rawlen; em++)
      flippybuffer[rawlen-em]=reverse(rawdata[em]);
  }
}

// Stop the motor and tidy up upon exit
void exitFunction()
{
  printf("Exit function\n");
  hw_done();

  if (samplebuffer!=NULL)
  {
    free(samplebuffer);
    samplebuffer=NULL;
  }

  if (flippybuffer!=NULL)
  {
    free(flippybuffer);
    flippybuffer=NULL;
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
  fprintf(stderr, "[-c] [[-ss [0|1]]|[-ds]] [-o output_file] [-spidiv spi_divider] [-r retries] [-sort] [-summary] [-l] [-sectors sectors_per_track] [-csv] [-tmax maxtracks] [-dblstep] [-title \"Title\"] [-v]\n");
}

int main(int argc,char **argv)
{
  int argn=0;
  unsigned int i, j, rate;
  unsigned char retry, retries, side, drivestatus;
  int sortsectors=0;
  int missingsectors=0;
  int csv=0;
  char modulation=AUTODETECT;
#ifdef NOPI
  char *samplefile;
#endif
  char *outputfilename=NULL;
  char title[100];

  // Check we have some arguments
  if (argc==1)
  {
    showargs(argv[0]);
    return 1;
  }

  // Set some defaults
  retries=RETRIES;
  rate=HW_SPIDIV32;
#ifdef NOPI
  samplefile=NULL;
#endif
  title[0]=0;

  printf("Compiled on hardware with ");
#ifdef HW_BCM2835
  printf("BCM2835");
#endif
#ifdef HW_BCM2836
  printf("BCM2836");
#endif
#ifdef HW_BCM2837
  printf("BCM2837");
#endif
#ifdef HW_BCM2711
  printf("BCM2711");
#endif
#ifdef HW_BCM2709
  printf("BCM2709");
#endif
#ifdef HW_UNKNOWN
  printf("UNKNOWN");
#endif
  printf(" processor\n");

  // Process command line arguments
  while (argn<argc)
  {
    if (strcmp(argv[argn], "-v")==0)
    {
      debug=1;
      catalogue=1;
    }
    else
    if (strcmp(argv[argn], "-c")==0)
    {
      catalogue=1;

      if (capturetype==DISKNONE)
        capturetype=DISKCAT;
    }
    else
    if (strcmp(argv[argn], "-l")==0)
    {
      layout=1;
    }
    else
    if (strcmp(argv[argn], "-sort")==0)
    {
      sortsectors=1;
    }
    else
    if (strcmp(argv[argn], "-summary")==0)
    {
      summary=1;
    }
    else
    if ((strcmp(argv[argn], "-sectors")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      if (sscanf(argv[argn], "%3d", &retval)==1)
        sectorspertrack=retval;
    }
    else
    if (strcmp(argv[argn], "-ds")==0)
    {
      printf("Forced double-sided capture\n");

      // Request double-sided
      sides=2;
    }
    else
    if (strcmp(argv[argn], "-ss")==0)
    {
      printf("Forced single-sided capture\n");

      // Request single-sided
      sides=1;
      sidetoread=0;

      if ((argn+1)<argc)
      {
        int retval;

        ++argn;

        if (sscanf(argv[argn], "%3d", &retval)==1)
        {
          sidetoread=retval;

          // Prevent invalid value for side to read
          if (sidetoread>1) sidetoread=1;
        }
        else
          --argn;
      }
    }
    else
    if (strcmp(argv[argn], "-dblstep")==0)
    {
      printf("Forced double stepped capture\n");

      // Request double stepping
      hw_stepping=HW_DOUBLESTEPPING;

      disktracks=40;
      drivetracks=80;
    }
    else
    if (strcmp(argv[argn], "-csv")==0)
    {
      printf("Failed sector CSV report requested\n");

      // Request CSV output
      csv=1;
    }
    else
    if (strcmp(argv[argn], "-pll")==0)
    {
      printf("Running with PLL processing\n");

      // Request PLL processing
      usepll=1;

      // Check for further parameters
      if ((argn+2)<argc)
      {
        float retval;

        ++argn;

        if (sscanf(argv[argn], "%f", &retval)==1)
        {
          pll_periodadjust=(retval/100);

          ++argn;

          if (sscanf(argv[argn], "%f", &retval)==1)
          {
            pll_phaseadjust=(retval/100);
          }
          else
            --argn;
        }
        else
          --argn;
      }

      printf("  PLL period adjustment %.0f%%\n", pll_periodadjust*100);
      printf("  PLL phase adjustment %.0f%%\n", pll_phaseadjust*100);
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
    if ((strcmp(argv[argn], "-tmax")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      // Override the maximum number of drive tracks
      if (sscanf(argv[argn], "%3d", &retval)==1)
      {
        hw_setmaxtracks(retval);
        printf("Override maximum number of drive tracks to %d\n", retval);
      }
    }
    else
    if ((strcmp(argv[argn], "-title")==0) && ((argn+1)<argc))
    {
      ++argn;

      // Override the disk title for certain disk image formats
      if (strlen(argv[argn])<(sizeof(title)-1))
      {
        strcpy(title, argv[argn]);
        printf("Override disk title to \"%s\"\n", title);
      }
    }
    else
    if ((strcmp(argv[argn], "-spidiv")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      if (sscanf(argv[argn], "%4d", &retval)==1)
      {
        switch (retval)
        {
          case 16:
          case 32:
          case 64:
            rate=retval;
            printf("Setting SPI divider to %u\n", rate);
            break;

          default:
            fprintf(stderr, "Invalid SPI divider\n");
            return 7;
            break;
        }
      }
    }
    else
    if ((strcmp(argv[argn], "-o")==0) && ((argn+1)<argc))
    {
      ++argn;
      outputfilename=argv[argn];

      if (strstr(argv[argn], ".ssd")!=NULL)
      {
        // .SSD is single sided
        sides=1;
        if (sidetoread==AUTODETECT)
          sidetoread=0;

        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGESSD;

          // DFS sectors per track - standard density DFS is 10 sectors
          if (sectorspertrack==AUTODETECT)
            sectorspertrack=DFS_SECTORSPERTRACK;
        }
        else
          printf("Unable to save ssd image\n");
      }
      else
      if (strstr(argv[argn], ".sdd")!=NULL)
      {
        // .SDD is single sided
        sides=1;
        if (sidetoread==AUTODETECT)
          sidetoread=0;

        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGESDD;

          // Double density DFS with e.g. Solidisk DDFS and Watford
          // is 16 sectors per track. Opus went to 18.
	  if (sectorspertrack==AUTODETECT)
            sectorspertrack=DFS_DDSECTORSPERTRACK;
        }
        else
          printf("Unable to save sdd image\n");
      }
      else
      if (strstr(argv[argn], ".dsd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEDSD;

          // DFS sectors per track - standard density DFS is 10 sectors
          if (sectorspertrack==AUTODETECT)
            sectorspertrack=DFS_SECTORSPERTRACK;
        }
        else
          printf("Unable to save dsd image\n");
      }
      else
      if (strstr(argv[argn], ".ddd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEDDD;

          // Double density DFS with e.g. Solidisk DDFS and Watford
          // is 16 sectors per track. Opus went to 18.
	  if (sectorspertrack==AUTODETECT)
            sectorspertrack=DFS_DDSECTORSPERTRACK;
        }
        else
          printf("Unable to save ddd image\n");
      }
      else
      if (strstr(argv[argn], ".fsd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEFSD;

          // Default to single sided when dual sided not specified
          if (sides==AUTODETECT)
          {
            sides=1;

            if (sidetoread==AUTODETECT)
              sidetoread=0;
          }
        }
        else
          printf("Unable to save fsd image\n");
      }
      else
      if ((strstr(argv[argn], ".img")!=NULL) || (strstr(argv[argn], ".adf")!=NULL))
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEIMG;
        }
        else
          printf("Unable to save img image\n");
      }
      else
      if (strstr(argv[argn], ".td0")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGETD0;
        }
        else
          printf("Unable to save td0 image\n");
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
      else
      if (strstr(argv[argn], ".dfi")!=NULL)
      {
        rawdata=fopen(argv[argn], "w+");
        if (rawdata!=NULL)
        {
          capturetype=DISKRAW;
          outputtype=IMAGEDFI;
        }
        else
          printf("Unable to save dfi image\n");
      }
      else
      if (strstr(argv[argn], ".scp")!=NULL)
      {
        rawdata=fopen(argv[argn], "w+");
        if (rawdata!=NULL)
        {
          capturetype=DISKRAW;
          outputtype=IMAGESCP;
        }
        else
          printf("Unable to save scp image\n");
      }
    }
#ifdef NOPI
    else
    if ((strcmp(argv[argn], "-i")==0) && ((argn+1)<argc))
    {
      ++argn;

      samplefile=argv[argn];

    }
#endif

    ++argn;
  }

  // Create a csv file with the same name as the output file
  // but with a .csv extension
  if ((csv!=0) && (outputfilename!=NULL))
  {
     char *buffer=NULL;

     buffer=malloc((strlen(outputfilename) + 5) * sizeof(char));
     if (buffer!=NULL)
     {
       strcpy(buffer, outputfilename);
       strcat(buffer, ".csv");

       csvhandle=fopen(buffer, "w+");

       free(buffer);
     }

     if (csvhandle==NULL)
       printf("Unable to create CSV report file\n");
  }

#ifdef NOPI
    if ((samplefile==NULL) || (!hw_init(samplefile, rate)))
    {
      fprintf(stderr, "Failed virtual hardware init\n");
      return 4;
    }
#endif

  // Make sure we have something to do
  if (capturetype==DISKNONE)
  {
    showargs(argv[0]);
    return 1;
  }

  diskstore_init(debug, usepll);

  mod_init(debug);

  PLL_init();

#ifndef NOPI
  if (geteuid() != 0)
  {
    fprintf(stderr,"Must be run as root\n");
    return 2;
  }
#endif

#ifndef NOPI
  if (!hw_init(rate))
  {
    fprintf(stderr, "Failed hardware init\n");
    return 4;
  }
#endif

  // Allocate memory for SPI buffer
  samplebuffsize=((hw_samplerate/HW_ROTATIONSPERSEC)/BITSPERBYTE)*ROTATIONS;
  samplebuffer=malloc(samplebuffsize);
  if (samplebuffer==NULL)
  {
    fprintf(stderr, "\n");
    return 3;
  }

  printf("Start with %lu byte sample buffer\n", samplebuffsize);

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

  // Start off assuming an 80 track disk in 80 track drive, unless overridden
  if (disktracks==AUTODETECT)
    disktracks=hw_maxtracks;

  if (drivetracks==AUTODETECT)
    drivetracks=hw_maxtracks;

  // Try to determine what type of disk is in what type of drive

  // Seek to track 2
  hw_seektotrack(2);

  if (sidetoread==AUTODETECT)
  {
    // Select lower side
    hw_sideselect(0);
  }
  else
    hw_sideselect(sidetoread);

  // Wait for a bit after seek to allow drive speed to settle
  hw_sleep(1);

  // Sample track
  hw_samplerawtrackdata((char *)samplebuffer, samplebuffsize);
  mod_process(samplebuffer, samplebuffsize, 99, usepll);

  // Check readability
  if ((fm_lasttrack==-1) && (fm_lasthead==-1) && (fm_lastsector==-1) && (fm_lastlength==-1))
    printf("No FM sector IDs found\n");
  else
    modulation=MODFM;

  if ((mfm_lasttrack==-1) && (mfm_lasthead==-1) && (mfm_lastsector==-1) && (mfm_lastlength==-1))
    printf("No MFM sector IDs found\n");
  else
    modulation=MODMFM;

  if ((gcr_lasttrack==-1) && (gcr_lastsector==-1))
    printf("No C64 GCR sector IDs found\n");
  else
    modulation=MODGCR;

  if ((applegcr_lasttrack==-1) && (applegcr_lastsector==-1))
    printf("No Apple GCR sector IDs found\n");
  else
    modulation=MODAPPLEGCR;

  if (modulation!=AUTODETECT)
  {
    int othertrack=-1;
    int otherhead=-1;
    int othersector=-1;

    // Check if it was FM sectors found
    if ((fm_lasttrack!=-1) && (fm_lasthead!=-1) && (fm_lastsector!=-1) && (fm_lastlength!=-1))
    {
      othertrack=fm_lasttrack;
      otherhead=fm_lasthead;
      othersector=fm_lastsector;
    }

    // Check if it was MFM sectors found
    if ((mfm_lasttrack!=-1) && (mfm_lasthead!=-1) && (mfm_lastsector!=-1) && (mfm_lastlength!=-1))
    {
      othertrack=mfm_lasttrack;
      otherhead=mfm_lasthead;
      othersector=mfm_lastsector;
    }

    // Check if it was C64 GCR sectors found
    if ((gcr_lasttrack!=-1) && (gcr_lastsector!=-1))
    {
      othertrack=gcr_lasttrack;
      othersector=gcr_lastsector;
    }

    // Check if it was Apple GCR sectors found
    if ((applegcr_lasttrack!=-1) && (applegcr_lastsector!=-1))
    {
      othertrack=applegcr_lasttrack;
      othersector=applegcr_lastsector;
    }

    // Only look for data on other side if user hasn't specified number of sides to capture
    if (sides==AUTODETECT)
    {
      // Select upper side
      hw_sideselect(1);

      // Wait for a bit after head switch to allow drive to settle
      hw_sleep(1);

      // Sample track
      hw_samplerawtrackdata((char *)samplebuffer, samplebuffsize);
      mod_process(samplebuffer, samplebuffsize, 99, usepll);

      // Check for flippy disk
      if ((fm_lasttrack==-1) && (fm_lasthead==-1) && (fm_lastsector==-1) && (fm_lastlength==-1)
         && (mfm_lasttrack==-1) && (mfm_lasthead==-1) && (mfm_lastsector==-1) && (mfm_lastlength==-1)
         && (gcr_lasttrack==-1) && (gcr_lastsector==-1)
         && (applegcr_lasttrack==-1) && (applegcr_lastsector==-1))
      {
        fillflippybuffer(samplebuffer, samplebuffsize);

        if (flippybuffer!=NULL)
          mod_process(flippybuffer, samplebuffsize, 99, usepll);

        if ((fm_lasttrack!=-1) || (fm_lasthead!=-1) || (fm_lastsector!=-1) || (fm_lastlength!=-1)
           || (mfm_lasttrack!=-1) || (mfm_lasthead!=-1) || (mfm_lastsector!=-1) || (mfm_lastlength!=-1)
           || (gcr_lasttrack!=-1) || (gcr_lastsector!=-1)
           || (applegcr_lasttrack!=-1) || (applegcr_lastsector!=-1))
        {
          printf("Flippy disk detected\n");
          flippy=1;
        }
      }

      // Check readability
      if ((fm_lasttrack==-1) && (fm_lasthead==-1) && (fm_lastsector==-1) && (fm_lastlength==-1)
         && (mfm_lasttrack==-1) && (mfm_lasthead==-1) && (mfm_lastsector==-1) && (mfm_lastlength==-1)
         && (gcr_lasttrack==-1) && (gcr_lastsector==-1)
         && (applegcr_lasttrack==-1) && (applegcr_lastsector==-1))
      {
        // Only lower side was readable
        printf("Single-sided disk assumed, only found data on side 0\n");

        sides=1;
      }
      else
      {
        // If IDAM shows same head, then double-sided separate
        if ((fm_lasthead==otherhead) || (mfm_lasthead==otherhead))
          printf("Double-sided with separate sides disk detected\n");
        else
          printf("Double-sided disk detected\n");

        // Only mark as double-sided when not using single-sided output
        if ((outputtype!=IMAGESSD) && (outputtype!=IMAGESDD) && (outputtype!=IMAGEFSD))
          sides=2;
        else
          sides=1;
      }
    }

    // Determine if double stepping is required, if not already forced
    if (hw_stepping!=HW_DOUBLESTEPPING)
    {
      switch (othertrack)
      {
        case 1:
          // If IDAM cylinder shows 1 then 40 track disk in 80 track drive
          printf("40 track disk detected in 80 track drive, enabled double stepping\n");

          // Enable double stepping
          hw_stepping=HW_DOUBLESTEPPING;

          disktracks=40;
          drivetracks=80;
          break;

        case 2:
          // If IDAM cylinder shows 2 then correct stepping
          printf("Correct drive stepping for this disk and drive\n");
          break;

        case 4:
          // If IDAM cylinder shows 4 then 80 track in 40 track drive
          printf("80 track disk detected in 40 track drive\n*** Unable to fully image this disk in this drive ***\n");

          disktracks=80;
          drivetracks=40;
          break;

        default:
          // Unexpected value for track in IDAM
          if (othertrack!=-1)
            printf("Unexpected track id %d\n*** Maybe copy protection ***\n", (char)othertrack);
          break;
      }
    }
  }

  // Number of sides failed to autodetect and was not forced, so assume 2
  if (sides==AUTODETECT)
  {
    printf("Unable to detect known data on either side, capturing both sides\n");
    sides=2;
  }

  // Write header when doing raw capture
  if (capturetype==DISKRAW)
  {
    switch (outputtype)
    {
      case IMAGERAW:
        rfi_writeheader(rawdata, drivetracks, sides, hw_samplerate, hw_writeprotected());
        break;

      case IMAGEDFI:
        dfi_writeheader(rawdata);
        break;

      case IMAGESCP:
        scp_writeheader(rawdata, ROTATIONS, 0, (drivetracks/hw_stepping)*sides, hw_measurerpm(), sides, sidetoread==AUTODETECT?0:sidetoread);
        break;

      default:
        break;
    }
  }

  // Start at track 0
  hw_seektotrackzero();

  // Loop through the tracks
  for (i=0; i<(drivetracks/hw_stepping); i++)
  {
    hw_seektotrack(i);

    // Process all available disk sides (heads)
    for (side=0; side<sides; side++)
    {
      // Read the specified side if in single side read mode
      if ((sides==1) && (sidetoread!=AUTODETECT))
        side=sidetoread;

      // Select the correct side
      hw_sideselect(side);

      // Retry the capture if any sectors are missing
      for (retry=0; retry<retries; retry++)
      {
        // Wait for a bit after seek/head select to allow drive speed to settle
        hw_sleep(1);

        if (retry==0)
          printf("Sampling data for track %.2X head %.2x\n", i, side);

        // Sampling data
        hw_samplerawtrackdata((char *)samplebuffer, samplebuffsize);

        // Process the raw sample data to extract encoded data
        if (capturetype!=DISKRAW)
        {
          if ((flippy==0) || (side==0))
          {
            mod_process(samplebuffer, samplebuffsize, retry, usepll);
          }
          else
          {
            fillflippybuffer(samplebuffer, samplebuffsize);

            if (flippybuffer!=NULL)
              mod_process(flippybuffer, samplebuffsize, retry, usepll);
          }

#ifdef NOPI
          // No point in retrying when not using real hardware
          break;
#endif

          // Don't retry unless imaging DFS disks
          if ((outputtype!=IMAGEDSD) && (outputtype!=IMAGESSD) &&
              (outputtype!=IMAGEDDD) && (outputtype!=IMAGESDD) )
            break;
	
          // See if we have successfully read a full track
          int trackstatus=1;

          for (j=0; j<sectorspertrack; j++)
	  {
            if (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, j)==NULL)
	    {
              // Failed to read at least one track
              trackstatus=0;
            }
	  }

	  if(trackstatus==1) break;

          printf("Retry attempt %d, sectors ", retry+1);
          for (j=0; j<sectorspertrack; j++)
            if (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, j)==NULL) printf("%.2u ", j);
          printf("\n");
        }
        else
          break; // No retries in RAW mode
      } // retry loop

      if (capturetype!=DISKRAW)
      {
        // Check if catalogue has been done
        if ((info<sides) && (catalogue==1))
        {
          if (dfs_validcatalogue(hw_currenthead, &totalsectors))
          {
            printf("\nDetected DFS, side : %d\n", hw_currenthead);
            dfs_showinfo(hw_currenthead, disktracks, sectorspertrack==-1?DFS_SECTORSPERTRACK:sectorspertrack);
            info++;
            printf("\n");
          }
          else
          if ((i==0) && (side==0))
          {
            int adfs_format;

            adfs_format=adfs_validate();

            if (adfs_format!=ADFS_UNKNOWN)
            {
              printf("\nDetected ADFS-");
              switch (adfs_format)
              {
                case ADFS_S:
                  printf("S");
                  break;

                case ADFS_M:
                  printf("M");
                  break;

                case ADFS_L:
                  printf("L");
                  break;

                case ADFS_D:
                  printf("D");
                  break;

                case ADFS_E:
                  printf("E");
                  break;

                case ADFS_F:
                  printf("F");
                  break;

                case ADFS_EX:
                  printf("E+");
                  break;

                case ADFS_FX:
                  printf("F+");
                  break;

                case ADFS_G:
                  printf("G");
                  break;

                default:
                  break;
              }
              printf("\n");
              adfs_showinfo(adfs_format, disktracks, debug);
              info++;
              printf("\n");
            }
            else
            {
              if (dos_validate()!=DOS_UNKNOWN)
              {
                printf("\nDetected DOS\n\n");
                dos_showinfo(disktracks, debug);
                info++;
              }
              else
              {
                if (amigamfm_validate()!=AMIGA_UNKNOWN)
                {
                  printf("\nDetected Amiga DOS\n\n");
                  amigamfm_showinfo(disktracks, debug);
                  info++;
                }
                else
                  printf("\nUnknown logical disk format\n\n");
              }
            }
          }
        }

        if (retry>=retries)
          printf("I/O error reading head %d track %u\n", hw_currenthead, i);
      }
      else
      {
        // Write the raw sample data if required
        if (rawdata!=NULL)
        {
          unsigned char *rawbuffer=samplebuffer;

          // Handle flippy data
          if ((flippy==1) && (side==1))
          {
            fillflippybuffer(samplebuffer, samplebuffsize);

            if (flippybuffer!=NULL)
              rawbuffer=flippybuffer;
          }

          switch (outputtype)
          {
            case IMAGERAW:
              rfi_writetrack(rawdata, i, side, hw_measurerpm(), "rle", samplebuffer, samplebuffsize);
              break;

            case IMAGEDFI:
              dfi_writetrack(rawdata, i, side, rawbuffer, samplebuffsize, ROTATIONS);
              break;

            case IMAGESCP:
              scp_writetrack(rawdata, ((i/hw_stepping)*sides)+side, rawbuffer, samplebuffsize, ROTATIONS, hw_measurerpm());
              break;

            default:
              break;
          }
        }

        // Flush raw track data before moving on to any further tracks
        fflush(rawdata);
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

  // Determine how many tracks we actually had data on
  if ((disktracks==80) && (diskstore_maxtrack<79))
    disktracks=(diskstore_maxtrack+1);

  // Check if sectors have been requested to be sorted logically by track/head/sectorid
  if (sortsectors)
    diskstore_sortsectors(SORTBYID, ROTATIONS);
  else
    diskstore_sortsectors(SORTBYPOS, ROTATIONS);

  // Write the data to disk image file (if required)
  if (diskimage!=NULL)
  {
    if (outputtype==IMAGETD0)
    {
      // When no title set, try to use title from source disk
      if (title[0]==0)
      {
        // If they were found and they appear to be DFS catalogue then extract title
        if (dfs_validcatalogue(0, &sectorspertrack))
        {
          dfs_gettitle(0, title, sizeof(title));
        }
        else
        if (dos_validate()!=DOS_UNKNOWN)
        {
          dos_gettitle(title, sizeof(title));
        }
        else
        {
          int adfs_format;

          adfs_format=adfs_validate();

          if (adfs_format!=ADFS_UNKNOWN)
            adfs_gettitle(adfs_format, title, sizeof(title));
        }
      }

      // If no title or blank title, then use default
      if (title[0]==0)
        strcpy(title, "NO TITLE");

      td0_write(diskimage, disktracks, title, sides, sidetoread==AUTODETECT?0:sidetoread);
    }
    else
    if (outputtype==IMAGEFSD)
    {
      // When no title set, try to use title from source disk
      if (title[0]==0)
      {
        // If they were found and they appear to be DFS catalogue then extract title
        if (dfs_validcatalogue(0, &sectorspertrack))
        {
          dfs_gettitle(0, title, sizeof(title));
        }
        else
        if (dos_validate()!=DOS_UNKNOWN)
        {
          dos_gettitle(title, sizeof(title));
        }
        else
        {
          int adfs_format;

          adfs_format=adfs_validate();

          if (adfs_format!=ADFS_UNKNOWN)
            adfs_gettitle(adfs_format, title, sizeof(title));
        }
      }

      // If no title or blank title, then use default
      if (title[0]==0)
        strcpy(title, "NO TITLE");

      fsd_write(diskimage, disktracks, title, sides, sidetoread==AUTODETECT?0:sidetoread);
    }
    else
    if ((outputtype==IMAGEDSD) || (outputtype==IMAGESSD) ||
        (outputtype==IMAGEDDD) || (outputtype==IMAGESDD))
    {
      Disk_Sector *sec;
      unsigned char blanksector[DFS_SECTORSIZE];
      int imgside;

      // Prepare a blank sector when no sector is found in store
      bzero(blanksector, sizeof(blanksector));

      for (i=0; ((i<hw_maxtracks) && (i<disktracks)); i++)
      {
        for (imgside=0; imgside<sides; imgside++)
        {
          for (j=0; j<sectorspertrack; j++)
          {
            // Write
            sec=diskstore_findhybridsector(i, sidetoread!=AUTODETECT?sidetoread:imgside, j);

            if ((sec!=NULL) && (sec->data!=NULL))
            {
              fwrite(sec->data, 1, DFS_SECTORSIZE, diskimage);
            }
            else
            {
              fwrite(blanksector, 1, DFS_SECTORSIZE, diskimage);
              missingsectors++;
            }
          }
        }
      }
    }
    else
    if (outputtype==IMAGEIMG)
    {
      Disk_Sector *sec;
      unsigned char blanksector[16384];
      int sectorsize;
      int imgside;

      // Prepare a blank sector when no sector is found in store
      bzero(blanksector, sizeof(blanksector));

      if ((diskstore_minsectorid!=-1) && (diskstore_maxsectorid!=-1))
      {
        if ((diskstore_minsectorsize!=-1) && (diskstore_maxsectorsize!=-1) && (diskstore_minsectorsize==diskstore_maxsectorsize))
          sectorsize=diskstore_minsectorsize;

        for (i=0; ((i<hw_maxtracks) && (i<disktracks)); i++)
        {
          for (imgside=0; imgside<sides; imgside++)
          {
            // Write sectors for this side
            for (j=(unsigned int)diskstore_minsectorid; j<=(unsigned int)diskstore_maxsectorid; j++)
            {
              sec=diskstore_findhybridsector(i, imgside, j);

              if ((sec!=NULL) && (sec->data!=NULL))
              {
                fwrite(sec->data, 1, sec->datasize, diskimage);
              }
              else
              {
                fwrite(blanksector, 1, sectorsize, diskimage);
                missingsectors++;
              }
            }
          }
        }
      }
      else
        printf("No sectors found to save\n");
    }
    else
      printf("Unknown output format\n");
  }

  // Finalise disk images
  if (rawdata!=NULL)
  {
    if (outputtype==IMAGESCP)
      scp_finalise(rawdata, (drivetracks/hw_stepping)*sides);
  }

  // Close disk image files (if open)
  if (diskimage!=NULL) fclose(diskimage);
  if (rawdata!=NULL) fclose(rawdata);

  // Free memory allocated to SPI buffer
  if (samplebuffer!=NULL)
  {
    free(samplebuffer);
    samplebuffer=NULL;
  }

  // Free any memory allocated to flippy buffer
  if (flippybuffer!=NULL)
  {
    free(flippybuffer);
    flippybuffer=NULL;
  }

  // When writing csv, close file (if open)
  if((csv) && (csvhandle!=NULL))
  {
    diskstore_dumpbadsectors(csvhandle);
    fclose(csvhandle);
  }

  // Dump a list of valid sectors
  if ((debug) || (summary))
  {
    unsigned int fmsectors=diskstore_countsectormod(MODFM);
    unsigned int mfmsectors=diskstore_countsectormod(MODMFM);
    unsigned int gcrsectors=diskstore_countsectormod(MODGCR);
    unsigned int applegcrsectors=diskstore_countsectormod(MODAPPLEGCR);

    diskstore_dumpsectorlist();

    printf("\nSummary: \n");

    if ((diskstore_mintrack!=AUTODETECT) && (diskstore_maxtrack!=AUTODETECT))
      printf("Disk tracks with data range from %d to %d\n", diskstore_mintrack, diskstore_maxtrack);

    printf("Drive tracks %u\n", drivetracks);

    if (sides==1)
      printf("Single sided capture\n");
    else
      printf("Double sided capture\n");

    printf("FM sectors found %u\n", fmsectors);
    printf("MFM sectors found %u\n", mfmsectors);
    printf("GCR sectors found %u\n", gcrsectors);
    printf("Apple GCR sectors found %u\n", applegcrsectors);

    printf("Detected density : ");
    if ((mod_density&MOD_DENSITYFMSD)!=0) printf("SD ");
    if ((mod_density&MOD_DENSITYMFMDD)!=0) printf("DD ");
    if ((mod_density&MOD_DENSITYMFMHD)!=0) printf("HD ");
    if ((mod_density&MOD_DENSITYMFMED)!=0) printf("ED ");
    if ((mod_density&MOD_DENSITYAPPLEGCR)!=0) printf("APPLEGCR ");
    if (mod_density==MOD_DENSITYAUTO) printf("Unknown density ");
    printf("\n");

    if ((diskstore_minsectorsize!=-1) && (diskstore_maxsectorsize!=-1))
      printf("Sector sizes range from %d to %d bytes\n", diskstore_minsectorsize, diskstore_maxsectorsize);

    if ((diskstore_minsectorid!=-1) && (diskstore_maxsectorid!=-1))
      printf("Sector ids range from %d to %d\n", diskstore_minsectorid, diskstore_maxsectorid);

    if ((diskstore_minsectorsize!=-1) && (diskstore_maxsectorsize!=-1) && (diskstore_minsectorid!=-1) && (diskstore_maxsectorid!=-1) && (diskstore_minsectorsize==diskstore_maxsectorsize))
    {
      long totalstorage;

      totalstorage=diskstore_maxsectorsize*((diskstore_maxsectorid-diskstore_minsectorid)+1);
      totalstorage*=disktracks;
      if (sides==2)
        totalstorage*=2;

      printf("Total storage is %ld bytes\n", totalstorage);
    }
  }

  // Show a layout map of where data was found on disk surface
  if (layout)
    diskstore_dumplayoutmap(ROTATIONS);

  if (missingsectors>0)
    printf("Missing %d sectors\n", missingsectors);

  // Calculate whole disk CRC32
  if (capturetype==DISKIMG)
  {
    diskstore_sortsectors(SORTBYPOS, ROTATIONS);
    printf("\nCRC32 %.8X\n", diskstore_calcdiskcrc((sides==1)?sidetoread:2));
  }

  return 0;
}
