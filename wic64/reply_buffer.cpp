#include "wic64/reply_buffer.h"
#include <string.h>
namespace bmx { namespace wic64 {
using network::io::IoResult;
using network::io::IoStatus;
ReplyBuffer::ReplyBuffer(Allocate allocate, Free release)
    : data_{}, size_(0), pages_(0), allocate_(allocate), free_(release) {}
ReplyBuffer::~ReplyBuffer() { Clear(); }
void ReplyBuffer::Clear() {
    for (size_t i = 0; i < pages_; ++i) { free_(data_[i]); data_[i] = nullptr; }
    size_ = pages_ = 0;
    allocation_failed_ = false;
}
IoResult ReplyBuffer::WriteSome(const uint8_t *data, size_t size) {
    if (!data || !size || size_ == MaximumBytes) return {IoStatus::Error, 0, 0};
    const size_t index = size_ / PageBytes, offset = size_ % PageBytes;
    if (index == pages_) {
        uint8_t *page = static_cast<uint8_t *>(allocate_(PageBytes));
        if (!page) { allocation_failed_ = true; return {IoStatus::Error, 0, 0}; }
        data_[pages_++] = page;
    }
    size_t count = PageBytes - offset;
    if (count > size) count = size;

    if (count > 4096) count = 4096;
    memcpy(data_[index] + offset, data, count);
    size_ += count;
    return {IoStatus::Progress, count, 0};
}
bool ReplyBuffer::Copy(size_t offset, void *out, size_t size) const {
    if ((!out && size) || offset > size_ || size > size_ - offset) return false;
    uint8_t *target = static_cast<uint8_t *>(out);
    while (size) {
        size_t n = PageBytes - offset % PageBytes;
        if (n > size) n = size;
        memcpy(target, data_[offset / PageBytes] + offset % PageBytes, n);
        size -= n; offset += n; target += n;
    }
    return true;
}
} }
