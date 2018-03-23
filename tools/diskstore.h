#ifndef _DISKSTORE_H_
#define _DISKSTORE_H_

// For sector status
#define NODATA 0
#define BADDATA 1
#define GOODDATA 2

#define MODFM 0
#define MODMFM 1

typedef struct DiskSector
{
  // Physical position of sector on disk
  unsigned char physical_track;
  unsigned char physical_head;
  unsigned char physical_sector;

  // Logical position of sector from IDAM
  unsigned int id_rawpos;
  unsigned char logical_track;
  unsigned char logical_head;
  unsigned char logical_sector;
  unsigned char logical_size;
  unsigned int idcrc;

  // Sector data
  unsigned char modulation;
  unsigned int data_rawpos;
  unsigned int datatype;
  unsigned int datasize;
  unsigned char *data;
  unsigned int datacrc;

  struct DiskSector *next;
} Disk_Sector;

// Linked list
extern Disk_Sector *Disk_SectorsRoot;

// Summary information
int diskstore_minsectorsize;
int diskstore_maxsectorsize;
int diskstore_minsectorid;
int diskstore_maxsectorid;

// Initialise disk storage
extern void diskstore_init();

// Add a sector to the disk storage
extern int diskstore_addsector(const unsigned char modulation, const unsigned char physical_track, const unsigned char physical_head, const unsigned char logical_track, const unsigned char logical_head, const unsigned char logical_sector, const unsigned char logical_size, const unsigned int idcrc, const unsigned int datatype, const unsigned int datasize, const unsigned char *data, const unsigned int datacrc);

// Search for a sector within the disk storage
extern Disk_Sector *diskstore_findexactsector(const unsigned char physical_track, const unsigned char physical_head, const unsigned char logical_track, const unsigned char logical_head, const unsigned char logical_sector, const unsigned char logical_size, const unsigned int idcrc, const unsigned int datatype, const unsigned int datasize, const unsigned int datacrc);
extern Disk_Sector *diskstore_findlogicalsector(const unsigned char logical_track, const unsigned char logical_head, const unsigned char logical_sector);
extern Disk_Sector *diskstore_findhybridsector(const unsigned char physical_track, const unsigned char physical_head, const unsigned char logical_sector);
extern Disk_Sector *diskstore_findnthsector(const unsigned char physical_track, const unsigned char physical_head, const unsigned char nth_sector);

// Processing of sectors
extern unsigned char diskstore_countsectors(const unsigned char physical_track, const unsigned char physical_head);
extern unsigned char diskstore_countsectormod(const unsigned char modulation);
extern void diskstore_sortsectors();

// Dump the contents of the disk storage for debug purposes
extern void diskstore_dumpsectorlist(const int maxtracks);

#endif
