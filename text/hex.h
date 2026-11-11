#ifndef BMX_TEXT_HEX_H
#define BMX_TEXT_HEX_H

#include <stddef.h>
#include <stdint.h>

namespace bmx {
namespace text {


enum class HexCase { Lower, Mixed };

inline int HexDigit(unsigned char value, HexCase policy) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (policy == HexCase::Mixed && value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}


inline bool IsHex(const char *input, size_t size, HexCase policy) {
    if (input == 0) return false;
    for (size_t i = 0; i < size; ++i) {
        if (HexDigit(static_cast<unsigned char>(input[i]), policy) < 0) return false;
    }
    return true;
}



inline bool DecodeHex(const char *input, size_t input_size, uint8_t *output,
                      size_t output_size, HexCase policy) {
    if (output == 0 || input_size % 2U != 0U ||
        input_size / 2U != output_size || !IsHex(input, input_size, policy)) {
        return false;
    }
    for (size_t i = 0; i < output_size; ++i) {
        output[i] = static_cast<uint8_t>(
            (HexDigit(input[i * 2U], policy) << 4) |
             HexDigit(input[i * 2U + 1U], policy));
    }
    return true;
}



inline bool EncodeHex(const uint8_t *input, size_t input_size, char *output,
                      size_t capacity) {
    if (input == 0 || output == 0 || capacity == 0U ||
        input_size > (capacity - 1U) / 2U) return false;
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < input_size; ++i) {
        output[i * 2U] = digits[input[i] >> 4U];
        output[i * 2U + 1U] = digits[input[i] & 15U];
    }
    output[input_size * 2U] = '\0';
    return true;
}

}
}
#endif
