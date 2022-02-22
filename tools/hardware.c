#define _GNU_SOURCE

#include <unistd.h>
#include <bcm2835.h>
#include <sys/time.h>
#include <sched.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware.h"
#include "pins.h"

unsigned int hw_maxtracks = HW_MAXTRACKS;
uint8_t hw_currenttrack = 0;
uint8_t hw_currenthead = 0;
unsigned long hw_samplerate = 0;
float hw_rpm = HW_DEFAULTRPM;

int hw_stepping = HW_NORMALSTEPPING;

void hw_setscaling(const char *scale)
{
  const char governor_policy[]="/sys/devices/system/cpu/cpufreq/policy0/scaling_governor";
  const char governor_cpu[]="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";
  FILE *fp;

  // Write the new scaling governor value to policy0 if available
  fp=fopen(governor_policy, "r+");
  if (fp!=NULL)
  {
    fprintf(fp, "%s\n", scale);

    fclose(fp);
  }

  // Write the new scaling governor value to cpu0 if available
  fp=fopen(governor_cpu, "r+");
  if (fp!=NULL)
  {
    fprintf(fp, "%s\n", scale);

    fclose(fp);
  }

  // Give it a chance to take effect
  hw_sleep(2);
}

// Initialise GPIO and SPI
int hw_init(const int spiclockdivider)
{
  struct sched_param priority;
  int curCPU;
  cpu_set_t CPUset;

  if (!bcm2835_init()) return 0;

  // Request higher priority thread scheduling
  priority.sched_priority=sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(0, SCHED_FIFO, &priority);

  // Request CPU scheduling affinity to stop program switching cores
  curCPU=sched_getcpu();
  CPU_ZERO(&CPUset);
  CPU_SET(curCPU, &CPUset);
  sched_setaffinity(0, sizeof(CPUset), &CPUset);

/*
  To test
    see
      https://wiki.linuxfoundation.org/realtime/documentation/technical_basics/sched_rt_throttling
      https://github.com/sbelectronics/pi-fdc
      https://access.redhat.com/solutions/1604133
      https://github.com/sbelectronics/smb-pi-lib/blob/master/smbpi/realtime_ext.c

    We may need to reserve a CPU core or two, then run code on those
      - add this at kernel boot prompt, to reserve cores 2 and 3 (first core is #0): isolcpus=2,3
      - start my bash script on core 2: /usr/bin/taskset -c 2 /root/ws2812-spi/ws2812.sh
      - in this script, call the python API (the one that really performs the SPI calls) on core 3: taskset -c 3 nice -n -20 /root/ws2812-spi/ws2812.05.py

  int rc;

  // This permits realtime processes to use 100% of a CPU, but on a
  // RPi that starves the kernel. Without this there are latencies
  // up to 50 MILLISECONDS.
  rc = system("echo -1 >/proc/sys/kernel/sched_rt_runtime_us");

*/

  // Set up GPIO data direction and pull ups
  bcm2835_gpio_fsel(DS0_OUT, GPIO_OUT);
  bcm2835_gpio_clr(DS0_OUT);

  bcm2835_gpio_fsel(MOTOR_ON, GPIO_OUT);
  bcm2835_gpio_clr(MOTOR_ON);

  bcm2835_gpio_fsel(DIR_SEL, GPIO_OUT);
  bcm2835_gpio_clr(DIR_SEL);

  bcm2835_gpio_fsel(DIR_STEP, GPIO_OUT);
  bcm2835_gpio_clr(DIR_STEP);

  bcm2835_gpio_fsel(WRITE_GATE, GPIO_OUT);
  bcm2835_gpio_clr(WRITE_GATE);

  bcm2835_gpio_fsel(SIDE_SELECT, GPIO_OUT);
  bcm2835_gpio_clr(SIDE_SELECT);

  bcm2835_gpio_fsel(WRITE_PROTECT, GPIO_IN);
  bcm2835_gpio_set_pud(WRITE_PROTECT, PULL_UP);

  bcm2835_gpio_fsel(TRACK_0, GPIO_IN);
  bcm2835_gpio_set_pud(TRACK_0, PULL_UP);

  bcm2835_gpio_fsel(INDEX_PULSE, GPIO_IN);
  bcm2835_gpio_set_pud(INDEX_PULSE, PULL_UP);

  //bcm2835_gpio_fsel(READ_DATA, GPIO_IN);
  //bcm2835_gpio_set_pud(READ_DATA, PULL_UP);

  switch (spiclockdivider)
  {
    case HW_SPIDIV1024:
      // 244.140kHz on RPI1/2, 390.625kHz on RPI3, 488.28125kHz on RPI4
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024);
      break;

    case HW_SPIDIV512:
      // 488.281kHz on RPI1/2, 781.25kHz on RPI3, 976.5625kHz on RPI4
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512);
      break;

    case HW_SPIDIV256:
      // 976.562kHz on RPI1/2, 1.562MHz on RPI3, 1.953125MHz on RPI4
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
      break;

    case HW_SPIDIV128:
      // 1.953MHz on RPI1/2, 3.125MHz on RPI3, 3.90625MHz on RPI4
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128);
      break;

    case HW_SPIDIV64:
      // 3.906MHz on RPI1/2, 6.250MHz on RPI3, 7.8125MHz on RPI4
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);
      break;

    case HW_SPIDIV32:
      // 7.812MHz on RPI1/2, 12.5MHz on RPI3, 15.625MHz on RPI4 - ** WORKS **
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
      break;

    case HW_SPIDIV16:
      // 15.625MHz on RPI1/2, 25MHz on RPI3, 31.25MHz on RPI4 - ** WORKS **
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16);
      break;

    case HW_SPIDIV8:
      // 31.25MHz on RPI1/2, 50MHz on RPI3, 62.5MHz on RPI4 - ** LESS RELIABLE **
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_8);
      break;

    case HW_SPIDIV4:
      // 62.5MHz on RPI1/2, 100MHz on RPI3, 125MHz on RPI4 - ** UNRELIABLE, GIVES SPURIOUS VALUES  **
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_4);
      break;

    default:
      return 0;
      break;
  }

#if defined(CLOCK_400)
  hw_samplerate=HW_400MHZ/spiclockdivider; // 400MHz core
#elif defined(CLOCK_500)
  hw_samplerate=HW_500MHZ/spiclockdivider; // 500MHz core
#else
  hw_samplerate=HW_250MHZ/spiclockdivider; // 250MHz core
#endif

  bcm2835_spi_setDataMode(BCM2835_SPI_MODE2); // CPOL (Clock Polarity) = 1, CPHA (Clock Phase) = 0

  bcm2835_spi_begin(); // sets all correct pin modes

  hw_setscaling("performance");

  return 1;
}

// Stop motor and release drive
void hw_stopmotor()
{
  bcm2835_gpio_clr(MOTOR_ON);
  bcm2835_gpio_clr(DS0_OUT);
  bcm2835_gpio_clr(DIR_SEL);
  bcm2835_gpio_clr(DIR_STEP);
  bcm2835_gpio_clr(SIDE_SELECT);
}

// Tidy up
void hw_done()
{
  hw_stopmotor();
  bcm2835_spi_end();
  bcm2835_close();

  hw_setscaling("ondemand");
}

// Determine if head is at track zero
int hw_attrackzero()
{
  // Read current state of track zero indicator
  return (bcm2835_gpio_lev(TRACK_0)==HIGH);
}

// Seek head in by 1 track
void hw_seekin()
{
  bcm2835_gpio_set(DIR_SEL);
  bcm2835_gpio_set(DIR_STEP);
  delayMicroseconds(8);
  bcm2835_gpio_clr(DIR_STEP);
  delay(40); // wait maximum time for step
}

// Seek head out by 1 track, towards track zero
void hw_seekout()
{
  bcm2835_gpio_clr(DIR_SEL);
  bcm2835_gpio_set(DIR_STEP);
  delayMicroseconds(8);
  bcm2835_gpio_clr(DIR_STEP);
  delay(40); // wait maximum time for step
}

// Seek head to track zero
void hw_seektotrackzero()
{
  // wait for a few milliseconds for track_zero to be set/reset
  delay(10);

  // Read current state of track zero indicator
  int track0 = bcm2835_gpio_lev(TRACK_0);

  // Keep seeking until at track zero
  while (track0 == LOW)
  {
    hw_seekout();

    track0 = bcm2835_gpio_lev(TRACK_0);
  }

  hw_currenttrack = 0;
}

// Seek head to given track
void hw_seektotrack(const int track)
{
  int steps;
  int i;

  // Sanity check the requested track is within range
  if ((track<0) || (track>=hw_maxtracks))
    return;

  // Sanity check our "current" track is within range
  if (hw_currenttrack>=hw_maxtracks)
    return;

  // For seek to track 00, seek until TRACK00 signal
  if (track==0)
  {
    hw_seektotrackzero();
    return;
  }

  // Check for double stepping for 40 track disk in 80 track drive
  if (hw_stepping==HW_DOUBLESTEPPING)
    steps=2;
  else
    steps=1;

  // Seek towards inside of disk
  while (hw_currenttrack < track)
  {
    for (i=0; i<steps; i++)
      hw_seekin();

    hw_currenttrack++;
  }

  // Seek towards outside of disk
  while (hw_currenttrack > track)
  {
    for (i=0; i<steps; i++)
    {
      // Prevent stepping past track 00
      if (bcm2835_gpio_lev(TRACK_0)==LOW)
        break;

      hw_seekout();
    }
    hw_currenttrack--;
  }
}

// Override maximum number of hardware tracks
void hw_setmaxtracks(const int maxtracks)
{
  hw_maxtracks=maxtracks;
}

// Try to see if both a disk and drive are detectable
unsigned char hw_detectdisk()
{
  int retval=HW_NODISK;
  unsigned long i;

  // Select drive
  bcm2835_gpio_set(DS0_OUT);

  // Start MOTOR
  bcm2835_gpio_set(MOTOR_ON);

  delay(500);

  // We need to see the index pulse go high to prove there is a drive with a disk in it
  for (i=0; i<200; i++)
  {
    // A drive with no disk will have an index pulse "stuck" low, so make sure it goes high within timeout
    if (bcm2835_gpio_lev(INDEX_PULSE)!=LOW)
      break;

    delay(2);
  }

  // If high pulse detected then check for it going low again
  if (i<200)
  {
    for (i=0; i<200; i++)
    {
      // Make sure index pulse goes low again, i.e. it's pulsing (disk going round)
      if (bcm2835_gpio_lev(INDEX_PULSE)==LOW)
        break;

      delay(2);
    }

    if (i<200) retval=HW_HAVEDISK;
  }

  // Test to see if there is no drive
  if ((retval!=HW_HAVEDISK) && (bcm2835_gpio_lev(TRACK_0)==LOW) && (bcm2835_gpio_lev(WRITE_PROTECT)==LOW) && (bcm2835_gpio_lev(INDEX_PULSE)==LOW))
  {
    // Likely no drive
    retval=HW_NODRIVE;
  }

  // If we have a disk and drive, then seek to track 00
  if (retval==HW_HAVEDISK)
  {
    if (!hw_attrackzero())
      hw_seektotrackzero();
  }

  delay(1000);

  // Stop MOTOR
  bcm2835_gpio_clr(MOTOR_ON);

  // De-select drive
  bcm2835_gpio_clr(DS0_OUT);

  return retval;
}

// Select drive, depending on jumper
void hw_driveselect()
{
  bcm2835_gpio_set(DS0_OUT);
}

// Start MOTOR
void hw_startmotor()
{
  bcm2835_gpio_set(MOTOR_ON);
}

// Determine disk write protection state
int hw_writeprotected()
{
  return (bcm2835_gpio_lev(WRITE_PROTECT)==HIGH);
}

// Wait for next rising edge on index pin
void hw_waitforindex()
{
  // If index is already high, wait for it to go low
  while (bcm2835_gpio_lev(INDEX_PULSE)!=LOW) { }

  // Wait for next rising edge
  while (bcm2835_gpio_lev(INDEX_PULSE)==LOW) { }
}

// Request data from side 0 = upper (label), or side 1 = lower side of disk
void hw_sideselect(const int side)
{
  // Check the requested side is within range
  if ((side==0) || (side==(HW_MAXHEADS-1)))
  {
    if (side==0)
      bcm2835_gpio_clr(SIDE_SELECT);
    else
      bcm2835_gpio_set(SIDE_SELECT);

    hw_currenthead=side;
  }
}

// Fix SPI sample buffer timings
void hw_fixspisamples(unsigned char *inbuf, long inlen, unsigned char *outbuf, long outlen)
{
  long inpos, outpos;
  unsigned char o, olen, bitpos;

  outpos=0; o=0; olen=0;

  for (inpos=0; inpos<inlen; inpos++)
  {
    unsigned char c;

    c=inbuf[inpos];

    // Insert extra sample, this is due to SPI sampling leaving a 1 sample gap between each group of 8 samples
    o=(o<<1)|((c&0x80)>>7);
    olen++;
    if (olen==BITSPERBYTE)
    {
      if (outpos<outlen)
        outbuf[outpos++]=o;

      olen=0; o=0;
    }

    // Process the 8 valid samples we did get
    for (bitpos=0; bitpos<BITSPERBYTE; bitpos++)
    {
      o=(o<<1)|((c&0x80)>>7);
      olen++;

      if (olen==BITSPERBYTE)
      {
        if (outpos<outlen)
          outbuf[outpos++]=o;

        olen=0; o=0;
      }

      c=c<<1;
    }

    // Stop on output buffer overflow
    if (outpos>=outlen) return;
  }
}

// Sample raw track data
void hw_samplerawtrackdata(unsigned char* buf, uint32_t len)
{
  char *rawbuf;

  // Clear output buffer to prevent failed reads potentially returning previous data
  bzero(buf, len);

  rawbuf=malloc(len);
  if (rawbuf==NULL) return;

  // Sample using SPI
  hw_waitforindex();
  bcm2835_spi_transfern(rawbuf, len);

  // Fix SPI timings
  hw_fixspisamples(rawbuf, len, buf, len);

  free(rawbuf);
}

void hw_sleep(const unsigned int seconds)
{
  sleep(seconds);
}

// Measure time between index pulses to determine RPM
float hw_measurerpm()
{
  unsigned long long starttime, endtime;
  struct timeval tv;

  // Wait for next index rising edge
  hw_waitforindex();

  // Get time
  gettimeofday(&tv, NULL);
  starttime=(((unsigned long long)tv.tv_sec)*USINSECOND)+tv.tv_usec;

  // Wait for next index rising edge
  hw_waitforindex();

  gettimeofday(&tv, NULL);
  endtime=(((unsigned long long)tv.tv_sec)*USINSECOND)+tv.tv_usec;

  hw_rpm=((USINSECOND/(float)(endtime-starttime))*SECONDSINMINUTE);

  return hw_rpm;
}
