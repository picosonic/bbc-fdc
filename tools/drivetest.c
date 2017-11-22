#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include <sys/time.h>

#include "hardware.h"

// For RPM calculation
#define SECONDSINMINUTE 60
#define MICROSECONDSINSECOND 1000000

int current_track = 0;
int current_head = 0;

// Stop the motor and tidy up upon exit
void exitFunction()
{
  printf("Exit function\n");
  hw_done();
}

// Handle signals by stopping motor and tidying up
void sig_handler(const int sig)
{
  if (sig==SIGSEGV)
    printf("SEG FAULT\n");

  hw_done();
  exit(0);
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
  if (!hw_init())
  {
    printf("Failed init\n");
    return 1;
  }

  // Install signal handlers to make sure motor is stopped
  atexit(exitFunction);
  signal(SIGINT, sig_handler); // Ctrl-C
  signal(SIGSEGV, sig_handler); // Seg fault
  signal(SIGTERM, sig_handler); // Termination request

  drivestatus=hw_detectdisk();

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
  hw_driveselect();

  // Start MOTOR
  hw_startmotor();

  // Wait for motor to get up to speed
  sleep(1);

  // Determine if head is at track 00
  if (hw_attrackzero())
    printf("Starting at track zero\n");
  else
    printf("Starting not at track zero\n");

  // Determine if disk in drive is write protected
  if (hw_writeprotected())
    printf("Disk is write-protected\n");
  else
    printf("Disk is writeable\n");

  printf("Sampling index pin ... \n");

  // Wait for next index rising edge
  hw_waitforindex();

  // Get time
  gettimeofday(&tv, NULL);
  starttime=(((unsigned long long)tv.tv_sec)*MICROSECONDSINSECOND)+tv.tv_usec;

  // Wait for next index falling edge
  hw_waitforindex();

  gettimeofday(&tv, NULL);
  endtime=(((unsigned long long)tv.tv_sec)*MICROSECONDSINSECOND)+tv.tv_usec;

  sleep(1);

  hw_stopmotor();

  printf("Approximate RPM %f\n", ((MICROSECONDSINSECOND/(float)(endtime-starttime))*SECONDSINMINUTE));

  return 0;
}
