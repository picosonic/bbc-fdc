#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "hardware.h"

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
  int argn=0;
  int counttracks=0;
  unsigned char drivestatus;

  // Check user permissions
  if (geteuid() != 0)
  {
    fprintf(stderr,"Must be run as root\n");
    exit(1);
  }

  while (argn<argc)
  {
    if ((strcmp(argv[argn], "-tmax")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      if (sscanf(argv[argn], "%3d", &retval)==1)
      {
        hw_setmaxtracks(retval);
        counttracks=1;
      }
    }

    ++argn;
  }

  printf("Start\n");

  // Initialise PCB
  if (!hw_init(HW_SPIDIV32))
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

  if (drivestatus==HW_NODRIVE)
  {
    fprintf(stderr, "Failed to detect drive\n");
    return 2;
  }

  if (drivestatus==HW_NODISK)
  {
    fprintf(stderr, "Failed to detect disk in drive\n");
    return 3;
  }

  // Select drive, depending on jumper
  hw_driveselect();

  // Start MOTOR
  hw_startmotor();

  // Wait for motor to get up to speed
  hw_sleep(1);

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

  printf("Approximate RPM %.2f\n", hw_measurerpm());

  // Determine the number of tracks we can seek to
  if (counttracks)
  {
    int numtracks;

    // Start from track zero
    hw_seektotrackzero();

    // Try seeking to the requested maximum track
    hw_seektotrack(hw_maxtracks-1);

    // There should always be a track zero
    numtracks=1;

    // Step back towards track 0, counting the tracks
    while (!hw_attrackzero())
    {
      hw_seekout();

      numtracks++;

      // Prevent seeking more than the requested maximum number of tracks
      if (numtracks>hw_maxtracks)
        break;
    }

    printf("Counted %d track positions\n", numtracks);
  }

  hw_sleep(1);

  hw_stopmotor();

  return 0;
}
