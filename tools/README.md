# bbcfdc
bbcfdc - Floppy disk raw flux capture and processor

bbcfdc is intended for capturing raw **.rfi** flux data from floppy disk and optionally converting it into a **.ssd**, **.dsd**, **.fsd**, **.td0**, **.adf** or a generic **.img** file.

It is designed to work with floppy disk interface PCB attached to GPIO of Raspberry Pi 2 or Raspberry Pi 3 running at 400Mhz (not overclocked). With some modifications the PCB and sample code can be made to work with the Raspberry Pi 1 running at 250Mhz.

Raw flux output is to **.rfi** files, these are raw capture data with JSON metadata, the format for these is detailed in [rfi.h](https://github.com/picosonic/bbc-fdc/blob/master/tools/rfi.h).

Also output to **.dfi** (DiscFerret flux dump) is possible (not tested).

## Syntax :

`[-i input_rfi_file] [[-c] | [-o output_file]] [-spidiv spi_divider] [[-ss]|[-ds]] [-r retries] [-sort] [-summary] [-l] [-tmax maxtracks] [-title "Title"] [-v]`

## Where :

 * `-i` Specify input **.rfi** file (when not being run on RPi hardware)
 * `-c` Catalogue the disk contents (DFS/ADFS/DOS only)
 * `-o` Specify output file, with one of the following extensions (.rfi, .dfi, .scp, .sdd, .ssd, .ddd, .dsd, .fsd, .td0, .img, .adf)
 * `-spidiv` Specify SPI clock divider to adjust sample rate (one of 16,32,64)
 * `-r` Specify number of retries per track when less than expected sectors are found (not in .rfi, .dfi, .scp or .raw)
 * `-l` Show a layout diagram of where sectors were found upon the disk surface for each track/side
 * `-ss` Force single-sided capture - optionally adding a 0 or 1 afterwards chooses that side e.g. `-ss 0` or `-ss 1`
 * `-ds` Force double-sided capture (unless output is to .ssd)
 * `-sectors` force DFS sectors e.g. 16 for Solidisk / Watford double density
 * `-sort` Sort sectors in diskstore prior to writing image
 * `-summary` Present a summary of operations once complete
 * `-csv` Create a csv of bad sectors named as <outputfile>.csv
 * `-tmax` Specify the maximum track number you wish to try stepping to
 * `-title` Override the title used in metadata for disk formats which support it (.td0 / .fsd)
 * `-v` Verbose

## Return codes :

 * `0` - Success
 * `1` - Error with command line arguments
 * `2` - Error not enough permissions
 * `3` - Error allocating memory
 * `4` - Error failed hardware initialisation
 * `5` - Error failed to detect drive
 * `6` - Error failed to detect disk in drive
 * `7` - Error invalid SPI divider
 
## Requirements :
 
 * bcm2835 library, available from [http://www.airspayce.com/mikem/bcm2835/](http://www.airspayce.com/mikem/bcm2835/)

*NOTE : For bcm2835 library to work on Raspberry Pi 4 you should get the latest version (I've not tested it past Pi 3B+ yet)*

# drivetest
drivetest - Floppy disk drive testing tool

drivetest is intended for testing basic PCB interface hardware functionality before doing capture.

It will check if the drive and disk are detected, check if read head is at track zero, determine if the disk is write-protected and calculate an approximate RPM.

Optionally it can count the number of tracks which can be stepped to by the hardware.

## Syntax :

`[-tmax maxtracks]`

## Where :

 * `-tmax` Specify the maximum track number you wish to try stepping to

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

# checktd0

checktd0 - Check the contents of a **.td0** file for debug purposes

checktd0 is intended for looking at the contents of **.td0** files, its primary use is for debugging to make sure the file has been written correctly.

It will check for the .td0 magic identifier, show the header details, decompress the remainder of the file if required, show the comment and date created if used, then iterate through the available tracks printing out the track and sector information. The sector data will be shown (with non-printable characters replaced by ".").

## Syntax :

`[input_td0_file]`

## Return codes :

 * `0` - Success
 * `1` - Error with command line arguments
 * `2` - Error opening td0 file
 * `3` - Unable to read header
 * `4` - Not a valid td0 file
 * `5` - Unable to read comment, or comment CRC mismatch
 * `6` - Unable to read track header, or track CRC mismatch
 * `7` - Unable to read sector header
 * `8` - Unable to read sector data
 * `9` - Unable to allocate memory for decompression
 * `10` - Sector data CRC mismatch

## BBC DFS Notes:
 * Using a `.ddd` or `.sdd` output file for BBC DFS disks will assume 16 sectors per track (i.e. double density)
 * Above can be overriden with the `-sectors` switch e.g. `-sectors 18`
 * Disks with different densities on different sides cannot currently be captured into `.ddd`
 * If a Solidisk chained catalogue is detected, it will notify during the catalogue operaton but not list it

 


