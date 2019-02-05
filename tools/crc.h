#ifndef _CRC_H_
#define _CRC_H_

extern unsigned int calc_crc_stream(unsigned char *data, int datalen, unsigned int initial, unsigned int polynomial);
extern unsigned int calc_crc(unsigned char *data, int datalen);

#endif
