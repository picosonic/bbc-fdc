#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "hardware.h"
#include "rfi.h"

#define HW_OLDRAWTRACKSIZE (1024*1024)

unsigned int hw_maxtracks = HW_MAXTRACKS;
uint8_t hw_currenttrack = 0;
uint8_t hw_currenthead = 0;
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
  hw_currenttrack=track*hw_stepping;
}

// Override maximum number of hardware tracks
void hw_setmaxtracks(const int maxtracks)
{
  hw_maxtracks=maxtracks;
}

// Seek head in by 1 track
void hw_seekin()
{
  if (hw_currenttrack<hw_maxtracks) hw_currenttrack++;
}

// Seek head out by 1 track, towards track zero
void hw_seekout()
{
  if (hw_currenttrack>0) hw_currenttrack--;
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

// Fix SPI sample buffer timings
void hw_fixspisamples(char *inbuf, long inlen, char *outbuf, long outlen)
{
  long inpos, outpos;
  unsigned char o, olen, bitpos;

  outpos=0; o=0; olen=0;

  for (inpos=0; inpos<inlen; inpos++)
  {
    unsigned char c;

    c=inbuf[inpos];

    // Insert extra sample, this is due to SPI sampling leaving a 1 sample gap between each group of 8 samples
    o=(o<<1)|((c&0x80)>>7);
    olen++;
    if (olen==BITSPERBYTE)
    {
      if (outpos<outlen)
        outbuf[outpos++]=o;

      olen=0; o=0;
    }

    // Process the 8 valid samples we did get
    for (bitpos=0; bitpos<BITSPERBYTE; bitpos++)
    {
      o=(o<<1)|((c&0x80)>>7);
      olen++;

      if (olen==BITSPERBYTE)
      {
        if (outpos<outlen)
          outbuf[outpos++]=o;

        olen=0; o=0;
      }

      c=c<<1;
    }

    // Stop on output buffer overflow
    if (outpos>=outlen) return;
  }
}

// Read raw flux data for current track/head
void hw_samplerawtrackdata(char* buf, uint32_t len)
{
  // Clear output buffer to prevent failed reads potentially returning previous data
  bzero(buf, len);

  // Find/Read track data into buffer
  if (hw_samplefile!=NULL)
  {
    // Obsolete .raw files were 8 megabits per track, sampled at 12.5Mhz, either 40 or 80 tracks, with second side (if any) folowing the whole of the first
    if (strstr(hw_samplefilename, ".raw")!=NULL)
    {
      if (fseek(hw_samplefile, ((hw_maxtracks*hw_currenthead)+hw_currenttrack)*HW_OLDRAWTRACKSIZE, SEEK_SET)==0)
      {
        char *rawbuf;

        rawbuf=malloc(HW_OLDRAWTRACKSIZE);
        if (rawbuf==NULL) return;

        fread(rawbuf, HW_OLDRAWTRACKSIZE, 1, hw_samplefile);

        hw_fixspisamples(rawbuf, HW_OLDRAWTRACKSIZE, buf, len);

        free(rawbuf);
      }
    }
    else
    if (strstr(hw_samplefilename, ".rfi")!=NULL)
      rfi_readtrack(hw_samplefile, hw_currenttrack, hw_currenthead, buf, len);
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

#ifdef NOPI
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
#endif

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
