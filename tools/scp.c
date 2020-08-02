#include <time.h>
#include <sys/time.h>

#include "scp.h"

/*

SuperCard Pro Image File Specification v2.2 - June 17, 2020

--------------------------------------------------------------------------------------

This information is copyright (C) 2012-2020 By Jim Drew.  Permission is granted
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

; FLAGS BIT DEFINES (BIT NUMBER)

FB_INDEX = 0x00                    ; clear = no index reference, set = flux data starts at index
FB_TPI = 0x01                      ; clear = drive is 48TPI, set = drive is 96TPI (only applies to 5.25" drives!)
FB_RPM = 0x02                      ; clear = drive is 300 RPM drive, set = drive is 360 RPM drive
FB_TYPE = 0x03                     ; clear = image is has original flux data, set = image is flux data that has been normalized
FB_MODE = 0x04                     ; clear = image is read-only, set = image is read/write capable
FB_FOOTER = 0x05                   ; clear = image does not contain a footer, set = image contains a footer at the end of it

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
tape_MFM = 0x01                    ; xxxx 0010

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

void scp_writeheader(FILE *scpfile, const unsigned int rotations, const unsigned int starttrack, const unsigned int endtrack, const float rpm, const unsigned int sides)
{
  unsigned int i;

  if (scpfile==NULL) return;

  // Magic and version
  fprintf(scpfile, "%s%c", SCP_MAGIC, SCP_VERSION);

  // Disk type ??
  fprintf(scpfile, "%c", 0x85);

  // Rotations captures, start and end tracks (multiplied by sides)
  fprintf(scpfile, "%c%c%c", rotations, starttrack, endtrack);

  // Flags
  fprintf(scpfile, "%c", ((rpm>330)?SCP_FLAGS_360RPM:0x0) | ((endtrack>40)?SCP_FLAGS_96TPI:0x0) | SCP_FLAGS_INDEX); // TODO add 0x20 when footer added

  // Bit cell encoding ??
  fprintf(scpfile, "%c", 0x00);

  // Sides / Heads
  fprintf(scpfile, "%c", sides);

  // Capture resolution, default is 80ns, which has closest SCP multiplier of 75ns
  fprintf(scpfile, "%c", 2); // TODO determine best value for this

  // Blank checksum  - to be filled in later (calculated from next byte to EOF)
  fprintf(scpfile, "%c%c%c%c", 0, 0, 0, 0);

  // Blank offsets to tracks - to be filled in later
  for (i=0; i<endtrack; i++)
    fprintf(scpfile, "%c%c%c%c", 0, 0, 0, 0);
}

void scp_writetrack(FILE *scpfile, const int track, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const unsigned int rotations)
{
  if (scpfile==NULL) return;

  // Track ID
  fprintf(scpfile, "%s", SCP_TRACK);

  // Track number
  fprintf(scpfile, "%c", track);

  // TODO
}

void scp_finalise(FILE *scpfile, const unsigned int endtrack)
{
  struct tm tim;
  struct timeval tv;

  if (scpfile==NULL) return;

  // Write timestamp
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tim);

  fprintf(scpfile, "%02d/%02d/%d %02d:%02d:%02d", tim.tm_mday, tim.tm_mon+1, tim.tm_year+1900, tim.tm_hour, tim.tm_min, tim.tm_sec);


  // TODO write optional footer

  // TODO update track data offsets

  // TODO update 32bit checksum
}
