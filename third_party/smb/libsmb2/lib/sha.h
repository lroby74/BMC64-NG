

#ifndef _SHA_H_
#define _SHA_H_

#ifndef USE_SHA1
  #define USE_SHA1 0
#endif

#ifndef USE_SHA224
  #define USE_SHA224 0
#endif

#ifndef USE_SHA384_SHA512
  #define USE_SHA384_SHA512 1
#endif






















#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif












#ifndef _SHA_enum_
#define _SHA_enum_



enum
{
  shaSuccess = 0,
  shaNull,
  shaInputTooLong,
  shaStateError,
  shaBadParam
};
#endif





enum
{
#if defined(USE_SHA1) && USE_SHA1
  SHA1_Message_Block_Size = 64,
  SHA1HashSize = 20,
  SHA1HashSizeBits = 160,
#endif
#if defined(USE_SHA224) && USE_SHA224
  SHA224_Message_Block_Size = 64,
  SHA224HashSize = 28,
  SHA224HashSizeBits = 224,
#endif
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
  SHA384_Message_Block_Size = 128,  
  SHA384HashSize = 48,
  SHA384HashSizeBits = 384,
#endif
  SHA256_Message_Block_Size = 64,
  SHA512_Message_Block_Size = 128,
  USHA_Max_Message_Block_Size = SHA512_Message_Block_Size,

  SHA256HashSize = 32,
  SHA512HashSize = 64,
  USHAMaxHashSize = SHA512HashSize,

  SHA256HashSizeBits = 256,
  SHA512HashSizeBits = 512, USHAMaxHashSizeBits = SHA512HashSizeBits
};




typedef enum SHAversion
{
#if defined(USE_SHA1) && USE_SHA1
  SHA1,
#endif
#if defined(USE_SHA224) && USE_SHA224
  SHA224,
#endif
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
  SHA384,
  SHA512,
#endif
  SHA256
} SHAversion;

#if defined(USE_SHA1) && USE_SHA1




typedef struct SHA1Context
{
  uint32_t Intermediate_Hash[SHA1HashSize / 4];

  uint32_t Length_Low;
  uint32_t Length_High;

  int_least16_t Message_Block_Index;

  uint8_t Message_Block[SHA1_Message_Block_Size];

  int Computed;
  int Corrupted;
} SHA1Context;
#endif





typedef struct SHA256Context
{
  uint32_t Intermediate_Hash[SHA256HashSize / 4];

  uint32_t Length_Low;
  uint32_t Length_High;

  int_least16_t Message_Block_Index;

  uint8_t Message_Block[SHA256_Message_Block_Size];

  int Computed;
  int Corrupted;
} SHA256Context;





typedef struct SHA512Context
{
#ifdef USE_32BIT_ONLY
  uint32_t Intermediate_Hash[SHA512HashSize / 4];
  uint32_t Length[4];
#else
  uint64_t Intermediate_Hash[SHA512HashSize / 8];
  uint64_t Length_Low, Length_High;
#endif
  int_least16_t Message_Block_Index;

  uint8_t Message_Block[SHA512_Message_Block_Size];

  int Computed;
  int Corrupted;
} SHA512Context;

#if defined(USE_SHA224) && USE_SHA224




typedef struct SHA256Context SHA224Context;
#endif

#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512




typedef struct SHA512Context SHA384Context;
#endif





typedef struct USHAContext
{
  SHAversion whichSha;
  union
  {
#if defined(USE_SHA1) && USE_SHA1
    SHA1Context sha1Context;
#endif
#if defined(USE_SHA224) && USE_SHA224
    SHA224Context sha224Context;
#endif
    SHA256Context sha256Context;
#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512
    SHA384Context sha384Context;
    SHA512Context sha512Context;
#endif
  } ctx;
} USHAContext;





typedef struct HMACContext
{
  SHAversion whichSha;
  int hashSize;
  int blockSize;
  USHAContext shaContext;
  unsigned char k_opad[USHA_Max_Message_Block_Size];

} HMACContext;





#if defined(USE_SHA1) && USE_SHA1

extern int SHA1Reset (SHA1Context *);
extern int SHA1Input (SHA1Context *, const uint8_t * bytes,
		      size_t bytecount);
extern int SHA1FinalBits (SHA1Context *, const uint8_t bits,
			  size_t bitcount);
extern int SHA1Result (SHA1Context *, uint8_t Message_Digest[SHA1HashSize]);
#endif

#if defined(USE_SHA224) && USE_SHA224

extern int SHA224Reset (SHA224Context *);
extern int SHA224Input (SHA224Context *, const uint8_t * bytes,
			size_t bytecount);
extern int SHA224FinalBits (SHA224Context *, const uint8_t bits,
			    size_t bitcount);
extern int SHA224Result (SHA224Context *,
			 uint8_t Message_Digest[SHA224HashSize]);
#endif


extern int SHA256Reset (SHA256Context *);
extern int SHA256Input (SHA256Context *, const uint8_t * bytes,
			size_t bytecount);
extern int SHA256FinalBits (SHA256Context *, const uint8_t bits,
			    size_t bitcount);
extern int SHA256Result (SHA256Context *,
			 uint8_t Message_Digest[SHA256HashSize]);

#if defined(USE_SHA384_SHA512) && USE_SHA384_SHA512

extern int SHA384Reset (SHA384Context *);
extern int SHA384Input (SHA384Context *, const uint8_t * bytes,
			size_t bytecount);
extern int SHA384FinalBits (SHA384Context *, const uint8_t bits,
			    size_t bitcount);
extern int SHA384Result (SHA384Context *,
			 uint8_t Message_Digest[SHA384HashSize]);


extern int SHA512Reset (SHA512Context *);
extern int SHA512Input (SHA512Context *, const uint8_t * bytes,
			size_t bytecount);
extern int SHA512FinalBits (SHA512Context *, const uint8_t bits,
			    size_t bitcount);
extern int SHA512Result (SHA512Context *,
			 uint8_t Message_Digest[SHA512HashSize]);
#endif


extern int USHAReset (USHAContext *, SHAversion whichSha);
extern int USHAInput (USHAContext *,
		      const uint8_t * bytes, size_t bytecount);
extern int USHAFinalBits (USHAContext *,
			  const uint8_t bits, size_t bitcount);
extern int USHAResult (USHAContext *,
		       uint8_t Message_Digest[USHAMaxHashSize]);
extern int USHABlockSize (enum SHAversion whichSha);
extern int USHAHashSize (enum SHAversion whichSha);
extern int USHAHashSizeBits (enum SHAversion whichSha);






extern int hmac (SHAversion whichSha,
		 const unsigned char *text,
		 size_t text_len,
		 const unsigned char *key,
		 size_t key_len,
		 uint8_t digest[USHAMaxHashSize]);






extern int hmacReset (HMACContext * ctx, enum SHAversion whichSha,
		      const unsigned char *key, size_t key_len);
extern int hmacInput (HMACContext * ctx, const unsigned char *text,
		      size_t text_len);

extern int hmacFinalBits (HMACContext * ctx, const uint8_t bits,
			  size_t bitcount);
extern int hmacResult (HMACContext * ctx, uint8_t *digest);

#endif
