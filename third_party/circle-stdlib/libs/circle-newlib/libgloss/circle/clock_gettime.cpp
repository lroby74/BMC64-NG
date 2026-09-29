#define _POSIX_TIMERS 1
#define _POSIX_MONOTONIC_CLOCK 200112L
#include "config.h"
#include <errno.h>
#include <time.h>

#include "circle_macros.h"
#include <circle/timer.h>

extern "C" int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    if (clock_id == CLOCK_MONOTONIC)
    {
        u64 const us = CTimer::GetClockTicks64();
        tp->tv_sec = static_cast<time_t>(us / 1000000ULL);
        tp->tv_nsec = static_cast<long>((us % 1000000ULL) * 1000ULL);
        return 0;
    }

    if (clock_id == CLOCK_REALTIME)
    {
        unsigned sec = 0;
        unsigned us = 0;
        if (CTimer::Get()->GetUniversalTime(&sec, &us))
        {
            tp->tv_sec = static_cast<time_t>(sec);
            tp->tv_nsec = static_cast<long>(us) * 1000L;
            return 0;
        }
        errno = EINVAL;
        return -1;
    }

    errno = EINVAL;
    return -1;
}
