#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diskstore.h"

Disk_Sector *Disk_SectorsRoot;

// Find sector in store to make sure there is no exact match when adding
Disk_Sector *diskstore_findexactsector(const unsigned char physical_track, const unsigned char physical_head, const unsigned char logical_track, const unsigned char logical_head, const unsigned char logical_sector, const unsigned int idcrc, const unsigned int datatype, const unsigned int datasize, const unsigned int datacrc)
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
Disk_Sector *diskstore_findlogicalsector(const unsigned char logical_track, const unsigned char logical_head, const unsigned char logical_sector)
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
Disk_Sector *diskstore_findhybridsector(const unsigned char physical_track, const unsigned char physical_head, const unsigned char logical_sector)
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

// Find nth sector for given track
Disk_Sector *diskstore_findnthsector(const unsigned char physical_track, const unsigned char nth_sector)
{
  Disk_Sector *curr;
  int n;

  curr=Disk_SectorsRoot;
  n=0;

  while (curr!=NULL)
  {
    if (curr->physical_track==physical_track)
    {
      if (n==nth_sector)
        return curr;
      n++;
    }

    curr=curr->next;
  }

  return NULL;
}

// Add a sector to linked list
void diskstore_addsector(const unsigned char physical_track, const unsigned char physical_head, const unsigned char logical_track, const unsigned char logical_head, const unsigned char logical_sector, const unsigned int idcrc, const unsigned int datatype, const unsigned int datasize, const unsigned char *data, const unsigned int datacrc)
{
  Disk_Sector *curr;
  Disk_Sector *newitem;

  // First check if we already have this sector
  if (diskstore_findexactsector(physical_track, physical_head, logical_track, logical_head, logical_sector, idcrc, datatype, datasize, datacrc)!=NULL)
    return;

  //printf("Adding physical T:%d H:%d  |  logical T:%d H:%d S:%d (%.4x) [%.2x] %d data bytes (%.4x)\n", physical_track, physical_head, logical_track, logical_head, logical_sector, idcrc, datatype, datasize, datacrc);

  newitem=malloc(sizeof(Disk_Sector));
  if (newitem==NULL) return;

  newitem->physical_track=physical_track;
  newitem->physical_head=physical_head;

  newitem->logical_track=logical_track;
  newitem->logical_head=logical_head;
  newitem->logical_sector=logical_sector;
  newitem->idcrc=idcrc;

  newitem->datatype=datatype;
  newitem->datasize=datasize;

  newitem->data=malloc(datasize);
  if (newitem->data!=NULL)
    memcpy(newitem->data, data, datasize);

  newitem->datacrc=datacrc;

  newitem->next=NULL;

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
}

// Delete all saved sectors
void diskstore_clearallsectors()
{
  Disk_Sector *curr;
  void *prev;

  curr=Disk_SectorsRoot;

  while (curr!=NULL)
  {
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
void diskstore_dumpsectorlist(const int maxtracks)
{
  Disk_Sector *curr;
  int dtrack;
  int n;
  int totalsectors=0;

  for (dtrack=0; dtrack<maxtracks; dtrack++)
  {
    fprintf(stderr, "TRACK %.2d: ", dtrack);

    n=0;
    do
    {
      curr=diskstore_findnthsector(dtrack, n++);

      if (curr!=NULL)
      {
        totalsectors++;
        fprintf(stderr, "%d ", curr->logical_sector);
      }
    } while (curr!=NULL);
    fprintf(stderr, "\n");
  }

  fprintf(stderr, "Total extracted sectors: %d\n", totalsectors);
}

void diskstore_init()
{
  Disk_SectorsRoot=NULL;

  atexit(diskstore_clearallsectors);
}
