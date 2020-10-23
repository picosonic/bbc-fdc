#include <stdlib.h>
#include <stdint.h>

#include "pll.h"

// Based on MAME implementation
//   https://github.com/mamedev/mame/blob/master/src/lib/formats/flopimg.cpp

// Tweaking PLL parameters
float pll_periodadjust=(5.0/100.0);
float pll_phaseadjust=(65.0/100.0);
float pll_minperiod=(75.0/100.0);
float pll_maxperiod=(125.0/100.0);

// Linked list of all the assigned PLLs
struct PLL *PLL_root=NULL;

// Reset a PLL entry
void PLL_reset(struct PLL *pll, const float bitcell)
{
  if (pll==NULL) return;

  pll->cellsize=bitcell;
  pll->period=bitcell;
  pll->cur_pos=0;
  pll->period_adjust_base=(bitcell*pll_periodadjust);
  pll->min_period=(bitcell*pll_minperiod);
  pll->max_period=(bitcell*pll_maxperiod);
  pll->phase_adjust=0;
  pll->freq_hist=0;
  pll->next=(pll->cur_pos+pll->period+pll->phase_adjust);
  pll->num_bits=0;
}

// Create a new PLL entity
struct PLL *PLL_create(const float bitcell, void (*callback))
{
  struct PLL *newpll;

  newpll=malloc(sizeof(struct PLL));

  if (newpll!=NULL)
  {
    PLL_reset(newpll, bitcell);

    newpll->callback=callback;
    newpll->nextpll=NULL;

    // Store this PLL instance for later cleanup
    if (PLL_root!=NULL)
    {
      struct PLL *rover;

      rover=PLL_root;
      while (rover->nextpll!=NULL)
        rover=rover->nextpll;

      rover->nextpll=newpll;
    }
    else
      PLL_root=newpll;
  }

  return newpll;
}

// Add a sample to the PLL processor
void PLL_addsample(struct PLL *pll, const unsigned long samples, const unsigned long datapos)
{
  unsigned long i=samples;

  if (pll==NULL) return;

  while (i>0)
  {
    i--;

    // Processing for rising edge
    if (i==0)
    {
      if (pll->cur_pos>=pll->next)
      {
        // No transition in the window means 0 and pll in free run mode
        pll->phase_adjust=0;
      }
      else
      {
        // Transition in the window means 1, and the pll is adjusted
        float delta=pll->cur_pos-(pll->next-(pll->period/2));
        pll->phase_adjust=pll_phaseadjust*delta;

        pll->num_bits++;

        // Adjust frequency based on error
        if (delta<0)
        {
          if (pll->freq_hist<0)
            pll->freq_hist--;
          else
            pll->freq_hist=-1;
        }
        else
        if (delta>0)
        {
          if (pll->freq_hist>0)
            pll->freq_hist++;
          else
            pll->freq_hist=1;
        }
        else
          pll->freq_hist=0;

        // Update the reference clock?
        if (pll->freq_hist)
        {
          int afh=pll->freq_hist<0?-pll->freq_hist:pll->freq_hist;

          if (afh>1)
          {
            float aper=pll->period_adjust_base*delta/pll->period;

            if (!aper)
              aper=pll->freq_hist<0?-1:1;

            pll->period+=aper;

            // Keep within bounds
            if (pll->period<pll->min_period)
              pll->period=pll->min_period;
            else
            if (pll->period>pll->max_period)
              pll->period=pll->max_period;
          }
        }
      }
    }

    pll->cur_pos++;

    if (pll->cur_pos>=pll->next)
    {
      pll->next=(pll->cur_pos+pll->period+pll->phase_adjust);
      (pll->callback)((pll->num_bits>0?1:0), datapos);

      pll->num_bits=0;
    }
  }
}

void PLL_done()
{
  struct PLL *cont;

  while (PLL_root!=NULL)
  {
    cont=PLL_root->nextpll;

    free(PLL_root);
    PLL_root=cont;
  }
}

void PLL_init()
{
  atexit(PLL_done);
}
