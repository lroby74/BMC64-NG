#ifndef BMX_NETWORK_HTTP_BODY_SOURCE_H
#define BMX_NETWORK_HTTP_BODY_SOURCE_H
#include <string.h>

#include "network/io/byte_stream.h"
namespace bmx {
namespace network {
namespace http {


class BodySource {
   public:
    virtual ~BodySource() {}
    virtual uint64_t Length() const = 0;
    virtual io::IoResult ReadSome(uint8_t* data, size_t capacity) = 0;
    virtual bool Reset() = 0;
};
class MemoryBodySource : public BodySource {
   public:
    MemoryBodySource(const uint8_t* data, size_t size) : data_(data), size_(size), offset_(0) {}
    uint64_t Length() const override { return size_; }
    io::IoResult ReadSome(uint8_t* data, size_t capacity) override {
        if ((!data_ && size_) || !data || !capacity) return {io::IoStatus::Error, 0, 0};
        if (offset_ == size_) return {io::IoStatus::EndOfStream, 0, 0};
        size_t n = size_ - offset_;
        if (n > capacity) n = capacity;
        memcpy(data, data_ + offset_, n);
        offset_ += n;
        return {io::IoStatus::Progress, n, 0};
    }
    bool Reset() override {
        offset_ = 0;
        return data_ || !size_;
    }

   private:
    const uint8_t* data_;
    size_t size_, offset_;
};
}
}
}
#endif
