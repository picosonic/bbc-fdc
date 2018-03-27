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
float hw_rpm = HW_DEFAULTRPM;

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
  // Motor not used in hardware emulation mode
}

void hw_stopmotor()
{
  // Motor not used in hardware emulation mode
}

// Find out if "head" is at track zero
int hw_attrackzero()
{
  return (hw_currenttrack==0);
}

// Seek to track zero
void hw_seektotrackzero()
{
  hw_currenttrack=0;
}

// Seek to given track number
void hw_seektotrack(const int track)
{
  // Actual seeking within input file will be done by sampling function
  hw_currenttrack=track;
}

// Switch disk sides
void hw_sideselect(const int side)
{
  hw_currenthead=side;
}

// Wait for an index pulse to synchronise capture
void hw_waitforindex()
{
  // Only used to sync sampling, so ignore for now
}

// Determine if disk is write protected
int hw_writeprotected()
{
  struct stat st;

  // Check sample file read/write status
  if (stat(hw_samplefilename, &st)==0)
    return ((st.st_mode&S_IWUSR)!=0);

  return 0;
}

// Read raw flux data for current track/head
void hw_samplerawtrackdata(char* buf, uint32_t len)
{
  // Clear output buffer to prevent failed reads potentially returning previous data
  bzero(buf, len);

  // Find/Read track data into buffer
  if (hw_samplefile!=NULL)
  {
    // Obsolete .raw files were 8 megabits per track, either 40 or 80 tracks, with second side (if any) folowing th whole of the first
    if (strstr(hw_samplefilename, ".raw")!=NULL)
    {
      if (fseek(hw_samplefile, ((HW_MAXTRACKS*hw_currenthead)+hw_currenttrack)*(1024*1024), SEEK_SET)==0)
        fread(buf, len, 1, hw_samplefile);
    }
    else
    if (strstr(hw_samplefilename, ".rfi")!=NULL)
    {
      long status;

      status=rfi_readtrack(hw_samplefile, hw_currenttrack, hw_currenthead, buf, len);
    }
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

  // Default to Pi2/Pi3 clock rate
  hw_samplerate=HW_400MHZ/spiclockdivider;

  // Open sample file
  hw_samplefile=fopen(rawfile, "rb");

  // If RFI opened and valid, read header values to determine capture settings
  if ((hw_samplefile!=NULL) && (strstr(hw_samplefilename, ".rfi")!=NULL))
  {
    rfi_readheader(hw_samplefile);
    if (rfi_tracks<=0) return 0;
  }

  return (hw_detectdisk()==HW_HAVEDISK);
}

// Sleep for a number of seconds
void hw_sleep(const unsigned int seconds)
{
  (void) seconds;

  // No sleep required as this is not using real hardware
}

// Measure RPM, defaults to 300RPM
float hw_measurerpm()
{
  return hw_rpm;
}
