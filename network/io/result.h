#ifndef BMX_NETWORK_IO_RESULT_H
#define BMX_NETWORK_IO_RESULT_H

#include <stddef.h>
#include <stdint.h>

namespace bmx {
namespace network {
namespace io {




enum class IoStatus : uint8_t {
    Progress = 0,
    PendingRead,
    PendingWrite,
    EndOfStream,
    Canceled,
    TimedOut,
    Error
};

struct IoResult {
    IoStatus status;



    size_t bytes;


    int native_error;
};

}
}
}

#endif
