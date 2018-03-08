#include <unistd.h>
#include <bcm2835.h>
#include <sys/time.h>

#include "hardware.h"
#include "pins.h"

int hw_currenttrack = 0;
int hw_currenthead = 0;
unsigned long hw_samplerate = 0;

int hw_stepping = HW_NORMALSTEPPING;

// Initialise GPIO and SPI
int hw_init(const int spiclockdivider)
{
  int i;

  if (!bcm2835_init()) return 0;

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
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); // 390.625kHz on RPI3
      break;

    case HW_SPIDIV512:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512); // 781.25kHz on RPI3
      break;

    case HW_SPIDIV256:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256); // 1.562MHz on RPI3
      break;

    case HW_SPIDIV128:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128); // 3.125MHz on RPI3
      break;

    case HW_SPIDIV64:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); // 6.250MHz on RPI3
      break;

    case HW_SPIDIV32:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); // 12.5MHz on RPI3 ***** WORKS
      break;

    case HW_SPIDIV16:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16); // 25MHz on RPI3
      break;

    case HW_SPIDIV8:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_8); // 50MHz on RPI3
      break;

    case HW_SPIDIV4:
      bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_4); // 100MHz on RPI3 - UNRELIABLE
      break;

    default:
      return 0;
      break;
  }

#ifdef RPI1
  hw_samplerate=250000000/spiclockdivider;
#else
  hw_samplerate=400000000/spiclockdivider;
#endif

  bcm2835_spi_setDataMode(BCM2835_SPI_MODE2); // CPOL (Clock Polarity) = 1, CPHA (Clock Phase) = 0 

  bcm2835_spi_begin(); // sets all correct pin modes

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
}

// Determine if head is at track zero
int hw_attrackzero()
{
  // Read current state of track zero indicator
  return (bcm2835_gpio_lev(TRACK_0)==HIGH);
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
    bcm2835_gpio_clr(DIR_SEL);
    bcm2835_gpio_set(DIR_STEP);
    delayMicroseconds(8);
    bcm2835_gpio_clr(DIR_STEP);
    delay(40); // wait maximum time for step
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
  if ((track<0) || (track>=HW_MAXTRACKS))
    return;

  // Sanity check our "current" track is within range
  if ((hw_currenttrack<0) || (hw_currenttrack>=HW_MAXTRACKS))
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
    {
      bcm2835_gpio_set(DIR_SEL);
      bcm2835_gpio_set(DIR_STEP);
      delayMicroseconds(8);
      bcm2835_gpio_clr(DIR_STEP);
      delay(40); // wait maximum time for step
    }
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

      bcm2835_gpio_clr(DIR_SEL);
      bcm2835_gpio_set(DIR_STEP);
      delayMicroseconds(8);
      bcm2835_gpio_clr(DIR_STEP);
      delay(40); // wait maximum time for step
    }
    hw_currenttrack--;
  }
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

// Sample raw track data
void hw_samplerawtrackdata(char* buf, uint32_t len)
{
  bcm2835_spi_transfern(buf, len);
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
  starttime=(((unsigned long long)tv.tv_sec)*MICROSECONDSINSECOND)+tv.tv_usec;

  // Wait for next index rising edge
  hw_waitforindex();

  gettimeofday(&tv, NULL);
  endtime=(((unsigned long long)tv.tv_sec)*MICROSECONDSINSECOND)+tv.tv_usec;

  return ((MICROSECONDSINSECOND/(float)(endtime-starttime))*SECONDSINMINUTE);
}
