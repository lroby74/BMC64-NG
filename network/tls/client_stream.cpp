#include "client_stream.h"
#include "network/io/dns_query.h"

#include <circle-mbedtls/tlssimpleclientsocket.h>
#include <circle/net/in.h>
#include <mbedtls/net_sockets.h>
#include <string.h>

namespace bmx {
namespace network {
namespace tls {
using namespace io;
namespace {
IoResult Error(int code = MBEDTLS_ERR_SSL_BAD_INPUT_DATA) { return {IoStatus::Error, 0, code}; }


bool CertificateEpoch(const mbedtls_x509_time& t, int64_t* epoch) {
    if (t.year < 1 || t.year > 9999 || t.mon < 1 || t.mon > 12 || t.day < 1 || t.day > 31 ||
        t.hour < 0 || t.hour > 23 || t.min < 0 || t.min > 59 || t.sec < 0 || t.sec > 59)
        return false;
    const int year = t.year - (t.mon <= 2);
    const int era = year / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153U * (t.mon + (t.mon > 2 ? -3 : 9)) + 2U) / 5U + t.day - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    const int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
    *epoch = days * 86400 + t.hour * 3600U + t.min * 60U + t.sec;
    return true;
}
}

ClientStream::ClientStream(TcpStream* tcp, CircleMbedTLS::CTLSSimpleSupport* support)
    : tcp_(tcp),
      support_(support),
      socket_(0),
      state_(State::Closed),
      terminal_(Error()),
      options_(),
      step_(),
      transport_result_(),
      bio_calls_(0),
      read_eof_(false),
      pending_write_(0),
      write_buffer_() {}
ClientStream::~ClientStream() {
    Close();
    delete tcp_;
}

void ClientStream::Close() {

    delete socket_;
    socket_ = 0;
    if (tcp_) tcp_->Close();
    state_ = State::Closed;
    options_ = {};
    step_ = {};
    pending_write_ = 0;
    memset(write_buffer_, 0, sizeof(write_buffer_));
    read_eof_ = false;
}
IoResult ClientStream::Fail(IoResult result) {
    Close();
    terminal_ = result;
    state_ = State::Terminal;
    return result;
}
bool ClientStream::ClockAvailable(uint64_t* now) const {
    if (!options_.utc_seconds || !options_.minimum_trusted_epoch) return false;
    *now = options_.utc_seconds(options_.clock_context);
    return *now >= options_.minimum_trusted_epoch && *now <= 253402300799ULL;
}
IoResult ClientStream::Check(const OperationContext& context) {
    if (state_ == State::Terminal) return terminal_;
    IoResult checked = CheckOperation(context);
    if (checked.status != IoStatus::Progress) return Fail(checked);
    if (state_ == State::Closed) return Fail(Error());
    return checked;
}
IoResult ClientStream::Begin(const char* host, uint16_t port, const ClientOptions& options,
                             const OperationContext& context) {
    Close();
    IoResult checked = CheckOperation(context);
    if (checked.status != IoStatus::Progress) return Fail(checked);
    if (!tcp_ || !support_ || !host || !*host || !port || !options.authorities ||
        !options.authority_count || options.authority_count > 16 ||
        (options.time_policy != CertificateTimePolicy::Strict &&
         options.time_policy != CertificateTimePolicy::AllowNotYetValid))
        return Fail(Error());
    uint8_t address[4];

    if (io::ParseIPv4(host, address)) return Fail(Error());
    size_t host_size = 0;
    while (host_size <= 253 && host[host_size]) ++host_size;
    if (host_size > 253) return Fail(Error());
    options_ = options;
    uint64_t now = 0;
    if (!ClockAvailable(&now)) return Fail(Error(MBEDTLS_ERR_X509_BAD_INPUT_DATA));
    socket_ = new CircleMbedTLS::CTLSSimpleClientSocket(support_, IPPROTO_TCP, TRUE);
    if (!socket_) return Fail(Error(MBEDTLS_ERR_SSL_ALLOC_FAILED));
    size_t authority_bytes = 0;
    for (size_t i = 0; i < options.authority_count; ++i) {
        const Certificate& ca = options.authorities[i];
        if (!ca.data || !ca.size || ca.size > kMaximumAuthorityBytes - authority_bytes)
            return Fail(Error());
        authority_bytes += ca.size;
        int result = socket_->AddCertificate(ca.data, ca.size);
        if (result != 0) return Fail(Error(result));
    }
    options_.authorities = 0;
    options_.authority_count = 0;
    socket_->SetVerifyCallback(Verify, this);
    int result = socket_->Setup(host, "BMX shared TLS v1");
    if (!result) result = socket_->SetExternalBIO(this, Send, Receive);
    if (result) return Fail(Error(result));
    state_ = State::Connecting;
    IoResult connected = tcp_->BeginConnect(host, port, context);
    if (connected.status == IoStatus::Progress) {
        state_ = State::Handshake;
        return {IoStatus::PendingWrite, 0, 0};
    }
    if (connected.status == IoStatus::PendingRead || connected.status == IoStatus::PendingWrite)
        return connected;
    return Fail(connected);
}
void ClientStream::StartStep(const OperationContext& context) {
    step_ = context;
    bio_calls_ = 0;
    transport_result_ = {IoStatus::Progress, 0, 0};
}
IoResult ClientStream::Translate(int result, bool handshake) {
    step_ = {};
    if (transport_result_.status == IoStatus::Canceled ||
        transport_result_.status == IoStatus::TimedOut ||
        transport_result_.status == IoStatus::Error)
        return Fail(transport_result_);
    if (result == MBEDTLS_ERR_SSL_WANT_READ) return {IoStatus::PendingRead, 0, 0};
    if (result == MBEDTLS_ERR_SSL_WANT_WRITE) return {IoStatus::PendingWrite, 0, 0};
    if (!handshake && result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        read_eof_ = true;
        return {IoStatus::EndOfStream, 0, 0};
    }
    if (result < 0 || (!handshake && result == 0))
        return Fail(Error(result ? result : MBEDTLS_ERR_SSL_CONN_EOF));
    return {IoStatus::Progress, static_cast<size_t>(result), 0};
}
IoResult ClientStream::PollConnect(const OperationContext& context) {
    IoResult checked = Check(context);
    if (checked.status != IoStatus::Progress) return checked;
    if (state_ == State::Open) return checked;
    uint64_t now = 0;
    if (!ClockAvailable(&now)) return Fail(Error(MBEDTLS_ERR_X509_BAD_INPUT_DATA));
    if (state_ == State::Connecting) {
        IoResult connected = tcp_->PollConnect(context);
        if (connected.status == IoStatus::PendingRead ||
            connected.status == IoStatus::PendingWrite)
            return connected;
        if (connected.status != IoStatus::Progress) return Fail(connected);
        state_ = State::Handshake;
    }
    StartStep(context);
    IoResult result = Translate(socket_->HandshakeSome(), true);
    if (result.status == IoStatus::Progress) state_ = State::Open;
    return result;
}
IoResult ClientStream::ReadSome(void* buffer, size_t capacity,
                                const OperationContext& context) {
    IoResult checked = Check(context);
    if (checked.status != IoStatus::Progress) return checked;
    if (state_ != State::Open || !buffer || !capacity || pending_write_) return Fail(Error());
    if (read_eof_) return {IoStatus::EndOfStream, 0, 0};
    StartStep(context);
    return Translate(socket_->ReadSome(buffer, capacity > 16384 ? 16384 : capacity));
}
IoResult ClientStream::WriteSome(const void* buffer, size_t size,
                                 const OperationContext& context) {
    IoResult checked = Check(context);
    if (checked.status != IoStatus::Progress) return checked;
    if (state_ != State::Open || !buffer || !size) return Fail(Error());
    const size_t amount = size > sizeof(write_buffer_) ? sizeof(write_buffer_) : size;
    if (pending_write_) {
        if (pending_write_ != amount || memcmp(write_buffer_, buffer, amount))
            return Fail(Error());
    } else {
        memcpy(write_buffer_, buffer, amount);
        pending_write_ = amount;
    }
    StartStep(context);
    IoResult result = Translate(socket_->WriteSome(write_buffer_, pending_write_));
    if (result.status == IoStatus::Progress) {
        pending_write_ = 0;
        memset(write_buffer_, 0, sizeof(write_buffer_));
    }
    return result;
}
int ClientStream::Transfer(bool write, void* buffer, size_t size) {


    if (bio_calls_++) return write ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_SSL_WANT_READ;
    if (size > 1600) size = 1600;
    transport_result_ =
        write ? tcp_->WriteSome(buffer, size, step_) : tcp_->ReadSome(buffer, size, step_);
    switch (transport_result_.status) {
        case IoStatus::Progress:
            if (!transport_result_.bytes || transport_result_.bytes > size ||
                transport_result_.bytes > 1600)
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            return static_cast<int>(transport_result_.bytes);
        case IoStatus::PendingRead:
            return MBEDTLS_ERR_SSL_WANT_READ;
        case IoStatus::PendingWrite:
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        case IoStatus::EndOfStream:
            return MBEDTLS_ERR_SSL_CONN_EOF;
        default:
            return write ? MBEDTLS_ERR_NET_SEND_FAILED : MBEDTLS_ERR_NET_RECV_FAILED;
    }
}
int ClientStream::Send(void* p, const unsigned char* data, size_t size) {
    return static_cast<ClientStream*>(p)->Transfer(true, const_cast<unsigned char*>(data),
                                                   size);
}
int ClientStream::Receive(void* p, unsigned char* data, size_t size) {
    return static_cast<ClientStream*>(p)->Transfer(false, data, size);
}
int ClientStream::Verify(void* p, mbedtls_x509_crt* cert, int, uint32_t* flags) {
    ClientStream* stream = static_cast<ClientStream*>(p);
    uint64_t now = 0;
    int64_t begin = 0, end = 0;
    if (!stream || !cert || !flags || !stream->ClockAvailable(&now) ||
        !CertificateEpoch(cert->valid_from, &begin) || !CertificateEpoch(cert->valid_to, &end))
        return MBEDTLS_ERR_X509_FATAL_ERROR;


    *flags &= ~(MBEDTLS_X509_BADCERT_FUTURE | MBEDTLS_X509_BADCERT_EXPIRED);
    if (static_cast<int64_t>(now) > end) *flags |= MBEDTLS_X509_BADCERT_EXPIRED;
    if (static_cast<int64_t>(now) < begin &&
        stream->options_.time_policy == CertificateTimePolicy::Strict)
        *flags |= MBEDTLS_X509_BADCERT_FUTURE;
    return 0;
}
}
}
}
