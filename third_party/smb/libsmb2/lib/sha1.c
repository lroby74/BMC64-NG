




































#include "compat.h"

#include "sha.h"
#include "sha-private.h"

#if defined(USE_SHA1) && USE_SHA1




#define SHA1_ROTL(bits,word) \
                (((word) << (bits)) | ((word) >> (32-(bits))))




#define SHA1AddLength(context, length)                     \
    (addTemp = (context)->Length_Low,                      \
     (context)->Corrupted =                                \
        (((context)->Length_Low += (length)) < addTemp) && \
        (++(context)->Length_High == 0) ? 1 : 0)


static void SHA1Finalize (SHA1Context * context, uint8_t Pad_Byte);
static void SHA1PadMessage (SHA1Context *, uint8_t Pad_Byte);
static void SHA1ProcessMessageBlock (SHA1Context *);
















int
SHA1Reset (SHA1Context * context)
{
  if (!context)
    return shaNull;

  context->Length_Low = 0;
  context->Length_High = 0;
  context->Message_Block_Index = 0;


  context->Intermediate_Hash[0] = 0x67452301;
  context->Intermediate_Hash[1] = 0xEFCDAB89;
  context->Intermediate_Hash[2] = 0x98BADCFE;
  context->Intermediate_Hash[3] = 0x10325476;
  context->Intermediate_Hash[4] = 0xC3D2E1F0;

  context->Computed = 0;
  context->Corrupted = 0;

  return shaSuccess;
}





















int
SHA1Input (SHA1Context * context,
	   const uint8_t * message_array, size_t length)
{
  uint32_t addTemp;

  if (!length)
    return shaSuccess;

  if (!context || !message_array)
    return shaNull;

  if (context->Computed)
    {
      context->Corrupted = shaStateError;
      return shaStateError;
    }

  if (context->Corrupted)
    return context->Corrupted;

  while (length-- && !context->Corrupted)
    {
      context->Message_Block[context->Message_Block_Index++] =
	(*message_array & 0xFF);

      if (!SHA1AddLength (context, 8) &&
	  (context->Message_Block_Index == SHA1_Message_Block_Size))
	SHA1ProcessMessageBlock (context);

      message_array++;
    }

  return shaSuccess;
}




















int
SHA1FinalBits (SHA1Context * context, const uint8_t message_bits,
	       size_t length)
{
  uint32_t addTemp;
  uint8_t masks[8] = {
      0x00,   0x80,
      0xC0,   0xE0,
      0xF0,   0xF8,
      0xFC,   0xFE
  };
  uint8_t markbit[8] = {
      0x80,   0x40,
      0x20,   0x10,
      0x08,   0x04,
      0x02,   0x01
  };

  if (!length)
    return shaSuccess;

  if (!context)
    return shaNull;

  if (context->Computed || (length >= 8) || (length == 0))
    {
      context->Corrupted = shaStateError;
      return shaStateError;
    }

  if (context->Corrupted)
    return context->Corrupted;

  SHA1AddLength (context, length);
  SHA1Finalize (context,
		(uint8_t) ((message_bits & masks[length]) | markbit[length]));

  return shaSuccess;
}




















int
SHA1Result (SHA1Context * context, uint8_t Message_Digest[SHA1HashSize])
{
  int i;

  if (!context || !Message_Digest)
    return shaNull;

  if (context->Corrupted)
    return context->Corrupted;

  if (!context->Computed)
    SHA1Finalize (context, 0x80);

  for (i = 0; i < SHA1HashSize; ++i)
    Message_Digest[i] = (uint8_t) (context->Intermediate_Hash[i >> 2]
				   >> 8 * (3 - (i & 0x03)));

  return shaSuccess;
}




















static void
SHA1Finalize (SHA1Context * context, uint8_t Pad_Byte)
{
  int i;
  SHA1PadMessage (context, Pad_Byte);

  for (i = 0; i < SHA1_Message_Block_Size; ++i)
    context->Message_Block[i] = 0;
  context->Length_Low = 0;
  context->Length_High = 0;
  context->Computed = 1;
}

























static void
SHA1PadMessage (SHA1Context * context, uint8_t Pad_Byte)
{






  if (context->Message_Block_Index >= (SHA1_Message_Block_Size - 8))
    {
      context->Message_Block[context->Message_Block_Index++] = Pad_Byte;
      while (context->Message_Block_Index < SHA1_Message_Block_Size)
	context->Message_Block[context->Message_Block_Index++] = 0;

      SHA1ProcessMessageBlock (context);
    }
  else
    context->Message_Block[context->Message_Block_Index++] = Pad_Byte;

  while (context->Message_Block_Index < (SHA1_Message_Block_Size - 8))
    context->Message_Block[context->Message_Block_Index++] = 0;




  context->Message_Block[56] = (uint8_t) (context->Length_High >> 24);
  context->Message_Block[57] = (uint8_t) (context->Length_High >> 16);
  context->Message_Block[58] = (uint8_t) (context->Length_High >> 8);
  context->Message_Block[59] = (uint8_t) (context->Length_High);
  context->Message_Block[60] = (uint8_t) (context->Length_Low >> 24);
  context->Message_Block[61] = (uint8_t) (context->Length_Low >> 16);
  context->Message_Block[62] = (uint8_t) (context->Length_Low >> 8);
  context->Message_Block[63] = (uint8_t) (context->Length_Low);

  SHA1ProcessMessageBlock (context);
}



















static void
SHA1ProcessMessageBlock (SHA1Context * context)
{

  const uint32_t K[4] = {
    0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
  };
  int t;
  uint32_t temp;
  uint32_t W[80];
  uint32_t A, B, C, D, E;




  for (t = 0; t < 16; t++)
    {
      W[t] = ((uint32_t) context->Message_Block[t * 4]) << 24;
      W[t] |= ((uint32_t) context->Message_Block[t * 4 + 1]) << 16;
      W[t] |= ((uint32_t) context->Message_Block[t * 4 + 2]) << 8;
      W[t] |= ((uint32_t) context->Message_Block[t * 4 + 3]);
    }
  for (t = 16; t < 80; t++)
    W[t] = SHA1_ROTL (1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);

  A = context->Intermediate_Hash[0];
  B = context->Intermediate_Hash[1];
  C = context->Intermediate_Hash[2];
  D = context->Intermediate_Hash[3];
  E = context->Intermediate_Hash[4];

  for (t = 0; t < 20; t++)
    {
      temp = SHA1_ROTL (5, A) + SHA_Ch (B, C, D) + E + W[t] + K[0];
      E = D;
      D = C;
      C = SHA1_ROTL (30, B);
      B = A;
      A = temp;
    }

  for (t = 20; t < 40; t++)
    {
      temp = SHA1_ROTL (5, A) + SHA_Parity (B, C, D) + E + W[t] + K[1];
      E = D;
      D = C;
      C = SHA1_ROTL (30, B);
      B = A;
      A = temp;
    }

  for (t = 40; t < 60; t++)
    {
      temp = SHA1_ROTL (5, A) + SHA_Maj (B, C, D) + E + W[t] + K[2];
      E = D;
      D = C;
      C = SHA1_ROTL (30, B);
      B = A;
      A = temp;
    }

  for (t = 60; t < 80; t++)
    {
      temp = SHA1_ROTL (5, A) + SHA_Parity (B, C, D) + E + W[t] + K[3];
      E = D;
      D = C;
      C = SHA1_ROTL (30, B);
      B = A;
      A = temp;
    }

  context->Intermediate_Hash[0] += A;
  context->Intermediate_Hash[1] += B;
  context->Intermediate_Hash[2] += C;
  context->Intermediate_Hash[3] += D;
  context->Intermediate_Hash[4] += E;

  context->Message_Block_Index = 0;
}

#endif
