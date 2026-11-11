#ifndef BMX_NETWORK_HTTP_CLIENT_H
#define BMX_NETWORK_HTTP_CLIENT_H
#include "body_decoder.h"
#include "body_source.h"
#include "url.h"
namespace bmx {
namespace network {
namespace http {
enum class Method { Get, Post };
class Connection : public io::ByteStream {
   public:
    virtual io::IoResult Begin(const Url& url, const io::OperationContext& context) = 0;
    virtual io::IoResult PollConnect(const io::OperationContext& context) = 0;
};
class RequestPolicy {
   public:
    virtual ~RequestPolicy() {}
    virtual bool Authorize(const Url& url, Method method, unsigned hop) = 0;
    virtual bool ForwardSensitiveHeaders(const Url&, const Url&) { return false; }
};
struct Header {
    const char* name;
    const char* value;
    bool sensitive;
};
struct Request {
    const char* url = 0;
    Method method = Method::Get;
    const char* user_agent = "BMX/1";
    const char* accept = "*/*";
    const char* content_type = 0;
    const Header* headers = 0;
    size_t header_count = 0;
    BodySource* body = 0;
    ResponseHeaders* response_headers = 0;
};


struct Profile {
    bool allow_chunked = false, allow_close_delimited = false;
    bool reject_content_encoding = true, reject_set_cookie = true, reject_location = true;
    unsigned required_status = 200, maximum_redirects = 0;
    bool allow_downgrade = false, rewrite_post_301_302 = false, allow_post_replay = false;
};
struct Limits {
    uint64_t response_body = 0;
    uint64_t request_bytes = UINT64_MAX;
    size_t response_headers = kMaximumHttpHeaderBytes, request_headers = 4096;
    uint32_t total_ms = 900000, connect_ms = 30000, idle_ms = 15000;
};
enum class Status {
    Pending,
    Ok,
    InvalidArgument,
    UrlRejected,
    ConnectFailed,
    RequestTooLarge,
    SendFailed,
    ReceiveFailed,
    HeaderInvalid,
    RedirectRejected,
    TooManyRedirects,
    ReplayRejected,
    UnexpectedStatus,
    BodyInvalid,
    SourceFailed,
    Canceled,
    TimedOut
};
enum class Phase {
    Idle,
    BeginConnect,
    Connect,
    SendHeaders,
    SendBody,
    ReadHeaders,
    Redirect,
    ReadBody,
    Done
};
struct Result {
    Status status = Status::InvalidArgument;
    Phase phase = Phase::Idle;
    io::IoStatus io_status = io::IoStatus::Progress;
    int native_error = 0;
    HttpParseStatus header_status = HttpParseStatus::InvalidArgument;
    HttpBodyStatus body_status = HttpBodyStatus::InvalidArgument;
    unsigned http_status = 0, redirects = 0;
    uint64_t body_bytes = 0;
};

struct ClientWorkspace {
    Url url, next_url;
    HttpResponseHead head;
    char metadata[4096], request[4096];
    uint8_t headers[kMaximumHttpHeaderBytes], io[4096];
};




class Client {
   public:
    explicit Client(ClientWorkspace* workspace, uint8_t* receive_buffer = 0,
                    size_t receive_capacity = 0);
    ~Client();
    Status Begin(const Request& request, const Profile& profile, const Limits& limits,
                 Connection* connection, BodySink* sink, RequestPolicy* policy, uint64_t now);
    Status Poll(uint64_t now, bool canceled = false);
    void Close();
    const Result& result() const { return result_; }
    const HttpResponseHead& head() const { return workspace_->head; }
    const Url& url() const { return workspace_->url; }

   private:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    const char* Copy(const char* text, bool value);
    bool BuildRequest();
    Status Finish(Status status);
    Status IoFailure(io::IoResult result, Status status);
    Status Redirect(uint64_t now);
    bool Context(uint64_t now, bool canceled, io::OperationContext* context);
    ClientWorkspace* workspace_;
    uint8_t* receive_buffer_;
    size_t receive_capacity_;
    Connection* connection_ = 0;
    BodySink* sink_ = 0;
    BodySource* source_ = 0;
    RequestPolicy* policy_ = 0;
    ResponseHeaders* response_headers_ = 0;
    Profile profile_;
    Limits limits_;
    Result result_;
    Phase phase_ = Phase::Idle;
    Method method_ = Method::Get;
    Header headers_[16];
    size_t header_count_ = 0, metadata_used_ = 0, request_size_ = 0, sent_ = 0;
    size_t header_size_ = 0, buffered_ = 0, offset_ = 0;
    const uint8_t* pending_ = 0;
    const char *user_agent_ = 0, *accept_ = 0, *content_type_ = 0;
    uint64_t source_length_ = 0, source_read_ = 0, source_sent_ = 0;
    uint64_t last_now_ = 0, idle_since_ = 0;
    bool sensitive_allowed_ = true;
    io::MonotonicDeadline total_{}, connect_{}, effective_{};
    alignas(HttpBodyDecoder) uint8_t decoder_storage_[sizeof(HttpBodyDecoder)];
    HttpBodyDecoder* decoder_ = 0;
};
}
}
}
#endif
