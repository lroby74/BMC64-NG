#ifndef BMX_WIC64_REPLY_BUFFER_H
#define BMX_WIC64_REPLY_BUFFER_H
#include "network/http/body_sink.h"
#include <stdlib.h>
namespace bmx { namespace wic64 {



class ReplyBuffer : public network::http::BodySink {
public:
    static const size_t PageBytes = 65536;
    static const size_t MaximumBytes = 16U * 1024U * 1024U;
    static const size_t MaximumPages = MaximumBytes / PageBytes;
    typedef void *(*Allocate)(size_t);
    typedef void (*Free)(void *);
    ReplyBuffer(Allocate allocate = malloc, Free release = free);
    ~ReplyBuffer() override;
    network::io::IoResult WriteSome(const uint8_t *data, size_t size) override;
    void Clear();
    size_t Size() const { return size_; }
    size_t AllocatedBytes() const { return pages_ * PageBytes; }
    bool AllocationFailed() const { return allocation_failed_; }
    const uint8_t *const *Pages() const { return data_; }
    bool Copy(size_t offset, void *out, size_t size) const;
private:
    ReplyBuffer(const ReplyBuffer &) = delete;
    ReplyBuffer &operator=(const ReplyBuffer &) = delete;
    uint8_t *data_[MaximumPages];
    size_t size_, pages_;
    Allocate allocate_;
    Free free_;
    bool allocation_failed_ = false;
};
} }
#endif
