#ifndef BMX_WIC64_ENGINE_H
#define BMX_WIC64_ENGINE_H
#include "wic64/service.h"
#include "wic64/policy.h"
#include "wic64/reply_buffer.h"
#include "network/http/client.h"
#include "network/http/multipart.h"
#include "network/io/tcp_stream.h"
namespace bmx { namespace wic64 {
struct NetworkSnapshot {
    bool configured = false, ready = false, wifi = false, signal_valid = false;
    char ip[16] = {}, ssid[33] = {};
    int rssi = 0;
};
class Platform {
public:
    virtual ~Platform() {}
    virtual uint64_t Now() = 0;
    virtual network::http::Connection *Http() = 0;
    virtual network::io::TcpStream *Tcp() = 0;
    virtual NetworkSnapshot Snapshot() = 0;
    virtual bool SaveIdentity(const Identity &identity) = 0;
};


class Engine : public Backend, private network::http::RequestPolicy,
               private network::http::ResponseHeaders {
public:
    Engine(Platform &platform, const Identity &identity,
           ReplyBuffer::Allocate allocate = malloc, ReplyBuffer::Free release = free);
    ~Engine() override;
    int Process(const Request &request, bool first, bool network_ready) override;
    void Cancel() override;
    void Reset() override;
    void Release() override;
    void Tick() override;
    bmx_wic64_reply Reply() const override { return reply_; }
    bmx_wic64_stats Stats() const override {
        bmx_wic64_stats result{};
        result.reply_bytes = buffer_.AllocatedBytes();
        result.tcp_open = tcp_ != nullptr; result.http_open = connection_ != nullptr;
        return result;
    }
    const Identity &GetIdentity() const { return identity_; }
    size_t ReplyBytes() const { return buffer_.AllocatedBytes(); }
private:
    bool Authorize(const network::http::Url &url, network::http::Method, unsigned) override;
    void Begin(const network::http::HttpResponseHead &head) override;
    void Field(const uint8_t *, size_t, const uint8_t *, size_t) override;
    int Finish(uint8_t code, const char *message, const char *legacy = nullptr);
    int Bytes(const void *data, size_t size, const char *legacy = nullptr);
    int Start(const Request &, bool ready);
    int StartHttp(const Request &);
    int FinishHttp(const Request &);
    int PollTcp(const Request &);
    void CloseTcp();
    bool UpdateIdentity(const Request &, size_t *body_offset);
    Platform &platform_;
    Identity identity_, proposed_;
    bool token_seen_ = false, name_seen_ = false, headers_valid_ = true;
    ReplyBuffer buffer_;
    bmx_wic64_reply reply_{};
    network::http::ClientWorkspace workspace_;
    network::http::Client http_;
    network::http::Connection *connection_ = nullptr;
    network::http::MemoryBodySource body_{nullptr, 0};
    network::http::MultipartSource multipart_;
    network::io::TcpStream *tcp_ = nullptr;
    network::io::MonotonicDeadline deadline_{};
    enum Phase { Idle, Http, Connecting, Writing, Reading, WaitingNetwork } phase_ = Idle;
    network::http::Url credential_origin_{};
    bool sensitive_url_ = false;
    char url_[MaximumUrl + 1] = {}, post_url_[MaximumUrl + 1] = {};
    char boundary_[64] = {};
    uint8_t receive_[65535];
    size_t received_ = 0, read_offset_ = 0, written_ = 0;
    bool connected_ = false, eof_ = false;
};
} }
#endif
