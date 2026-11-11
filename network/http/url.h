#ifndef BMX_NETWORK_HTTP_URL_H
#define BMX_NETWORK_HTTP_URL_H
#include <stddef.h>
#include <stdint.h>
namespace bmx {
namespace network {
namespace http {
static const size_t kUrlBytes = 4096;
struct Url {
    bool secure;
    uint16_t port;
    char host[254];
    char target[kUrlBytes];
};


bool ParseUrl(const char* input, Url* out);
bool ResolveUrl(const Url& base, const char* reference, Url* out);
bool SameOrigin(const Url& a, const Url& b);
}
}
}
#endif
