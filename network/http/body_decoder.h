#ifndef BMX_NETWORK_HTTP_BODY_DECODER_H
#define BMX_NETWORK_HTTP_BODY_DECODER_H

#include "network/http/response_parser.h"
#include "network/http/body_sink.h"

namespace bmx {
namespace network {
namespace http {

class HttpBodySink : public BodySink {
 public:
    virtual ~HttpBodySink() {}
    virtual bool Write(const uint8_t *data, size_t size) = 0;
    io::IoResult WriteSome(const uint8_t *data, size_t size) override {
        return Write(data, size) ? io::IoResult{io::IoStatus::Progress, size, 0}
                                 : io::IoResult{io::IoStatus::Error, 0, 0};
    }
};

enum class HttpBodyStatus : uint8_t {
    Ok = 0,
    Complete,
    InvalidArgument,
    TooLarge,
    SinkRejected,
    InvalidChunk,
    UnexpectedData,
    Truncated,
    Pending
};




class HttpBodyDecoder {
 public:
    HttpBodyDecoder(const HttpResponseHead &head, uint64_t maximum_body_bytes,
                    BodySink *sink);


    HttpBodyStatus Feed(const uint8_t *data, size_t size, size_t *consumed,
                        size_t sink_call_budget = static_cast<size_t>(-1));
    HttpBodyStatus Finish();
    uint64_t decoded_bytes() const { return decoded_bytes_; }
    bool complete() const { return state_ == kComplete; }

 private:
    enum State : uint8_t {
        kContentLength,
        kCloseDelimited,
        kChunkSize,
        kChunkSizeLf,
        kChunkData,
        kChunkDataCr,
        kChunkDataLf,
        kFinalCr,
        kFinalLf,
        kComplete,
        kFailed
    };

    HttpBodyStatus Fail(HttpBodyStatus status);
    HttpBodyStatus Write(const uint8_t *data, size_t size, size_t *written);

    State state_;
    uint64_t remaining_;
    uint64_t decoded_bytes_;
    uint64_t maximum_body_bytes_;
    BodySink *sink_;
    char chunk_line_[18];
    size_t chunk_line_size_;
};

const char *HttpBodyStatusString(HttpBodyStatus status);

}
}
}

#endif
