#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "hardware.h"

// Acorn DFS geometry and layout
#define SECTORSIZE 256
#define SECTORSPERTRACK 10
#define TRACKSIZE (SECTORSIZE*SECTORSPERTRACK)
#define MAXFILES 31

// SPI read buffer size
#define SPIBUFFSIZE (1024*1024)

// Microseconds in a bitcell window for single-density FM
#define BITCELL 4

// Sample rate in Hz
#define SAMPLERATE 12500000

// Microseconds in a second
#define USINSECOND 1000000

// Disk bitstream block size
#define BLOCKSIZE 2048

// Whole disk image
#define SECTORSPERSIDE (SECTORSPERTRACK*MAXTRACKS)
#define WHOLEDISKSIZE (SECTORSPERSIDE*SECTORSIZE)

// For sector status
#define NODATA 0
#define BADDATA 1
#define GOODDATA 2

// For type of capture
#define DISKCAT 0
#define DISKIMG 1
#define DISKRAW 2

#define RETRIES 10

int debug=0;
int singlesided=1;
int capturetype=DISKCAT;

unsigned char *spibuffer;
unsigned char *ibuffer;
unsigned int datacells;
int bits=0;
int hadAM=0;
int info=0;

// Most recent address mark
int blocktype=0;
int blocksize=0;

// Most recent address mark
unsigned long idpos;
unsigned char track, head, sector;
unsigned int datasize;
unsigned char idamtrack, idamhead, idamsector, idamlength;

int maxtracks=MAXTRACKS;

unsigned char bitstream[BLOCKSIZE];
unsigned int bitlen=0;

// Store the whole disk image in RAM
unsigned char wholedisk[MAXHEADS][WHOLEDISKSIZE];
unsigned char sectorstatus[MAXHEADS][SECTORSPERSIDE];

unsigned long datapos=0;

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

// Read nth DFS filename from catalogue
//   but don't add "$."
//   return the "Locked" state of the file
int getfilename(int entry, char *filename)
{
  int i;
  int len;
  unsigned char fchar;
  int locked;

  len=0;

  locked=(ibuffer[(entry*8)+7] & 0x80)?1:0;

  fchar=ibuffer[(entry*8)+7] & 0x7f;

  if (fchar!='$')
  {
    filename[len++]=fchar;
    filename[len++]='.';
  }

  for (i=0; i<7; i++)
  {
    fchar=ibuffer[(entry*8)+i] & 0x7f;

    if (fchar==' ') break;
    filename[len++]=fchar;
  }

  filename[len++]=0;

  return locked;
}

// Return load address for nth entry in DFS catalogue
unsigned long getloadaddress(int entry)
{
  unsigned long loadaddress;

  loadaddress=((((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+6]&0x0c)>>2)<<16) |
               ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+1])<<8) |
               ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)])));

  if (loadaddress & 0x30000) loadaddress |= 0xFF0000;

  return loadaddress;
}

// Return execute address for nth entry in DFS catalogue
unsigned long getexecaddress(int entry)
{
  unsigned long execaddress;

  execaddress=((((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+6]&0xc0)>>6)<<16) |
               ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+3])<<8) |
               ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+2])));

  if (execaddress & 0x30000) execaddress |= 0xFF0000;

  return execaddress;
}

// Return file length for nth entry in DFS catalogue
unsigned long getfilelength(int entry)
{
  return ((((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+6]&0x30)>>4)<<16) |
          ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+5])<<8) |
          ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+4])));
}

// Return file starting sector for nth entry in DFS catalogue
unsigned long getstartsector(int entry)
{
  return (((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+6]&0x03)<<8) |
          ((ibuffer[(1*SECTORSIZE)+8+((entry-1)*8)+7])));
}

// Display info read from the disk catalogue from sectors 00 and 01
void showinfo(int infohead)
{
  int i, j;
  int numfiles;
  int locked;
  unsigned char bootoption;
  size_t tracks, totalusage, totalsectors, totalsize, sectorusage;
  char filename[10];

  ibuffer=&wholedisk[infohead][0];

  printf("Head: %d\n", infohead);
  printf("Disk title : \"");
  for (i=0; i<8; i++)
  {
    if (ibuffer[i]==0) break;
    printf("%c", ibuffer[i]);
  }
  for (i=0; i<4; i++)
  {
    if (ibuffer[(1*SECTORSIZE)+i]==0) break;
    printf("%c", ibuffer[(1*SECTORSIZE)+i]);
  }
  printf("\"\n");

  totalsectors=(((ibuffer[(1*SECTORSIZE)+6]&0x03)<<8) | (ibuffer[(1*SECTORSIZE)+7]));
  tracks=totalsectors/SECTORSPERTRACK;
  //maxtracks=tracks;
  totalsize=totalsectors*SECTORSIZE;
  printf("Disk size : %d tracks (%d sectors, %d bytes)\n", tracks, totalsectors, totalsize);

  bootoption=(ibuffer[(1*SECTORSIZE)+6]&0x30)>>4;
  printf("Boot option: %d ", bootoption);
  switch (bootoption)
  {
    case 0:
      printf("Nothing");
      break;

    case 1:
      printf("*LOAD !BOOT");
      break;

    case 2:
      printf("*RUN !BOOT");
      break;

    case 3:
      printf("*EXEC !BOOT");
      break;

    default:
      printf("Unknown");
      break;
  }
  printf("\n");

  totalusage=0; sectorusage=2;
  printf("Write operations made to disk : %.2x\n", ibuffer[(1*SECTORSIZE)+4]); // Stored in BCD

  numfiles=ibuffer[(1*SECTORSIZE)+5]/8;
  printf("Catalogue entries : %d\n", numfiles);

  for (i=1; ((i<=numfiles) && (i<MAXFILES)); i++)
  {
    locked=getfilename(i, filename);

    printf("%-9s", filename);

    printf(" %.6lx %.6lx %.6lx %.3lx", getloadaddress(i), getexecaddress(i), getfilelength(i), getstartsector(i));
    totalusage+=getfilelength(i);
    sectorusage+=(getfilelength(i)/SECTORSIZE);
    if (((getfilelength(i)/SECTORSIZE)*SECTORSIZE)!=getfilelength(i))
      sectorusage++;

    if (locked) printf(" L");
    printf("\n");
  }

  printf("Total disk usage : %d bytes (%d%% of disk)\n", totalusage, (totalusage*100)/(totalsize-(2*SECTORSIZE)));
  printf("Remaining catalogue space : %d files, %d unused disk sectors\n", MAXFILES-numfiles, (((ibuffer[(1*SECTORSIZE)+6]&0x03)<<8) | (ibuffer[(1*SECTORSIZE)+7])) - sectorusage);
}

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void addbit(unsigned char bit)
{
  unsigned char clock, data;
  unsigned char dataCRC;

  datacells=((datacells<<1)&0xffff);
  datacells|=bit;
  bits++;

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

    // Detect standard address marks
    switch (datacells)
    {
      case 0xf77a: // d7 fc
        if (debug)
          printf("\n[%x] Index Address Mark\n", datapos);
        blocktype=data;
        bitlen=0;
        hadAM=1;
        break;

      case 0xf57e: // c7 fe
        if (debug)
          printf("\n[%x] ID Address Mark\n", datapos);
        blocktype=data;
        blocksize=7;
        bitlen=0;
        hadAM=1;
        idpos=datapos;
        break;

      case 0xf56f: // c7 fb
        if (debug)
          printf("\n[%x] Data Address Mark, distance from ID %x\n", datapos, datapos-idpos);
        blocktype=data;
        bitlen=0;
        hadAM=1;
        break;

      case 0xf56a: // c7 f8
        if (debug)
          printf("\n[%x] Deleted Data Address Mark\n", datapos);
        blocktype=data;
        bitlen=0;
        hadAM=1;
        break;

      default:
        break;
    }

    // Process block data depending on type
    switch (blocktype)
    {
      case 0xf8: // Deleted data block
      case 0xfb: // Data block
        // force it to be standard DFS 256 bytes/sector (+blocktype+CRC)
        blocksize=SECTORSIZE+3;

        // Keep reading until we have the whole block in bitsteam[]
        if (bitlen<blocksize)
        {
          //printf(" %.4x %.2x %c %.2x %c\n", datacells, clock, ((clock>=' ')&&(clock<='~'))?clock:'.', data, ((data>=' ')&&(data<='~'))?data:'.');

          if (debug)
          {
            if ((clock!=0xff) && (bitlen>0))
              printf("Invalid CLOCK %.4x %.2x %c %.2x %c\n", datacells, clock, ((clock>=' ')&&(clock<='~'))?clock:'.', data, ((data>=' ')&&(data<='~'))?data:'.');
          }

          bitstream[bitlen++]=data;
        }
        else
        {
          // All the bytes for this "data" block have been read, so process them
          if (debug)
            printf("  %.2x CRC %.2x%.2x", blocktype, bitstream[bitlen-2], bitstream[bitlen-1]);

          dataCRC=(calc_crc(&bitstream[0], bitlen-2)==((bitstream[bitlen-2]<<8)|bitstream[bitlen-1]))?GOODDATA:BADDATA;

          // Report if the CRC matches
          if (debug)
          {
            if (dataCRC==GOODDATA)
              printf(" OK\n");
            else
              printf(" BAD\n");
          }

          // Sanitise the data from the most recent unused address mark
          if ((track<MAXTRACKS) && (track<maxtracks) && (sector<SECTORSPERTRACK))
          {
            unsigned int sectorsize=(blocksize-3);
            unsigned int tracksize=(sectorsize*10);

            // Check the track number matches
            if (track!=hw_currenttrack)
            {
              if (debug)
                printf("*** Track ID mismatch %d != %d ***\n", track, hw_currenttrack);
 
              // Override the read track number with the track we should be on
              track=hw_currenttrack;
            }

            // See if we need this sector, store it if current status is either EMPTY or BAD
            if (sectorstatus[hw_currenthead][(SECTORSPERTRACK*track)+sector]!=GOODDATA)
            {
              memcpy(&wholedisk[hw_currenthead][(track*tracksize)+(sector*sectorsize)], &bitstream[1], sectorsize);

              sectorstatus[hw_currenthead][(SECTORSPERTRACK*track)+sector]=dataCRC;
            }
          }
          else
          {
            if (debug)
              printf("  Invalid ID Track %d Sector %d\n", track, sector);
          }

          // Do a catalogue if we haven't already and sector 00 and 01 have been read correctly for this side
          if ((info==0) && (sectorstatus[hw_currenthead][0]==GOODDATA) && (sectorstatus[hw_currenthead][1]==GOODDATA))
          {
            showinfo(hw_currenthead);
            info++;
          }

          // Require subsequent data blocks to have a valid ID block
          track=0xff;
          head=0xff;
          sector=0xff;
          datasize=0;
          idpos=0;

          blocktype=0;
        }
        break;

      case 0xfe: // ID block
        if (bitlen<blocksize)
        {
          bitstream[bitlen++]=data;
        }
        else
        {
          dataCRC=(calc_crc(&bitstream[0], bitlen-2)==((bitstream[5]<<8)|bitstream[6]))?GOODDATA:BADDATA;

          if (debug)
          {
            printf("Track %d ", bitstream[1]);
            printf("Head %d ", bitstream[2]);
            printf("Sector %d ", bitstream[3]);
            printf("Data size %d ", bitstream[4]);
            printf("CRC %.2x%.2x", bitstream[5], bitstream[6]);

            if (dataCRC==GOODDATA)
              printf(" OK\n");
            else
              printf(" BAD\n");
          }

          if (dataCRC==GOODDATA)
          {
            track=bitstream[1];
            head=bitstream[2];
            sector=bitstream[3];

            // Record IDAM values
            idamtrack=track;
            idamhead=head;
            idamsector=sector;
            idamlength=bitstream[4];

            switch(bitstream[4])
            {
              case 0x00:
              case 0x01:
              case 0x02:
              case 0x03:
                datasize=(128<<bitstream[4])+3;
                break;

              default:
                if (debug)
                printf("Invalid record length %.2x\n", bitstream[4]);

                datasize=256+3;
                break;
            }
          }

          blocktype=0;
        }
        break;

      case 0xfc: // Index block
        if (debug)
          printf("Index address mark\n");
        blocktype=0;
        break;

      default:
        if (blocktype!=0)
        {
          if (debug)
            printf("** Unknown block address mark %.2x **\n", blocktype);
          blocktype=0;
        }
        break;
    }

    // Look for any GAP (outside of the data block/deleted data block) to resync bitstream
    if ((clock==0xff) && (data==0xff))
    {
        if (bitlen>=blocksize)
          hadAM=0;
    }

    if (hadAM==0)
      bits=16;
    else
      bits=0;
  }
}

void process(int attempt)
{
  int j,k, pos;
  char state,bi=0;
  unsigned char c, clock, data;
  int count;
  unsigned long avg[50];
  int bitwidth=0;
  float defaultwindow;
  int bucket1, bucket01;

  defaultwindow=((float)BITCELL/((float)1/((float)SAMPLERATE/(float)USINSECOND)));
  bucket1=defaultwindow+(defaultwindow/2);
  bucket01=(defaultwindow*2)+(defaultwindow/2);

  state=(spibuffer[0]&0x80)>>7;
  bi=state;
  count=0;
  datacells=0;

  // Check for processing in disk detection mode
  if (attempt==99)
  {
    idamtrack=0xff;
    idamhead=0xff;
    idamsector=0xff;
    idamlength=0xff;
  }

  // Initialise last sector ID mark to blank
  track=0xff;
  head=0xff;
  sector=0xff;
  datasize=0;
  blocktype=0;
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

      if (bi!=state)
      {
        state=1-state;

        // Look for rising edge
        if (state==1)
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

int main(int argc,char **argv)
{
  unsigned int i, j, trackpos;
  unsigned char retry, side, drivestatus;

  if (geteuid() != 0)
  {
    fprintf(stderr,"Must be run as root\n");
    return 1;
  }

  // Allocate memory for SPI buffer
  spibuffer=malloc(SPIBUFFSIZE);
  if (spibuffer==NULL)
  {
    fprintf(stderr, "\n");
    return 2;
  }

  printf("Start\n");

  if (!hw_init())
  {
    fprintf(stderr, "Failed hardware init\n");
    return 3;
  }

  // Install signal handlers to make sure motor is stopped
  atexit(exitFunction);
  signal(SIGINT, sig_handler); // Ctrl-C
  signal(SIGSEGV, sig_handler); // Seg fault
  signal(SIGTERM, sig_handler); // Termination request

  drivestatus=hw_detectdisk();

  if (drivestatus==NODRIVE)
  {
    fprintf(stderr, "Failed to detect drive\n");
    return 4;
  }

  if (drivestatus==NODISK)
  {
    fprintf(stderr, "Failed to detect disk in drive\n");
    return 5;
  }

  // Select drive, depending on jumper
  hw_driveselect();

  // Start MOTOR
  hw_startmotor();

  // Wait for motor to get up to speed
  sleep(1);

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

  // Work out what type of capture we are doing
  if (argc==2)
  {
    if (strstr(argv[1], ".ssd")!=NULL)
    {
      singlesided=1;

      diskimage=fopen(argv[1], "w+");
      if (diskimage==NULL)
        printf("Unable to save disk image\n");
      else
        capturetype=DISKIMG;
    }
    else
    if (strstr(argv[1], ".dsd")!=NULL)
    {
      diskimage=fopen(argv[1], "w+");
      if (diskimage==NULL)
        printf("Unable to save disk image\n");
      else
        capturetype=DISKIMG;
    }
    else
    if (strstr(argv[1], ".raw")!=NULL)
    {
      rawdata=fopen(argv[1], "w+");
      if (rawdata==NULL)
        printf("Unable to save rawdata\n");
      else
        capturetype=DISKRAW;
    }
  }

  // In catalogue mode, try to determine what type of disk is in what type of drive
  if (capturetype==DISKCAT)
  {
    // Seek to track 2
    hw_seektotrack(2);
    // Select upper side
    hw_sideselect(0);

    // Wait for a bit after seek to allow drive speed to settle
    sleep(1);

    // Sample track
    hw_waitforindex();
    hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);
    process(99);
    // Check readability
    if ((idamtrack==0xff) || (idamhead==0xff) || (idamsector==0xff) || (idamlength==0xff))
    {
      printf("No valid sector IDs found\n");
    }
    else
    {
      unsigned char othertrack=idamtrack;
      unsigned char otherhead=idamhead;
      unsigned char othersector=idamsector;
      unsigned char otherlength=idamlength;

      // Select lower side
      hw_sideselect(1);

      // Wait for a bit after head switch to allow drive to settle
      sleep(1);

      // Sample track
      hw_waitforindex();
      hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);
      process(99);
      // Check readability
      if ((idamtrack==0xff) || (idamhead==0xff) || (idamsector==0xff) || (idamlength==0xff))
      {
        // Only upper side was readable
        printf("Single-sided disk detected\n");
      }
      else
      {
        // Both sides readable
        singlesided=0;

        // If IDAM shows same head, then double-sided separate
        if (idamhead==otherhead)
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
          printf("40 track disk detected in 80 track drive\n");

        // If IDAM cylinder shows 4 then 80 track in 40 track drive
        if (othertrack==4)
          printf("80 track disk detected in 40 track drive\n");
      }
    }
  }

  // Mark each sector as being unread
  for (i=0; i<SECTORSPERSIDE; i++)
  {
    for (j=0; j<MAXHEADS; j++)
      sectorstatus[j][i]=NODATA;
  }

  maxtracks=80; // TODO

  for (side=0; side<MAXHEADS; side++)
  {
    info=0; // Request a directory listing for this side of the disk
    hw_sideselect(side);

    hw_seektotrackzero();

    for (i=0; i<maxtracks; i++)
    {
      for (retry=0; retry<RETRIES; retry++)
      {
        hw_seektotrack(i);

        // Wait for a bit after seek to allow drive speed to settle
        sleep(1);

        if ((retry==0) && (debug))
        {
          printf("Sampling data for track %.2X head %.2x\n", i, side);
        }

        // Wait for index rising edge prior to sampling to align as much as possible with index
        // Values seen on a scope are 200ms between pulses of 4.28ms width
        hw_waitforindex();

        // Sampling data
        hw_samplerawtrackdata((char *)spibuffer, SPIBUFFSIZE);

        // Process the raw sample data to extract FM encoded data
        if (capturetype!=DISKRAW)
        {
          process(retry);

          // Determine if we have successfully read the whole track
          trackpos=(SECTORSPERTRACK*i);
          if ((sectorstatus[side][trackpos+0]==GOODDATA) &&
              (sectorstatus[side][trackpos+1]==GOODDATA) &&
              (sectorstatus[side][trackpos+2]==GOODDATA) &&
              (sectorstatus[side][trackpos+3]==GOODDATA) &&
              (sectorstatus[side][trackpos+4]==GOODDATA) &&
              (sectorstatus[side][trackpos+5]==GOODDATA) &&
              (sectorstatus[side][trackpos+6]==GOODDATA) &&
              (sectorstatus[side][trackpos+7]==GOODDATA) &&
              (sectorstatus[side][trackpos+8]==GOODDATA) &&
              (sectorstatus[side][trackpos+9]==GOODDATA))
            break;

          printf("Retry attempt %d, sectors ", retry+1);
          for (j=0; j<SECTORSPERTRACK; j++)
            if (sectorstatus[side][trackpos+j]!=GOODDATA) printf("%.2d ", j);
          printf("\n");
        }
        else
          break; // No retries in RAW mode
      }

      if (capturetype!=DISKRAW)
      {
        // If we're on side 1 track 1 and no second catalogue found, then assume single sided
        if ((side==1) && (i==1))
        {
          if (info==0)
          {
            singlesided=1;

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

      // If we're only doing a catalogue, then don't read any more tracks from this side
      if (capturetype==DISKCAT)
        break;
    } // track loop

    // If disk image is being written but we only have a single sided disk, then stop here
    if (singlesided)
      break;

    printf("\n");
  } // side loop

  // Return the disk head to track 0 following disk imaging
  hw_seektotrackzero();

  printf("Finished\n");

  // Output list of good/bad tracks, but only if we're doing a whole disk image
  if (diskimage!=NULL)
  {
    for (i=0; i<SECTORSPERSIDE; i++)
    {
      if ((i%10)==0) printf("%d:%.2d ", 0, i/SECTORSPERTRACK);

      switch (sectorstatus[0][i])
      {
        case NODATA:
          printf("_");
          break;

        case BADDATA:
          printf("/");
          break;

        case GOODDATA:
          printf("*");
          break;

        default:
          break;
      }
      if ((i%10)==9) printf("\n");
    }
    printf("\n");

    if (!singlesided)
    {
      for (i=0; i<SECTORSPERSIDE; i++)
      {
        if ((i%10)==0) printf("%d:%.2d ", 1, i/SECTORSPERTRACK);

        switch (sectorstatus[1][i])
        {
          case NODATA:
            printf("_");
            break;

          case BADDATA:
            printf("/");
            break;

          case GOODDATA:
            printf("*");
            break;

          default:
            break;
        }
        if ((i%10)==9) printf("\n");
      }
      printf("\n");
    }
  }

  // Write the data to disk image file (if required)
  if (diskimage!=NULL)
  {
    for (i=0; ((i<MAXTRACKS) && (i<maxtracks)); i++)
    {
      for (j=0; j<SECTORSPERTRACK; j++)
      {
        // Write
        fwrite(&wholedisk[0][(i*TRACKSIZE)+(j*SECTORSIZE)], 1, SECTORSIZE, diskimage);
      }

      // Write DSD interlaced as per BeebEm
      if (!singlesided)
      {
        for (j=0; j<SECTORSPERTRACK; j++)
          fwrite(&wholedisk[1][(i*TRACKSIZE)+(j*SECTORSIZE)], 1, SECTORSIZE, diskimage);
      }
    }
  }

  // Close disk image files (if open)
  if (diskimage!=NULL) fclose(diskimage);
  if (rawdata!=NULL) fclose(rawdata);

  hw_stopmotor();

  if (spibuffer!=NULL)
  {
    free(spibuffer);
    spibuffer=NULL;
  }

  return 0;
}
