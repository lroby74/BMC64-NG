#ifndef BMX_WIC64_POLICY_H
#define BMX_WIC64_POLICY_H
#include <stddef.h>
#include <stdint.h>
namespace bmx { namespace wic64 {
static const size_t MaximumUrl = 2000;
static const size_t MaximumToken = 64;
static const size_t MaximumTokenName = 31;
struct Identity {
    char mac[18];
    char token[MaximumToken + 1];
    char token_name[MaximumTokenName + 1];
};
enum class UrlStatus { Ok, Invalid, TooLarge };


UrlStatus ExpandUrl(const uint8_t *data, size_t size, bool encoded, bool legacy,
                    const char *server, const Identity &identity,
                    char out[MaximumUrl + 1]);
bool ValidIdentity(const Identity &identity);
bool DeriveMac(const char *serial, char out[18]);
} }
#endif
