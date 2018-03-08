#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "hardware.h"
#include "rfi.h"

int hw_currenttrack = 0;
int hw_currenthead = 0;
unsigned long hw_samplerate = 0;

int hw_stepping = HW_NORMALSTEPPING;

FILE *samplefile = NULL;
char samplefilename[1024];

// Drive control
unsigned char hw_detectdisk()
{
  unsigned char retval=HW_NODISK;
  struct stat st;

  // Check sample file exists
  if (stat(samplefilename, &st)==0)
    retval=HW_HAVEDISK;

  return retval;
}

void hw_driveselect()
{
  // Only one drive (sample file) supported
}

void hw_startmotor()
{
  // Open sample file
  samplefile=fopen(samplefilename, "rb");
}

void hw_stopmotor()
{
  // Close sample file
  if (samplefile!=NULL)
  {
    fclose(samplefile);

    samplefile=NULL;
  }
}

// Track seeking
int hw_attrackzero()
{
  return (hw_currenttrack==0);
}

void hw_seektotrackzero()
{
  hw_currenttrack=0;
}

void hw_seektotrack(const int track)
{
  // Seeking will be done by sampling function
  hw_currenttrack=track;
}

void hw_sideselect(const int side)
{
  hw_currenthead=side;
}

// Signaling and data sampling
void hw_waitforindex()
{
  // Only used to sync sampling, so ignore for now
}

int hw_writeprotected()
{
  struct stat st;

  // Check sample file read/write status
  if (stat(samplefilename, &st)==0)
    return ((st.st_mode&S_IWUSR)!=0);

  return 0;
}

void hw_samplerawtrackdata(char* buf, uint32_t len)
{
  // Find/Read track data into buffer
  if (samplefile!=NULL)
  {
    if (hw_currenthead==0)
    {
      fseek(samplefile, hw_currenttrack*(1024*1024), SEEK_SET);
      fread(buf, len, 1, samplefile);
    }
    else
      bzero(buf, len);
  }
}

// Clean up
void hw_done()
{
  // Close sample file if open
  if (samplefile!=NULL)
  {
    fclose(samplefile);

    samplefile=NULL;
  }
}

// Initialisation
int hw_init(const char *rawfile, const int spiclockdivider)
{
  // Blank out filename string
  samplefilename[0]=0;

  // Check raw filename is not blank and will fit into our buffer
  if ((rawfile[0]!=0) && (strlen(rawfile)<(sizeof(samplefilename)+1)))
    strcpy(samplefilename, rawfile);

  hw_samplerate=400000000/spiclockdivider;

  return (hw_detectdisk()==HW_HAVEDISK);
}

void hw_sleep(const unsigned int seconds)
{
  (void) seconds;

  // No sleep required as this is not using real hardware
}

// Assume 300 RPM
float hw_measurerpm()
{
  return 300;
}
