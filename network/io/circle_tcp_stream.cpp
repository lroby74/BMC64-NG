#include "circle_tcp_stream.h"
#include <circle/net/socket.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/in.h>
#include <circle/netdevice.h>
#include <errno.h>
#include <string.h>





namespace bmx { namespace network { namespace io {
namespace {
static_assert(FRAME_BUFFER_SIZE <= 1600, "Circle hands over whole segments");

const size_t kWriteBytes = 1024;

const unsigned kSendWaitMicroseconds = 1000;
IoResult Error(int code) { return {IoStatus::Error, 0, code}; }
IoResult CircleResult(int code, IoStatus pending) {
    if (code > 0) return {IoStatus::Progress, static_cast<size_t>(code), 0};
    if (!code || code == -NET_ERROR_WOULD_BLOCK) return {pending, 0, 0};
    if (code == -NET_ERROR_CONNECTION_TIMED_OUT) return {IoStatus::TimedOut, 0, 0};
    switch (-code) {
        case NET_ERROR_CONNECTION_REFUSED: return Error(ECONNREFUSED);
        case NET_ERROR_CONNECTION_RESET: return Error(ECONNRESET);
        case NET_ERROR_DESTINATION_UNREACHABLE: return Error(EHOSTUNREACH);
        case NET_ERROR_NOT_CONNECTED: return Error(ENOTCONN);
        case NET_ERROR_INVALID_VALUE: return Error(EINVAL);
        default: return Error(EIO);
    }
}
}

CircleTcpStream::CircleTcpStream(CNetSubSystem *network)
    : network_(network), socket_(0), phase_(Phase::Idle), failure_phase_(Phase::Idle),
      terminal_(Error(ENOTCONN)), last_dns_send_(0), dns_attempts_(0), port_(0), read_eof_(false),
      staged_(0), staged_offset_(0) {}
CircleTcpStream::~CircleTcpStream() { Close(); }
void CircleTcpStream::Release() {
    delete socket_;
    socket_ = 0;
    staged_ = staged_offset_ = 0;
}
void CircleTcpStream::Close() {
    Release();
    phase_ = Phase::Idle;
    terminal_ = Error(ENOTCONN);
    read_eof_ = false;
}
IoResult CircleTcpStream::Fail(IoResult result) {
    failure_phase_ = phase_;
    Release();
    phase_ = Phase::Idle;
    terminal_ = result;
    return result;
}
IoResult CircleTcpStream::Check(const OperationContext &context) {
    if (phase_ == Phase::Idle) return terminal_;
    IoResult result = CheckOperation(context);
    if (result.status != IoStatus::Progress) return Fail(result);
    return result;
}
IoResult CircleTcpStream::StartTcp(const uint8_t address[4]) {
    Release();
    phase_ = Phase::Connect;
    socket_ = new CSocket(network_, IPPROTO_TCP);
    if (!socket_) return Fail(Error(ENOMEM));

    int result = socket_->Connect(CIPAddress(address), port_);
    if (result < 0) return Fail(CircleResult(result, IoStatus::Error));
    socket_->SetOptionSendTimeout(kSendWaitMicroseconds);
    phase_ = Phase::Open;
    return {IoStatus::Progress, 0, 0};
}
IoResult CircleTcpStream::BeginConnect(const char *host, uint16_t port,
                                      const OperationContext &context) {
    if (phase_ != Phase::Idle) return Error(EISCONN);
    failure_phase_ = Phase::Idle;
    IoResult result = CheckOperation(context);
    if (result.status != IoStatus::Progress) return Fail(result);
    if (!host || !*host || strlen(host) > 254 || !port) return Fail(Error(EINVAL));
    if (!network_ || !network_->IsRunning()) return Fail(Error(ENETDOWN));
    port_ = port;
    read_eof_ = false;
    uint8_t address[4];
    if (ParseIPv4(host, address)) return StartTcp(address);
    if (strcmp(host, "localhost") == 0)
        return StartTcp(network_->GetConfig()->GetIPAddress()->Get());
    phase_ = Phase::Dns;

    static uint16_t transaction = 0;
    if (!query_.Initialize(host, ++transaction)) return Fail(Error(EINVAL));
    const CIPAddress *dns = network_->GetConfig()->GetDNSServer();
    if (dns->IsNull()) return Fail(Error(EHOSTUNREACH));
    socket_ = new CSocket(network_, IPPROTO_UDP);
    if (!socket_) return Fail(Error(ENOMEM));
    int code = socket_->Connect(*dns, 53);
    if (code < 0) return Fail(CircleResult(code, IoStatus::PendingWrite));
    dns_attempts_ = 0;
    last_dns_send_ = context.now_microseconds;
    return {IoStatus::PendingWrite, 0, 0};
}
IoResult CircleTcpStream::PollConnect(const OperationContext &context) {
    IoResult result = Check(context);
    if (result.status != IoStatus::Progress) return result;
    if (phase_ == Phase::Open) return result;
    if (phase_ != Phase::Dns) return Fail(Error(EIO));

    uint8_t packet[512], address[4];
    int received = socket_->Receive(packet, sizeof(packet), MSG_DONTWAIT);
    if (received < 0) return Fail(CircleResult(received, IoStatus::PendingRead));
    if (received > 0) {
        DnsQuery::Status status = query_.Parse(packet, received, address);
        if (status == DnsQuery::Status::Complete) return StartTcp(address);
        if (status != DnsQuery::Status::Ignore) return Fail(Error(EHOSTUNREACH));
    }
    if (context.now_microseconds < last_dns_send_) return Fail({IoStatus::TimedOut, 0, 0});
    if (!dns_attempts_ || context.now_microseconds - last_dns_send_ >= 1000000) {
        if (dns_attempts_ == 3) return Fail({IoStatus::TimedOut, 0, 0});
        int sent = socket_->Send(query_.Data(), query_.Size(), MSG_DONTWAIT);
        if (sent == -NET_ERROR_WOULD_BLOCK) return {IoStatus::PendingWrite, 0, 0};
        if (sent != static_cast<int>(query_.Size())) return Fail(Error(EIO));
        ++dns_attempts_;
        last_dns_send_ = context.now_microseconds;
    }
    return {IoStatus::PendingRead, 0, 0};
}
IoResult CircleTcpStream::ReadSome(void *buffer, size_t capacity,
                                  const OperationContext &context) {
    IoResult result = Check(context);
    if (result.status != IoStatus::Progress) return result;
    if (phase_ != Phase::Open) return Error(ENOTCONN);
    if (!buffer || !capacity) return Error(EINVAL);
    if (staged_offset_ == staged_) {
        if (read_eof_) return {IoStatus::EndOfStream, 0, 0};
        int received = socket_->Receive(staging_, sizeof(staging_), MSG_DONTWAIT);
        if (!received) return {IoStatus::PendingRead, 0, 0};

        if (received == -NET_ERROR_CONNECTION_RESET) {
            read_eof_ = true;
            return {IoStatus::EndOfStream, 0, 0};
        }
        if (received < 0 || static_cast<size_t>(received) > sizeof(staging_)) {
            phase_ = Phase::Read;
            return Fail(received < 0 ? CircleResult(received, IoStatus::PendingRead) : Error(EIO));
        }
        staged_ = static_cast<size_t>(received);
        staged_offset_ = 0;
    }
    size_t count = staged_ - staged_offset_;
    if (count > capacity) count = capacity;
    memcpy(buffer, staging_ + staged_offset_, count);
    staged_offset_ += count;
    if (staged_offset_ == staged_) staged_ = staged_offset_ = 0;
    return {IoStatus::Progress, count, 0};
}
IoResult CircleTcpStream::WriteSome(const void *buffer, size_t size,
                                   const OperationContext &context) {
    IoResult result = Check(context);
    if (result.status != IoStatus::Progress) return result;
    if (phase_ != Phase::Open) return Error(ENOTCONN);
    if (!buffer || !size) return Error(EINVAL);
    if (size > kWriteBytes) size = kWriteBytes;
    result = CircleResult(socket_->Send(buffer, size, 0), IoStatus::PendingWrite);
    if (result.status == IoStatus::Error || result.status == IoStatus::TimedOut) {
        phase_ = Phase::Write;
        return Fail(result);
    }
    return result;
}
}}}
