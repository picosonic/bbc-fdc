#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "diskstore.h"
#include "hardware.h"
#include "mod.h"
#include "crc32.h"

Disk_Sector *Disk_SectorsRoot;

// For stats
int diskstore_mintrack=-1;
int diskstore_maxtrack=-1;
int diskstore_minhead=-1;
int diskstore_maxhead=-1;
int diskstore_minsectorsize=-1;
int diskstore_maxsectorsize=-1;
int diskstore_minsectorid=-1;
int diskstore_maxsectorid=-1;

// For absolute disk access
int diskstore_abstrack=-1;
int diskstore_abshead=-1;
int diskstore_abssector=-1;
int diskstore_abssecoffs=-1;
unsigned long diskstore_absoffset=0;

int diskstore_usepll=0;
int diskstore_debug=0;

// Find sector in store to make sure there is no exact match when adding
Disk_Sector *diskstore_findexactsector(const uint8_t physical_track, const uint8_t physical_head, const uint8_t logical_track, const uint8_t logical_head, const uint8_t logical_sector, const uint8_t logical_size, const unsigned int idcrc, const unsigned int datatype, const unsigned int datasize, const unsigned int datacrc)
{
  Disk_Sector *curr;

  curr=Disk_SectorsRoot;

  while (curr!=NULL)
  {
    if ((curr->physical_track==physical_track) &&
        (curr->physical_head==physical_head) &&
        (curr->logical_track==logical_track) &&
        (curr->logical_head==logical_head) &&
        (curr->logical_sector==logical_sector) &&
        (curr->logical_size==logical_size) &&
        (curr->idcrc==idcrc) &&
        (curr->datatype==datatype) &&
        (curr->datasize==datasize) &&
        (curr->datacrc==datacrc))
      return curr;

    curr=curr->next;
  }

  return NULL;
}

// Find sector by logical position
Disk_Sector *diskstore_findlogicalsector(const uint8_t logical_track, const uint8_t logical_head, const uint8_t logical_sector)
{
  Disk_Sector *curr;

  curr=Disk_SectorsRoot;

  while (curr!=NULL)
  {
    if ((curr->logical_track==logical_track) &&
        (curr->logical_head==logical_head) &&
        (curr->logical_sector==logical_sector))
      return curr;

    curr=curr->next;
  }

  return NULL;
}

// Find sector by hybrid physical/logical position
Disk_Sector *diskstore_findhybridsector(const uint8_t physical_track, const uint8_t physical_head, const uint8_t logical_sector)
{
  Disk_Sector *curr;

  curr=Disk_SectorsRoot;

  while (curr!=NULL)
  {
    if ((curr->physical_track==physical_track) &&
        (curr->physical_head==physical_head) &&
        (curr->logical_sector==logical_sector))
      return curr;

    curr=curr->next;
  }

  return NULL;
}

// Find nth sector for given physical track/head
Disk_Sector *diskstore_findnthsector(const uint8_t physical_track, const uint8_t physical_head, const uint8_t nth_sector)
{
  Disk_Sector *curr;
  int n;

  curr=Disk_SectorsRoot;
  n=0;

  while (curr!=NULL)
  {
    if ((curr->physical_track==physical_track) && (curr->physical_head==physical_head))
    {
      if (n==nth_sector)
        return curr;
      n++;
    }

    curr=curr->next;
  }

  return NULL;
}

// Count how many sectors we have for given physical track/head
unsigned char diskstore_countsectors(const uint8_t physical_track, const uint8_t physical_head)
{
  Disk_Sector *curr;
  int n;

  curr=Disk_SectorsRoot;
  n=0;

  while (curr!=NULL)
  {
    if ((curr->physical_track==physical_track) && (curr->physical_head==physical_head))
      n++;

    curr=curr->next;
  }

  return n;
}

// Count how many sectors were found with given modulation
unsigned int diskstore_countsectormod(const unsigned char modulation)
{
  Disk_Sector *curr;
  unsigned int n;

  curr=Disk_SectorsRoot;
  n=0;

  while (curr!=NULL)
  {
    if (curr->modulation==modulation)
      n++;

    curr=curr->next;
  }

  return n;
}

// Compare two sectors to determine if they should be swapped
int diskstore_comparesectors(Disk_Sector *item1, Disk_Sector *item2, const int sortmethod, const int rotations)
{
  if ((item1==NULL) || (item2==NULL))
    return 0;

  // Compare physical tracks
  if (item1->physical_track > item2->physical_track)
    return 1;

  if (item1->physical_track < item2->physical_track)
    return -1;

  // Compare physical heads, tracks were the same
  if (item1->physical_head > item2->physical_head)
    return 1;

  if (item1->physical_head < item2->physical_head)
    return -1;

  if (sortmethod==SORTBYID)
  {
    // Compare sectors, heads were the same
    if (item1->logical_sector > item2->logical_sector)
      return 1;

    if (item1->logical_sector < item2->logical_sector)
      return -1;
  }
  else
  if (sortmethod==SORTBYPOS)
  {
    unsigned long samplesperrotation;
    int item1dpos;
    int item2dpos;

    samplesperrotation=(mod_samplesize/rotations);

    item1dpos=((item1->data_pos%samplesperrotation)*100)/samplesperrotation;
    item2dpos=((item2->data_pos%samplesperrotation)*100)/samplesperrotation;

    if (item1dpos > item2dpos)
      return 1;

    if (item1dpos < item2dpos)
      return -1;
  }

  // Everything was the same
  return 0;
}

// Bubble sort the sectors to one of the sort methods
void diskstore_sortsectors(const int sortmethod, const int rotations)
{
  int swaps;
  Disk_Sector *curr;
  Disk_Sector *swap;

  // Check for empty diskstore
  if (Disk_SectorsRoot==NULL)
    return;

  do
  {
    Disk_Sector *prev;

    swaps=0;
    curr=Disk_SectorsRoot;
    prev=NULL;

    while ((curr!=NULL) && (curr->next!=NULL))
    {
      // Check if these two sectors need swapping
      if (diskstore_comparesectors(curr, curr->next, sortmethod, rotations)==1)
      {
        // Swap the pointers over in the linked list
        swap=curr->next;
        curr->next=swap->next;
        swap->next=curr;

        if (prev==NULL)
          Disk_SectorsRoot=swap;
        else
          prev->next=swap;

        swaps++;
      }

      // Move on to the next pair of sectors
      prev=curr;
      curr=curr->next;
    }
  } while (swaps>0);
}

// Add a sector to linked list
int diskstore_addsector(const unsigned char modulation, const uint8_t physical_track, const uint8_t physical_head, const uint8_t logical_track, const uint8_t logical_head, const uint8_t logical_sector, const uint8_t logical_size, const long id_pos, const unsigned int idcrc, const long data_pos, const unsigned int datatype, const unsigned int datasize, const unsigned char *data, const unsigned int datacrc)
{
  Disk_Sector *curr;
  Disk_Sector *newitem;

  // First check if we already have this sector
  if (diskstore_findexactsector(physical_track, physical_head, logical_track, logical_head, logical_sector, logical_size, idcrc, datatype, datasize, datacrc)!=NULL)
    return 0;

//  fprintf(stderr, "Adding physical T:%d H:%d  |  logical C:%d H:%d R:%d N:%d (%.4x) [%.2x] %d data bytes (%.4x)\n", physical_track, physical_head, logical_track, logical_head, logical_sector, logical_size, idcrc, datatype, datasize, datacrc);

  newitem=malloc(sizeof(Disk_Sector));
  if (newitem==NULL) return 0;

  newitem->physical_track=physical_track;
  newitem->physical_head=physical_head;

  newitem->logical_track=logical_track;
  newitem->logical_head=logical_head;
  newitem->logical_sector=logical_sector;
  newitem->logical_size=logical_size;
  newitem->idcrc=idcrc;
  newitem->id_pos=id_pos;
  newitem->data_pos=data_pos;
  newitem->data_endpos=mod_datapos;

  newitem->modulation=modulation;

  newitem->datatype=datatype;
  newitem->datasize=datasize;

  newitem->data=malloc(datasize);
  if (newitem->data!=NULL)
    memcpy(newitem->data, data, datasize);

  newitem->datacrc=datacrc;

  newitem->next=NULL;

  if ((diskstore_mintrack==-1) || (physical_track<diskstore_mintrack))
    diskstore_mintrack=physical_track;

  if ((diskstore_maxtrack==-1) || (physical_track>diskstore_maxtrack))
    diskstore_maxtrack=physical_track;

  if ((diskstore_minhead==-1) || (physical_head<diskstore_minhead))
    diskstore_minhead=physical_head;

  if ((diskstore_maxhead==-1) || (physical_head>diskstore_maxhead))
    diskstore_maxhead=physical_head;

  if ((diskstore_minsectorsize==-1) || (datasize<(unsigned int)diskstore_minsectorsize))
    diskstore_minsectorsize=datasize;

  if ((diskstore_maxsectorsize==-1) || (datasize>(unsigned int)diskstore_maxsectorsize))
    diskstore_maxsectorsize=datasize;

  if ((diskstore_maxsectorid==-1) || (logical_sector>diskstore_maxsectorid))
    diskstore_maxsectorid=logical_sector;

  if ((diskstore_minsectorid==-1) || (logical_sector<diskstore_minsectorid))
    diskstore_minsectorid=logical_sector;

  // Add the new sector to the dynamic linked list
  if (Disk_SectorsRoot==NULL)
  {
    Disk_SectorsRoot=newitem;
  }
  else
  {
    curr=Disk_SectorsRoot;

    while (curr->next!=NULL)
      curr=curr->next;

    curr->next=newitem;
  }

  return 1;
}

// Delete all saved sectors
void diskstore_clearallsectors()
{
  Disk_Sector *curr;

  curr=Disk_SectorsRoot;

  while (curr!=NULL)
  {
    void *prev;

    if (curr->data!=NULL)
      free(curr->data);

    prev=curr;
    curr=curr->next;

    if (prev!=NULL)
      free(prev);
  }

  Disk_SectorsRoot=NULL;
}

// Dump a list of all sectors found
void diskstore_dumpsectorlist()
{
  Disk_Sector *curr;
  int dtrack, dhead;
  int n;
  int totalsectors=0;

  for (dtrack=0; dtrack<(diskstore_maxtrack+1); dtrack+=hw_stepping)
  {
    fprintf(stderr, "TRACK %.2d: ", dtrack/hw_stepping);

    for (dhead=(diskstore_minhead==-1?0:diskstore_minhead); dhead<(diskstore_maxhead==-1?2:diskstore_maxhead+1); dhead++)
    {
      n=0;
      do
      {
        curr=diskstore_findnthsector(dtrack, dhead, n++);

        if (curr!=NULL)
        {
          totalsectors++;
          fprintf(stderr, "%d[%d] ", curr->logical_sector, curr->physical_head);
        }
      } while (curr!=NULL);
    }
    fprintf(stderr, "\n");
  }

  fprintf(stderr, "Total extracted sectors: %d\n", totalsectors);
}

// Dump a list of all sectors found
void diskstore_dumpbadsectors(FILE* fh)
{
  int dtrack, dhead,dsector;

  fprintf(fh, "Head, Track, Sector\n");

  for (dhead=0; dhead<(diskstore_maxhead+1); dhead++)
    for (dtrack=0; dtrack<(diskstore_maxtrack+1); dtrack+=hw_stepping)
      for(dsector=0; dsector<(diskstore_maxsectorid+1); dsector++)
         if(diskstore_findhybridsector(dtrack, dhead, dsector)==NULL)
            fprintf(fh, "%.2X, %.2X, %.2X\n", dhead, dtrack, dsector);
}

// Dump a layout map of where data was found on the disk surface
void diskstore_dumplayoutmap(const int rotations)
{
  Disk_Sector *curr;
  int dtrack, dhead;
  char cyldata[100+1];
  int i, n, ppos, ppos2;
  unsigned long samplesperrotation;
  int mtrack;

  if ((diskstore_maxtrack>-1) && (diskstore_maxtrack<(int)hw_maxtracks))
    mtrack=diskstore_maxtrack+1;
  else
    mtrack=hw_maxtracks+1;

  fprintf(stderr, "Samples : %lu  Rotations : %d\n", mod_samplesize, rotations);
  samplesperrotation=(mod_samplesize/rotations);

  fprintf(stderr, "TRACK[HEAD]\n");
  for (dtrack=0; ((dtrack<mtrack) && (dtrack<(int)hw_maxtracks)); dtrack+=hw_stepping)
  {
    for (dhead=(diskstore_minhead==-1?0:diskstore_minhead); dhead<(diskstore_maxhead==-1?2:diskstore_maxhead+1); dhead++)
    {
      // Clear cylinder data
      for (i=0; i<(100+1); i++)
        cyldata[i]='.';
      cyldata[100]=0;

      fprintf(stderr, "%.2d[%.1d]: ", dtrack/hw_stepping, dhead);

      n=0;
      do
      {
        curr=diskstore_findnthsector(dtrack, dhead, n++);

        if (curr!=NULL)
        {
          ppos=((curr->data_pos%samplesperrotation)*100)/samplesperrotation;
          ppos2=((curr->data_endpos%samplesperrotation)*100)/samplesperrotation;

          // Check for data block wrap
          if (ppos>ppos2)
          {
            for (i=ppos; i<100; i++)
              cyldata[i%100]='d';

            ppos=0;
          }

          for (i=ppos; i<ppos2; i++)
            cyldata[i%100]='d';

          ppos=((curr->id_pos%samplesperrotation)*100)/samplesperrotation;
          cyldata[ppos%100]='s';
        }
      } while (curr!=NULL);

      fprintf(stderr, "%s\n", cyldata);
    }
  }
}

// Absolute seek
void diskstore_absoluteseek(const unsigned long offset, const int interlacing, const int maxtracks)
{
  unsigned long diskoffs;

  // Validate track range
  if ((diskstore_maxtrack==-1) || (diskstore_mintrack==-1))
    return;

  // Validate head range
  if ((diskstore_maxhead==-1) || (diskstore_minhead==-1) || (diskstore_maxhead>1))
    return;

  // Validate sector size
  if ((diskstore_maxsectorsize==-1) || (diskstore_minsectorsize==-1) || (diskstore_minsectorsize!=diskstore_maxsectorsize))
    return;

  // Initialise to start of disk
  diskstore_abstrack=diskstore_mintrack;
  diskstore_abshead=diskstore_minhead;
  diskstore_abssector=diskstore_minsectorid;
  diskstore_abssecoffs=0;

  diskoffs=offset;

  // Convert absolute offset to C/H/S/sector offset
  while (diskoffs>=(unsigned int)diskstore_minsectorsize)
  {
    diskstore_abssecoffs+=diskstore_minsectorsize;
    diskoffs-=diskstore_minsectorsize;

    // Check for pointer going to next sector
    if (diskstore_abssecoffs>=diskstore_minsectorsize)
    {
      diskstore_abssecoffs-=diskstore_minsectorsize;
      diskstore_abssector++;

      // Check for pointer going to next track or head
      if (diskstore_abssector>diskstore_maxsectorid)
      {
        diskstore_abssector=diskstore_minsectorid;

        switch (interlacing)
        {
          case SEQUENCED: // All of head 0, then all of head 1 (if head 1 exists)
            diskstore_abstrack++;

            if (diskstore_abstrack>maxtracks)
            {
              diskstore_abstrack=diskstore_mintrack;
              diskstore_abshead++;
            }
            break;

          case INTERLEAVED: // For each track, head 0 then head 1 (most common for double sided)
            diskstore_abshead++;

            if (diskstore_abshead>diskstore_maxhead)
            {
              diskstore_abshead=diskstore_minhead;
              diskstore_abstrack++;
            }
            break;

          default:
            break;
        }
      }
    }

    // Check for seeking past end of disk, to wrap around back to start
    if ((diskstore_abshead>diskstore_maxhead) || (diskstore_abstrack>maxtracks))
    {
//printf("DS wrap around\n");
      diskstore_abstrack=diskstore_mintrack;
      diskstore_abshead=diskstore_minhead;
      diskstore_abssector=diskstore_minsectorid;
    }
  }

  // Store new offsets
  diskstore_abssecoffs=diskoffs;
  diskstore_absoffset=offset;
}

// Absolute read
unsigned long diskstore_absoluteread(char *buffer, const unsigned long bufflen, const int interlacing, const int maxtracks)
{
  Disk_Sector *curr;
  unsigned long numread=0; // Total bytes returned so far

  // Start by blanking out the response buffer
  bzero(buffer, bufflen);

  // Continue reading until requested length satisfied
  while (numread<bufflen)
  {
    unsigned long toread=0; // Number of bytes to read from current sector

    // Determine how much to read from this sector
    toread=diskstore_minsectorsize-diskstore_abssecoffs;
    if ((numread+toread)>bufflen)
      toread=bufflen-numread;

    // Find this sector
    curr=diskstore_findhybridsector(diskstore_abstrack, diskstore_abshead, diskstore_abssector);

    // If sector not found in the store, maybe it hasn't been read yet
    if ((curr==NULL) || (curr->data==NULL))
    {
      unsigned char *samplebuffer;
      unsigned long samplebuffsize;

      samplebuffsize=((hw_samplerate/HW_ROTATIONSPERSEC)/BITSPERBYTE)*3;
      samplebuffer=malloc(samplebuffsize);

      if (samplebuffer!=NULL)
      {
        hw_seektotrack(diskstore_abstrack);
        hw_sideselect(diskstore_abshead);
        hw_sleep(1);
        hw_samplerawtrackdata(samplebuffer, samplebuffsize);
        mod_process(samplebuffer, samplebuffsize, 99, 0);

        if (diskstore_usepll)
          mod_process(samplebuffer, samplebuffsize, 99, diskstore_usepll);

        free(samplebuffer);
        samplebuffer=NULL;
      }
      else
        return numread;

      // Look again
      curr=diskstore_findhybridsector(diskstore_abstrack, diskstore_abshead, diskstore_abssector);
    }

    if ((curr!=NULL) && (curr->data!=NULL))
    {
      // Prevent reads beyond current sector memory
      if ((diskstore_abssecoffs+toread)>curr->datasize)
        toread=curr->datasize-diskstore_abssecoffs;

      memcpy(&buffer[numread], &curr->data[diskstore_abssecoffs], toread);
    }
    else
      return numread;

    numread+=toread;

    // Move absolute position forward
    diskstore_absoffset+=toread;

    // Seek to next sector
    diskstore_absoluteseek(diskstore_absoffset, interlacing, maxtracks);
  }

  return numread;
}

uint32_t diskstore_calctrackcrc(const uint32_t initial, const uint8_t physical_track, const uint8_t physical_head)
{
  Disk_Sector *curr;
  uint32_t crc=initial;
  int n;

  n=0;
  do
  {
    curr=diskstore_findnthsector(physical_track, physical_head, n++);

    if (curr!=NULL)
      if (curr->data!=NULL)
      {
        unsigned char dtype=curr->datatype;

        crc=CRC32_CalcStream(crc, &dtype, sizeof(dtype));
        crc=CRC32_CalcStream(crc, curr->data, curr->datasize);
      }
  } while (curr!=NULL);

  return crc;
}

uint32_t diskstore_calcdiskcrc(const uint8_t physical_head)
{
  int dtracks, dtrack, dhead;
  uint32_t diskcrc=0x0;

  if (diskstore_maxtrack>60)
    dtracks=80;
  else
    dtracks=40;

  for (dtrack=0; dtrack<(dtracks+1); dtrack+=hw_stepping)
    for (dhead=((physical_head!=1)?0:1); dhead<((physical_head==0)?1:2); dhead++)
    {
      uint32_t trackcrc=0x0;

      // Get track CRC32
      trackcrc=diskstore_calctrackcrc(0, dtrack, dhead);
      if (diskstore_debug)
        fprintf(stderr, "T%d.%d : CRC32 %.8X\n", dtrack/hw_stepping, dhead, trackcrc);

      // Update disk CRC32
      diskcrc=CRC32_CalcStream(diskcrc, (unsigned char *)&trackcrc, sizeof(uint32_t));
    }

  return diskcrc;
}

void diskstore_init(const int debug, const int usepll)
{
  Disk_SectorsRoot=NULL;

  diskstore_debug=debug;
  diskstore_usepll=usepll;

  diskstore_mintrack=-1;
  diskstore_maxtrack=-1;
  diskstore_minhead=-1;
  diskstore_maxhead=-1;
  diskstore_minsectorsize=-1;
  diskstore_maxsectorsize=-1;
  diskstore_minsectorid=-1;
  diskstore_maxsectorid=-1;

  diskstore_abstrack=-1;
  diskstore_abshead=-1;
  diskstore_abssector=-1;
  diskstore_abssecoffs=-1;
  diskstore_absoffset=0;

  atexit(diskstore_clearallsectors);
}
