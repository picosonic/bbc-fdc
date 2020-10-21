#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>

extern uint16_t calc_crc_stream(const unsigned char *data, const int datalen, const uint16_t initial, const uint16_t polynomial);
extern uint16_t calc_crc(const unsigned char *data, const int datalen);

#endif
