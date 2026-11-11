#ifndef BMX_NETWORK_HTTP_BODY_SINK_H
#define BMX_NETWORK_HTTP_BODY_SINK_H
#include "network/io/result.h"
namespace bmx {
namespace network {
namespace http {


class BodySink {
   public:
    virtual ~BodySink() {}
    virtual io::IoResult WriteSome(const uint8_t* data, size_t size) = 0;
};
}
}
}
#endif
