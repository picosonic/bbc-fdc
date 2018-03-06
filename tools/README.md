# bbcfdc
bbcfdc - Floppy disk raw flux capture and processor

bbcfdc is intended for capturing raw flux data from floppy disk and optionally converting it into a .SSD or .DSD file.

It is designed to work with floppy disk interface PCB attached to GPIO of Raspberry Pi 2 or Raspberry Pi 3 running at 400Mhz (not overclocked). With some modifications the PCB and sample code can be made to work with the Raspberry Pi 1.

## Syntax :

`[[-c] | [-o outputfile]] [-v]`

## Where :

 * `-c` Catalogue the disk contents (DFS)
 * `-o` Specify output file, with one of the following extensions (.raw, .ssd, .dsd)
 * `-v` Verbose

## Return codes :

 * `0` - Success
 * `1` - Error with command line arguments
 * `2` - Error not enough permissions
 * `3` - Error allocating memory
 * `4` - Error failed hardware initialisation
 * `5` - Error failed to detect drive
 * `6` - Error failed to detect disk in drive
 
## Requirements :
 
 * bcm2835 library, available from [http://www.airspayce.com/mikem/bcm2835/](http://www.airspayce.com/mikem/bcm2835/)
