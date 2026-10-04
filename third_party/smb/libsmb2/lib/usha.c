






#include "compat.h"

#include "sha.h"


















int
USHAReset (USHAContext * ctx, enum SHAversion whichSha)
{
  if (ctx)
    {
      ctx->whichSha = whichSha;
      switch (whichSha)
	{
#if defined(USE_SHA1) && USE_SHA1
	case SHA1:
	  return SHA1Reset ((SHA1Context *) & ctx->ctx);
#endif
#if defined(USE_SHA224) && USE_SHA224
	case SHA224:
	  return SHA224Reset ((SHA224Context *) & ctx->ctx);
#endif
	case SHA256:
	  return SHA256Reset ((SHA256Context *) & ctx->ctx);
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
	case SHA384:
	  return SHA384Reset ((SHA384Context *) & ctx->ctx);
	case SHA512:
	  return SHA512Reset ((SHA512Context *) & ctx->ctx);
#endif
	default:
	  return shaBadParam;
	}
    }
  else
    {
      return shaNull;
    }
}





















int
USHAInput (USHAContext * ctx, const uint8_t * bytes, size_t bytecount)
{
  if (ctx)
    {
      switch (ctx->whichSha)
	{
#if defined(USE_SHA1) && USE_SHA1
	case SHA1:
	  return SHA1Input ((SHA1Context *) & ctx->ctx, bytes, bytecount);
#endif
#if defined(USE_SHA224) && USE_SHA224
	case SHA224:
	  return SHA224Input ((SHA224Context *) & ctx->ctx, bytes, bytecount);
#endif
	case SHA256:
	  return SHA256Input ((SHA256Context *) & ctx->ctx, bytes, bytecount);
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
	case SHA384:
	  return SHA384Input ((SHA384Context *) & ctx->ctx, bytes, bytecount);
	case SHA512:
	  return SHA512Input ((SHA512Context *) & ctx->ctx, bytes, bytecount);
#endif
	default:
	  return shaBadParam;
	}
    }
  else
    {
      return shaNull;
    }
}




















int
USHAFinalBits (USHAContext * ctx, const uint8_t bits, size_t bitcount)
{
  if (ctx)
    {
      switch (ctx->whichSha)
	{
#if defined(USE_SHA1) && USE_SHA1
	case SHA1:
	  return SHA1FinalBits ((SHA1Context *) & ctx->ctx, bits, bitcount);
#endif
#if defined(USE_SHA224) && USE_SHA224
	case SHA224:
	  return SHA224FinalBits ((SHA224Context *) & ctx->ctx, bits,
				  bitcount);
#endif
	case SHA256:
	  return SHA256FinalBits ((SHA256Context *) & ctx->ctx, bits,
				  bitcount);
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
	case SHA384:
	  return SHA384FinalBits ((SHA384Context *) & ctx->ctx, bits,
				  bitcount);
	case SHA512:
	  return SHA512FinalBits ((SHA512Context *) & ctx->ctx, bits,
				  bitcount);
#endif
	default:
	  return shaBadParam;
	}
    }
  else
    {
      return shaNull;
    }
}




















int
USHAResult (USHAContext * ctx, uint8_t Message_Digest[USHAMaxHashSize])
{
  if (ctx)
    {
      switch (ctx->whichSha)
	{
#if defined(USE_SHA1) && USE_SHA1
	case SHA1:
	  return SHA1Result ((SHA1Context *) & ctx->ctx, Message_Digest);
#endif
#if defined(USE_SHA224) && USE_SHA224
	case SHA224:
	  return SHA224Result ((SHA224Context *) & ctx->ctx, Message_Digest);
#endif
	case SHA256:
	  return SHA256Result ((SHA256Context *) & ctx->ctx, Message_Digest);
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
	case SHA384:
	  return SHA384Result ((SHA384Context *) & ctx->ctx, Message_Digest);
	case SHA512:
	  return SHA512Result ((SHA512Context *) & ctx->ctx, Message_Digest);
#endif
	default:
	  return shaBadParam;
	}
    }
  else
    {
      return shaNull;
    }
}
















int
USHABlockSize (enum SHAversion whichSha)
{
  switch (whichSha)
    {
#if defined(USE_SHA1) && USE_SHA1
    case SHA1:
      return SHA1_Message_Block_Size;
#endif
#if defined(USE_SHA224) && USE_SHA224
    case SHA224:
      return SHA224_Message_Block_Size;
#endif
    case SHA256:
      return SHA256_Message_Block_Size;
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
    case SHA384:
      return SHA384_Message_Block_Size;
    case SHA512:
      return SHA512_Message_Block_Size;
#endif
    default:
      return SHA512_Message_Block_Size;
    }
}
















int
USHAHashSize (enum SHAversion whichSha)
{
  switch (whichSha)
    {
#if defined(USE_SHA1) && USE_SHA1
    case SHA1:
      return SHA1HashSize;
#endif
#if defined(USE_SHA224) && USE_SHA224
    case SHA224:
      return SHA224HashSize;
#endif
    case SHA256:
      return SHA256HashSize;
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
    case SHA384:
      return SHA384HashSize;
    case SHA512:
      return SHA512HashSize;
#endif
    default:
      return SHA512HashSize;
    }
}
















int
USHAHashSizeBits (enum SHAversion whichSha)
{
  switch (whichSha)
    {
#if defined(USE_SHA1) && USE_SHA1
    case SHA1:
      return SHA1HashSizeBits;
#endif
#if defined(USE_SHA224) && USE_SHA224
    case SHA224:
      return SHA224HashSizeBits;
#endif
    case SHA256:
      return SHA256HashSizeBits;
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
    case SHA384:
      return SHA384HashSizeBits;
    case SHA512:
      return SHA512HashSizeBits;
#endif
    default:
      return SHA512HashSizeBits;
    }
}
