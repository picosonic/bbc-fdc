#include "scp.h"

/*

SuperCard Pro Image File Specification v2.2 - June 17, 2020

--------------------------------------------------------------------------------------

This information is copyright (C) 2012-2020 By Jim Drew.  Permission is granted
for inclusion with any source code when keeping this copyright notice.

See www.cbmstuff.com for information on purchasing SuperCard Pro.

--------------------------------------------------------------------------------------

From : https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt

*/

void scp_writeheader(FILE *scpfile)
{
  if (scpfile==NULL) return;

  // TODO
}

void scp_writetrack(FILE *scpfile, const int track, const int side, const unsigned char *rawtrackdata, const unsigned long rawdatalength, const unsigned int rotations)
{
  if (scpfile==NULL) return;

  // TODO
}
