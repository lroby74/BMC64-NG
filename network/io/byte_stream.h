#ifndef BMX_NETWORK_IO_BYTE_STREAM_H
#define BMX_NETWORK_IO_BYTE_STREAM_H

#include "deadline.h"
#include "result.h"

namespace bmx { namespace network { namespace io {




struct OperationContext {
    uint64_t now_microseconds;
    const MonotonicDeadline *deadline;
    bool canceled;
};

inline IoResult CheckOperation(const OperationContext &context) {
    if (context.canceled) return {IoStatus::Canceled, 0, 0};
    uint32_t remaining = 0;
    if (context.deadline && !RemainingDeadlineMilliseconds(
            *context.deadline, context.now_microseconds, UINT32_MAX, &remaining))
        return {IoStatus::TimedOut, 0, 0};
    return {IoStatus::Progress, 0, 0};
}




class ByteStream {
public:
    virtual ~ByteStream() {}
    virtual IoResult ReadSome(void *buffer, size_t capacity,
                              const OperationContext &context) = 0;
    virtual IoResult WriteSome(const void *buffer, size_t size,
                               const OperationContext &context) = 0;
    virtual void Close() = 0;
};

}}}
#endif
