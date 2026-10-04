




/* Copyright (C) 1990-2, RSA Data Security, Inc. All rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD4 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD4 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "compat.h"

#include "md4.h"



#define S11 3
#define S12 7
#define S13 11
#define S14 19
#define S21 3
#define S22 5
#define S23 9
#define S24 13
#define S31 3
#define S32 9
#define S33 11
#define S34 15

static void MD4Transform(uint32_t [4], unsigned char [64]);
static void Encode(unsigned char *, uint32_t *, unsigned int);
static void Decode(uint32_t *, unsigned char *, unsigned int);
static void MD4_memcpy(unsigned char *, unsigned char *, unsigned int);
static void MD4_memset(unsigned char *, int, unsigned int);

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};



#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))



#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))




#define FF(a, b, c, d, x, s) { \
    (a) += F ((b), (c), (d)) + (x); \
    (a) = ROTATE_LEFT ((a), (s)); \
  }
#define GG(a, b, c, d, x, s) { \
    (a) += G ((b), (c), (d)) + (x) + (uint32_t)0x5a827999; \
    (a) = ROTATE_LEFT ((a), (s)); \
  }
#define HH(a, b, c, d, x, s) { \
    (a) += H ((b), (c), (d)) + (x) + (uint32_t)0x6ed9eba1; \
    (a) = ROTATE_LEFT ((a), (s)); \
  }



void MD4Init(MD4_CTX *context)
{
  context->count[0] = context->count[1] = 0;



  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}





void MD4Update(MD4_CTX *context, unsigned char *input, unsigned int inputLen)
{
  unsigned int i, index, partLen;


  index = (unsigned int)((context->count[0] >> 3) & 0x3F);

  if ((context->count[0] += ((uint32_t)inputLen << 3))
      < ((uint32_t)inputLen << 3))
    context->count[1]++;
  context->count[1] += ((uint32_t)inputLen >> 29);

  partLen = 64 - index;



  if (inputLen >= partLen) {
    MD4_memcpy
      ((unsigned char *)&context->buffer[index], (unsigned char *)input, partLen);
    MD4Transform (context->state, context->buffer);

    for (i = partLen; i + 63 < inputLen; i += 64)
      MD4Transform (context->state, &input[i]);

    index = 0;
  }
  else
    i = 0;


  MD4_memcpy
    ((unsigned char *)&context->buffer[index], (unsigned char *)&input[i],
     inputLen-i);
}




void MD4Final(unsigned char digest[16], MD4_CTX *context)
{
  unsigned char bits[8];
  unsigned int index, padLen;


  Encode (bits, context->count, 8);



  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD4Update (context, PADDING, padLen);


  MD4Update (context, bits, 8);

  Encode (digest, context->state, 16);



  MD4_memset ((unsigned char *)context, 0, sizeof (*context));

}



static void MD4Transform(uint32_t state[4], unsigned char block[64])
{
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode (x, block, 64);


  FF (a, b, c, d, x[ 0], S11);
  FF (d, a, b, c, x[ 1], S12);
  FF (c, d, a, b, x[ 2], S13);
  FF (b, c, d, a, x[ 3], S14);
  FF (a, b, c, d, x[ 4], S11);
  FF (d, a, b, c, x[ 5], S12);
  FF (c, d, a, b, x[ 6], S13);
  FF (b, c, d, a, x[ 7], S14);
  FF (a, b, c, d, x[ 8], S11);
  FF (d, a, b, c, x[ 9], S12);
  FF (c, d, a, b, x[10], S13);
  FF (b, c, d, a, x[11], S14);
  FF (a, b, c, d, x[12], S11);
  FF (d, a, b, c, x[13], S12);
  FF (c, d, a, b, x[14], S13);
  FF (b, c, d, a, x[15], S14);


  GG (a, b, c, d, x[ 0], S21);
  GG (d, a, b, c, x[ 4], S22);
  GG (c, d, a, b, x[ 8], S23);
  GG (b, c, d, a, x[12], S24);
  GG (a, b, c, d, x[ 1], S21);
  GG (d, a, b, c, x[ 5], S22);
  GG (c, d, a, b, x[ 9], S23);
  GG (b, c, d, a, x[13], S24);
  GG (a, b, c, d, x[ 2], S21);
  GG (d, a, b, c, x[ 6], S22);
  GG (c, d, a, b, x[10], S23);
  GG (b, c, d, a, x[14], S24);
  GG (a, b, c, d, x[ 3], S21);
  GG (d, a, b, c, x[ 7], S22);
  GG (c, d, a, b, x[11], S23);
  GG (b, c, d, a, x[15], S24);


  HH (a, b, c, d, x[ 0], S31);
  HH (d, a, b, c, x[ 8], S32);
  HH (c, d, a, b, x[ 4], S33);
  HH (b, c, d, a, x[12], S34);
  HH (a, b, c, d, x[ 2], S31);
  HH (d, a, b, c, x[10], S32);
  HH (c, d, a, b, x[ 6], S33);
  HH (b, c, d, a, x[14], S34);
  HH (a, b, c, d, x[ 1], S31);
  HH (d, a, b, c, x[ 9], S32);
  HH (c, d, a, b, x[ 5], S33);
  HH (b, c, d, a, x[13], S34);
  HH (a, b, c, d, x[ 3], S31);
  HH (d, a, b, c, x[11], S32);
  HH (c, d, a, b, x[ 7], S33);
  HH (b, c, d, a, x[15], S34);

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;



  MD4_memset ((unsigned char *)x, 0, sizeof (x));
}




static void Encode(unsigned char *output, uint32_t *input, unsigned int len)
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
    output[j] = (unsigned char)(input[i] & 0xff);
    output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
    output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
    output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
  }
}




static void Decode(uint32_t *output, unsigned char *input, unsigned int len)
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4)
    output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j+1]) << 8) |
      (((uint32_t)input[j+2]) << 16) | (((uint32_t)input[j+3]) << 24);
}



static void MD4_memcpy(unsigned char *output, unsigned char *input, unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
    output[i] = input[i];
}



static void MD4_memset(unsigned char *output, int value, unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
    ((char *)output)[i] = (char)value;
}
