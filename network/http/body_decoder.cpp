#include "network/http/body_decoder.h"

#include <limits.h>
#include <string.h>

namespace bmx {
namespace network {
namespace http {

HttpBodyDecoder::HttpBodyDecoder(const HttpResponseHead &head,
                                 uint64_t maximum_body_bytes,
                                 BodySink *sink)
    : state_(head.chunked ? kChunkSize : head.close_delimited ? kCloseDelimited : kContentLength),
      remaining_(head.has_content_length ? head.content_length : 0U),
      decoded_bytes_(0U),
      maximum_body_bytes_(maximum_body_bytes),
      sink_(sink),
      chunk_line_size_(0U) {
    if (sink_ && head.bodyless) {
        state_ = kComplete;
    } else if (sink_ == 0 || (unsigned(head.has_content_length) + unsigned(head.chunked) + unsigned(head.close_delimited) != 1)) {
        state_ = kFailed;
    } else if (head.has_content_length && remaining_ > maximum_body_bytes_) {
        state_ = kFailed;
    } else if (head.has_content_length && remaining_ == 0U) {
        state_ = kComplete;
    }
}

HttpBodyStatus HttpBodyDecoder::Fail(HttpBodyStatus status) {
    state_ = kFailed;
    return status;
}

HttpBodyStatus HttpBodyDecoder::Write(const uint8_t *data, size_t size, size_t *written) {
    *written = 0;
    if (size == 0U) return HttpBodyStatus::Ok;
    if (decoded_bytes_ > maximum_body_bytes_ ||
        static_cast<uint64_t>(size) > maximum_body_bytes_ - decoded_bytes_) {
        return Fail(HttpBodyStatus::TooLarge);
    }
    const io::IoResult result = sink_->WriteSome(data, size);
    if (result.status == io::IoStatus::PendingWrite && result.bytes == 0)
        return HttpBodyStatus::Pending;
    if (result.status != io::IoStatus::Progress || !result.bytes || result.bytes > size)
        return Fail(HttpBodyStatus::SinkRejected);
    *written = result.bytes;
    decoded_bytes_ += result.bytes;
    return result.bytes == size ? HttpBodyStatus::Ok : HttpBodyStatus::Pending;
}

HttpBodyStatus HttpBodyDecoder::Feed(const uint8_t *data, size_t size,
                                     size_t *consumed, size_t sink_call_budget) {
    if (consumed == 0 || (data == 0 && size != 0U)) {
        return HttpBodyStatus::InvalidArgument;
    }
    *consumed = 0U;
    if (state_ == kFailed) return HttpBodyStatus::InvalidArgument;
    if (state_ == kComplete) {
        return size == 0U ? HttpBodyStatus::Complete
                          : HttpBodyStatus::UnexpectedData;
    }

    while (*consumed < size) {
        if (state_ == kContentLength || state_ == kChunkData || state_ == kCloseDelimited) {
            if (!sink_call_budget) return HttpBodyStatus::Ok;
            --sink_call_budget;
            size_t available = size - *consumed;
            if (state_ != kCloseDelimited && static_cast<uint64_t>(available) > remaining_)
                available = static_cast<size_t>(remaining_);
            size_t written = 0;
            const HttpBodyStatus status = Write(data + *consumed, available, &written);
            *consumed += written;
            if (state_ != kCloseDelimited) remaining_ -= written;
            if (status != HttpBodyStatus::Ok) return status;
            if (state_ == kContentLength && remaining_ == 0) {
                state_ = kComplete;
                return *consumed == size ? HttpBodyStatus::Complete : HttpBodyStatus::UnexpectedData;
            }
            if (state_ == kChunkData && remaining_ == 0) state_ = kChunkDataCr;
            continue;
        }

        const uint8_t c = data[(*consumed)++];
        if (state_ == kChunkSize) {
            if (c == '\r') {
                if (chunk_line_size_ == 0U) return Fail(HttpBodyStatus::InvalidChunk);
                uint64_t chunk_size = 0U;
                for (size_t i = 0; i < chunk_line_size_; ++i) {
                    const char digit_char = chunk_line_[i];
                    unsigned digit;
                    if (digit_char >= '0' && digit_char <= '9') {
                        digit = static_cast<unsigned>(digit_char - '0');
                    } else if (digit_char >= 'a' && digit_char <= 'f') {
                        digit = static_cast<unsigned>(digit_char - 'a' + 10);
                    } else if (digit_char >= 'A' && digit_char <= 'F') {
                        digit = static_cast<unsigned>(digit_char - 'A' + 10);
                    } else {
                        return Fail(HttpBodyStatus::InvalidChunk);
                    }
                    if (chunk_size > (UINT64_MAX - digit) / 16U) {
                        return Fail(HttpBodyStatus::InvalidChunk);
                    }
                    chunk_size = chunk_size * 16U + digit;
                }
                remaining_ = chunk_size;
                chunk_line_size_ = 0U;
                state_ = kChunkSizeLf;
                continue;
            }
            if (c == '\n' || c == ';' || chunk_line_size_ >= sizeof(chunk_line_)) {
                return Fail(HttpBodyStatus::InvalidChunk);
            }
            chunk_line_[chunk_line_size_++] = static_cast<char>(c);
            continue;
        }
        if (state_ == kChunkSizeLf) {
            if (c != '\n') return Fail(HttpBodyStatus::InvalidChunk);
            state_ = remaining_ == 0U ? kFinalCr : kChunkData;
            continue;
        }
        if (state_ == kChunkDataCr) {
            if (c != '\r') return Fail(HttpBodyStatus::InvalidChunk);
            state_ = kChunkDataLf;
            continue;
        }
        if (state_ == kChunkDataLf) {
            if (c != '\n') return Fail(HttpBodyStatus::InvalidChunk);
            state_ = kChunkSize;
            continue;
        }
        if (state_ == kFinalCr) {
            if (c != '\r') return Fail(HttpBodyStatus::InvalidChunk);
            state_ = kFinalLf;
            continue;
        }
        if (state_ == kFinalLf) {
            if (c != '\n') return Fail(HttpBodyStatus::InvalidChunk);
            state_ = kComplete;
            return *consumed == size ? HttpBodyStatus::Complete
                                     : HttpBodyStatus::UnexpectedData;
        }
    }
    return state_ == kComplete ? HttpBodyStatus::Complete : HttpBodyStatus::Ok;
}

HttpBodyStatus HttpBodyDecoder::Finish() {
    if (state_ == kCloseDelimited) state_ = kComplete;
    if (state_ == kComplete) return HttpBodyStatus::Complete;
    if (state_ == kFailed) return HttpBodyStatus::InvalidArgument;
    return Fail(HttpBodyStatus::Truncated);
}

const char *HttpBodyStatusString(HttpBodyStatus status) {
    switch (status) {
        case HttpBodyStatus::Ok: return "ok";
        case HttpBodyStatus::Pending: return "pending sink";
        case HttpBodyStatus::Complete: return "complete";
        case HttpBodyStatus::InvalidArgument: return "invalid argument";
        case HttpBodyStatus::TooLarge: return "HTTP body too large";
        case HttpBodyStatus::SinkRejected: return "HTTP body sink rejected data";
        case HttpBodyStatus::InvalidChunk: return "invalid chunked HTTP body";
        case HttpBodyStatus::UnexpectedData: return "data after HTTP body";
        case HttpBodyStatus::Truncated: return "truncated HTTP body";
    }
    return "unknown HTTP body error";
}

}
}
}
