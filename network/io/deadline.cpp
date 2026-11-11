#include "network/io/deadline.h"

#include <limits.h>

namespace bmx {
namespace network {
namespace io {

bool InitializeMonotonicDeadline(uint64_t now_microseconds,
                                 uint32_t timeout_milliseconds,
                                 MonotonicDeadline *deadline) {
    if (deadline == 0 || timeout_milliseconds == 0U) return false;
    deadline->start_microseconds = now_microseconds;
    deadline->duration_microseconds =
        static_cast<uint64_t>(timeout_milliseconds) * 1000U;
    return deadline->duration_microseconds != 0U;
}

bool RemainingDeadlineMilliseconds(const MonotonicDeadline &deadline,
                                   uint64_t now_microseconds,
                                   uint32_t per_operation_cap_milliseconds,
                                   uint32_t *remaining_milliseconds) {
    if (remaining_milliseconds == 0 ||
        per_operation_cap_milliseconds == 0U ||
        deadline.duration_microseconds == 0U ||
        now_microseconds < deadline.start_microseconds) {
        return false;
    }
    const uint64_t elapsed = now_microseconds - deadline.start_microseconds;
    if (elapsed >= deadline.duration_microseconds) return false;
    const uint64_t remaining_microseconds =
        deadline.duration_microseconds - elapsed;
    uint64_t remaining = remaining_microseconds / 1000U;
    if (remaining_microseconds % 1000U != 0U) ++remaining;
    if (remaining > per_operation_cap_milliseconds) {
        remaining = per_operation_cap_milliseconds;
    }
    if (remaining == 0U || remaining > UINT_MAX) return false;
    *remaining_milliseconds = static_cast<uint32_t>(remaining);
    return true;
}

}
}
}
