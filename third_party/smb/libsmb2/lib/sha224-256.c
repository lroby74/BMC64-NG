





































#include "compat.h"

#include "sha.h"
#include "sha-private.h"

#define SHA256_SHR(bits,word)      ((word) >> (bits))
#define SHA256_ROTL(bits,word)                         \
  (((word) << (bits)) | ((word) >> (32-(bits))))
#define SHA256_ROTR(bits,word)                         \
  (((word) >> (bits)) | ((word) << (32-(bits))))


#define SHA256_SIGMA0(word)   \
  (SHA256_ROTR( 2,word) ^ SHA256_ROTR(13,word) ^ SHA256_ROTR(22,word))
#define SHA256_SIGMA1(word)   \
  (SHA256_ROTR( 6,word) ^ SHA256_ROTR(11,word) ^ SHA256_ROTR(25,word))
#define SHA256_sigma0(word)   \
  (SHA256_ROTR( 7,word) ^ SHA256_ROTR(18,word) ^ SHA256_SHR( 3,word))
#define SHA256_sigma1(word)   \
  (SHA256_ROTR(17,word) ^ SHA256_ROTR(19,word) ^ SHA256_SHR(10,word))




#define SHA224_256AddLength(context, length)               \
  (addTemp = (context)->Length_Low, (context)->Corrupted = \
    (((context)->Length_Low += (length)) < addTemp) &&     \
    (++(context)->Length_High == 0) ? 1 : 0)


static void SHA224_256Finalize (SHA256Context * context, uint8_t Pad_Byte);
static void SHA224_256PadMessage (SHA256Context * context, uint8_t Pad_Byte);
static void SHA224_256ProcessMessageBlock (SHA256Context * context);
static int SHA224_256Reset (SHA256Context * context, uint32_t * H0);
static int SHA224_256ResultN (SHA256Context * context,
			      uint8_t Message_Digest[], int HashSize);


static uint32_t SHA256_H0[SHA256HashSize / 4] = {
  0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
  0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

#if defined(USE_SHA224) && USE_SHA224

static uint32_t SHA224_H0[SHA256HashSize / 4] = {
  0xC1059ED8, 0x367CD507, 0x3070DD17, 0xF70E5939,
  0xFFC00B31, 0x68581511, 0x64F98FA7, 0xBEFA4FA4
};















int
SHA224Reset (SHA224Context * context)
{
  return SHA224_256Reset (context, SHA224_H0);
}





















int
SHA224Input (SHA224Context * context, const uint8_t * message_array,
	     size_t length)
{
  return SHA256Input (context, message_array, length);
}




















int
SHA224FinalBits (SHA224Context * context,
		 const uint8_t message_bits, size_t length)
{
  return SHA256FinalBits (context, message_bits, length);
}



















int
SHA224Result (SHA224Context * context, uint8_t Message_Digest[SHA224HashSize])
{
  return SHA224_256ResultN (context, Message_Digest, SHA224HashSize);
}

#endif















int
SHA256Reset (SHA256Context * context)
{
  return SHA224_256Reset (context, SHA256_H0);
}




















int
SHA256Input (SHA256Context * context, const uint8_t * message_array,
	     size_t length)
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

      if (!SHA224_256AddLength (context, 8) &&
	  (context->Message_Block_Index == SHA256_Message_Block_Size))
	SHA224_256ProcessMessageBlock (context);

      message_array++;
    }

  return shaSuccess;

}




















int
SHA256FinalBits (SHA256Context * context,
		 const uint8_t message_bits, size_t length)
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

  if ((context->Computed) || (length >= 8) || (length == 0))
    {
      context->Corrupted = shaStateError;
      return shaStateError;
    }

  if (context->Corrupted)
    return context->Corrupted;

  SHA224_256AddLength (context, (uint32_t)length);
  SHA224_256Finalize (context, (uint8_t)
		      ((message_bits & masks[length]) | markbit[length]));

  return shaSuccess;
}



















int
SHA256Result (SHA256Context * context, uint8_t Message_Digest[SHA256HashSize])
{
  return SHA224_256ResultN (context, Message_Digest, SHA256HashSize);
}



















static void
SHA224_256Finalize (SHA256Context * context, uint8_t Pad_Byte)
{
  int i;
  SHA224_256PadMessage (context, Pad_Byte);

  for (i = 0; i < SHA256_Message_Block_Size; ++i)
    context->Message_Block[i] = 0;
  context->Length_Low = 0;
  context->Length_High = 0;
  context->Computed = 1;
}

























static void
SHA224_256PadMessage (SHA256Context * context, uint8_t Pad_Byte)
{






  if (context->Message_Block_Index >= (SHA256_Message_Block_Size - 8))
    {
      context->Message_Block[context->Message_Block_Index++] = Pad_Byte;
      while (context->Message_Block_Index < SHA256_Message_Block_Size)
	context->Message_Block[context->Message_Block_Index++] = 0;
      SHA224_256ProcessMessageBlock (context);
    }
  else
    context->Message_Block[context->Message_Block_Index++] = Pad_Byte;

  while (context->Message_Block_Index < (SHA256_Message_Block_Size - 8))
    context->Message_Block[context->Message_Block_Index++] = 0;




  context->Message_Block[56] = (uint8_t) (context->Length_High >> 24);
  context->Message_Block[57] = (uint8_t) (context->Length_High >> 16);
  context->Message_Block[58] = (uint8_t) (context->Length_High >> 8);
  context->Message_Block[59] = (uint8_t) (context->Length_High);
  context->Message_Block[60] = (uint8_t) (context->Length_Low >> 24);
  context->Message_Block[61] = (uint8_t) (context->Length_Low >> 16);
  context->Message_Block[62] = (uint8_t) (context->Length_Low >> 8);
  context->Message_Block[63] = (uint8_t) (context->Length_Low);

  SHA224_256ProcessMessageBlock (context);
}




















static void
SHA224_256ProcessMessageBlock (SHA256Context * context)
{

  static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
    0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
    0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
  };
  int t, t4;
  uint32_t temp1, temp2;
  uint32_t W[64];
  uint32_t A, B, C, D, E, F, G, H;




  for (t = t4 = 0; t < 16; t++, t4 += 4)
    W[t] = (((uint32_t) context->Message_Block[t4]) << 24) |
      (((uint32_t) context->Message_Block[t4 + 1]) << 16) |
      (((uint32_t) context->Message_Block[t4 + 2]) << 8) |
      (((uint32_t) context->Message_Block[t4 + 3]));

  for (t = 16; t < 64; t++)
    W[t] = SHA256_sigma1 (W[t - 2]) + W[t - 7] +
      SHA256_sigma0 (W[t - 15]) + W[t - 16];

  A = context->Intermediate_Hash[0];
  B = context->Intermediate_Hash[1];
  C = context->Intermediate_Hash[2];
  D = context->Intermediate_Hash[3];
  E = context->Intermediate_Hash[4];
  F = context->Intermediate_Hash[5];
  G = context->Intermediate_Hash[6];
  H = context->Intermediate_Hash[7];

  for (t = 0; t < 64; t++)
    {
      temp1 = H + SHA256_SIGMA1 (E) + SHA_Ch (E, F, G) + K[t] + W[t];
      temp2 = SHA256_SIGMA0 (A) + SHA_Maj (A, B, C);
      H = G;
      G = F;
      F = E;
      E = D + temp1;
      D = C;
      C = B;
      B = A;
      A = temp1 + temp2;
    }

  context->Intermediate_Hash[0] += A;
  context->Intermediate_Hash[1] += B;
  context->Intermediate_Hash[2] += C;
  context->Intermediate_Hash[3] += D;
  context->Intermediate_Hash[4] += E;
  context->Intermediate_Hash[5] += F;
  context->Intermediate_Hash[6] += G;
  context->Intermediate_Hash[7] += H;

  context->Message_Block_Index = 0;
}

















static int
SHA224_256Reset (SHA256Context * context, uint32_t * H0)
{
  if (!context)
    return shaNull;

  context->Length_Low = 0;
  context->Length_High = 0;
  context->Message_Block_Index = 0;

  context->Intermediate_Hash[0] = H0[0];
  context->Intermediate_Hash[1] = H0[1];
  context->Intermediate_Hash[2] = H0[2];
  context->Intermediate_Hash[3] = H0[3];
  context->Intermediate_Hash[4] = H0[4];
  context->Intermediate_Hash[5] = H0[5];
  context->Intermediate_Hash[6] = H0[6];
  context->Intermediate_Hash[7] = H0[7];

  context->Computed = 0;
  context->Corrupted = 0;

  return shaSuccess;
}





















static int
SHA224_256ResultN (SHA256Context * context,
		   uint8_t Message_Digest[], int HashSize)
{
  int i;

  if (!context || !Message_Digest)
    return shaNull;

  if (context->Corrupted)
    return context->Corrupted;

  if (!context->Computed)
    SHA224_256Finalize (context, 0x80);

  for (i = 0; i < HashSize; ++i)
    Message_Digest[i] = (uint8_t)
      (context->Intermediate_Hash[i >> 2] >> 8 * (3 - (i & 0x03)));

  return shaSuccess;
}
