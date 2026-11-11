#ifndef BMX_NETWORK_IO_TCP_ENDPOINT_H
#define BMX_NETWORK_IO_TCP_ENDPOINT_H
#include <stddef.h>
#include <stdint.h>
namespace bmx { namespace network { namespace io {

bool ParseTcpEndpoint(const char *target, char *host, size_t capacity, uint16_t *port);
}}}
#endif
