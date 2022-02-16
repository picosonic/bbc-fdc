#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "hardware.h"
#include "scp.h"
#include "mod.h"

/*

SuperCard Pro Image File Specification v2.4 - November 13, 2021

--------------------------------------------------------------------------------------

This information is copyright (C) 2012-2021 By Jim Drew.  Permission is granted
for inclusion with any source code when keeping this copyright notice.

See www.cbmstuff.com for information on purchasing SuperCard Pro.

--------------------------------------------------------------------------------------

; SCP IMAGE FILE FORMAT
; ------------------------------------------------------------------
;
; 0000              'SCP' (ASCII CHARS)
; 0003              VERSION (nibbles major/minor)
; 0004              DISK TYPE
;                   UPPER 4 BITS ARE USED TO DEFINE A DISK CLASS (MANUFACTURER)
;                   LOWER 4 BITS ARE USED TO DEFINE A DISK SUB-CLASS (MACHINE)
;
;                   MANUFACTURER BIT DEFINITIONS:
;                   0000 = COMMODORE
;                   0001 = ATARI
;                   0010 = APPLE
;                   0011 = PC
;                   0100 = TANDY
;                   0101 = TEXAS INSTRUMENTS
;                   0110 = ROLAND
;                   1000 = OTHER
;
;					SEE DISK TYPE BIT DEFINITIONS BELOW
;
; 0005              NUMBER OF REVOLUTIONS (1-5)
; 0006              START TRACK (0-167)
; 0007              END TRACK (0-167)
; 0008              FLAGS BITS (0=INDEX, 1=TPI, 2=RPM, 3=TYPE, 4=MODE, 5=FOOTER)
; 0009              BIT CELL ENCODING (0=16 BITS, >0=NUMBER OF BITS USED)
; 000A              NUMBER OF HEADS
; 000B              RESOLUTION (BASE 25), ie 0=25ns, 1=50ns, 2=75ns, 3=100ns, etc.
; 000C-F            32 BIT CHECKSUM OF DATA FROM 0x10-EOF (END OF FILE)
; 0010              OFFSET TO 1st TRACK DATA HEADER (4 bytes of 0 if track is skipped)
; 0014              OFFSET TO 2nd TRACK DATA HEADER (4 bytes of 0 if track is skipped)
; 0018              OFFSET TO 3rd TRACK DATA HEADER (4 bytes of 0 if track is skipped)
; ....
; 02AC              OFFSET TO 168th TRACK DATA HEADER (4 bytes of 0 if track is skipped)
; 02B0              TYPICAL START OF 1st TRACK DATA HEADER (always the case with SCP created images)
;
; ....              END OF TRACK DATA
; ????              TIMESTAMP AFTER LAST TRACK'S DATA (AS ASCII STRING - ie. 7/17/2013 12:45:49 PM)
;
; Start of extension footer
;
; ????              OFFSET TO DRIVE MANUFACTURER STRING (optional)
; ????              OFFSET TO DRIVE MODEL STRING (optional)
; ????              OFFSET TO DRIVE SERIAL NUMBER STRING (optional)
; ????              OFFSET TO CREATOR STRING (optional)
; ????              OFFSET TO APPLICATION NAME STRING (optional)
; ????              OFFSET TO COMMENTS (optional)
; ????              UTC TIME/DATE OF IMAGE CREATION
; ????              UTC TIME/DATE OF IMAGE MODIFICATION
; ????              VERSION/SUBVERSION OF APPLICATION THAT CREATED IMAGE
; ????              VERSION/SUBVERSION OF SUPERCARD PRO HARDWARE
; ????              VERSION/SUBVERSION OF SUPERCARD PRO FIRMWARE
; ????              VERSION/SUBVERSION OF THIS IMAGE FORMAT
; ????              ASCII 'FPCS'

; ## FILE FORMAT DEFINES ##

IFF_ID = 0x00                      ; "SCP" (ASCII CHARS)
IFF_VER = 0x03                     ; version (nibbles major/minor)
IFF_DISKTYPE = 0x04                ; disk type (0=CBM, 1=AMIGA, 2=APPLE II, 3=ATARI ST, 4=ATARI 800, 5=MAC 800, 6=360K/720K, 7=1.44MB)
IFF_NUMREVS = 0x05                 ; number of revolutions (2=default)
IFF_START = 0x06                   ; start track (0-167)
IFF_END = 0x07                     ; end track (0-167)
IFF_FLAGS = 0x08                   ; FLAGS bits (0=INDEX, 1=TPI, 2=RPM, 3=TYPE, 4=TYPE, 5=FOOTER, - see defines below)
IFF_ENCODING = 0x09                ; BIT CELL ENCODING (0=16 BITS, >0=NUMBER OF BITS USED)
IFF_HEADS = 0x0A                   ; 0=both heads are in image, 1=side 0 only, 2=side 1 only
IFF_RESOLUTION = 0x0B              ; 0=25ns, 1=50, 2=75, 3=100, 4=125, etc.
IFF_CHECKSUM = 0x0C                ; 32 bit checksum of data added together starting at 0x0010 through EOF
IFF_THDOFFSET = 0x10               ; first track data header offset
IFF_THDSTART = 0x2B0               ; start of first Track Data Header

; FLAGS BIT DEFINES

FB_INDEX = 0x01                    ; clear = no index reference, set = flux data starts at index
FB_TPI = 0x02                      ; clear = drive is 48TPI, set = drive is 96TPI (only applies to 5.25" drives!)
FB_RPM = 0x04                      ; clear = drive is 300 RPM drive, set = drive is 360 RPM drive
FB_TYPE = 0x08                     ; clear = image is has original flux data, set = image is flux data that has been normalized
FB_MODE = 0x10                     ; clear = image is read-only, set = image is read/write capable
FB_FOOTER = 0x20                   ; clear = image does not contain a footer, set = image contains a footer at the end of it
FB_EXTENDED = 0x40                 ; clear = image is for floppy drives only, set = image is for other media
FB_CREATOR = 0x80                  ; clear = image was generated by SuperCard Pro, set = image was created with any other device

; MANUFACTURERS                      7654 3210
man_CBM = 0x00                     ; 0000 xxxx
man_Atari = 0x10                   ; 0001 xxxx
man_Apple = 0x20                   ; 0010 xxxx
man_PC = 0x30                      ; 0011 xxxx
man_Tandy = 0x40                   ; 0100 xxxx
man_TI = 0x50                      ; 0101 xxxx
man_Roland = 0x60                  ; 0110 xxxx
man_Amstrad = 0x70                 ; 0111 xxxx
man_Other = 0x80                   ; 1000 xxxx
man_TapeDrive = 0xE0               ; 1110 xxxx
man_HardDrive = 0xF0               ; 1111 xxxx

; DISK TYPE BIT DEFINITIONS
;
; CBM DISK TYPES
disk_C64 = 0x00                    ; xxxx 0000
disk_Amiga = 0x04                  ; xxxx 0100
disk_AmigaHD = 0x08                ; xxxx 1000

; ATARI DISK TYPES
disk_AtariFMSS = 0x00              ; xxxx 0000
disk_AtariFMDS = 0x01              ; xxxx 0001
disk_AtariFMEx = 0x02              ; xxxx 0010
disk_AtariSTSS = 0x04              ; xxxx 0100
disk_AtariSTDS = 0x05              ; xxxx 0101

; APPLE DISK TYPES
disk_AppleII = 0x00                ; xxxx 0000
disk_AppleIIPro = 0x01             ; xxxx 0001
disk_Apple400K = 0x04              ; xxxx 0100
disk_Apple800K = 0x05              ; xxxx 0101
disk_Apple144 = 0x06               ; xxxx 0110

; PC DISK TYPES
disk_PC360K = 0x00                 ; xxxx 0000
disk_PC720K = 0x01                 ; xxxx 0001
disk_PC12M = 0x02                  ; xxxx 0010
disk_PC144M = 0x03                 ; xxxx 0011

; TANDY DISK TYPES
disk_TRS80SSSD = 0x00              ; xxxx 0000
disk_TRS80SSDD = 0x01              ; xxxx 0001
disk_TRS80DSSD = 0x02              ; xxxx 0010
disk_TRS80DSDD = 0x03              ; xxxx 0011

; TI DISK TYPES
disk_TI994A = 0x00                 ; xxxx 0000

; ROLAND DISK TYPES
disk_D20 = 0x00                    ; xxxx 0000

; AMSTRAD DISK TYPES
disk_CPC = 0x00                    ; xxxx 0000

; OTHER DISK TYPES
disk_360 = 0x00                    ; xxxx 0000
disk_12M = 0x01                    ; xxxx 0001
disk_Rrsvd1 = 0x02                 ; xxxx 0010
disk_Rsrvd2 = 0x03                 ; xxxx 0011
disk_720 = 0x04                    ; xxxx 0100
disk_144M = 0x05                   ; xxxx 0101

; TAPE DRIVE DISK TYPES
tape_GCR1 = 0x00                   ; xxxx 0000
tape_GCR2 = 0x01                   ; xxxx 0001
tape_MFM = 0x02                    ; xxxx 0010

; HARD DRIVE DISK TYPES
drive_MFM = 0x00                   ; xxxx 0000
drive_RLL = 0x01                   ; xxxx 0001

; ------------------------------------------------------------------
; TRACK DATA HEADER FORMAT
; ------------------------------------------------------------------
;
; 0000              'TRK' (ASCII CHARS)             - 3 chars
; 0003              TRACK NUMBER                    - 1 byte
; ....              START OF TABLE OF ENTRIES FOR EACH REVOLUTION
; 0004              INDEX TIME (1st REVOLUTION)     - 4 bytes
; 0008              TRACK LENGTH (1st REVOLUTION)   - 4 bytes
; 000C              DATA OFFSET (1st REVOLUTION)    - 4 bytes (offset is from start of Track Data Header!)
; ....              ADDITIONAL ENTRIES FOR EACH REVOLUTION (IF AVAILABLE, OTHERWISE THIS WILL LIKELY BE THE FLUX DATA!)
; 0010              INDEX TIME (2nd REVOLUTION)     - 4 bytes
; 0014              TRACK LENGTH (2nd REVOLUTION)   - 4 bytes
; 0018              DATA OFFSET (2nd REVOLUTION)    - 4 bytes
; 001C              INDEX TIME (3rd REVOLUTION)     - 4 bytes
; 0020              TRACK LENGTH (3rd REVOLUTION)   - 4 bytes
; 0024              DATA OFFSET (3rd REVOLUTION)    - 4 bytes
; 0028              INDEX TIME (4th REVOLUTION)     - 4 bytes
; 002C              TRACK LENGTH (4th REVOLUTION)   - 4 bytes
; 0030              DATA OFFSET (4th REVOLUTION)    - 4 bytes
; 0034              INDEX TIME (5th REVOLUTION)     - 4 bytes
; 0038              TRACK LENGTH (5th REVOLUTION)   - 4 bytes
; 003C              DATA OFFSET (5th REVOLUTION)    - 4 bytes
; .... etc. etc.
;
;
; INDEX TIME = 32 BIT VALUE, TIME IN NANOSECONDS/25ns FOR ONE REVOLUTION
;
; i.e. 0x007A1200 = 8000000, 8000000*25 = 200000000 = 200.00000ms
;
; TRACK LENGTH = NUMBER OF BITCELLS FOR THIS TRACK
;
; i.e. 0x00015C8F = 89231 bitcell entries in the TRACK DATA area
;
; TRACK DATA = 16 BIT VALUE, TIME IN NANOSECONDS/25ns FOR ONE BIT CELL TIME
;
; i.e. 0x00DA = 218, 218*25 = 5450ns = 5.450us
;
; Special note when a bit cell time is 0x0000.  This occurs when there is no flux transition
; for at least 65536*25ns.  This means that the time overflowed.  When this occurs, the next
; bit cell time will be added to 65536 and that will be the total bit cell time.  If there
; are more than one 0x0000 entry, then 65536 is added for each entry until a non-0x0000 entry
; is found.  You will see 0x0000 when encountering 'strongbits' (no flux area) type of
; copy protection schemes.
;
; i.e. 0x0000, 0x0000, 0x7FFF = 65536 + 65536 + 32767 = 163839*25 = 4095975ns
;
; The number of bitcells only increases by the number of entries, and is not affected by the
; overall time.  So, in above example even though the time could be what thousands of bitcells
; times would normally be, the number of bitcells might only be 3 entries.  The SuperCard Pro
; hardware itself re-creates the no flux transitions, using the bitcell lengths as described.


; ## TRACK DATA HEADER DEFINES ##

TDH_ID = 0x00                      ; "TRK" (ASCII CHARS)
TDH_TRACKNUM = 0x03                ; track number
TDH_TABLESTART = 0x04              ; table of entries (3 longwords per revolution stored)
TDH_DURATION = 0x4                 ; duration of track, from index pulse to index pulse (1st revolution)
TDH_LENGTH = 0x08                  ; length of track (1st revolution)
TDH_OFFSET = 0x0C                  ; offset to flux data from start of TDH (1st revolution)

; ------------------------------------------------------------------
; EXTENSION FOOTER FORMAT
; ------------------------------------------------------------------
;
; 0000           DRIVE MANUFACTURER STRING OFFSET            - 4 bytes
; 0004           DRIVE MODEL STRING OFFSET                   - 4 bytes
; 0008           DRIVE SERIAL NUMBER STRING OFFSET           - 4 bytes
; 000C           CREATOR STRING OFFSET                       - 4 bytes
; 0010           APPLICATION NAME STRING OFFSET              - 4 bytes
; 0014           COMMENTS STRING OFFSET                      - 4 bytes
; 0018           IMAGE CREATION TIMESTAMP                    - 8 bytes
; 0020           IMAGE MODIFICATION TIMESTAMP                - 8 bytes
; 0028           APPLICATION VERSION (nibbles major/minor)   - 1 byte
; 0029           SCP HARDWARE VERSION (nibbles major/minor)  - 1 byte
; 002A           SCP FIRMWARE VERSION (nibbles major/minor)  - 1 byte
; 002B           IMAGE FORMAT REVISION (nibbles major/minor) - 1 byte
; 002C           'FPCS' (ASCII CHARS)                        - 4 bytes

--------------------------------------------------------------------------------------

From : https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt

*/

struct scp_header scpheader;
long scp_endofheader=0;
uint32_t *scp_trackoffsets=NULL;
long scprate=0;

uint32_t scp_checksum(FILE *scpfile)
{
  uint32_t checksum;
  uint8_t block[256];
  size_t i;

  if (scpfile==NULL) return 0;
  if (scp_endofheader==0) return 0;

  fseek(scpfile, scp_endofheader, SEEK_SET);

  checksum=0;

  while (!feof(scpfile))
  {
    size_t blocklen;

    blocklen=fread(block, 1, sizeof(block), scpfile);
    if (blocklen>0)
    {
      for (i=0; i<blocklen; i++)
        checksum+=block[i];
    }
    else
      return 0;
  }

  fseek(scpfile, scp_endofheader, SEEK_SET);

  return checksum;
}

int scp_readheader(FILE *scpfile)
{
  if (scpfile==NULL) return -1;

  if (fread(&scpheader, sizeof(scpheader), 1, scpfile)==0) return -1;

  if (strncmp((char *)&scpheader.magic, SCP_MAGIC, strlen(SCP_MAGIC))!=0)
  {
    bzero(&scpheader, sizeof(scpheader));

    return -1;
  }

  // Only support 16bit timings
  if (scpheader.bitcellencoding!=0x00) return -1;

  // Make a note of where the header ends
  scp_endofheader=ftell(scpfile);

  // Verify checksum
  if (scp_checksum(scpfile)!=scpheader.checksum) return -1;

  // Set RPM
  if ((scpheader.flags & SCP_FLAGS_360RPM)!=0)
    hw_rpm=360;
  else
    hw_rpm=300;

  // Determine sample rate in Hz
  scprate=(NSINUS/((scpheader.resolution+1)*SCP_BASE_NS))*USINSECOND;

  // Skip over extended data if used
  if ((scpheader.flags & SCP_FLAGS_EXTENDED)!=0)
    fseek(scpfile, 0x80, SEEK_SET);

  // Allocate memory to store track data offsets
  scp_trackoffsets=malloc(sizeof(uint32_t) * SCP_MAXTRACKS);
  if (scp_trackoffsets!=NULL)
  {
    if (fread(scp_trackoffsets, (scpheader.endtrack-scpheader.starttrack+1)*sizeof(uint32_t), 1, scpfile)==0)
    {
      free(scp_trackoffsets);
      bzero(&scpheader, sizeof(scpheader));

      return -1;
    }
  }
  else
  {
    bzero(&scpheader, sizeof(scpheader));

    return -1;
  }

  if ((scpheader.starttrack==0) && (scpheader.endtrack==0)) return -1;

  return 0;
}

long scp_readtrack(FILE * scpfile, const int track, const int side, char* buf, const uint32_t buflen)
{
  struct scp_tdh thdr;
  struct scp_timings timings;
  double scalar=(double)scprate/(double)hw_samplerate;

  if (scpfile==NULL) return 0;

  bzero(buf, buflen);

  // Don't process empty tracks
  if (scp_trackoffsets[(track*2)+side]==0) return 0;

  // Ensure requested track is in range
  if ((track<0) || ((track*2)>=scpheader.endtrack)) return 0;

  // Seek to data for this track
  fseek(scpfile, scp_trackoffsets[(track*2)+side], SEEK_SET);

  // Verify track header
  if (fread(&thdr, sizeof(thdr), 1, scpfile)==0) return 0;

  if (strncmp((char *)&thdr.magic, SCP_TRACK, strlen(SCP_TRACK))==0)
  {
    uint8_t i;
    uint32_t sample;
    uint16_t bitcell;
    uint32_t bufpos=0;

    //printf("Track %d.%d -> %d.%d\n", track, side, thdr.track/2, thdr.track%2);

    for (i=0; i<scpheader.revolutions; i++)
    {
      unsigned char b, blen;
      long trackpos;

      if (fread(&timings, sizeof(timings), 1, scpfile)==0) return 0;

      trackpos=ftell(scpfile);

      // Seek to start of flux data
      fseek(scpfile, scp_trackoffsets[(track*2)+side]+timings.dataoffset, SEEK_SET);

      blen=0; b=0;

      for (sample=0; sample<timings.tracklen; sample++)
      {
        if (fread(&bitcell, sizeof(bitcell), 1, scpfile)==0) return 0;

        // Swap byte order (if required)
        bitcell=be16toh(bitcell);

        // Convert to hardware samplerate
        bitcell=((double)bitcell/scalar);

        while (bitcell>0)
        {
          bitcell--;

          b=(b<<1)|((bitcell==0)?1:0);
          blen++;

          if (blen==BITSPERBYTE)
          {
            if (bufpos<buflen)
              buf[bufpos++]=b;

            b=0; blen=0;
          }
        }
      }

      fseek(scpfile, trackpos, SEEK_SET);
    }
  }

  return 0;
}

void scp_writeheader(FILE *scpfile, const uint8_t rotations, const uint8_t starttrack, const uint8_t endtrack, const float rpm, const uint8_t sides, const int sidetoread)
{
  uint8_t i;

  if (scpfile==NULL) return;

  bzero(&scpheader, sizeof(scpheader));

  // Magic and version
  memcpy(scpheader.magic, SCP_MAGIC, sizeof(scpheader.magic));
  scpheader.version=SCP_VERSION; // Or 0x00 if footer used

  // Disk type ??
  scpheader.disktype=SCP_MAN_OTHER | SCP_DISK_144M;

  // Rotations captured
  scpheader.revolutions=rotations;

  // Start and end tracks (multiplied by sides)
  scpheader.starttrack=starttrack;
  scpheader.endtrack=endtrack;

  // Flags
  scpheader.flags=SCP_FLAGS_CREATOR | ((rpm>330)?SCP_FLAGS_360RPM:0x0) | ((endtrack>44)?SCP_FLAGS_96TPI:0x0) | SCP_FLAGS_INDEX; // TODO add 0x20 if footer added

  // Bit cell encoding - for future expansion, so always 0x00 for now
  scpheader.bitcellencoding=0x00;

  // Sides / Heads
  scpheader.heads=((sides==2)?0:((sidetoread==1)?2:1));

  // Capture resolution, default in .rfi files is 80ns, which has closest SCP multiplier of 2 (i.e. 75ns)
  scpheader.resolution=0; // TODO determine programatically the best value for this based on rate

  // Blank checksum  - to be filled in later (calculated from next byte to EOF)
  scpheader.checksum=0x0;

  // Write the header
  fwrite(&scpheader, 1, sizeof(scpheader), scpfile);

  // If we're using extended mode, reserve some space for extended variables
  if ((scpheader.flags&SCP_FLAGS_EXTENDED)!=0)
  {
    struct scp_extensions extensions;

    bzero(extensions.extdata, sizeof(extensions));

    fwrite(&extensions, 1, sizeof(extensions), scpfile);
  }

  // Prepare for storing track offsets, and created space to store them in file
  scp_trackoffsets=malloc(sizeof(uint32_t) * SCP_MAXTRACKS);

  if (scp_trackoffsets==NULL) return;

  // Cache file position after header
  scp_endofheader=ftell(scpfile);

  // Blank offsets to tracks - to be filled in later
  for (i=0; i<(endtrack-starttrack+1); i++)
    fprintf(scpfile, "%c%c%c%c", 0, 0, 0, 0);
}

void scp_writetrack(FILE *scpfile, const uint8_t track, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const uint8_t rotations, const float rpm)
{
  long scppos;
  uint8_t i;
  unsigned char c,j;
  unsigned long fluxdatapos;
  float celltime;
  uint32_t value;
  unsigned long rotpoint;
  struct scp_tdh tdh;
  struct scp_timings timings;

  if (scpfile==NULL) return;
  if (scp_trackoffsets==NULL) return;

  // Remember where this track starts and cache this for adding to track offsets table in header
  scppos=ftell(scpfile);
  scp_trackoffsets[track]=scppos;

  memcpy(tdh.magic, SCP_TRACK, sizeof(tdh.magic)); // Track ID
  tdh.track=track; // Track number

  // Write the track header
  fwrite(&tdh, 1, sizeof(tdh), scpfile);

  rotpoint=rawdatalength/rotations;

  // Write track timings
  for (i=0; i<rotations; i++)
  {
    // Index time - duration of first revolution between index pulses (in nanoseconds/25)
    timings.indextime=(1/(rpm/SECONDSINMINUTE))*(NSINSECOND/SCP_BASE_NS);

    // Track length (in bitcells)
    timings.tracklen=0;

    // Data offset for track flux (from start of track)
    timings.dataoffset=0;

    fwrite(&timings, 1, sizeof(timings), scpfile);
  }

  // Split raw data into rotations
  for (i=0; i<rotations; i++)
  {
    long scpdatapos;
    long trackpos;
    uint32_t fluxtime;
    uint32_t numfluxes;
    char level,bi=0;

    // 16 bit big-endian time in nanoseconds/25 between fluxes
    level=(rawtrackdata[rotpoint*i]&0x80)>>7;
    bi=level;
    fluxtime=0;
    numfluxes=0;

    scpdatapos=ftell(scpfile);

    // Process each byte of the raw flux data
    for (fluxdatapos=(rotpoint*i); ((fluxdatapos<rotpoint*(i+1)) && (fluxdatapos<rawdatalength)); fluxdatapos++)
    {
      // Extract byte from buffer
      c=rawtrackdata[fluxdatapos];

      // Process each bit of the extracted byte
      for (j=0; j<BITSPERBYTE; j++)
      {
        // Determine next level
        bi=((c&0x80)>>7);

        // Increment samples counter
        fluxtime++;

        // Look for level changes
        if (bi!=level)
        {
          // Flip level cache
          level=1-level;

          // Look for rising edge
          if (level==1)
          {
            // Increment total number of fluxes
            numfluxes++;

            // Convert samples into nanoseconds/25
            celltime=(mod_samplestous(fluxtime)*NSINUS)/SCP_BASE_NS;

            // Convert back from float to uint16_t
            fluxtime=roundf(celltime);

            // Check for time overflow
            while (fluxtime>65536)
            {
              fprintf(scpfile, "%c%c", 0, 0);
              fluxtime-=65536;
            }

            // Write sample between fluxes, big-endian
            fprintf(scpfile, "%c%c", (fluxtime>>8)&0xff, fluxtime&0xff);

            // Reset samples counter
            fluxtime=0;
          }
        }

        // Move on to next sample level (bit)
        c=c<<1;
      }
    }

    // Store where we are
    trackpos=ftell(scpfile);

    // Go back to update table for this rotation
    fseek(scpfile, scppos+4+(sizeof(timings)*i)+4, SEEK_SET);

    value=numfluxes;
    fwrite(&value, 1, sizeof(uint32_t), scpfile);

    value=(uint32_t)(scpdatapos-scppos);
    fwrite(&value, 1, sizeof(uint32_t), scpfile);

    // Reset back to where we were
    fseek(scpfile, trackpos, SEEK_SET);
  }
}

void scp_finalise(FILE *scpfile, const uint8_t endtrack)
{
  struct tm tim;
  struct timeval tv;
  uint32_t checksum;
  uint8_t block[256];
  size_t i;

  if (scpfile==NULL) return;

  // Write timestamp
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tim);

  fprintf(scpfile, "%02d/%02d/%d %02d:%02d:%02d", tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900, tim.tm_hour, tim.tm_min, tim.tm_sec);

  // TODO write optional footer

  // update track data offsets table
  if (scp_trackoffsets!=NULL)
  {
    fseek(scpfile, scp_endofheader, SEEK_SET);
    fwrite(scp_trackoffsets, 1, endtrack*sizeof(uint32_t), scpfile);
    free(scp_trackoffsets);
    scp_trackoffsets=NULL;
  }

  // Calculate 32bit checksum
  fseek(scpfile, scp_endofheader, SEEK_SET);
  checksum=0;
  while (!feof(scpfile))
  {
    size_t blocklen;

    blocklen=fread(block, 1, sizeof(block), scpfile);
    if (blocklen>0)
    {
      for (i=0; i<blocklen; i++)
        checksum+=block[i];
    }
    else
      return;
  }

  // Update checksum in header
  fseek(scpfile, scp_endofheader-(sizeof(uint32_t)), SEEK_SET);
  fwrite(&checksum, 1, sizeof(uint32_t), scpfile);
}
