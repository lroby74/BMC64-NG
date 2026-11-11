#include "client.h"

#include <string.h>

#include <new>

#include "text/http_token.h"
namespace bmx {
namespace network {
namespace http {
namespace {
using io::IoStatus;
bool Pending(io::IoResult r) {
    return r.status == IoStatus::PendingRead || r.status == IoStatus::PendingWrite;
}
bool Equal(const char* a, const char* b) {
    while (*a && *b) {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return false;
    }
    return *a == *b;
}
bool RedirectStatus(unsigned status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}
struct Writer {
    char* data;
    size_t capacity, size;
    bool ok;
    void Add(const char* s) {
        if (!s || !ok) {
            ok = false;
            return;
        }
        size_t n = strlen(s);
        if (n >= capacity - size) {
            ok = false;
            return;
        }
        memcpy(data + size, s, n);
        size += n;
        data[size] = 0;
    }
    void Number(uint64_t n) {
        char digits[21];
        size_t p = sizeof(digits) - 1;
        digits[p] = 0;
        do {
            digits[--p] = static_cast<char>('0' + n % 10);
            n /= 10;
        } while (n);
        Add(digits + p);
    }
};
}
Client::Client(ClientWorkspace* workspace, uint8_t* receive_buffer, size_t receive_capacity)
    : workspace_(workspace),
      receive_buffer_(receive_buffer ? receive_buffer : workspace ? workspace->io : 0),
      receive_capacity_(receive_buffer ? receive_capacity : 4096U) {}
Client::~Client() { Close(); }
void Client::Close() {
    if (connection_) connection_->Close();
    connection_ = 0;
    if (decoder_) {
        decoder_->~HttpBodyDecoder();
        decoder_ = 0;
    }
    if (workspace_) {
        memset(workspace_->metadata, 0, sizeof(workspace_->metadata));
        memset(workspace_->request, 0, sizeof(workspace_->request));
        memset(workspace_->io, 0, sizeof(workspace_->io));
        if (receive_buffer_ && receive_capacity_ <= 16384U)
            memset(receive_buffer_, 0, receive_capacity_);
    }
    if (result_.status == Status::Pending) result_.status = Status::Canceled;
    phase_ = Phase::Done;
}
Status Client::Finish(Status status) {
    result_.status = status;
    result_.phase = phase_;
    if (decoder_) result_.body_bytes = decoder_->decoded_bytes();
    Close();
    return result_.status;
}
Status Client::IoFailure(io::IoResult r, Status status) {
    result_.io_status = r.status;
    result_.native_error = r.native_error;
    if (r.status == IoStatus::Canceled) status = Status::Canceled;
    if (r.status == IoStatus::TimedOut) status = Status::TimedOut;
    return Finish(status);
}
const char* Client::Copy(const char* s, bool value) {
    if (!s) return 0;
    size_t n = 0, available = sizeof(workspace_->metadata) - metadata_used_;
    while (n < available && s[n]) {
        unsigned char c = s[n];
        if (value ? (c < 32 || c > 126) : !text::IsHttpTokenCharacter(c)) return 0;
        ++n;
    }
    if (!n || n >= available) return 0;
    char* p = workspace_->metadata + metadata_used_;
    memcpy(p, s, n + 1);
    metadata_used_ += n + 1;
    return p;
}
Status Client::Begin(const Request& request, const Profile& profile, const Limits& limits,
                     Connection* connection, BodySink* sink, RequestPolicy* policy, uint64_t now) {
    Close();
    result_ = Result();
    phase_ = Phase::Idle;
    if (!receive_buffer_ || !receive_capacity_ || receive_capacity_ > 16384U || !workspace_ || !connection || !sink || !policy || !limits.response_body ||
        !limits.response_headers || limits.response_headers > sizeof(workspace_->headers) ||
        limits.request_headers < 2 || limits.request_headers > sizeof(workspace_->request) ||
        !limits.total_ms || !limits.connect_ms || !limits.idle_ms ||
        profile.maximum_redirects > 16 || request.header_count > 16 ||
        (request.header_count && !request.headers) ||
        (request.method != Method::Get && request.method != Method::Post) ||
        (request.method == Method::Get && request.body && request.body->Length()))
        return result_.status;
    if (!ParseUrl(request.url, &workspace_->url)) {
        result_.status = Status::UrlRejected;
        return result_.status;
    }
    connection_ = connection;
    sink_ = sink;
    policy_ = policy;
    source_ = request.body;
    response_headers_ = request.response_headers;
    profile_ = profile;
    limits_ = limits;
    method_ = request.method;
    source_length_ = source_ ? source_->Length() : 0;
    header_count_ = metadata_used_ = 0;
    sensitive_allowed_ = true;
    user_agent_ = Copy(request.user_agent, true);
    accept_ = Copy(request.accept, true);
    content_type_ = request.content_type ? Copy(request.content_type, true) : 0;
    if (!user_agent_ || !accept_ || (request.content_type && !content_type_))
        return Finish(Status::InvalidArgument);
    for (size_t i = 0; i < request.header_count; ++i) {
        Header& h = headers_[header_count_];
        h.name = Copy(request.headers[i].name, false);
        h.value = Copy(request.headers[i].value, true);
        if (!h.name || !h.value) return Finish(Status::InvalidArgument);
        const char* reserved[] = {
            "Host",         "Connection", "Content-Length", "Transfer-Encoding",
            "Content-Type", "User-Agent", "Accept",         "Accept-Encoding",
            "Expect"};
        for (const char* name : reserved)
            if (Equal(h.name, name)) return Finish(Status::InvalidArgument);
        for (size_t n = 0; n < header_count_; ++n)
            if (Equal(h.name, headers_[n].name)) return Finish(Status::InvalidArgument);
        h.sensitive = request.headers[i].sensitive || Equal(h.name, "Authorization") ||
                      Equal(h.name, "Proxy-Authorization") || Equal(h.name, "Cookie");
        ++header_count_;
    }
    if (!policy_->Authorize(workspace_->url, method_, 0)) return Finish(Status::UrlRejected);
    io::InitializeMonotonicDeadline(now, limits.total_ms, &total_);
    io::InitializeMonotonicDeadline(now, limits.connect_ms, &connect_);
    last_now_ = idle_since_ = now;
    header_size_ = buffered_ = offset_ = sent_ = request_size_ = 0;
    source_read_ = source_sent_ = 0;
    pending_ = 0;
    memset(&workspace_->head, 0, sizeof(workspace_->head));
    phase_ = Phase::BeginConnect;
    result_.status = Status::Pending;
    return result_.status;
}
bool Client::Context(uint64_t now, bool canceled, io::OperationContext* context) {
    *context = {now, &total_, canceled};
    io::IoResult checked = io::CheckOperation(*context);
    if (checked.status != IoStatus::Progress) {
        IoFailure(checked, Status::TimedOut);
        return false;
    }
    if (now < last_now_) {
        IoFailure({IoStatus::TimedOut, 0, 0}, Status::TimedOut);
        return false;
    }
    last_now_ = now;
    uint64_t remaining = total_.duration_microseconds - (now - total_.start_microseconds);
    io::MonotonicDeadline phase_deadline;
    if (phase_ == Phase::BeginConnect || phase_ == Phase::Connect)
        phase_deadline = connect_;
    else
        io::InitializeMonotonicDeadline(idle_since_, limits_.idle_ms, &phase_deadline);
    io::OperationContext phase_context = {now, &phase_deadline, false};
    if (io::CheckOperation(phase_context).status != IoStatus::Progress) {
        IoFailure({IoStatus::TimedOut, 0, 0}, Status::TimedOut);
        return false;
    }
    uint64_t phase_remaining =
        phase_deadline.duration_microseconds - (now - phase_deadline.start_microseconds);
    if (phase_remaining < remaining) remaining = phase_remaining;
    effective_ = {now, remaining};
    context->deadline = &effective_;
    return true;
}
bool Client::BuildRequest() {
    Writer out = {workspace_->request, limits_.request_headers, 0, true};
    out.Add(method_ == Method::Get ? "GET " : "POST ");
    out.Add(workspace_->url.target);
    out.Add(" HTTP/1.1\r\nHost: ");
    out.Add(workspace_->url.host);
    if (workspace_->url.port != (workspace_->url.secure ? 443 : 80)) {
        out.Add(":");
        out.Number(workspace_->url.port);
    }
    out.Add("\r\nUser-Agent: ");
    out.Add(user_agent_);
    out.Add("\r\nAccept: ");
    out.Add(accept_);
    out.Add("\r\nAccept-Encoding: identity\r\n");
    for (size_t i = 0; i < header_count_; ++i) {
        const Header& h = headers_[i];
        if (h.sensitive && !sensitive_allowed_) continue;
        out.Add(h.name);
        out.Add(": ");
        out.Add(h.value);
        out.Add("\r\n");
    }
    if (method_ == Method::Post) {
        if (content_type_) {
            out.Add("Content-Type: ");
            out.Add(content_type_);
            out.Add("\r\n");
        }
        out.Add("Content-Length: ");
        out.Number(source_length_);
        out.Add("\r\n");
    }
    out.Add("Connection: close\r\n\r\n");
    request_size_ = out.size;
    sent_ = 0;
    return out.ok && out.size <= limits_.request_bytes &&
           source_length_ <= limits_.request_bytes - out.size;
}
Status Client::Redirect(uint64_t now) {
    if (!profile_.maximum_redirects) return Finish(Status::RedirectRejected);
    if (result_.redirects >= profile_.maximum_redirects) return Finish(Status::TooManyRedirects);
    Url& next = workspace_->next_url;
    const Url& current = workspace_->url;
    if (!ResolveUrl(current, workspace_->head.location, &next) ||
        (current.secure && !next.secure && !profile_.allow_downgrade))
        return Finish(Status::RedirectRejected);
    Method next_method = method_;
    if (method_ == Method::Post) {
        if (result_.http_status == 303 ||
            ((result_.http_status == 301 || result_.http_status == 302) &&
             profile_.rewrite_post_301_302))
            next_method = Method::Get;
        else if (result_.http_status != 307 && result_.http_status != 308)
            return Finish(Status::RedirectRejected);
        else if (!profile_.allow_post_replay)
            return Finish(Status::ReplayRejected);
    }
    if (!policy_->Authorize(next, next_method, result_.redirects + 1))
        return Finish(Status::RedirectRejected);
    if (!SameOrigin(current, next) && !policy_->ForwardSensitiveHeaders(current, next))
        sensitive_allowed_ = false;
    if (method_ == Method::Post && next_method == Method::Post &&
        (!source_ || source_->Length() != source_length_ || !source_->Reset()))
        return Finish(Status::ReplayRejected);
    if (next_method == Method::Get) {
        source_ = 0;
        source_length_ = 0;
    }
    method_ = next_method;
    connection_->Close();
    workspace_->url = next;
    ++result_.redirects;
    header_size_ = buffered_ = offset_ = 0;
    source_read_ = source_sent_ = 0;
    pending_ = 0;
    io::InitializeMonotonicDeadline(now, limits_.connect_ms, &connect_);
    idle_since_ = now;
    phase_ = Phase::BeginConnect;
    return Status::Pending;
}
Status Client::Poll(uint64_t now, bool canceled) {
    if (result_.status != Status::Pending) return result_.status;
    io::OperationContext context;
    if (!Context(now, canceled, &context)) return result_.status;
    result_.phase = phase_;
    if (phase_ == Phase::Redirect) return Redirect(now);
    if (phase_ == Phase::BeginConnect || phase_ == Phase::Connect) {
        io::IoResult r = phase_ == Phase::BeginConnect
                             ? connection_->Begin(workspace_->url, context)
                             : connection_->PollConnect(context);
        phase_ = Phase::Connect;
        if (Pending(r)) return Status::Pending;
        if (r.status != IoStatus::Progress) return IoFailure(r, Status::ConnectFailed);
        if (!BuildRequest()) return Finish(Status::RequestTooLarge);
        idle_since_ = now;
        phase_ = Phase::SendHeaders;
        return Status::Pending;
    }
    if (phase_ == Phase::SendHeaders) {
        const size_t n = request_size_ - sent_;
        io::IoResult r = connection_->WriteSome(workspace_->request + sent_, n, context);
        if (Pending(r) && !r.bytes) return Status::Pending;
        if (r.status != IoStatus::Progress || !r.bytes || r.bytes > n)
            return IoFailure(r, Status::SendFailed);
        sent_ += r.bytes;
        idle_since_ = now;
        if (sent_ == request_size_) {
            memset(workspace_->request, 0, sizeof(workspace_->request));
            phase_ = source_length_ ? Phase::SendBody : Phase::ReadHeaders;
        }
        return Status::Pending;
    }
    if (phase_ == Phase::SendBody) {
        if (offset_ == buffered_) {
            size_t capacity = sizeof(workspace_->io);
            if (source_length_ - source_read_ < capacity)
                capacity = static_cast<size_t>(source_length_ - source_read_);
            io::IoResult r = source_->ReadSome(workspace_->io, capacity);
            if (r.status == IoStatus::PendingRead && !r.bytes) return Status::Pending;
            if (r.status != IoStatus::Progress || !r.bytes || r.bytes > capacity)
                return IoFailure(r, Status::SourceFailed);
            source_read_ += r.bytes;
            buffered_ = r.bytes;
            offset_ = 0;
            idle_since_ = now;
            return Status::Pending;
        }
        size_t n = buffered_ - offset_;
        io::IoResult r = connection_->WriteSome(workspace_->io + offset_, n, context);
        if (Pending(r) && !r.bytes) return Status::Pending;
        if (r.status != IoStatus::Progress || !r.bytes || r.bytes > n)
            return IoFailure(r, Status::SendFailed);
        offset_ += r.bytes;
        source_sent_ += r.bytes;
        idle_since_ = now;
        if (source_sent_ == source_length_) {
            buffered_ = offset_ = 0;
            phase_ = Phase::ReadHeaders;
        }
        return Status::Pending;
    }
    if (phase_ == Phase::ReadHeaders) {
        size_t n = limits_.response_headers - header_size_;
        if (n > 4096) n = 4096;
        if (!n) {
            result_.header_status = HttpParseStatus::HeaderTooLarge;
            return Finish(Status::HeaderInvalid);
        }
        io::IoResult r = connection_->ReadSome(workspace_->headers + header_size_, n, context);
        if (Pending(r) && !r.bytes) return Status::Pending;
        if (r.status != IoStatus::Progress || !r.bytes || r.bytes > n)
            return IoFailure(r, Status::ReceiveFailed);
        header_size_ += r.bytes;
        idle_since_ = now;
        size_t head_bytes = 0;
        result_.header_status =
            ParseHttpResponseHead(workspace_->headers, header_size_, &workspace_->head, &head_bytes,
                                  profile_.allow_close_delimited);
        if (result_.header_status == HttpParseStatus::NeedMoreData) return Status::Pending;
        if (result_.header_status != HttpParseStatus::Ok) return Finish(Status::HeaderInvalid);
        const HttpResponseHead& head = workspace_->head;
        if (response_headers_)
            VisitHttpResponseHeaders(workspace_->headers, head_bytes, head, *response_headers_);
        result_.http_status = head.status_code;
        if (RedirectStatus(head.status_code)) {
            phase_ = Phase::Redirect;
            return Status::Pending;
        }
        if ((!head.has_content_length && !head.chunked && !head.close_delimited && !head.bodyless) ||
            (head.chunked && !head.bodyless && !profile_.allow_chunked) ||
            (head.has_content_encoding && profile_.reject_content_encoding) ||
            (head.has_set_cookie && profile_.reject_set_cookie) ||
            (head.location[0] && profile_.reject_location))
            return Finish(Status::HeaderInvalid);
        if (head.status_code < 200 ||
            (profile_.required_status && head.status_code != profile_.required_status))
            return Finish(Status::UnexpectedStatus);
        decoder_ = new (decoder_storage_) HttpBodyDecoder(head, limits_.response_body, sink_);
        pending_ = workspace_->headers + head_bytes;
        buffered_ = header_size_ - head_bytes;
        offset_ = 0;
        phase_ = Phase::ReadBody;
        return Status::Pending;
    }
    if (phase_ == Phase::ReadBody) {
        if (pending_) {
            size_t consumed = 0;
            result_.body_status =
                decoder_->Feed(pending_ + offset_, buffered_ - offset_, &consumed, 1);
            offset_ += consumed;
            result_.body_bytes = decoder_->decoded_bytes();
            if (consumed) idle_since_ = now;
            if (result_.body_status == HttpBodyStatus::Complete) return Finish(Status::Ok);
            if (result_.body_status != HttpBodyStatus::Ok &&
                result_.body_status != HttpBodyStatus::Pending)
                return Finish(Status::BodyInvalid);
            if (offset_ == buffered_) {
                pending_ = 0;
                buffered_ = offset_ = 0;
            }
            return Status::Pending;
        }
        io::IoResult r = connection_->ReadSome(receive_buffer_, receive_capacity_, context);
        if (Pending(r) && !r.bytes) return Status::Pending;
        if (r.status == IoStatus::EndOfStream) {
            result_.body_status = decoder_->Finish();
            return Finish(result_.body_status == HttpBodyStatus::Complete ? Status::Ok
                                                                          : Status::BodyInvalid);
        }
        if (r.status != IoStatus::Progress || !r.bytes || r.bytes > receive_capacity_) {
            result_.body_status = decoder_->Finish();
            return IoFailure(r, Status::ReceiveFailed);
        }
        idle_since_ = now;
        pending_ = receive_buffer_;
        buffered_ = r.bytes;
        offset_ = 0;
        return Status::Pending;
    }
    return Finish(Status::InvalidArgument);
}
}
}
}
