#include <stdint.h>
#include "crc.h"

// Configurable CRC16 stream algorithm
uint16_t calc_crc_stream(const unsigned char *data, const int datalen, const uint16_t initial, const uint16_t polynomial)
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
uint16_t calc_crc(const unsigned char *data, const int datalen)
{
  return (calc_crc_stream(data, datalen, 0xffff, 0x1021));
}

// Configurable CRC32 stream algorithm
uint32_t calc_crc32_stream(const unsigned char *data, const int datalen, const uint32_t initial, const uint32_t polynomial)
{
  uint32_t crc=initial;
  int i, j;

  for (i=0; i<datalen; i++)
  {
    char ch=data[i];

    for (j=0; j<8; j++)
    {
      uint32_t b=(ch^crc)&1;

      crc>>=1;

      if(b) crc=crc^polynomial;

      ch>>=1;
    }
  }

  return ~crc;
}

// CRC-32b (ISO 3309)
uint32_t calc_crc32(const unsigned char *data, const int datalen)
{
  return (calc_crc32_stream(data, datalen, 0xffffffff, 0xedb88320));
}
