# bbc-fdc
Floppy disk interface for Raspberry Pi

This project is to allow the direct connection of floppy disk drives with 34-pin [Shugart bus](https://en.wikipedia.org/wiki/Shugart_bus) ribbon cables to the [Raspberry Pi](https://www.raspberrypi.org/) for the purpose of reading floppy disks.

It controls the drive using GPIO and samples the read data pin using SPI to obtain a forensic level capture of the raw magnetic flux transitions on the floppy disk.

Initially this was created to read my BBC Micro 5.25 inch disks formatted in [Acorn DFS](http://beebwiki.mdfs.net/Acorn_DFS_disc_format), but I've also been able to read and extract data from Acorn ADFS, MS-DOS, Commodore 64 and Apple II 5.25 inch disks.

The Cumana dual 5.25 inch disk drive (built November 1985) I'm using is capable of reading multiple different logical formats.

I've read both 40 and 80 track 5.25 inch disks, both using the 40/80 track selector switch on the back of the drive and by double stepping. These tracks are spaced at 48 and 96 tpi respectively.

Single and double sided disks can be read by switching between upper and lower heads during the capture process. Flippy disks can also be detected and imaged without the need for flipping the disk in the drive.

The BBC Micro Acorn DFS format used 40 or 80 tracks, 10 sectors per track (numbered 0 to 9), 256 bytes per sector, with FM encoding (single density). This gives a maximum data capacity for a double sided 80 track DFS disk of 409,600 bytes. However 2 sectors on each side of the disk are reserved for the catalogue.

More recently I've also been able to read 3.5 inch disks using a TEAC FD-235HG PC drive. Due to the drive being internally set to DS1 I've had to connect it to the 34 pin ribbon cable prior to the swap and by moving the drive select jumper on my board to DS1.

I've so far been able to read and extract data from several 3.5 inch disks including Archimedes Acorn ADFS S/M/L/D/E/F, MS-DOS, Amiga and Atari ST.

![Top of board](/circuit/top.jpg?raw=true "Top of board")
