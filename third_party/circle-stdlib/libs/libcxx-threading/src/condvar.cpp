//
// condvar.cpp
//
// libc++ external threading — condition variable implementation for the Circle
// bare-metal framework.
//
// Design: semaphore + waiter count.
//
//   wait:
//     ++waiters
//     mutex.Release()      // does not yield; sem.Down() below yields
//     sem.Down()           // if count > 0 (signal already delivered):
//                          //   returns immediately
//                          // otherwise: blocks until signal/broadcast calls
//                          //   Up()
//     mutex.Acquire()
//     // waiters already decremented by signaler
//
//   signal:
//     if (waiters > 0) { --waiters; sem.Up(); }
//
//   broadcast:
//     n = waiters; waiters = 0; for n: sem.Up()
//
//   timedwait:
//     ++waiters
//     mutex.Release()
//     timedout = sem.DownWithTimeout(delta_us)
//     if (timedout):
//       if sem.TryDown():        // racing signal arrived after timeout woke us
//         timedout = false       //   signaler already decremented waiters;
//                                //   consume the Up
//       else:
//         --waiters              // genuine timeout: no signal consumed us
//     mutex.Acquire()
//     return timedout ? ETIMEDOUT : 0
//
// The TryDown() guard closes the window where the scheduler marks us ready
// (timeout) and another task observes the still-inflated waiters count,
// pre-decrements it, and calls Up() — which would otherwise cause a double
// decrement of waiters and leave an orphaned semaphore count.
//

#include <__external_threading>
#include "mutex_impl.h"
#include <circle/sched/semaphore.h>
#include <circle/timer.h>
#include <new>

_LIBCPP_BEGIN_NAMESPACE_STD


// ---------------------------------------------------------------------------
// CondvarImpl — overlaid on __libcpp_condvar_t::__storage
// ---------------------------------------------------------------------------

struct CondvarImpl
{
    CSemaphore sem; // count=0 when zero-initialised — correct for condvar
    int waiters;
};

static_assert(sizeof(CondvarImpl) <= sizeof(__libcpp_condvar_t::__storage),
              "CondvarImpl does not fit in __libcpp_condvar_t storage");
static_assert(alignof(CondvarImpl) <= alignof(__libcpp_condvar_t),
              "CondvarImpl alignment exceeds __libcpp_condvar_t alignment");

static CondvarImpl *as_condvar(__libcpp_condvar_t *__cv)
{
    return reinterpret_cast<CondvarImpl *>(__cv->__storage);
}

static NonRecursiveMutexImpl *as_mutex(__libcpp_mutex_t *__m)
{
    return as_nonrecursive_mutex(__m);
}

// ---------------------------------------------------------------------------
// Helpers for absolute-timespec → relative-microseconds conversion
// ---------------------------------------------------------------------------

static long long timespec_diff_us(__libcpp_timespec_t const *__abs)
{
    unsigned now_sec = 0;
    unsigned now_us = 0;
    CTimer::Get()->GetUniversalTime(&now_sec, &now_us);

    long long const abs_us = static_cast<long long>(__abs->tv_sec) * 1000000LL +
                             static_cast<long long>(__abs->tv_nsec) / 1000LL;

    long long const now_total_us = static_cast<long long>(now_sec) * 1000000LL +
                                   static_cast<long long>(now_us);

    long long const delta = abs_us - now_total_us;
    return delta > 0 ? delta : 0;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

int __libcpp_condvar_signal(__libcpp_condvar_t *__cv)
{
    CondvarImpl *const impl = as_condvar(__cv);
    if (impl->waiters > 0)
    {
        --impl->waiters;
        impl->sem.Up();
    }
    return 0;
}

int __libcpp_condvar_broadcast(__libcpp_condvar_t *__cv)
{
    CondvarImpl *const impl = as_condvar(__cv);
    int const n = impl->waiters;
    impl->waiters = 0;
    for (int i = 0; i < n; ++i)
        impl->sem.Up();
    return 0;
}

int __libcpp_condvar_wait(__libcpp_condvar_t *__cv, __libcpp_mutex_t *__m)
{
    CondvarImpl *const impl = as_condvar(__cv);
    NonRecursiveMutexImpl *const mutex = as_mutex(__m);

    ++impl->waiters;
    mutex->Release(); // fully unlocks; any concurrent signal is buffered in sem
    impl->sem.Down(); // blocks until Up() is called (or returns immediately if
                      // already Up'd)
    mutex->Acquire();
    return 0;
}

int __libcpp_condvar_timedwait(__libcpp_condvar_t *__cv, __libcpp_mutex_t *__m,
                               __libcpp_timespec_t *__ts)
{
    CondvarImpl *const impl = as_condvar(__cv);
    NonRecursiveMutexImpl *const mutex = as_mutex(__m);

    long long const delta_us = timespec_diff_us(__ts);

    ++impl->waiters;
    mutex->Release();

    // DownWithTimeout returns TRUE on timeout, FALSE if semaphore was acquired.
    bool timed_out = impl->sem.DownWithTimeout(
        static_cast<unsigned>(delta_us < 0 ? 0 : delta_us));

    if (timed_out)
    {
        // A racing signal may have arrived after the scheduler woke us via
        // timeout but before we get CPU: the signaler pre-decremented waiters
        // and called Up(), leaving an orphaned semaphore count.  TryDown()
        // consumes that count atomically; if it succeeds the signal already
        // decremented waiters so we must not do so again, and we return
        // success.
        if (impl->sem.TryDown())
        {
            timed_out = false;
        }
        else
        {
            --impl->waiters;
        }
    }

    mutex->Acquire();
    return timed_out ? ETIMEDOUT : 0;
}

int __libcpp_condvar_destroy(__libcpp_condvar_t *__cv)
{
    as_condvar(__cv)->~CondvarImpl();
    return 0;
}

_LIBCPP_END_NAMESPACE_STD
