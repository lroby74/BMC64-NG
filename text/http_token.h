#ifndef BMX_TEXT_HTTP_TOKEN_H
#define BMX_TEXT_HTTP_TOKEN_H

#include <stdint.h>

namespace bmx {
namespace text {



inline bool IsHttpTokenCharacter(uint8_t value) {
    if ((value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9')) return true;
    switch (value) {
        case '!': case '#': case '$': case '%': case '&': case '\'':
        case '*': case '+': case '-': case '.': case '^': case '_':
        case '`': case '|': case '~': return true;
        default: return false;
    }
}

}
}
#endif
