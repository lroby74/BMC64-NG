#ifndef BMX_NETWORK_IO_DEADLINE_H
#define BMX_NETWORK_IO_DEADLINE_H

#include <stdint.h>

namespace bmx {
namespace network {
namespace io {



struct MonotonicDeadline {
    uint64_t start_microseconds;
    uint64_t duration_microseconds;
};

bool InitializeMonotonicDeadline(uint64_t now_microseconds,
                                 uint32_t timeout_milliseconds,
                                 MonotonicDeadline *deadline);
bool RemainingDeadlineMilliseconds(const MonotonicDeadline &deadline,
                                   uint64_t now_microseconds,
                                   uint32_t per_operation_cap_milliseconds,
                                   uint32_t *remaining_milliseconds);

}
}
}

#endif
