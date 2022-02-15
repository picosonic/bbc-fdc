#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "hardware.h"
#include "woz.h"

long woz_readtrack(FILE *wozfile, const int track, const int side, char* buf, const uint32_t buflen)
{
  return -1; // 0 on success
}

int woz_readheader(FILE *wozfile)
{
  if (wozfile==NULL) return -1;

  return -1; // 0 on success
}
