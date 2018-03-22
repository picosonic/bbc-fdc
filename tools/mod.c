#include "hardware.h"
#include "fm.h"
#include "mfm.h"
#include "mod.h"

int mod_debug=0;

void mod_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt)
{
  fm_process(sampledata, samplesize, attempt);
  //mfm_process(sampledata, samplesize, attempt);
}

// Initialise modulation
void mod_init(const int debug)
{
  mod_debug=debug;

  fm_init(debug);
  mfm_init(debug);
}
