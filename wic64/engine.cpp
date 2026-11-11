#include "wic64/engine.h"
#include "network/io/tcp_endpoint.h"
#include "network/io/dns_query.h"
#include "text/hex.h"
#include <stdio.h>
#include <string.h>
namespace bmx { namespace wic64 {
namespace http = network::http;
namespace io = network::io;
namespace {
bool Pending(io::IoStatus s) { return s == io::IoStatus::PendingRead || s == io::IoStatus::PendingWrite; }
bool Equal(const uint8_t *s, size_t n, const char *expected) {
    if (n != strlen(expected)) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned c = s[i]; if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != static_cast<unsigned char>(expected[i])) return false;
    }
    return true;
}
bool SameCredentialOrigin(const http::Url &from, const http::Url &to) {
    return http::SameOrigin(from, to) ||
           (!from.secure && to.secure && from.port == 80 && to.port == 443 &&
            !strcmp(from.host, to.host));
}
bool Contains(const uint8_t *data, size_t size, const char *text) {
    size_t n = strlen(text);
    for (size_t i = 0; n <= size && i <= size - n; ++i)
        if (!memcmp(data + i, text, n)) return true;
    return false;
}
bool ContainsToken(const char *target, const char *token) {
    if (!*token) return false;
    uint8_t decoded[MaximumUrl + 1]; size_t used = 0;
    for (size_t i = 0; target[i]; ++i) {
        unsigned char c = target[i];
        if (c == '%' && target[i + 1] && target[i + 2]) {
            int a = text::HexDigit(target[i + 1], text::HexCase::Mixed);
            int b = text::HexDigit(target[i + 2], text::HexCase::Mixed);
            if (a >= 0 && b >= 0) { c = a * 16 + b; i += 2; }
        }
        decoded[used++] = c;
    }
    return Contains(decoded, used, token);
}
}
Engine::Engine(Platform &p, const Identity &identity, ReplyBuffer::Allocate allocate,
               ReplyBuffer::Free release) : platform_(p), identity_(identity),
    proposed_(identity), buffer_(allocate, release), workspace_(), http_(&workspace_) {}
Engine::~Engine() { Reset(); }
void Engine::CloseTcp() {
    if (tcp_) { tcp_->Close(); delete tcp_; tcp_ = nullptr; }
    connected_ = eof_ = false; received_ = read_offset_ = 0;
}
void Engine::Cancel() {
    http_.Close(); delete connection_; connection_ = nullptr;

    if (phase_ == Connecting || phase_ == Writing) CloseTcp();
    phase_ = Idle; buffer_.Clear(); reply_ = {};
}
void Engine::Release() { buffer_.Clear(); reply_ = {}; }
void Engine::Reset() { Cancel(); CloseTcp(); post_url_[0] = 0; }
void Engine::Tick() {
    if (!connected_ || eof_ || received_ == sizeof receive_) return;
    size_t tail = (read_offset_ + received_) % sizeof receive_;
    size_t count = sizeof receive_ - tail;
    if (count > sizeof receive_ - received_) count = sizeof receive_ - received_;
    if (count > 4096) count = 4096;
    io::IoResult r = tcp_->ReadSome(receive_ + tail, count, {platform_.Now(), nullptr, false});
    if (r.status == io::IoStatus::Progress && r.bytes && r.bytes <= count) received_ += r.bytes;
    else if (!Pending(r.status)) {


        eof_ = true; tcp_->Close(); delete tcp_; tcp_ = nullptr;
    }
}
int Engine::Finish(uint8_t code, const char *message, const char *legacy) {
    phase_ = Idle;
    if (code) { buffer_.Clear(); reply_.offset = 0; }
    reply_.pages = buffer_.Pages();
    reply_.length = static_cast<uint32_t>(buffer_.Size() - reply_.offset);
    reply_.code = code;
    snprintf(reply_.message, sizeof reply_.message, "%s", message);
    snprintf(reply_.legacy, sizeof reply_.legacy, "%s", legacy ? legacy : code ? "!E" : "");
    return BMX_WIC64_REPLY;
}
int Engine::Bytes(const void *data, size_t size, const char *legacy) {
    if (size && buffer_.WriteSome(static_cast<const uint8_t *>(data), size).bytes != size)
        return Finish(1, "Out of memory");
    return Finish(0, "Success", legacy);
}
bool Engine::Authorize(const http::Url &url, http::Method method, unsigned hop) {
    uint8_t address[4];
    if (strlen(url.target) > MaximumUrl || (url.secure && io::ParseIPv4(url.host, address)))
        return false;
    if (!hop) {
        credential_origin_ = url;
        sensitive_url_ = ContainsToken(url.target, identity_.token);
        return true;
    }


    return !(sensitive_url_ || method == http::Method::Post) ||
           SameCredentialOrigin(credential_origin_, url);
}
void Engine::Begin(const http::HttpResponseHead &) {
    proposed_ = identity_; token_seen_ = name_seen_ = false; headers_valid_ = true;
}
void Engine::Field(const uint8_t *name, size_t n, const uint8_t *value, size_t size) {
    char *out = nullptr; size_t capacity = 0; bool *seen = nullptr;
    if (Equal(name, n, "wic64-security-token")) {
        out = proposed_.token; capacity = sizeof proposed_.token; seen = &token_seen_;
    } else if (Equal(name, n, "wic64-security-token-key")) {
        out = proposed_.token_name; capacity = sizeof proposed_.token_name; seen = &name_seen_;
    } else return;
    if (*seen || size >= capacity) { headers_valid_ = false; return; }
    *seen = true; memcpy(out, value, size); out[size] = 0;
    if (!ValidIdentity(proposed_)) headers_valid_ = false;
}
int Engine::StartHttp(const Request &request) {
    const bool post = request.command == 0x2b;
    if (post) {
        if (!post_url_[0]) return Finish(2, "POST URL not specified", "!0");
        strcpy(url_, post_url_);
    } else {
        UrlStatus s = ExpandUrl(request.payload, request.length, request.command == 0x0f,
                               request.protocol == 'W', request.server, identity_, url_);
        if (s != UrlStatus::Ok)
            return Finish(2, s == UrlStatus::TooLarge ? "URL too long (max 2000 bytes)" : "Malformed URL");
        if (request.command == 0x28) { strcpy(post_url_, url_); return Bytes(nullptr, 0); }
    }
    connection_ = platform_.Http();
    if (!connection_) return Finish(1, "Out of memory");
    http::Request h;
    h.url = url_;



    h.user_agent = request.protocol == 'W' ? "ESP32HTTPClient" : "WiC64/2.1.0 (BMX)";
    h.response_headers = this;
    if (post) {

        bool unique = false;
        for (unsigned i = 0; i < 4; ++i) {
            snprintf(boundary_, sizeof boundary_, "BMX-WiC64-%016llx-%016llx-%u",
                     static_cast<unsigned long long>(platform_.Now()),
                     static_cast<unsigned long long>(request.token.request), i);
            if (!Contains(request.payload, request.length, boundary_)) { unique = true; break; }
        }
        body_ = http::MemoryBodySource(request.payload, request.length);
        http::MultipartField field = {"data", nullptr, "application/octet-stream", &body_};
        if (!unique || !multipart_.Initialize(boundary_, &field, 1)) {
            delete connection_; connection_ = nullptr;
            return Finish(2, "Invalid multipart body");
        }
        h.method = http::Method::Post; h.body = &multipart_; h.content_type = multipart_.ContentType();
    }
    http::Profile profile;
    profile.allow_chunked = profile.allow_close_delimited = true;
    profile.reject_set_cookie = profile.reject_location = false;
    profile.required_status = 0; profile.maximum_redirects = 5;
    profile.rewrite_post_301_302 = profile.allow_post_replay = true;
    http::Limits limits;
    limits.response_body = (request.protocol == 'E' || request.command == 0x25) ?
                           ReplyBuffer::MaximumBytes : 65535;
    limits.request_bytes = BMX_WIC64_MAX_REQUEST + 4096;
    limits.total_ms = limits.connect_ms = limits.idle_ms = request.timeout * 1000;
    phase_ = Http;
    http::Status status = http_.Begin(h, profile, limits, connection_, &buffer_, this, platform_.Now());
    return status == http::Status::Pending ? BMX_WIC64_PENDING : FinishHttp(request);
}
bool Engine::UpdateIdentity(const Request &request, size_t *offset) {
    if (!headers_valid_) return false;
    const bool preferences = http_.head().status_code == 201;
    if (!preferences && !token_seen_ && !name_seen_) return true;
    http::Url server;
    if (!http::ParseUrl(request.server, &server) || !SameCredentialOrigin(server, http_.url()))
        return !preferences;
    if (preferences) {
        uint8_t prefix[MaximumToken + MaximumTokenName + 4] = {};
        size_t n = buffer_.Size(); if (n > sizeof prefix) n = sizeof prefix;
        if (!buffer_.Copy(0, prefix, n) || n < 3 || prefix[0] != 1) return false;
        size_t split = 1; while (split < n && prefix[split] != 1) ++split;
        size_t end = split + 1; while (end < n && prefix[end] != 1) ++end;
        if (split >= n || end >= n || split - 1 > MaximumTokenName) return false;
        char key[MaximumTokenName + 1] = {};
        memcpy(key, prefix + 1, split - 1);
        char *value = nullptr; size_t capacity = 0;
        if (!strcmp(key, "sectokenname")) { value = proposed_.token_name; capacity = sizeof proposed_.token_name; }
        else if (proposed_.token_name[0] && !strcmp(key, proposed_.token_name)) {
            value = proposed_.token; capacity = sizeof proposed_.token;
        }
        if (value) {
            if (end - split - 1 >= capacity) return false;
            memcpy(value, prefix + split + 1, end - split - 1); value[end - split - 1] = 0;
        }
        *offset = end + 1;
    }
    if (!ValidIdentity(proposed_)) return false;
    if (memcmp(&proposed_, &identity_, sizeof identity_) && !platform_.SaveIdentity(proposed_)) return false;
    identity_ = proposed_;
    return true;
}
int Engine::FinishHttp(const Request &request) {
    const http::Result result = http_.result();
    int code = 0; const char *message = "Success";
    if (result.status != http::Status::Ok) {
        code = result.status == http::Status::ConnectFailed ? 3 :
               result.status == http::Status::UrlRejected || result.status == http::Status::RedirectRejected ||
               result.status == http::Status::TooManyRedirects || result.status == http::Status::BodyInvalid ? 2 : 4;
        message = result.status == http::Status::TimedOut ? "Remote timeout" : "HTTP transfer failed";
        if (http_.url().secure && result.native_error < 0)
            message = "TLS verification or handshake failed";
        if (buffer_.AllocationFailed()) { code = 1; message = "Out of memory"; }
    } else if (result.http_status != 200 && result.http_status != 201) {
        code = 5; message = "HTTP server error";
    } else {
        size_t offset = 0;
        if (!UpdateIdentity(request, &offset)) { code = 1; message = "Invalid or unsaved preferences"; }
        reply_.offset = static_cast<uint32_t>(offset);
        if (request.protocol == 'W' && request.command != 0x25 && buffer_.Size() - offset > 2) {
            const char *target = http_.url().target;
            size_t path = strcspn(target, "?");
            if (path >= 4 && !memcmp(target + path - 4, ".prg", 4)) reply_.legacy_length_adjust = -2;
        }
    }
    http_.Close(); delete connection_; connection_ = nullptr;
    return Finish(code, message, code ? "!0" : nullptr);
}
int Engine::Start(const Request &r, bool ready) {
    Release(); written_ = 0;
    io::InitializeMonotonicDeadline(platform_.Now(), r.timeout * 1000, &deadline_);
    if (!ValidIdentity(identity_)) return Finish(1, "Identity unavailable");
    const bool legacy = r.protocol == 'W';
    NetworkSnapshot net = platform_.Snapshot();
    if (!r.reporting.real) {
        net.wifi = net.signal_valid = true;
        strcpy(net.ssid, r.reporting.ssid); net.rssi = r.reporting.rssi;
    }
    char value[96] = {};
    switch (r.command) {
    case 0x14: return Bytes(identity_.mac, strlen(identity_.mac) + 1);
    case 0x06: return ready && net.ip[0] ? Bytes(net.ip, strlen(net.ip) + 1) : Finish(4, "Network unavailable");
    case 0x2f: { const uint8_t zero = 0; return net.configured ? Bytes(&zero, 1) : Finish(4, "Network not configured"); }
    case 0x2c:
        if (ready) return Bytes(nullptr, 0, "0");
        if (r.length == 1 && r.payload[0]) {
            unsigned seconds = r.payload[0] < r.timeout ? r.payload[0] : r.timeout;
            io::InitializeMonotonicDeadline(platform_.Now(), seconds * 1000, &deadline_);
            phase_ = WaitingNetwork; return BMX_WIC64_PENDING;
        }
        return Finish(4, "Network unavailable");
    case 0x10:
        if (!net.wifi || !net.ssid[0]) return Finish(4, "SSID unavailable on active adapter");
        return Bytes(net.ssid, strlen(net.ssid) + !legacy);
    case 0x11:
        if (!net.wifi || !net.signal_valid) return Finish(4, "RSSI unavailable on active adapter");
        snprintf(value, sizeof value, "%ddBm", net.rssi); return Bytes(value, strlen(value) + !legacy);
    case 0x0c:
        if (r.reporting.real) return Finish(2, "Use BMX Network scan");
        { const char sep = legacy ? 1 : 0;
          int n = snprintf(value, sizeof value, "0%c%s%c%d%c%c", sep, net.ssid, sep, -net.rssi, sep, 255);
          return Bytes(value, n + !legacy); }
    case 0x02: case 0x0d: return Finish(2, "Use BMX Network settings");
    case 0x28: return StartHttp(r);
    case 0x2e: CloseTcp(); return Bytes(nullptr, 0, "0");
    default: break;
    }
    if (!ready) { CloseTcp(); return Finish(4, "Network unavailable"); }
    switch (r.command) {
    case 0x01: case 0x0f: case 0x25: case 0x2b: return StartHttp(r);
    case 0x21: {
        CloseTcp();
        if (!r.length || r.length > 300 || memchr(r.payload, 0, r.length)) return Finish(2, "Invalid TCP endpoint");
        char endpoint[301], host[254]; uint16_t port = 0;
        memcpy(endpoint, r.payload, r.length); endpoint[r.length] = 0;
        if (!io::ParseTcpEndpoint(endpoint, host, sizeof host, &port)) return Finish(2, "Invalid TCP endpoint");
        tcp_ = platform_.Tcp(); if (!tcp_) return Finish(1, "Out of memory");
        phase_ = Connecting;
        io::IoResult result = tcp_->BeginConnect(host, port, {platform_.Now(), &deadline_, false});
        if (result.status == io::IoStatus::Progress) { connected_ = true; return Bytes(nullptr, 0, "0"); }
        if (Pending(result.status)) return BMX_WIC64_PENDING;
        CloseTcp(); return Finish(3, "TCP connection failed");
    }
    case 0x30: {
        if (!connected_) return Finish(4, "No TCP connection");
        uint8_t count[2] = {static_cast<uint8_t>(received_), static_cast<uint8_t>(received_ >> 8)};
        if (!received_ && eof_) { CloseTcp(); return Finish(4, "TCP connection closed"); }
        return Bytes(count, sizeof count);
    }
    case 0x22: case 0x23:
        if (!connected_) return Finish(4, "No TCP connection");
        if (r.command == 0x23 && eof_) return Finish(4, "TCP connection closed");
        phase_ = r.command == 0x22 ? Reading : Writing;
        return PollTcp(r);
    default: return Finish(2, "Command unavailable");
    }
}
int Engine::PollTcp(const Request &r) {
    io::OperationContext context{platform_.Now(), &deadline_, false};
    if (io::CheckOperation(context).status != io::IoStatus::Progress) {
        if (phase_ == Connecting || phase_ == Writing) CloseTcp();
        return Finish(4, "Remote timeout");
    }
    if (phase_ == Reading) {
        if (received_) {
            uint8_t data[240]; size_t n = received_ < sizeof data ? received_ : sizeof data;
            for (size_t i = 0; i < n; ++i) data[i] = receive_[(read_offset_ + i) % sizeof receive_];

            int result = Bytes(data, n);
            if (!reply_.code) { received_ -= n; read_offset_ = (read_offset_ + n) % sizeof receive_; }
            return result;
        }
        if (eof_) { CloseTcp(); return Finish(4, "TCP connection closed"); }


        return Bytes(nullptr, 0);
    }
    io::IoResult result;
    if (phase_ == Connecting) result = tcp_->PollConnect(context);
    else {
        if (written_ == r.length) return Bytes(nullptr, 0, "0");
        if (eof_) return Finish(4, "TCP connection closed");
        size_t n = r.length - written_; if (n > 4096) n = 4096;
        result = tcp_->WriteSome(r.payload + written_, n, context);
    }
    if (Pending(result.status)) return BMX_WIC64_PENDING;
    if (result.status == io::IoStatus::Progress) {
        if (phase_ == Connecting) { connected_ = true; return Bytes(nullptr, 0, "0"); }
        if (result.bytes && result.bytes <= r.length - written_) { written_ += result.bytes; return BMX_WIC64_PENDING; }
    }
    const bool connecting = phase_ == Connecting;
    CloseTcp(); return Finish(connecting ? 3 : 4, "TCP operation failed");
}
int Engine::Process(const Request &request, bool first, bool ready) {
    if (first) return Start(request, ready);
    if (phase_ == WaitingNetwork) {
        if (ready) return Bytes(nullptr, 0, "0");
        if (io::CheckOperation({platform_.Now(), &deadline_, false}).status == io::IoStatus::Progress)
            return BMX_WIC64_PENDING;
        return Finish(4, "Network unavailable");
    }
    if (!ready) { Cancel(); CloseTcp(); return Finish(4, "Network unavailable"); }
    if (phase_ == Http)
        return http_.Poll(platform_.Now()) == http::Status::Pending ? BMX_WIC64_PENDING : FinishHttp(request);
    return PollTcp(request);
}
} }
