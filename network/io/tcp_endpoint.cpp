#include "tcp_endpoint.h"
#include <string.h>
namespace bmx { namespace network { namespace io {
bool ParseTcpEndpoint(const char *target, char *host, size_t capacity, uint16_t *port) {
    if (!target || !host || !capacity || !port) return false;
    if (strncmp(target, "ip4://", 6) == 0) target += 6;
    const char *colon = strchr(target, ':');
    if (!colon || colon == target || static_cast<size_t>(colon - target) >= capacity) return false;
    const char *p = colon + 1;
    unsigned value = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 5) return false;
        value = value * 10 + (*p++ - '0');
    }
    if (*p || !value || value > 65535) return false;
    memcpy(host, target, colon - target);
    host[colon - target] = 0;
    *port = value;
    return true;
}
}}}
