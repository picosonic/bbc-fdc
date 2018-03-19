#include "crc.h"
#include "hardware.h"
#include "diskstore.h"
#include "mfm.h"

int mfm_debug=0;

void mfm_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt)
{
}

void mfm_init(const int debug)
{
  mfm_debug=debug;
}
