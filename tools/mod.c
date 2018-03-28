#include <stdio.h>

#include "hardware.h"
#include "fm.h"
#include "mfm.h"
#include "mod.h"

int mod_debug=0;

long mod_hist[MOD_HISTOGRAMSIZE];
int mod_peak[MOD_PEAKSIZE];
int mod_peaks;
char mod_density=MOD_DENSITYAUTO;

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

int mod_haspeak(const float ms)
{
  int i;
  float peakms;

  for (i=0; i<mod_peaks; i++)
  {
    peakms=mod_samplestoms(mod_peak[i]);

    // Look within 10% of nominal
    if ((ms>=(peakms*0.90)) && (ms<=(peakms*1.1)))
      return 1;
  }

  return 0;
}

void mod_checkdensity()
{
  // MFM ED
  // 01=1ms, 001=1.5ms, 0001=2ms
  if ((mod_haspeak(1)+mod_haspeak(1.5)+mod_haspeak(2))==3)
  {
    mod_density|=MOD_DENSITYMFMED;

    return;
  }

  // MFM HD
  // 01=2ms, 001=3ms, 0001=4ms
  if ((mod_haspeak(2)+mod_haspeak(3)+mod_haspeak(4))==3)
  {
    mod_density|=MOD_DENSITYMFMHD;

    return;
  }

  // MFM DD
  // 01=4ms, 001=6ms, 0001=8ms
  if ((mod_haspeak(4)+mod_haspeak(6)+mod_haspeak(8))==3)
  {
    mod_density|=MOD_DENSITYMFMDD;

    return;
  }

  // FM SD
  // 1=4ms, 01=8ms
  if ((mod_haspeak(4)+mod_haspeak(8))==2)
  {
    mod_density|=MOD_DENSITYFMSD;

    return;
  }
}

unsigned char mod_getclock(const unsigned int datacells)
{
  unsigned char clock;

  clock=((datacells&0x8000)>>8);
  clock|=((datacells&0x2000)>>7);
  clock|=((datacells&0x0800)>>6);
  clock|=((datacells&0x0200)>>5);
  clock|=((datacells&0x0080)>>4);
  clock|=((datacells&0x0020)>>3);
  clock|=((datacells&0x0008)>>2);
  clock|=((datacells&0x0002)>>1);

  return clock;
}

unsigned char mod_getdata(const unsigned int datacells)
{
  unsigned char data;

  data=((datacells&0x4000)>>7);
  data|=((datacells&0x1000)>>6);
  data|=((datacells&0x0400)>>5);
  data|=((datacells&0x0100)>>4);
  data|=((datacells&0x0040)>>3);
  data|=((datacells&0x0010)>>2);
  data|=((datacells&0x0004)>>1);
  data|=((datacells&0x0001)>>0);

  return data;
}

void mod_process(const unsigned char *sampledata, const unsigned long samplesize, const int attempt)
{
  mod_findpeaks(sampledata, samplesize);

  mod_checkdensity();

  if ((mod_density==MOD_DENSITYAUTO) || ((mod_density&MOD_DENSITYFMSD)!=0))
    fm_process(sampledata, samplesize, FM_BITCELL, attempt);

  if ((mod_density==MOD_DENSITYAUTO) || ((mod_density&MOD_DENSITYMFMDD)!=0))
    mfm_process(sampledata, samplesize, MFM_BITCELLDD, attempt);

  if ((mod_density==MOD_DENSITYAUTO) || ((mod_density&MOD_DENSITYMFMHD)!=0))
    mfm_process(sampledata, samplesize, MFM_BITCELLHD, attempt);

  if ((mod_density==MOD_DENSITYAUTO) || ((mod_density&MOD_DENSITYMFMED)!=0))
    mfm_process(sampledata, samplesize, MFM_BITCELLED, attempt);
}

// Initialise modulation
void mod_init(const int debug)
{
  mod_debug=debug;

  mod_peaks=0;

  fm_init(debug);
  mfm_init(debug);
}
