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

FILE *hw_samplefile = NULL;
char hw_samplefilename[1024];

// Drive control
unsigned char hw_detectdisk()
{
  unsigned char retval=HW_NODISK;
  struct stat st;

  // Check sample file exists
  if (stat(hw_samplefilename, &st)==0)
    retval=HW_HAVEDISK;

  return retval;
}

void hw_driveselect()
{
  // Only one drive (sample file) supported
}

void hw_startmotor()
{
}

void hw_stopmotor()
{
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
  if (stat(hw_samplefilename, &st)==0)
    return ((st.st_mode&S_IWUSR)!=0);

  return 0;
}

void hw_samplerawtrackdata(char* buf, uint32_t len)
{
  // Find/Read track data into buffer
  if (hw_samplefile!=NULL)
  {
    if (strstr(hw_samplefilename, ".raw")!=NULL)
    {
      if (hw_currenthead==0)
      {
        fseek(hw_samplefile, hw_currenttrack*(1024*1024), SEEK_SET);
        fread(buf, len, 1, hw_samplefile);
      }
      else
        bzero(buf, len);
    }
    else
    if (strstr(hw_samplefilename, ".rfi")!=NULL)
    {
      long status;

      status=rfi_readtrack(hw_samplefile, hw_currenttrack, hw_currenthead, buf, len);

      if (status<=0)
        bzero(buf, len);
    }
    else // Unknown sample file format
      bzero(buf, len);
  }
}

// Clean up
void hw_done()
{
  // Close sample file if open
  if (hw_samplefile!=NULL)
  {
    fclose(hw_samplefile);

    hw_samplefile=NULL;
  }
}

// Initialisation
int hw_init(const char *rawfile, const int spiclockdivider)
{
  // Blank out filename string
  hw_samplefilename[0]=0;

  // Check raw filename is not blank and will fit into our buffer
  if ((rawfile[0]!=0) && (strlen(rawfile)<(sizeof(hw_samplefilename)+1)))
    strcpy(hw_samplefilename, rawfile);

  hw_samplerate=400000000/spiclockdivider;

  // Open sample file
  hw_samplefile=fopen(rawfile, "rb");

  // If opened and valid, read header values
  if ((hw_samplefile!=NULL) && (strstr(hw_samplefilename, ".rfi")!=NULL))
  {
    rfi_readheader(hw_samplefile);
    if (rfi_tracks<=0) return 0;
  }

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
