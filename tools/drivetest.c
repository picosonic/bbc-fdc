#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <bcm2835.h>
#include <string.h>

#include <sys/time.h>

#include "pins.h"

// For disk/drive status
#define NODRIVE 0
#define NODISK 1
#define HAVEDISK 2

// For RPM calculation
#define SECONDSINMINUTE 60
#define MICROSECONDSINSECOND 1000000

int current_track = 0;
int current_head = 0;

// Initialise GPIO and SPI
int init()
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

//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); // 390.625kHz on RPI3
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512); // 781.25kHz on RPI3
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256); // 1.562MHz on RPI3
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128); // 3.125MHz on RPI3
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); // 6.250MHz on RPI3
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); // 12.5MHz on RPI3 ***** WORKS
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16); // 25MHz on RPI3
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_8); // 50MHz on RPI3
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_4); // 100MHz on RPI3 - UNRELIABLE

  bcm2835_spi_setDataMode(BCM2835_SPI_MODE2); // CPOL = 1, CPHA = 0 

  bcm2835_spi_begin(); // sets all correct pin modes

  return 1;
}

// Stop motor and release drive
void stopMotor()
{
  bcm2835_gpio_clr(MOTOR_ON);
  bcm2835_gpio_clr(DS0_OUT);
  bcm2835_gpio_clr(DIR_SEL);
  bcm2835_gpio_clr(DIR_STEP);
  bcm2835_gpio_clr(SIDE_SELECT);
  printf("Stopped motor\n");
}

// Stop the motor and tidy up upon exit
void exitFunction()
{
  printf("Exit function\n");
  stopMotor();
  bcm2835_spi_end();
  bcm2835_close();
}

// Handle signals by stopping motor and tidying up
void sig_handler(const int sig)
{
  if (sig==SIGSEGV)
    printf("SEG FAULT\n");

  stopMotor();
  bcm2835_spi_end();
  exit(0);
}

// Seek head to track zero
void seekToTrackZero()
{
  // wait for a few milliseconds for track_zero to be set/reset
  delay(10);

  // Read current state of track zero indicator
  int track0 = bcm2835_gpio_lev(TRACK_0);

  if (track0 == LOW)
    printf("Seeking to track zero\r\n");

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

  printf("At track zero\n");
  current_track = 0;
}

// Try to see if both a disk and drive are detectable
unsigned char detect_disk()
{
  int retval=NODISK;
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

    if (i<200) retval=HAVEDISK;
  }

  // Test to see if there is no drive
  if ((retval!=HAVEDISK) && (bcm2835_gpio_lev(TRACK_0)==LOW) && (bcm2835_gpio_lev(WRITE_PROTECT)==LOW) && (bcm2835_gpio_lev(INDEX_PULSE)==LOW))
  {
    // Likely no drive
    retval=NODRIVE;
  }

  // If we have a disk and drive, then seek to track 00
  if (retval==HAVEDISK)
  {
    seekToTrackZero();
  }

  delay(1000);

  // Stop MOTOR
  bcm2835_gpio_clr(MOTOR_ON);

  // De-select drive
  bcm2835_gpio_clr(DS0_OUT);

  return retval;
}

// Program enty pont
int main(int argc,char **argv)
{
  struct timeval tv;
  unsigned long long starttime, endtime;
  unsigned char drivestatus;

  // Check user permissions
  if (geteuid() != 0)
  {
    fprintf(stderr,"Must be run as root\n");
    exit(1);
  }

  printf("Start\n");

  // Initialise PCB
  if (!init())
  {
    printf("Failed init\n");
    return 1;
  }

  // Install signal handlers to make sure motor is stopped
  atexit(exitFunction);
  signal(SIGINT, sig_handler); // Ctrl-C
  signal(SIGSEGV, sig_handler); // Seg fault
  signal(SIGTERM, sig_handler); // Termination request

  drivestatus=detect_disk();

  if (drivestatus==NODRIVE)
  {
    fprintf(stderr, "Failed to detect drive\n");
    return 2;
  }

  if (drivestatus==NODISK)
  {
    fprintf(stderr, "Failed to detect disk in drive\n");
    return 3;
  }

  // Select drive, depending on jumper
  bcm2835_gpio_set(DS0_OUT);

  // Start MOTOR
  bcm2835_gpio_set(MOTOR_ON);

  // Wait for motor to get up to speed
  sleep(1);

  // Determine if head is at track 00
  if (bcm2835_gpio_lev(TRACK_0)==LOW)
    printf("Starting not at track zero\n");
  else
    printf("Starting at track zero\n");

  // Determine if disk in drive is write protected
  if (bcm2835_gpio_lev(WRITE_PROTECT)==LOW)
    printf("Disk is writeable\n");
  else
    printf("Disk is write-protected\n");

  printf("Sampling index pin ... \n");

  // Wait for first index falling edge
  while (bcm2835_gpio_lev(INDEX_PULSE)!=LOW) { }
  while (bcm2835_gpio_lev(INDEX_PULSE)==LOW) { }

  // Get time
  gettimeofday(&tv, NULL);
  starttime=(((unsigned long long)tv.tv_sec)*MICROSECONDSINSECOND)+tv.tv_usec;

  // Wait for next index falling edge
  while (bcm2835_gpio_lev(INDEX_PULSE)!=LOW) { }
  while (bcm2835_gpio_lev(INDEX_PULSE)==LOW) { }

  gettimeofday(&tv, NULL);
  endtime=(((unsigned long long)tv.tv_sec)*MICROSECONDSINSECOND)+tv.tv_usec;

  sleep(1);

  stopMotor();

  printf("Approximate RPM %f\n", ((MICROSECONDSINSECOND/(float)(endtime-starttime))*SECONDSINMINUTE));

  return 0;
}
