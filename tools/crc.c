#include "crc.h"

// Configurable CRC16 stream algorithm
unsigned int calc_crc_stream(const unsigned char *data, const int datalen, const unsigned int initial, const unsigned int polynomial)
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
unsigned int calc_crc(const unsigned char *data, const int datalen)
{
  return (calc_crc_stream(data, datalen, 0xffff, 0x1021));
}
