# bbcfdc
bbcfdc - Floppy disk raw flux capture and processor

bbcfdc is intended for capturing raw **.rfi** flux data from floppy disk and optionally converting it into a **.ssd**, **.dsd** or **.fsd** file.

It is designed to work with floppy disk interface PCB attached to GPIO of Raspberry Pi 2 or Raspberry Pi 3 running at 400Mhz (not overclocked). With some modifications the PCB and sample code can be made to work with the Raspberry Pi 1 running at 250Mhz.

Raw flux output is to **.rfi** files, these are raw capture data with JSON metadata, the format for these is detailed in rfi.h

## Syntax :

`[-i input_rfi_file] [[-c] | [-o output_file]] [-spidiv spi_divider] [-r retries] [-s] [-v]`

## Where :

 * `-i` Specify input **.rfi** file (when not being run on RPi hardware)
 * `-c` Catalogue the disk contents (DFS)
 * `-o` Specify output file, with one of the following extensions (.rfi, .ssd, .dsd, .fsd)
 * `-spidiv` Specify SPI clock divider to adjust sample rate (one of 4,8,16,32,64,128,256,512,1024)
 * `-r` Specify number of retries per track when less than expected sectors are found (not in .rfi or .raw)
 * `-s` Sort sectors in diskstore prior to writing image
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

# drivetest
drivetest - Floppy disk drive testing tool

drivetest is intended for testing basic PCB interface hardware functionality before doing capture.

It will check if the drive and disk are detected, check if read head is at track zero, determine if the disk is write-protected and calculate an approximate RPM.

## Return codes :

 * `0` - Success
 * `1` - Error failed hardware initialisation, or not enough user permissions
 * `2` - Error failed to detect drive
 * `3` - Error failed to detect disk in drive

# checkfsd

checkfsd - Check the contents of a **.fsd** file for debug purposes

checkfsd is intended for looking at the contents of **.fsd** files, its primary use is for debugging to make sure the file has been written correctly.

It will check for the fsd magic identifier, show the creation details (date stamp, creator, release and the unused data), show the title, number of tracks then iterate through the available tracks printing out the track and sector information. The sector data will be shown (with non-printable characters replaced by ".").

## Syntax :

`[input_fsd_file]`

## Return codes :

 * `0` - Success
 * `1` - Error with command line arguments
 * `2` - Error opening fsd file
