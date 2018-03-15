#ifndef _DFS_H_
#define _DFS_H_

#include "diskstore.h"

// Acorn DFS geometry and layout
#define DFS_SECTORSIZE 256
#define DFS_SECTORSPERTRACK 10
#define DFS_TRACKSIZE (DFS_SECTORSIZE*DFS_SECTORSPERTRACK)

#define DFS_MAXTRACKS 80

#define DFS_SECTORSPERSIDE (DFS_SECTORSPERTRACK*DFS_MAXTRACKS)
#define DFS_WHOLEDISKSIZE (DFS_SECTORSPERSIDE*DFS_SECTORSIZE)

#define DFS_MAXFILES 31

extern int dfs_getfilename(Disk_Sector *sector0, const int entry, char *filename);
extern unsigned long dfs_getloadaddress(Disk_Sector *sector1, const int entry);
extern unsigned long dfs_getexecaddress(Disk_Sector *sector1, const int entry);
extern unsigned long dfs_getfilelength(Disk_Sector *sector1, const int entry);
extern unsigned long dfs_getstartsector(Disk_Sector *sector1, const int entry);
extern void dfs_showinfo(Disk_Sector *sector0, Disk_Sector *sector1);
extern int dfs_validcatalogue(Disk_Sector *sector0, Disk_Sector *sector1);

#endif
