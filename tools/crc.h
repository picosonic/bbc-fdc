#ifndef _CRC_H_
#define _CRC_H_

extern unsigned int calc_crc_stream(const unsigned char *data, const int datalen, const unsigned int initial, const unsigned int polynomial);
extern unsigned int calc_crc(const unsigned char *data, const int datalen);

#endif
