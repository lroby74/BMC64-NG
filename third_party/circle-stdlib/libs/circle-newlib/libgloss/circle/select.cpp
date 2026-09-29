#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include "circle_macros.h"
#include "filetable.h"
#include "cglueio.h"
#include <circle/timer.h>
#include <circle/sched/scheduler.h>

static_assert(_CircleStdlib::FileTable::MAX_OPEN_FILES <= FD_SETSIZE,
              "MAX_OPEN_FILES exceeds FD_SETSIZE");
static_assert(sizeof(fd_set::_x) * CHAR_BIT >= FD_SETSIZE,
              "fd_set too small for MAX_OPEN_FILES");

extern "C" void FD_CLR(int fd, fd_set *fdset)
{
    if (!fdset || fd < 0 || fd >= FD_SETSIZE)
    {
        return;
    }

    int const mask = ~(1 << fd);
    fdset->_x &= mask;
}

extern "C" int FD_ISSET(int fd, fd_set *fdset)
{
    if (!fdset || fd < 0 || fd >= FD_SETSIZE)
    {
        return 0;
    }

    int const mask = 1 << fd;
    return (fdset->_x & mask) != 0;
}

extern "C" void FD_SET(int fd, fd_set *fdset)
{
    if (!fdset || fd < 0 || fd >= FD_SETSIZE)
    {
        return;
    }

    int const mask = 1 << fd;
    fdset->_x |= mask;
}

extern "C" void FD_ZERO(fd_set *fdset)
{
    fdset->_x = 0;
}

extern "C" int pselect(int nfds, fd_set *readfds,
                       fd_set *writefds, fd_set *errorfds,
                       const struct timespec *timeout,
                       const sigset_t *sigmask)
{
    errno = ENOSYS;
    return -1;
}

extern "C" int select(int nfds, fd_set *readfds,
                      fd_set *writefds, fd_set *errorfds,
                      struct timeval *timeout)
{
    using _CircleStdlib::CGlueIO;
    using _CircleStdlib::CircleFile;

    // Validate nfds.
    if (nfds < 0 || nfds > FD_SETSIZE)
    {
        errno = EINVAL;
        return -1;
    }

    // Validate struct timeval if given.
    if (timeout)
    {
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec >= 1000000)
        {
            errno = EINVAL;
            return -1;
        }
    }

    auto const prepare_copy = [](fd_set *param) -> fd_set
    {
        if (param)
        {
            return *param;
        }
        fd_set copy;
        FD_ZERO(&copy);
        return copy;
    };

    fd_set read_copy = prepare_copy(readfds);
    fd_set write_copy = prepare_copy(writefds);
    fd_set err_copy = prepare_copy(errorfds);

    fd_set read_out;
    fd_set write_out;
    fd_set err_out;
    FD_ZERO(&read_out);
    FD_ZERO(&write_out);
    FD_ZERO(&err_out);

    int bit_set_count = 0;

    auto const check_fd = [&bit_set_count](int fd, fd_set &in, bool is_open, bool status, fd_set &out) -> bool
    {
        if (FD_ISSET(fd, &in))
        {
            if (!is_open)
            {
                errno = EBADF;
                return false;
            }
            if (status)
            {
                FD_SET(fd, &out);
                bit_set_count += 1;
            }
        }
        return true;
    };

    // Compute timeout deadline (in clock ticks)
    u64 const start = CTimer::GetClockTicks64();
    u64 const wait_ticks =
        timeout ? static_cast<u64>(timeout->tv_sec) * CLOCKHZ + static_cast<u64>(timeout->tv_usec) * (CLOCKHZ / 1000000) : static_cast<u64>(-1);

    while (bit_set_count == 0)
    {
        for (int fd = 0; fd < nfds; fd += 1)
        {
            CircleFile *const file = _CircleStdlib::FileTable::GetFile(fd);
            CGlueIO *const glueIO = file && file->IsOpen() ? file->GetGlueIO() : nullptr;

            CGlueIO::TStatus const status = glueIO ? glueIO->GetSelectStatus() : CGlueIO::TStatus{false, false, false, false};
            bool const is_open = glueIO != nullptr;

            if (!check_fd(fd, read_copy, is_open, status.bRxReady, read_out)
                || !check_fd(fd, write_copy, is_open, status.bTxReady, write_out)
                || !check_fd(fd, err_copy, is_open, status.bException, err_out))
            {
                CScheduler::Get()->Yield();
                return -1;
            }
        }

        // Doing the Yield here guarantees that it is called at least
        // once for the select() call.
        CScheduler::Get()->Yield();

        // Check for timeout. If the timeout argument points to an object
        // of type struct timeval whose members are 0, select() does not block.
        if (timeout)
        {
            if (timeout->tv_sec == 0 && timeout->tv_usec == 0
                || CTimer::GetClockTicks64() - start >= wait_ticks)
            {
                break;
            }
        }
    }

    if (readfds)
    {
        *readfds = read_out;
    }
    if (writefds)
    {
        *writefds = write_out;
    }
    if (errorfds)
    {
        *errorfds = err_out;
    }

    return bit_set_count;
}
