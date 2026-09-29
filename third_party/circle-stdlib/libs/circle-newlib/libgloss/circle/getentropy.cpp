#include "config.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "circle_macros.h"
#include <circle/bcmrandom.h>

extern "C" int
_getentropy(void *buf, size_t buflen)
{
    if (buflen > 256) {
        errno = EIO;
        return -1;
    }

    CBcmRandomNumberGenerator rng;
    uint8_t * const dest = static_cast<uint8_t *>(buf);
    size_t offset = 0;

    while (offset + sizeof(u32) <= buflen) {
        u32 const word = rng.GetNumber();
        memcpy(dest + offset, &word, sizeof(u32));
        offset += sizeof(u32);
    }

    if (offset < buflen) {
        u32 const word = rng.GetNumber();
        memcpy(dest + offset, &word, buflen - offset);
    }

    return 0;
}
