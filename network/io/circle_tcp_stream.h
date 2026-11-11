#ifndef BMX_NETWORK_IO_CIRCLE_TCP_STREAM_H
#define BMX_NETWORK_IO_CIRCLE_TCP_STREAM_H
#include "tcp_stream.h"
#include "dns_query.h"
class CNetSubSystem;
class CSocket;
namespace bmx { namespace network { namespace io {









class CircleTcpStream : public TcpStream {
public:
    explicit CircleTcpStream(CNetSubSystem *network);
    ~CircleTcpStream() override;
    IoResult BeginConnect(const char *host, uint16_t port,
                          const OperationContext &context) override;
    IoResult PollConnect(const OperationContext &context) override;
    IoResult ReadSome(void *buffer, size_t capacity,
                      const OperationContext &context) override;
    IoResult WriteSome(const void *buffer, size_t size,
                       const OperationContext &context) override;
    void Close() override;
    enum class Phase { Idle, Dns, Connect, Open, Read, Write };
    Phase FailurePhase() const { return failure_phase_; }
private:
    IoResult Check(const OperationContext &context);
    IoResult Fail(IoResult result);
    IoResult StartTcp(const uint8_t address[4]);
    void Release();
    CNetSubSystem *network_;
    CSocket *socket_;
    DnsQuery query_;
    Phase phase_, failure_phase_;
    IoResult terminal_;
    uint64_t last_dns_send_;
    unsigned dns_attempts_;
    uint16_t port_;
    bool read_eof_;
    uint8_t staging_[1600];
    size_t staged_, staged_offset_;
    CircleTcpStream(const CircleTcpStream &) = delete;
    CircleTcpStream &operator=(const CircleTcpStream &) = delete;
};
}}}
#endif
