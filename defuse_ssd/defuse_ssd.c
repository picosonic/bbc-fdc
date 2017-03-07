#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fuse.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/statvfs.h>

#include "defuse_ssd.h"

// Command line options
static struct options
{
  const char *ssd_filename;
} options;

#define OPTION(t, p) { t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
  OPTION("--name=%s", ssd_filename),
  FUSE_OPT_END
};

unsigned char diskbuff[1024*1024];
size_t disksize=0;

// Store the whole disk image in RAM
unsigned char wholedisk[WHOLEDISKSIZE];
unsigned char sectorstatus[SECTORSTATUSSIZE];

// Read a sector from disk buffer to the whole disk image
int readsector(const int sector)
{
  int track;

  // Make sure sector is within the bounds of the disk image
  if ((sector<0) || (sector>=((MAXTRACKS-1)*SECTORSPERTRACK)))
    return 0;

  // Determine which track this sector belongs to
  track=sector/SECTORSPERTRACK;

  // Check if we need to read the sector from disk
  if (sectorstatus[sector]!=GOODDATA)
  {
    memcpy(&wholedisk[sector*SECTORSIZE], &diskbuff[sector*SECTORSIZE], SECTORSIZE);
    sectorstatus[sector]=GOODDATA;
  }

  return SECTORSIZE;
}

// Make sure the first two sectors (containing the catalogue) are loaded
int readcatalogue()
{
  return (readsector(0)+readsector(1));
}

// Invalidate the status of each sector to force a read
void clearcache()
{
  int i;

  for (i=0; i<SECTORSTATUSSIZE; i++)
    sectorstatus[i]=NODATA;
}

// Read nth DFS filename from catalogue
//   but don't add "$."
//   return the "Locked" state of the file
int getfilename(const int entry, char *filename)
{
  int i;
  int len;
  unsigned char fchar;
  int locked;

  len=0;

  locked=(wholedisk[(entry*8)+7] & 0x80)?1:0;

  fchar=wholedisk[(entry*8)+7] & 0x7f;

  if (fchar!='$')
  {
    filename[len++]=fchar;
    filename[len++]='.';
  }

  for (i=0; i<7; i++)
  {
    fchar=wholedisk[(entry*8)+i] & 0x7f;

    if (fchar==' ') break;
    filename[len++]=fchar;
  }

  filename[len++]=0;

  return locked;
}

// Search catalogue for an entry by name
int findentry(const char *path)
{
  int numfiles;
  char filename[15];
  int i;

  numfiles=wholedisk[(1*SECTORSIZE)+5]/8;
  filename[0]='/';

  for (i=1; ((i<=numfiles) && (i<MAXFILES)); i++)
  {
    getfilename(i, &filename[1]);
    if (strcmp(filename, path)==0) return i;
  }

  return -1;
}

// Return file length for nth entry in DFS catalogue
unsigned long getfilelength(const int entry)
{
  unsigned long offset;

  offset=(1*SECTORSIZE)+8+((entry-1)*8);

  return ((((wholedisk[offset+6]&0x30)>>4)<<16) |
          ((wholedisk[offset+5])<<8) |
          ((wholedisk[offset+4])));
}

// Return start sector for nth entry in DFS catalogue
unsigned long getstartsector(const int entry)
{
  unsigned long offset;

  offset=(1*SECTORSIZE)+8+((entry-1)*8);

  return (((wholedisk[offset+6]&0x03)<<8) |
          ((wholedisk[offset+7])));
}

// Get file attributes (only length is useful) for named file
static int dfs_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;
  int entry;
  unsigned long len;

  // Make sure we have the catalogue loaded
  readcatalogue();

  // Clear the return values
  memset(stbuf, 0, sizeof(struct stat));

  // Check if the root folder is being enumerated
  if (strcmp(path, "/") == 0)
  {
    stbuf->st_mode = S_IFDIR | 0555; // r-xr-xr-x
    stbuf->st_nlink = 2;
  }
  else
  if (findentry(path)>=1)
  {
    entry=findentry(path);
    stbuf->st_mode = S_IFREG | 0444; // r--r--r--
    stbuf->st_nlink = 1;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    len=getfilelength(entry);
    stbuf->st_size = len;
    stbuf->st_blocks = len/SECTORSIZE;
    if ((stbuf->st_blocks*SECTORSIZE)!=len) stbuf->st_blocks++;
    stbuf->st_blksize=SECTORSIZE;
  }
  else
    res = -ENOENT;

  return res;
}

// Return filenames for all entries in the catalogue
static int dfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
       off_t offset, struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;
  int numfiles;
  char filename[10];
  int i;

  // Make sure we have the catalogue loaded
  readcatalogue();

  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  numfiles=wholedisk[(1*SECTORSIZE)+5]/8;

  for (i=1; ((i<=numfiles) && (i<MAXFILES)); i++)
  {
    getfilename(i, filename);
    filler(buf, filename, NULL, 0);
  }

  return 0;
}

// "open()" request handler
static int dfs_open(const char *path, struct fuse_file_info *fi)
{
  // Make sure we have the catalogue loaded
  readcatalogue();

  if (findentry(path)<1)
    return -ENOENT;

  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;

  return 0;
}

// "read()" request handler
static int dfs_read(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fi)
{
  size_t len;
  int entry;
  int startsector;
  int numsectors;
  int i;
  (void) fi;

  // Make sure we have the catalogue loaded
  readcatalogue();

  // Find the named filename
  entry=findentry(path);

  if (entry<1)
    return -ENOENT;

  // Determine file length and number of sectors used
  len = getfilelength(entry);
  startsector = getstartsector(entry);
  numsectors = (len/SECTORSIZE);
  if ((numsectors*SECTORSIZE)!=len)
    numsectors++;

  // Check read() offset falls within file boundary
  if (offset < len)
  {
    // If read() length is more than file length, then truncate to file length
    if (offset + size > len)
      size = len - offset;

    // Make sure we have enough sectors loaded to satisfy any requests for this file
    for (i=startsector; i<=((startsector+numsectors)-1); i++)
      readsector(i);

    // Copy file data
    memcpy(buf, &wholedisk[(startsector*SECTORSIZE)+offset], size);
  }
  else
    size = 0;

  return size;
}

// "statfs()" request handler
static int dfs_statfs(const char *path, struct statvfs *stbuf)
{
  (void) path;
  int sectorusage=2;
  int numfiles;
  int i;

  // Make sure we have the catalogue loaded
  readcatalogue();

  // Clear the return variables
  memset(stbuf, 0, sizeof(struct statvfs));

  // Determine number of files in the catalogue
  numfiles=wholedisk[(1*SECTORSIZE)+5]/8;

  // Iterate through each file in catalogue to determine sector usage
  for (i=1; ((i<=numfiles) && (i<MAXFILES)); i++)
  {
    sectorusage+=(getfilelength(i)/SECTORSIZE);

    if (((getfilelength(i)/SECTORSIZE)*SECTORSIZE)!=getfilelength(i))
      sectorusage++;
  }

  stbuf->f_bsize = SECTORSIZE;
  stbuf->f_frsize = SECTORSIZE;
  stbuf->f_blocks = SECTORSTATUSSIZE; // Total number of sectors
  stbuf->f_bfree = SECTORSTATUSSIZE - sectorusage; // Total number of free sectors
  stbuf->f_bavail = stbuf->f_bfree;
  stbuf->f_flag = ST_RDONLY | ST_NOSUID;
  stbuf->f_namemax = 10;

  return 0;
}

// Report write-protected to truncate operations
static int dfs_truncate(const char *path, off_t size)
{
  (void) path;
  (void) size;

  return -EACCES;
}

// Report write-protected to write operations
static int dfs_write(const char *path, const char *buf, size_t size,
          off_t offset, struct fuse_file_info *fi)
{
  (void) path;
  (void) buf;
  (void) size;
  (void) offset;
  (void) fi;

  return -EACCES;
}

// Report write-protected to "mknod"
static int dfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
  (void) path;
  (void) mode;
  (void) rdev;

  return -EACCES;
}

// Report write-protected to "unlink" ("rm")
static int dfs_unlink(const char *path)
{
  (void) path;

  return -EACCES;
}

// Report write-protected to "mkdir"
static int dfs_mkdir(const char *path, mode_t mode)
{
  (void) path;
  (void) mode;

  return -EACCES;
}

// FUSE operations, function table
static struct fuse_operations dfs_oper =
{
  .getattr  = dfs_getattr,
  .readdir  = dfs_readdir,
  .open     = dfs_open,
  .read     = dfs_read,
  .statfs   = dfs_statfs,
  // Non-used ones below
  .truncate = dfs_truncate,
  .write    = dfs_write,
  .mknod    = dfs_mknod,
  .unlink   = dfs_unlink,
  .mkdir    = dfs_mkdir,
};

// Program entry point
int main(int argc, char **argv)
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  FILE *fp;

  // Start with a blank SSD filename
  options.ssd_filename = strdup("");

  // Set the file mode creation mask
  umask(0);

  // Mark each sector as initially unread
  clearcache();

  // Parse command line options
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

  // Check for a SSD filename on command line
  if (strlen(options.ssd_filename)!=0)
  {
    // Check file is readable
    fp=fopen(options.ssd_filename, "r");
    if (fp!=NULL)
    {
      // Attempt to read the whole disk from the file
      disksize=fread(diskbuff, 1, sizeof(diskbuff), fp);

      fclose(fp);
    }
    else
    {
      fprintf(stderr, "Unable to open file '%s'\n", options.ssd_filename);
      return 1;
    }
  }
  else
  {
    fprintf(stderr, "Specify SSD file on commandline with --name=<ssdfile>\n");
    return 1;
  }

  return fuse_main(args.argc, args.argv, &dfs_oper, NULL);
}
