#include "network/http/response_parser.h"
#include "text/http_token.h"

#include <limits.h>
#include <string.h>

namespace bmx {
namespace network {
namespace http {

namespace {

char AsciiLower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool NameEquals(const uint8_t *name, size_t size, const char *expected) {
    const size_t expected_size = strlen(expected);
    if (size != expected_size) return false;
    for (size_t i = 0; i < size; ++i) {
        if (AsciiLower(static_cast<char>(name[i])) != expected[i]) return false;
    }
    return true;
}

bool ValueEqualsInsensitive(const uint8_t *value, size_t size,
                            const char *expected) {
    const size_t expected_size = strlen(expected);
    if (size != expected_size) return false;
    for (size_t i = 0; i < size; ++i) {
        if (AsciiLower(static_cast<char>(value[i])) != expected[i]) return false;
    }
    return true;
}

bool CopyValue(const uint8_t *value, size_t size, char *output,
               size_t output_size) {
    if (size >= output_size) return false;
    memcpy(output, value, size);
    output[size] = '\0';
    return true;
}

bool ParseDecimal(const uint8_t *value, size_t size, uint64_t *result) {
    if (size == 0U) return false;
    uint64_t parsed = 0U;
    for (size_t i = 0; i < size; ++i) {
        if (value[i] < '0' || value[i] > '9') return false;
        const uint64_t digit = static_cast<uint64_t>(value[i] - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    *result = parsed;
    return true;
}

const uint8_t *FindCrlf(const uint8_t *begin, const uint8_t *end) {
    for (const uint8_t *p = begin; p + 1 < end; ++p) {
        if (p[0] == '\r' && p[1] == '\n') return p;
        if (p[0] == '\n') return 0;
    }
    return 0;
}

bool StatusMayHaveUnframedBody(unsigned status) {
    return (status >= 100U && status < 200U) || status == 204U ||
           status == 304U || (status >= 300U && status < 400U);
}

}

void VisitHttpResponseHeaders(const uint8_t *input, size_t size,
                              const HttpResponseHead &head, ResponseHeaders &visitor) {
    visitor.Begin(head);
    const uint8_t *end = input + size;
    const uint8_t *line = FindCrlf(input, end);
    if (!line) return;
    line += 2;
    while (line + 1 < end && *line != '\r') {
        const uint8_t *last = FindCrlf(line, end);
        if (!last) return;
        const uint8_t *colon = static_cast<const uint8_t *>(memchr(line, ':', last - line));
        if (!colon) return;
        const uint8_t *value = colon + 1, *tail = last;
        while (value < tail && (*value == ' ' || *value == '\t')) ++value;
        while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t')) --tail;
        visitor.Field(line, colon - line, value, tail - value);
        line = last + 2;
    }
}

HttpParseStatus ParseHttpResponseHead(const uint8_t *input,
                                      size_t input_size,
                                      HttpResponseHead *head,
                                      size_t *header_bytes, bool allow_close_delimited) {
    if (input == 0 || head == 0 || header_bytes == 0) {
        return HttpParseStatus::InvalidArgument;
    }
    if (input_size > kMaximumHttpHeaderBytes) {
        input_size = kMaximumHttpHeaderBytes;
    }

    const uint8_t *begin = input;
    const uint8_t *end = input + input_size;
    const uint8_t *line_end = FindCrlf(begin, end);
    if (line_end == 0) {
        return input_size == kMaximumHttpHeaderBytes
                   ? HttpParseStatus::HeaderTooLarge
                   : HttpParseStatus::NeedMoreData;
    }
    const size_t status_size = static_cast<size_t>(line_end - begin);
    if (status_size < 12U ||
        (memcmp(begin, "HTTP/1.1 ", 9U) != 0 &&
         memcmp(begin, "HTTP/1.0 ", 9U) != 0) ||
        begin[9] < '1' || begin[9] > '5' ||
        begin[10] < '0' || begin[10] > '9' ||
        begin[11] < '0' || begin[11] > '9' ||
        (status_size > 12U && begin[12] != ' ')) {
        return HttpParseStatus::InvalidStatusLine;
    }



    for (size_t i = 12U; i < status_size; ++i) {
        if (begin[i] < 0x20U || begin[i] > 0x7eU) {
            return HttpParseStatus::InvalidStatusLine;
        }
    }

    memset(head, 0, sizeof(*head));
    head->status_code = static_cast<unsigned>((begin[9] - '0') * 100 +
                                              (begin[10] - '0') * 10 +
                                              (begin[11] - '0'));

    bool saw_transfer_encoding = false;
    bool saw_content_encoding = false;
    bool saw_location = false;
    bool saw_content_type = false;
    bool saw_connection = false;
    const uint8_t *line = line_end + 2;
    for (;;) {
        if (line + 1 >= end) {
            return input_size == kMaximumHttpHeaderBytes
                       ? HttpParseStatus::HeaderTooLarge
                       : HttpParseStatus::NeedMoreData;
        }
        if (line[0] == '\r' && line[1] == '\n') {
            *header_bytes = static_cast<size_t>(line + 2 - begin);
            break;
        }
        if (line[0] == ' ' || line[0] == '\t') {
            return HttpParseStatus::InvalidHeader;
        }
        line_end = FindCrlf(line, end);
        if (line_end == 0) {
            return input_size == kMaximumHttpHeaderBytes
                       ? HttpParseStatus::HeaderTooLarge
                       : HttpParseStatus::NeedMoreData;
        }
        const uint8_t *colon = static_cast<const uint8_t *>(
            memchr(line, ':', static_cast<size_t>(line_end - line)));
        if (colon == 0 || colon == line) return HttpParseStatus::InvalidHeader;
        for (const uint8_t *p = line; p < colon; ++p) {
            if (!text::IsHttpTokenCharacter(*p)) return HttpParseStatus::InvalidHeader;
        }
        const uint8_t *value = colon + 1;
        while (value < line_end && (*value == ' ' || *value == '\t')) ++value;
        const uint8_t *value_end = line_end;
        while (value_end > value &&
               (value_end[-1] == ' ' || value_end[-1] == '\t')) {
            --value_end;
        }
        for (const uint8_t *p = value; p < value_end; ++p) {
            if ((*p < 0x20U && *p != '\t') || *p == 0x7fU) {
                return HttpParseStatus::InvalidHeader;
            }
        }
        const size_t name_size = static_cast<size_t>(colon - line);
        const size_t value_size = static_cast<size_t>(value_end - value);

        if (NameEquals(line, name_size, "content-length")) {
            if (head->has_content_length) return HttpParseStatus::DuplicateHeader;
            if (!ParseDecimal(value, value_size, &head->content_length)) {
                return HttpParseStatus::InvalidContentLength;
            }
            head->has_content_length = true;
        } else if (NameEquals(line, name_size, "transfer-encoding")) {
            if (saw_transfer_encoding) return HttpParseStatus::DuplicateHeader;
            saw_transfer_encoding = true;
            if (!ValueEqualsInsensitive(value, value_size, "chunked")) {
                return HttpParseStatus::UnsupportedTransferEncoding;
            }
            head->chunked = true;
        } else if (NameEquals(line, name_size, "content-encoding")) {
            if (saw_content_encoding) return HttpParseStatus::DuplicateHeader;
            saw_content_encoding = true;
            head->has_content_encoding = true;
            if (!ValueEqualsInsensitive(value, value_size, "identity")) {
                return HttpParseStatus::UnsupportedContentEncoding;
            }
        } else if (NameEquals(line, name_size, "set-cookie")) {
            head->has_set_cookie = true;
        } else if (NameEquals(line, name_size, "location")) {
            if (saw_location) return HttpParseStatus::DuplicateHeader;
            saw_location = true;
            if (!CopyValue(value, value_size, head->location,
                           sizeof(head->location))) {
                return HttpParseStatus::OutputTooSmall;
            }
        } else if (NameEquals(line, name_size, "content-type")) {
            if (saw_content_type) return HttpParseStatus::DuplicateHeader;
            saw_content_type = true;
            if (!CopyValue(value, value_size, head->content_type,
                           sizeof(head->content_type))) {
                return HttpParseStatus::OutputTooSmall;
            }
        } else if (NameEquals(line, name_size, "connection")) {
            if (saw_connection) return HttpParseStatus::DuplicateHeader;
            saw_connection = true;
            head->connection_close =
                ValueEqualsInsensitive(value, value_size, "close");
        } else if (NameEquals(line, name_size, "content-range")) {
            return HttpParseStatus::InvalidHeader;
        }
        line = line_end + 2;
    }

    if (head->has_content_length && head->chunked) {
        return HttpParseStatus::AmbiguousBodyLength;
    }
    if (allow_close_delimited) {


        if (head->status_code == 204 && (head->has_content_length || head->chunked))
            return HttpParseStatus::InvalidHeader;
        if (head->status_code == 204 || head->status_code == 304) {
            head->bodyless = true;
        } else if (!head->has_content_length && !head->chunked) {
            head->close_delimited = true;
        }
    }
    if (!head->has_content_length && !head->chunked && !head->close_delimited && !head->bodyless &&
        !StatusMayHaveUnframedBody(head->status_code)) {
        return HttpParseStatus::MissingBodyLength;
    }
    if (head->status_code >= 300U && head->status_code < 400U &&
        head->location[0] == '\0' && !(allow_close_delimited && head->status_code == 304)) {
        return HttpParseStatus::MissingLocation;
    }
    return HttpParseStatus::Ok;
}

const char *HttpParseStatusString(HttpParseStatus status) {
    switch (status) {
        case HttpParseStatus::Ok: return "ok";
        case HttpParseStatus::NeedMoreData: return "need more HTTP header data";
        case HttpParseStatus::InvalidArgument: return "invalid argument";
        case HttpParseStatus::HeaderTooLarge: return "HTTP header too large";
        case HttpParseStatus::InvalidStatusLine: return "invalid HTTP status line";
        case HttpParseStatus::InvalidHeader: return "invalid HTTP header";
        case HttpParseStatus::DuplicateHeader: return "duplicate HTTP header";
        case HttpParseStatus::UnsupportedTransferEncoding: return "unsupported transfer encoding";
        case HttpParseStatus::UnsupportedContentEncoding: return "unsupported content encoding";
        case HttpParseStatus::AmbiguousBodyLength: return "ambiguous HTTP body length";
        case HttpParseStatus::MissingBodyLength: return "missing HTTP body length";
        case HttpParseStatus::InvalidContentLength: return "invalid content length";
        case HttpParseStatus::MissingLocation: return "redirect location missing";
        case HttpParseStatus::OutputTooSmall: return "HTTP field too large";
    }
    return "unknown HTTP parse error";
}

}
}
}
