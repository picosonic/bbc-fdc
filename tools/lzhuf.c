/**************************************************************
 lzhuf.c
 written by Haruyasu Yoshizaki 11/20/1988
 some minor changes 4/6/1989
 comments translated by Haruhiko Okumura 4/7/1989
**************************************************************/

/*
 LZHUF.C (c)1989 by Haruyasu Yoshizaki, Haruhiko Okumura, and Kenji Rikitake.
 All rights reserved. Permission granted for non-commercial use.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

uint8_t *lz_input;
uint8_t *lz_output;
uint32_t lz_textsize=0;
uint32_t lz_codesize=0;
uint32_t lz_pos=0;

/********** LZSS compression **********/

#define lz_N         4096 /* buffer size */
#define lz_F         60   /* lookahead buffer size */
#define lz_THRESHOLD 2
#define lz_NIL       lz_N /* leaf of tree */

uint8_t lz_text_buf[lz_N+lz_F-1];
uint16_t lz_match_position;
uint16_t lz_match_length;
uint16_t lz_lson[lz_N+1];
uint16_t lz_rson[lz_N+0x101];
uint16_t lz_dad[lz_N+1];

/* initialize trees */
void lz_InitTree()
{
  int32_t i;

  for (i=lz_N+1; i<=lz_N+0x100; i++)
    lz_rson[i]=lz_NIL; /* root */

  for (i=0; i<lz_N; i++)
    lz_dad[i]=lz_NIL; /* node */
}

/* insert to tree */
void lz_InsertNode(int32_t r)
{
  int32_t i, p, cmp;
  uint8_t *key;
  uint32_t c;

  cmp=1;
  key=&lz_text_buf[r];
  p=lz_N+1+key[0];
  lz_rson[r]=lz_lson[r]=lz_NIL;
  lz_match_length=0;

  for (;;)
  {
    if (cmp>=0)
    {
      if (lz_rson[p]!=lz_NIL)
        p=lz_rson[p];
      else
      {
        lz_rson[p]=r;
        lz_dad[r]=p;

        return;
      }
    }
    else
    {
      if (lz_lson[p]!=lz_NIL)
        p=lz_lson[p];
      else
      {
        lz_lson[p]=r;
        lz_dad[r]=p;

        return;
      }
    }

    for (i=1; i<lz_F; i++)
      if ((cmp=key[i]-lz_text_buf[p+i])!=0)
        break;

    if (i>lz_THRESHOLD)
    {
      if (i>lz_match_length)
      {
        lz_match_position=((r-p)&(lz_N-1))-1;

        if ((lz_match_length=i)>=lz_F)
          break;
      }

      if (i==lz_match_length)
        if ((c=((r-p)&(lz_N-1))-1)<lz_match_position)
          lz_match_position=c;
    }
  }

  lz_dad[r]=lz_dad[p];
  lz_lson[r]=lz_lson[p];
  lz_rson[r]=lz_rson[p];
  lz_dad[lz_lson[p]]=r;
  lz_dad[lz_rson[p]]=r;

  if (lz_rson[lz_dad[p]]==p)
    lz_rson[lz_dad[p]]=r;
  else
    lz_lson[lz_dad[p]]=r;

  lz_dad[p]=lz_NIL;  /* remove p */
}

/* remove from tree */
void lz_DeleteNode(int32_t p)
{
  int32_t  q;

  if (lz_dad[p]==lz_NIL)
    return; /* not registered */

  if (lz_rson[p]==lz_NIL)
    q=lz_lson[p];
  else
  {
    if (lz_lson[p]==lz_NIL)
      q=lz_rson[p];
    else
    {
      q=lz_lson[p];

      if (lz_rson[q]!=lz_NIL)
      {
        do
        {
          q=lz_rson[q];
        } while (lz_rson[q]!=lz_NIL);

        lz_rson[lz_dad[q]]=lz_lson[q];
        lz_dad[lz_lson[q]]=lz_dad[q];
        lz_lson[q]=lz_lson[p];
        lz_dad[lz_lson[p]]=q;
      }

      lz_rson[q]=lz_rson[p];
      lz_dad[lz_rson[p]]=q;
    }
  }

  lz_dad[q]=lz_dad[p];

  if (lz_rson[lz_dad[p]]==p)
    lz_rson[lz_dad[p]]=q;
  else
    lz_lson[lz_dad[p]]=q;

  lz_dad[p]=lz_NIL;
}

/********** Huffman coding **********/

#define lz_N_CHAR   (0x100-lz_THRESHOLD+lz_F) /* = 314 */
/* kinds of characters (character code = 0..N_CHAR-1) */
#define lz_T        (lz_N_CHAR*2-1) /* size of table = 627 */
#define lz_R        (lz_T-1)        /* position of root = 626 */
#define lz_MAX_FREQ 0x8000          /* updates tree when the */
/* root frequency comes to this value. */

/* tables for encoding and decoding the upper 6 bits of position */

/* for encoding */
uint8_t lz_p_len[64]=
{
  0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

uint8_t lz_p_code[64]=
{
  0x00, 0x20, 0x30, 0x40, 0x50, 0x58, 0x60, 0x68,
  0x70, 0x78, 0x80, 0x88, 0x90, 0x94, 0x98, 0x9C,
  0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC,
  0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
  0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE,
  0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* for decoding */
uint8_t lz_d_code[256]=
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
  0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
  0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
  0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
  0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
  0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
  0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
  0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
  0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
  0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
  0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
  0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
  0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
  0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

uint8_t lz_d_len[256]=
{
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};

uint16_t lz_freq[lz_T+1]; /* frequency table */

uint16_t lz_prnt[lz_T+lz_N_CHAR];
/* pointers to parent nodes, except for the */
/* elements [T..T + N_CHAR - 1] which are used to get */
/* the positions of leaves corresponding to the codes. */

uint16_t lz_son[lz_T]; /* pointers to child nodes (son[], son[] + 1) */

uint16_t lz_getbuf=0;
uint8_t lz_getlen=0;

/* get one bit */
uint16_t lz_GetBit()
{
  uint16_t i;

  while (lz_getlen<=8)
  {
    // Check for overflow
    if (lz_pos<lz_codesize)
      i=lz_input[lz_pos++];
    else
      i=0;

    lz_getbuf|=(i<<(8-lz_getlen));
    lz_getlen+=8;
  }

  i=lz_getbuf;
  lz_getbuf<<=1;
  lz_getlen--;

  return (i>>15);
}

/* get one byte */
uint16_t lz_GetByte()
{
  uint16_t i;

  while (lz_getlen<=8)
  {
    // Check for overflow
    if (lz_pos<lz_codesize)
      i=lz_input[lz_pos++];
    else
      i=0;

    lz_getbuf|=(i<<(8-lz_getlen));
    lz_getlen+=8;
  }

  i=lz_getbuf;
  lz_getbuf<<=8;
  lz_getlen-=8;

  return (i>>8);
}

uint32_t lz_putbuf=0;
uint8_t lz_putlen=0;

/* output c bits of code */
void lz_Putcode(int32_t l, uint32_t c)
{
  lz_putbuf|=c>>lz_putlen;

  if ((lz_putlen+=l)>=8)
  {
    lz_output[lz_codesize++]=(lz_putbuf>>8);

    if ((lz_putlen-=8)>=8)
    {
      lz_output[lz_codesize++]=lz_putbuf;
      lz_putlen-=8;
      lz_putbuf=c<<(l-lz_putlen);
    }
    else
      lz_putbuf<<=8;
  }
}

/* initialization of tree */
void lz_StartHuff(void)
{
  unsigned short i, j;

  for (i=0; i<lz_N_CHAR; i++)
  {
    lz_freq[i]=1;
    lz_son[i]=i+lz_T;
    lz_prnt[i+lz_T]=i;
  }

  i=0;
  j=lz_N_CHAR;

  while (j<=lz_R)
  {
    lz_freq[j]=lz_freq[i]+lz_freq[i+1];
    lz_son[j]=i;
    lz_prnt[i]=lz_prnt[i+1]=j;
    i+=2;
    j++;
  }

  lz_freq[lz_T]=0xffff;
  lz_prnt[lz_R]=0;
}

/* reconstruction of tree */
void lz_Reconst(void)
{
  int32_t i, j, k;
  uint32_t f, l;

  /* collect leaf nodes in the first half of the table */
  /* and replace the freq by (freq + 1) / 2. */
  j=0;
  for (i=0; i<lz_T; i++)
  {
    if (lz_son[i]>=lz_T)
    {
      lz_freq[j]=(lz_freq[i]+1)/2;
      lz_son[j]=lz_son[i];
      j++;
    }
  }

  /* begin constructing tree by connecting sons */
  for (i=0, j=lz_N_CHAR; j<lz_T; i+=2, j++)
  {
    k=i+1;
    f=lz_freq[j]=lz_freq[i]+lz_freq[k];

    for (k=j-1; f<lz_freq[k]; k--);

    k++;
    l=(j-k)*2;
    memmove(&lz_freq[k+1], &lz_freq[k], l);

    lz_freq[k]=f;
    memmove(&lz_son[k+1], &lz_son[k], l);

    lz_son[k]=i;
  }

  /* connect prnt */
  for (i=0; i<lz_T; i++)
  {
    if ((k=lz_son[i])>=lz_T)
      lz_prnt[k]=i;
    else
      lz_prnt[k]=lz_prnt[k+1]=i;
  }
}

/* increment frequency of given code by one, and update tree */
void lz_Update(uint32_t c)
{
  uint32_t i, j, k, l;

  if (lz_freq[lz_R]==lz_MAX_FREQ)
    lz_Reconst();

  c=lz_prnt[c+lz_T];

  do
  {
    k=++lz_freq[c];

    /* if the order is disturbed, exchange nodes */
    if (k>lz_freq[l=c+1])
    {
      while (k>lz_freq[++l]);
      l--;
      lz_freq[c]=lz_freq[l];
      lz_freq[l]=k;

      i=lz_son[c];
      lz_prnt[i]=l;
      if (i<lz_T) lz_prnt[i+1]=l;

      j=lz_son[l];
      lz_son[l]=i;

      lz_prnt[j]=c;
      if (j<lz_T) lz_prnt[j+1]=c;

      lz_son[c]=j;

      c=l;
    }
  } while ((c=lz_prnt[c])!=0);   /* repeat up to root */
}

uint32_t lz_code, lz_len;

void lz_EncodeChar(uint32_t c)
{
  uint32_t i;
  int32_t j, k;

  i=0;
  j=0;
  k=lz_prnt[c+lz_T];

  /* travel from leaf to root */
  do
  {
    i>>=1;

    /* if node's address is odd-numbered, choose bigger brother node */
    if (k&1)
      i+=0x8000;

    j++;
  } while ((k=lz_prnt[k])!=lz_R);

  lz_Putcode(j, i);
  lz_code=i;
  lz_len=j;
  lz_Update(c);
}

void lz_EncodePosition(uint32_t c)
{
  uint32_t i;

  /* output upper 6 bits by table lookup */
  i=c>>6;
  lz_Putcode(lz_p_len[i], (uint32_t)lz_p_code[i]<<8);

  /* output lower 6 bits verbatim */
  lz_Putcode(6, (c&0x3f)<<10);
}

void lz_EncodeEnd(void)
{
  if (lz_putlen)
    lz_output[lz_codesize++]=(lz_putbuf>>8);
}

uint32_t lz_DecodeChar()
{
  uint32_t c;

  c=lz_son[lz_R];

  /* travel from root to leaf, */
  /* choosing the smaller child node (son[]) if the read bit is 0, */
  /* the bigger (son[]+1} if 1 */
  while (c<lz_T)
  {
    c+=lz_GetBit();
    c=lz_son[c];
  }

  c-=lz_T;
  lz_Update(c);

  return c;
}

uint32_t lz_DecodePosition()
{
  uint32_t i, j, c;

  /* recover upper 6 bits from table */
  i=lz_GetByte();
  c=(uint32_t)lz_d_code[i]<<6;
  j=lz_d_len[i];

  /* read lower 6 bits verbatim */
  j-=2;
  while (j--)
  {
    i=(i<<1)+lz_GetBit();
  }

  return (c|(i&0x3f));
}

/* compression */
uint32_t lz_Encode(uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t outlen)
{
  int32_t  i, c, r, s, last_match_length;
  uint32_t len;
  uint32_t offset=0;

  if (inlen==0)
    return 0;

  lz_input=in;
  lz_output=out;

  *(unsigned int*)lz_output=inlen;
  lz_codesize+=4;

  lz_textsize=0;
  lz_StartHuff();
  lz_InitTree();

  s=0;
  r=lz_N-lz_F;

  for (i=s; i<r; i++)
    lz_text_buf[i]=' ';

  for (len=0; ((len<lz_F) && (len<inlen)); len++)
    lz_text_buf[r+len]=lz_input[offset++];

  lz_textsize=len;

  for (i=1; i<=lz_F; i++)
    lz_InsertNode(r-i);

  lz_InsertNode(r);

  do
  {
    if (lz_match_length>len)
      lz_match_length=len;

    if (lz_match_length<=lz_THRESHOLD)
    {
      lz_match_length=1;
      lz_EncodeChar(lz_text_buf[r]);
    }
    else
    {
      lz_EncodeChar(0xff-lz_THRESHOLD+lz_match_length);
      lz_EncodePosition(lz_match_position);
    }

    last_match_length=lz_match_length;

    for (i=0; ((i<last_match_length) && (offset<inlen)); i++)
    {
      c=lz_input[offset++];
      lz_DeleteNode(s);
      lz_text_buf[s]=c;

      if (s<lz_F-1)
        lz_text_buf[s+lz_N]=c;

      s=(s+1)&(lz_N-1);
      r=(r+1)&(lz_N-1);

      lz_InsertNode(r);
    }

    while (i++<last_match_length)
    {
      lz_DeleteNode(s);

      s=(s+1)&(lz_N-1);
      r=(r+1)&(lz_N-1);

      if (--len) lz_InsertNode(r);
    }
  } while (len>0);

  lz_EncodeEnd();

  return lz_codesize;
}

uint32_t lz_DecodedLength(uint8_t *in)
{
  return *(uint32_t*)in;
}

uint32_t lz_Decode(uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t outlen)
{
  uint32_t c, count;
  uint32_t i, j, k, r;

  lz_input=in;
  lz_textsize=*(uint32_t*)lz_input;
  lz_pos=4;

  lz_codesize=inlen;

  if (lz_textsize==0) return 0;

  lz_StartHuff();

  for (i=0; i<lz_N-lz_F; i++)
    lz_text_buf[i]=' ';

  r=lz_N-lz_F;

  for (count=0; ((count<lz_textsize) && (count<outlen) && (lz_pos<lz_codesize));)
  {
    c=lz_DecodeChar();

    if (c<0x100)
    {
      out[count]=c;
      lz_text_buf[r++]=c;
      r&=(lz_N-1);

      count++;
    }
    else
    {
      i=(r-lz_DecodePosition()-1)&(lz_N-1);
      j=c-0xff+lz_THRESHOLD;

      for (k=0; ((k<j) && (count<outlen) && (lz_pos<lz_codesize)); k++)
      {
        c=lz_text_buf[(i+k)&(lz_N-1)];
        out[count]=c;
        lz_text_buf[r++]=c;
        r&=(lz_N-1);

        count++;
      }
    }
  }

  return count;
}

void lz_Init()
{
  lz_textsize=0;
  lz_codesize=0;
  lz_pos=0;

  lz_getbuf=0;
  lz_getlen=0;

  lz_putbuf=0;
  lz_putlen=0;
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
  FILE *infile;
  FILE *outfile;

  uint8_t *inbuffer;
  uint8_t *outbuffer;

  struct stat instat;

  uint32_t resultlen;

  if (argc!=4)
  {
    printf("'lzhuf e file1 file2' encodes file1 into file2.\n"
           "'lzhuf d file2 file1' decodes file2 into file1.\n");
    return EXIT_FAILURE;
  }

  infile=fopen(argv[2], "rb");
  if (infile==NULL) return EXIT_FAILURE;

  outfile=fopen(argv[3], "wb");
  if (outfile==NULL)
  {
    fclose(infile);
    return EXIT_FAILURE;
  }

  lz_Init();

  fstat(fileno(infile), &instat);

  inbuffer=malloc(instat.st_size);

  printf("Input size %ld bytes\n", instat.st_size);

  if (inbuffer!=NULL)
  {
    fread(inbuffer, instat.st_size, 1, infile);

    if ((argv[1][0]|0x20)=='e')
    {
      outbuffer=malloc(instat.st_size*2); // compressed *may* be bigger
      resultlen=lz_Encode(inbuffer, instat.st_size, outbuffer);
    }
    else
    {
      outbuffer=malloc(lz_DecodedLength(inbuffer));
      resultlen=lz_Decode(inbuffer, instat.st_size, outbuffer);
    }

    fwrite(outbuffer, resultlen, 1, outfile);

    free(outbuffer);
    free(inbuffer);
  }

  printf("Output size %d bytes\n", resultlen);

  fclose(infile);
  fclose(outfile);

  return EXIT_SUCCESS;
}
#endif
