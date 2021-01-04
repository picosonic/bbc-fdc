#ifndef _AMIGAMFM_H_
#define _AMIGAMFM_H_

/*

From : http://lclevy.free.fr/adflib/adf_info.html

		bytes/sector	sector/track	track/cyl	cyl/disk
------------------------------------------------------------------------
DD disks	512		11		2		80
HD disks 	512		22		2		80

The relations between sectors, sides and cylinders are for a DD disk :

Block	sector	side	cylinder
--------------------------------
0	0	0	0
1	1	0	0
2	2	0	0
...
10	10	0	0
11	0	1	0
...
21	10	1	0
22	0	0	1
..
1759	10	1	79

Order = increasing sectors, then increasing sides, then increasing cylinders.

A DD disk has 11*2*80=1760 (0 to 1759) blocks, a HD disk has 22*2*80=3520 blocks.

-------------------------------------------------------------------------------------

00/0x00	word	2	MFM value 0xAAAA AAAA (when decoded : two bytes of 00 data)

	SYNCHRONIZATION
04/0x04	word	1	MFM value 0x4489 (encoded version of the 0xA1 byte)
06/0x06	word	1	MFM value 0x4489

	HEADER
08/0x08	long	1	info (odd bits)
12/0x0c	long	1	info (even bits)
			decoded long is : 0xFF TT SS SG
				0xFF = Amiga v1.0 format
				TT = track number ( 3 means cylinder 1, head 1)
				SS = sector number ( 0 upto 10/21 )
					sectors are not ordered !!!
				SG = sectors until end of writing (including
					current one)

			Example for cylinder 0, head 1 of a DD disk :
				0xff010009
				0xff010108
				0xff010207
				0xff010306
				0xff010405
				0xff010504
				0xff010603
				0xff010702
				0xff010801
				0xff01090b
				0xff010a0a
                        the order of the track written was sector 9, sector 10,
                         sector 0, sector 1 ...

                        (see also the note below from RKRM)

            Sector Label Area : OS recovery info, reserved for future use

16/0x10	long	4	sector label (odd)
32/0x20	long	4	sector label (even)
                    decoded value is always 0

            This is operating system dependent data and relates to how AmigaDOS
            assigns sectors to files.

            Only available to 'trackdisk.device', but not with any other floppy
            or hard disk device.

	END OF HEADER

48/0x30	long	1	header checksum (odd)
52/0x34	long	1	header checksum (even)
			(computed on mfm longs,
			longs between offsets 8 and 44
			== 2*(1+4) longs)

56/0x38	long	1	data checksum (odd)
60/0x3c	long	1	data checksum (even)
			(from 64 to 1088 == 2*512 bytes)

	DATA
64/0x40	  byte	512	coded data (odd)
576/0x240 byte	512	coded data (even)
1088/0x440
	END OF DATA

*/

#define AMIGA_SECTOR_SIZE 1088
#define AMIGA_DATASIZE 512

#define AMIGA_INFO_OFFSET 0x8
#define AMIGA_SECTOR_LABEL_OFFSET 0x10
#define AMIGA_HEADER_CXSUM_OFFSET 0x30
#define AMIGA_DATA_CXSUM_OFFSET 0x38
#define AMIGA_DATA_OFFSET 0x40

#define AMIGA_MFM_MASK 0x55555555

extern void amigamfm_addsample(const unsigned long samples, const unsigned long datapos, const int usepll);

extern void amigamfm_init(const int debug, const char density);

#endif
