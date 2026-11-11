#ifndef BMX_NETWORK_TLS_CLIENT_STREAM_H
#define BMX_NETWORK_TLS_CLIENT_STREAM_H

#include "network/io/tcp_stream.h"

class CNetSubSystem;
namespace CircleMbedTLS {
class CTLSSimpleSupport;
class CTLSSimpleClientSocket;
}
struct mbedtls_x509_crt;

namespace bmx {
namespace network {
namespace tls {


static const unsigned kMaximumClientStreams = 2;
static const size_t kMaximumAuthorityBytes = 256U * 1024U;

struct Certificate {
    const uint8_t* data;
    size_t size;
};
enum class CertificateTimePolicy : uint8_t {
    Strict,

    AllowNotYetValid
};
struct ClientOptions {
    const Certificate* authorities;
    size_t authority_count;
    uint64_t (*utc_seconds)(void*);
    void* clock_context;
    uint64_t minimum_trusted_epoch;
    CertificateTimePolicy time_policy;
};




class ClientStream : public io::ByteStream {
   public:
    ClientStream(io::TcpStream* tcp, CircleMbedTLS::CTLSSimpleSupport* support);
    ~ClientStream() override;
    io::IoResult Begin(const char* host, uint16_t port, const ClientOptions& options,
                       const io::OperationContext& context);
    io::IoResult PollConnect(const io::OperationContext& context);
    io::IoResult ReadSome(void* buffer, size_t capacity,
                          const io::OperationContext& context) override;



    io::IoResult WriteSome(const void* buffer, size_t size,
                           const io::OperationContext& context) override;
    void Close() override;
   private:
    ClientStream(const ClientStream&) = delete;
    ClientStream& operator=(const ClientStream&) = delete;
    enum class State { Closed, Connecting, Handshake, Open, Terminal };
    io::IoResult Check(const io::OperationContext& context);
    io::IoResult Fail(io::IoResult result);
    io::IoResult Translate(int result, bool handshake = false);
    void StartStep(const io::OperationContext& context);
    int Transfer(bool write, void* buffer, size_t size);
    static int Send(void*, const unsigned char*, size_t);
    static int Receive(void*, unsigned char*, size_t);
    static int Verify(void*, mbedtls_x509_crt*, int, uint32_t*);
    bool ClockAvailable(uint64_t* now) const;

    io::TcpStream* tcp_;
    CircleMbedTLS::CTLSSimpleSupport* support_;
    CircleMbedTLS::CTLSSimpleClientSocket* socket_;
    State state_;
    io::IoResult terminal_;
    ClientOptions options_;
    io::OperationContext step_;
    io::IoResult transport_result_;
    unsigned bio_calls_;
    bool read_eof_;
    size_t pending_write_;
    uint8_t write_buffer_[4096];
};


ClientStream* CreateNetworkTlsStream(CNetSubSystem* network);
uint64_t NetworkUtcSeconds(void*);

}
}
}
#endif
