#ifndef _PLL_H_
#define _PLL_H_

struct PLL
{
  float cellsize;

  uint32_t cur_pos; // Position in stream
  uint32_t next; // Expected next transition

  float period;
  float period_adjust_base;

  float min_period; // Minimum accepted bit cell width
  float max_period; // Maximum accepted bit cell width

  int32_t phase_adjust;
  int32_t freq_hist;

  uint32_t num_bits; // Number of bits observed within current bit cell

  void (*callback)(const unsigned long samples, const unsigned long datapos); // Function to send recovered bits to

  void *nextpll; // Pointer to the next PLL for cleanup purposes
};

extern float pll_periodadjust;
extern float pll_phaseadjust;
extern float pll_minperiod;
extern float pll_maxperiod;

extern void PLL_init();
extern struct PLL *PLL_create(const float bitcell, void *callback);
extern void PLL_reset(struct PLL *pll, const float bitcell);
extern void PLL_addsample(struct PLL *pll, const unsigned long samples, const unsigned long datapos);

#endif
