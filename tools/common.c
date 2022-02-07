#include <string.h>
#include <strings.h>

#include "common.h"

int compare_extension(const char *filename, const char *ext)
{
  char *dot=strrchr(filename, '.');

  // Check for no extension
  if ((!dot) || (dot==filename) || (dot[1]==0))
    return ((strlen(ext)==0) || (ext[1]=='.'));

  // Do comparision
  return (strcasecmp(dot, ext)==0);
}
