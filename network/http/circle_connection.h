#ifndef BMX_NETWORK_HTTP_CIRCLE_CONNECTION_H
#define BMX_NETWORK_HTTP_CIRCLE_CONNECTION_H
#include "client.h"
#include "network/tls/client_stream.h"
namespace bmx {
namespace network {
namespace http {


class CircleConnection : public Connection {
   public:
    CircleConnection(CNetSubSystem* network, const tls::ClientOptions* options);
    ~CircleConnection() override;
    io::IoResult Begin(const Url&, const io::OperationContext&) override;
    io::IoResult PollConnect(const io::OperationContext&) override;
    io::IoResult ReadSome(void*, size_t, const io::OperationContext&) override;
    io::IoResult WriteSome(const void*, size_t, const io::OperationContext&) override;
    void Close() override;

   private:
    CircleConnection(const CircleConnection&) = delete;
    CircleConnection& operator=(const CircleConnection&) = delete;
    CNetSubSystem* network_;
    tls::ClientOptions options_{};
    tls::Certificate authorities_[16];
    io::TcpStream* plain_ = 0;
    tls::ClientStream* secure_ = 0;
    bool has_options_ = false;
};
}
}
}
#endif
