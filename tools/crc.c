#include "crc.h"

// Configurable CRC16 stream algorithm
unsigned int calc_crc_stream(unsigned char *data, int datalen, unsigned int initial, unsigned int polynomial)
{
  unsigned int crc=initial;
  int i, j;

  for (i=0; i<datalen; i++)
  {
    crc ^= data[i] << 8;
    for (j=0; j<8; j++)
      crc = (crc & 0x8000) ? (crc << 1) ^ polynomial : crc << 1;
  }

  return (crc & 0xffff);
}

// CCITT CRC16 (Floppy Disk Data)
unsigned int calc_crc(unsigned char *data, int datalen)
{
  return (calc_crc_stream(data, datalen, 0xffff, 0x1021));
}
