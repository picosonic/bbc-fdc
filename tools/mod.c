#include <stdio.h>

#include "hardware.h"
#include "fm.h"
#include "mfm.h"
#include "mod.h"

int mod_debug=0;

long mod_hist[MOD_HISTOGRAMSIZE];
int mod_peak[MOD_PEAKSIZE];
int mod_peaks;

void mod_buildhistogram(const unsigned char *sampledata, const unsigned long samplesize)
{
  int j;
  char level,bi;
  unsigned char c;
  int count;
  unsigned long datapos;

  if (mod_debug)
    fprintf(stderr, "Creating histogram for data sampled at %ld\n", hw_samplerate);

  // Clear histogram
  for (j=0; j<MOD_HISTOGRAMSIZE; j++) mod_hist[j]=0;

  // Build histogram
  level=(sampledata[0]&0x80)>>7;
  bi=level;
  count=0;

  for (datapos=0; datapos<samplesize; datapos++)
  {
    c=sampledata[datapos];

    for (j=0; j<BITSPERBYTE; j++)
    {
      bi=((c&0x80)>>7);

      count++;

      if (bi!=level)
      {
        level=1-level;

        // Look for rising edge
        if (level==1)
        {
          if (count<MOD_HISTOGRAMSIZE)
            mod_hist[count]++;

          count=0;
        }
      }

      c=c<<1;
    }
  }
}

float mod_samplestoms(const long samples)
{
  return ((float)1/(float)hw_samplerate)*(float)MICROSECONDSINSECOND*(float)samples;
}

int mod_findpeaks(const unsigned char *sampledata, const unsigned long samplesize)
{
  int j;
  long localmaxima;
  long threshold;
  int inpeak;

  mod_buildhistogram(sampledata, samplesize);

  // Find largest histogram value
  localmaxima=0;
  for (j=0; j<MOD_HISTOGRAMSIZE; j++)
    if (mod_hist[j]>mod_hist[localmaxima])
      localmaxima=j;

  if (mod_debug)
    fprintf(stderr, "Maximum peak at %ld samples, %.3fms\n", localmaxima, mod_samplestoms(localmaxima));

  // Set threshold at 10% of maximum
  threshold=mod_hist[localmaxima]/10;

  // Decimate histogram to remove values below threshold
  for (j=0; j<MOD_HISTOGRAMSIZE; j++)
    if (mod_hist[j]<=threshold)
      mod_hist[j]=0;

  // Find peaks
  inpeak=0; mod_peaks=0; localmaxima=0;
  for (j=0; j<MOD_HISTOGRAMSIZE; j++)
  {
    if (mod_hist[j]!=0)
    {
      if (mod_hist[j]>mod_hist[localmaxima])
        localmaxima=j;

      // Mark the start of a new peak
      if (inpeak==0)
      {
        mod_peaks++;
        inpeak=1;
      }
    }
    else
    {
      if (inpeak==1)
      {
        if (mod_debug)
          fprintf(stderr, "  Peak at %ld %.3fms\n", localmaxima, mod_samplestoms(localmaxima));

        if (mod_peaks<MOD_PEAKSIZE)
          mod_peak[mod_peaks-1]=localmaxima;

        localmaxima=0;
      }

      inpeak=0;
    }
  }

  if (mod_debug)
    fprintf(stderr, "Found %d peaks\n", mod_peaks);

  return mod_peaks;
}

void mod_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt)
{
  mod_findpeaks(sampledata, samplesize);

  fm_process(sampledata, samplesize, attempt);
  //mfm_process(sampledata, samplesize, attempt);
}

// Initialise modulation
void mod_init(const int debug)
{
  mod_debug=debug;

  mod_peaks=0;

  fm_init(debug);
  mfm_init(debug);
}
