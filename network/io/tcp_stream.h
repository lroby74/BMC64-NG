#ifndef BMX_NETWORK_IO_TCP_STREAM_H
#define BMX_NETWORK_IO_TCP_STREAM_H
#include "byte_stream.h"
namespace bmx { namespace network { namespace io {
class TcpStream : public ByteStream {
public:
    virtual IoResult BeginConnect(const char *host, uint16_t port,
                                  const OperationContext &context) = 0;
    virtual IoResult PollConnect(const OperationContext &context) = 0;
};


TcpStream *CreateNetworkTcpStream();
uint64_t NetworkMonotonicMicroseconds();
}}}
#endif
