




#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "compat.h"

#include "md5.h"








void
smb2_hmac_md5(unsigned char *text, int text_len, unsigned char *key, unsigned int key_len,
	 unsigned char *digest)
{
        struct MD5Context context;
        unsigned char k_ipad[65];


        unsigned char k_opad[65];


        unsigned char tk[16];
        int i;

        if (key_len > 64) {
		struct MD5Context tctx;

                MD5Init(&tctx);
                MD5Update(&tctx, key, key_len);
                MD5Final(tk, &tctx);

                key = tk;
                key_len = 16;
        }












        memset(k_ipad, 0, sizeof k_ipad);
        memset(k_opad, 0, sizeof k_opad);
        memmove(k_ipad, key, key_len);
        memmove(k_opad, key, key_len);


        for (i=0; i<64; i++) {
                k_ipad[i] ^= 0x36;
                k_opad[i] ^= 0x5c;
        }



        MD5Init(&context);

        MD5Update(&context, k_ipad, 64);
        MD5Update(&context, text, text_len);
        MD5Final(digest, &context);



        MD5Init(&context);

        MD5Update(&context, k_opad, 64);
        MD5Update(&context, digest, 16);

        MD5Final(digest, &context);
}
