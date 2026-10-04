/* Based on https://github.com/bitdust/tiny-AES128-C
 * Licenced as Public Domain
 */

#ifndef _AES_REFERENCE_H_
#define _AES_REFERENCE_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
 
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif







#ifndef CBC
  #define CBC 0
#endif

#ifndef ECB
  #define ECB 1
#endif



#if defined(ECB) && ECB

void AES128_ECB_encrypt_reference(uint8_t* input, const uint8_t* key, uint8_t *output);
void AES128_ECB_decrypt_reference(uint8_t* input, const uint8_t* key, uint8_t *output);

#endif


#if defined(CBC) && CBC

void AES128_CBC_encrypt_buffer_reference(uint8_t* output, uint8_t* input, uint32_t length, const uint8_t* key, uint8_t* iv);
void AES128_CBC_decrypt_buffer_reference(uint8_t* output, uint8_t* input, uint32_t length, const uint8_t* key, uint8_t* iv);

#endif



#endif
