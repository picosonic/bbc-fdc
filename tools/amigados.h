#ifndef _AMIGADOS_H_
#define _AMIGADOS_H_

/*

From : http://lclevy.free.fr/adflib/adf_info.html

*/

#define AMIGADOS_DD_ROOTBLOCK 880
#define AMIGADOS_HD_ROOTBLOCK 1760

#define AMIGADOS_BOOTBLOCKSIZE 1024

#define AMIGADOS_UNKNOWN 0
#define AMIGADOS_DOS_FORMAT 1
#define AMIGADOS_PFS_FORMAT 2

// Offset from unix epoch to 1st Jan 1978
#define AMIGADOS_EPOCH 252460800

#define AMIGADOS_FILE 0xfffffffd
#define AMIGADOS_DIR  2

extern void amigados_gettitle(const unsigned int disktracks, char *title, const int titlelen);

extern void amigados_showinfo(const unsigned int disktracks, const int debug);

extern int amigados_validate();

extern void amigados_init(const int debug);

#endif
