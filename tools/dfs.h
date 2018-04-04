#ifndef _DFS_H_
#define _DFS_H_

// Acorn DFS geometry and layout
#define DFS_SECTORSIZE 256
#define DFS_SECTORSPERTRACK 10
#define DFS_TRACKSIZE (DFS_SECTORSIZE*DFS_SECTORSPERTRACK)

#define DFS_MAXTRACKS 80

#define DFS_SECTORSPERSIDE (DFS_SECTORSPERTRACK*DFS_MAXTRACKS)
#define DFS_WHOLEDISKSIZE (DFS_SECTORSPERSIDE*DFS_SECTORSIZE)

#define DFS_MAXFILES 31

extern void dfs_gettitle(const int head, char *title, const int titlelen);
extern void dfs_showinfo(const int head);
extern int dfs_validcatalogue(const int head);

#endif
