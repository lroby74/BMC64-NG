#ifndef AES_APPLE_H_
#define AES_APPLE_H_

/*
   Copyright (C) 2025 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "aes_apple.h"








#if defined(__APPLE__) && defined(HAVE_COMMONCRYPTO_COMMONCRYPTOR_H)

#include <CommonCrypto/CommonCryptor.h>

#define AES128_KEY_LEN 16
#define AES128_BLOCK_SIZE 16

void AES128_ECB_encrypt_apple(const uint8_t *input, const uint8_t *key, uint8_t *output) {
    CCCryptorRef cryptor = NULL;


    CCCryptorStatus status = CCCryptorCreate(
        kCCEncrypt,
        kCCAlgorithmAES128,
        kCCOptionECBMode,
        key,                   
        AES128_KEY_LEN,         
        NULL,                    
        &cryptor           
    );



    if (status != kCCSuccess) {
        return;
    }
 
    size_t dataOutMoved = 0;
    size_t dataOutAvailable = AES128_BLOCK_SIZE;


    status = CCCryptorUpdate(
        cryptor,          
        input,           
        AES128_BLOCK_SIZE,
        output,          
        dataOutAvailable,
        &dataOutMoved   
    );


    CCCryptorRelease(cryptor);
}

#endif

#endif

