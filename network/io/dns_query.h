#ifndef BMX_NETWORK_IO_DNS_QUERY_H
#define BMX_NETWORK_IO_DNS_QUERY_H
#include <stddef.h>
#include <stdint.h>
namespace bmx { namespace network { namespace io {

bool ParseIPv4(const char *host, uint8_t address[4]);


class DnsQuery {
public:
    DnsQuery() : size_(0) {}
    enum class Status { Ignore, Complete, Invalid, NotFound, Truncated };
    bool Initialize(const char *host, uint16_t transaction);
    const uint8_t *Data() const { return packet_; }
    size_t Size() const { return size_; }
    Status Parse(const uint8_t *packet, size_t size, uint8_t address[4]) const;
private:
    uint8_t packet_[272];
    size_t size_;
    char host_[254];
};
}}}
#endif
