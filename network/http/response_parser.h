#ifndef BMX_NETWORK_HTTP_RESPONSE_PARSER_H
#define BMX_NETWORK_HTTP_RESPONSE_PARSER_H

#include <stddef.h>
#include <stdint.h>

namespace bmx {
namespace network {
namespace http {



static const size_t kMaximumHttpHeaderBytes = 32768U;
static const size_t kMaximumHttpLocationBytes = 2048U;
static const size_t kMaximumHttpContentTypeBytes = 96U;

enum class HttpParseStatus : uint8_t {
    Ok = 0,
    NeedMoreData,
    InvalidArgument,
    HeaderTooLarge,
    InvalidStatusLine,
    InvalidHeader,
    DuplicateHeader,
    UnsupportedTransferEncoding,
    UnsupportedContentEncoding,
    AmbiguousBodyLength,
    MissingBodyLength,
    InvalidContentLength,
    MissingLocation,
    OutputTooSmall
};

struct HttpResponseHead {
    unsigned status_code;
    bool has_content_length;
    uint64_t content_length;
    bool chunked;
    bool connection_close;
    bool has_content_encoding;
    bool has_set_cookie;
    char location[kMaximumHttpLocationBytes];
    char content_type[kMaximumHttpContentTypeBytes];
    bool close_delimited;
    bool bodyless;
};



class ResponseHeaders {
public:
    virtual ~ResponseHeaders() {}
    virtual void Begin(const HttpResponseHead &head) = 0;
    virtual void Field(const uint8_t *name, size_t name_size,
                       const uint8_t *value, size_t value_size) = 0;
};
void VisitHttpResponseHeaders(const uint8_t *input, size_t header_bytes,
                              const HttpResponseHead &head, ResponseHeaders &visitor);

HttpParseStatus ParseHttpResponseHead(const uint8_t *input,
                                      size_t input_size,
                                      HttpResponseHead *head,
                                      size_t *header_bytes,
                                      bool allow_close_delimited = false);

const char *HttpParseStatusString(HttpParseStatus status);

}
}
}

#endif
