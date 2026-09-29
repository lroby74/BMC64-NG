#include <poll.h>
#include <errno.h>

#include "circle_macros.h"
#include "filetable.h"
#include "cglueio.h"
#include <circle/timer.h>
#include <circle/sched/scheduler.h>

extern "C" int poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
    using _CircleStdlib::CGlueIO;
    using _CircleStdlib::CircleFile;
    using _CircleStdlib::FileTable;

    // Validate nfds against our file table size
    if (nfds > static_cast<nfds_t>(_CircleStdlib::FileTable::MAX_OPEN_FILES))
    {
        errno = EINVAL;
        return -1;
    }

    // timeout: -1 means infinite, 0 means poll and return immediately
    if (timeout < -1)
    {
        errno = EINVAL;
        return -1;
    }

    // Convert timeout milliseconds to clock ticks (CLOCKHZ per second)
    u64 const start = CTimer::GetClockTicks64();
    u64 const wait_ticks = (timeout >= 0)
                               ? static_cast<u64>(timeout) * (CLOCKHZ / 1000)
                               : static_cast<u64>(-1);

    auto const check_set_flag = [](struct pollfd &p, bool condition, short flag) -> void {
        if ((p.events & flag) != 0 && condition)
        {
            p.revents |= flag;
        }
    };

    int total_revents = 0;

    while (true)
    {
        // Call Yield at the beginning of the loop tp guarantee that is
        // called at least once.
        CScheduler::Get()->Yield();

        for (nfds_t i = 0; i < nfds; ++i)
        {
            struct pollfd &p = fds[i];

            // per spec: negative fd => events ignored, revents set to 0
            p.revents = 0;
            if (p.fd < 0)
            {
                continue;
            }

            CircleFile * const file = FileTable::GetFile(p.fd);
            if (!file || !file->IsOpen())
            {
                p.revents |= POLLNVAL;
                total_revents += 1;
                continue;
            }

            CGlueIO * const glue = file->GetGlueIO();
            if (!glue)
            {
                p.revents |= POLLNVAL;
                total_revents += 1;
                continue;
            }

            CGlueIO::TStatus const st {glue->GetSelectStatus()};
            if (!st.bConnected)
            {
                p.revents |= POLLHUP;
            }

            if (st.bException)
            {
                p.revents |= POLLERR;
            }

            check_set_flag(p, st.bRxReady, POLLIN);
            check_set_flag(p, st.bRxReady, POLLRDNORM);
            check_set_flag(p, st.bRxReady, POLLRDBAND);
            check_set_flag(p, st.bRxReady, POLLPRI);

            check_set_flag(p, st.bTxReady, POLLOUT);
            check_set_flag(p, st.bTxReady, POLLWRNORM);
            check_set_flag(p, st.bTxReady, POLLWRBAND);

            if (p.revents != 0)
            {
                total_revents += 1;
            }
        }

        if (timeout == 0 || total_revents > 0)
        {
            // If timeout is zero, we only do one check
            break;
        }
        else if (timeout > 0)
        {
            // Check for timeout
            if (CTimer::GetClockTicks64() - start >= wait_ticks)
            {
                // Timed out
                break;
            }
        }
    }

    return total_revents;
}
