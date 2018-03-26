#include <stdlib.h>
#include <strings.h>

#include "dfi.h"

/*

All integers in this file format are stored in big-endian form.

The DFI file consists of a 4-byte magic string, followed by a series of disc sample blocks.

The magic string is "DFER" for old-style DiscFerret images, or "DFE2" for new-style DiscFerret images.

Each sample block has a header --

uint16_be cylinder;
uint16_be head;
uint16_be sector;
uint32_be data_length;

The cylinder number starts at zero and carrys up to the number of cylinders on the disk. The head number follows the same rule (starts at zero, increments for each additional head). The sector number is optional, and only used for hard-sectored discs. For soft-sectored discs, it is set to zero. Data_length indicates the number of bytes of data which follow. 

Decoding data
=============

Old-style images
----------------
carry = 0
For every byte in the stream:
  if (byte AND 0x7f) == 0x00:
    carry = carry + 127
  else:
    emit((byte AND 0x7f) + carry)
    carry = 0
if carry > 0:
  emit(carry)

New-style images
----------------
carry = 0   // running carry
abspos = 0  // absolute timing position in stream
For every byte in the stream:
  if ((byte AND 0x7f) == 0x7f):    // if lower 7 bit value is 0x7f
    carry = carry + 127            // ... then there was a carry
    abspos = abspos + 127
  else if (byte AND 0x80) != 0:    // if high bit set in byte
    carry = carry + (byte & 0x7F)  // add lower 7 bit value to carry and absolute-position
    abspos = abspos + (byte & 0x7F)
    add_index_position(abspos)     // this store was caused by an index pulse: save its absolute position
  else:                            // here byte < 0x7f
    emit((byte AND 0x7f) + carry)  // this store was caused by a data transition: store the time-delta since last transition
    abspos = abspos + (byte & 0x7F)
    carry = 0                      // reset carry

// Carry may be nonzero at the end of this loop. In this case, there was an incomplete transition, which should be discarded.

// emit()                 stores the timing delta for a data pulse
// add_index_position()   stores the absolute timing position of an index pulse
// 

From : https://www.discferret.com/wiki/DFI_image_format

-----------------------------------------

DiscFerret data dumps contain the raw contents of the acquisition RAM.

Briefly:
  The Most Significant bit (0x80) means "index pulse active during this timeslot"
  A 0x00 means the carryer overflowed. Increment carry by 127.
  Anything else is a timing value. T_val = carry + data_byte, then set the carry to zero.

*/

void dfi_writeheader(FILE *dfifile)
{
  if (dfifile==NULL) return;

  fprintf(dfifile, "%s", DFI_MAGIC);
}

// DFE2 encode raw binary sample data
unsigned long dfi_encodedata(unsigned char *buffer, const unsigned long maxdfilen, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  unsigned long dfilen=0;
  unsigned char c;
  char state=0;
  unsigned int i, j;
  unsigned char carry=0;

  // Determine starting sample level
  state=(rawtrackdata[0]&0x80)>>7;

  for (i=0; i<rawdatalength; i++)
  {
    c=rawtrackdata[i];

    // Process each of the 8 sample bits looking for state change
    for (j=0; j<8; j++)
    {
      carry++;

      if (carry==DFI_CARRY)
      {
        // Check for buffer overflow
        if ((dfilen+1)>=maxdfilen) return 0;

        buffer[dfilen++]=DFI_CARRY;
        carry=0;
      }

      if (((c&0x80)>>7)!=state)
      {
        state=1-state;

        // Having seen an "original" .dfi file, it looks like it only stores READ pin rising edge deltas
        if (state==1)
        {
          // Check for buffer overflow
          if ((dfilen+1)>=maxdfilen) return 0;

          buffer[dfilen++]=carry;
          carry=0;
        }
      }

      c=c<<1;
    }
  }

  return dfilen;
}

void dfi_writetrack(FILE *dfifile, const int track, const int side, const unsigned char *rawtrackdata, const unsigned long rawdatalength)
{
  unsigned char trackheader[10];
  unsigned char *dfidata;
  unsigned long dfidatalength;

  if (dfifile==NULL) return;

  // Clear header values
  bzero(trackheader, sizeof(trackheader));

  // Track/Cylinder
  trackheader[0]=(track&0xff00)>>8;
  trackheader[1]=track&0xff;

  // Head/Side
  trackheader[2]=(side&0xff00)>>8;
  trackheader[3]=side&0xff;

  // Sector/Record
  // Assume 0 - soft sectored

  // Convert data to DFI 2 format
  dfidata=malloc(rawdatalength);
  if (dfidata==NULL) return;

  dfidatalength=dfi_encodedata(dfidata, rawdatalength, rawtrackdata, rawdatalength);
  if (dfidatalength==0)
  {
    free(dfidata);
    return;
  }
 
  // Data length
  trackheader[6]=(dfidatalength&0xff000000)>>24;
  trackheader[7]=(dfidatalength&0xff0000)>>16;
  trackheader[8]=(dfidatalength&0xff00)>>8;
  trackheader[9]=dfidatalength&0xff;

  // Write track
  fwrite(trackheader, sizeof(trackheader), 1, dfifile);
  fwrite(dfidata, dfidatalength, 1, dfifile);

  free(dfidata);
}
