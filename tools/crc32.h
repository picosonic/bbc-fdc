#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdio.h>

/* CRC-32-IEEE 802.3 reversed */
#define CRC32_POLYNOMIAL 0xedb88320

extern uint32_t CRC32_Calc(const char *buf, const int len);
extern uint32_t CRC32_CalcStream(const uint32_t currcrc, const char *buf, const int len);

#endif
