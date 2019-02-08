#ifndef _LZHUF_H_
#define _LZHUF_H_

extern void lz_Init();

extern uint32_t lz_DecodedLength(uint8_t *in);

extern uint32_t lz_Decode(uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t outlen);
extern uint32_t lz_Encode(uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t outlen);

#endif
