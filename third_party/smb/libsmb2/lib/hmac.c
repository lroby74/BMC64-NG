








#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "compat.h"

#include "sha.h"



























int
hmac (SHAversion whichSha, const unsigned char *text, size_t text_len,
      const unsigned char *key, size_t key_len, uint8_t digest[USHAMaxHashSize])
{
  HMACContext ctx;
  return hmacReset (&ctx, whichSha, key, key_len) ||
    hmacInput (&ctx, text, text_len) || hmacResult (&ctx, digest);
}






















int
hmacReset (HMACContext * ctx, enum SHAversion whichSha,
	   const unsigned char *key, size_t key_len)
{
  int i, blocksize, hashsize;


  unsigned char k_ipad[USHA_Max_Message_Block_Size];


  unsigned char tempkey[USHAMaxHashSize];

  if (!ctx)
    return shaNull;

  blocksize = ctx->blockSize = USHABlockSize (whichSha);
  hashsize = ctx->hashSize = USHAHashSize (whichSha);

  ctx->whichSha = whichSha;





  if (key_len > (size_t)blocksize)
    {
      USHAContext tctx;
      int err = USHAReset (&tctx, whichSha) ||
	USHAInput (&tctx, key, key_len) || USHAResult (&tctx, tempkey);
      if (err != shaSuccess)
	return err;

      key = tempkey;
      key_len = hashsize;
    }













  for (i = 0; i < (int)key_len; i++)
    {
      k_ipad[i] = key[i] ^ 0x36;
      ctx->k_opad[i] = key[i] ^ 0x5c;
    }

  for (; i < blocksize; i++)
    {
      k_ipad[i] = 0x36;
      ctx->k_opad[i] = 0x5c;
    }



  return USHAReset (&ctx->shaContext, whichSha) ||

    USHAInput (&ctx->shaContext, k_ipad, blocksize);
}





















int
hmacInput (HMACContext * ctx, const unsigned char *text, size_t text_len)
{
  if (!ctx)
    return shaNull;

  return USHAInput (&ctx->shaContext, text, text_len);
}




















int
hmacFinalBits (HMACContext * ctx, const uint8_t bits, size_t bitcount)
{
  if (!ctx)
    return shaNull;

  return USHAFinalBits (&ctx->shaContext, bits, bitcount);
}






















int
hmacResult (HMACContext * ctx, uint8_t * digest)
{
  if (!ctx)
    return shaNull;



  return USHAResult (&ctx->shaContext, digest) ||


    USHAReset (&ctx->shaContext, ctx->whichSha) ||

    USHAInput (&ctx->shaContext, ctx->k_opad, ctx->blockSize) ||

    USHAInput (&ctx->shaContext, digest, ctx->hashSize) ||

    USHAResult (&ctx->shaContext, digest);
}
