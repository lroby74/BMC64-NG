//
// mutex_impl.h
//
// Private implementation header — non-recursive mutex backing __libcpp_mutex_t.
// Included by both mutex.cpp and condvar.cpp.
//
// CMutex is recursive by design in Circle: a re-entrant Acquire() by the same
// task increments an internal counter and returns immediately.  Mapping
// __libcpp_mutex_t directly to CMutex therefore lets a user silently double-
// lock a std::mutex (which is undefined behaviour, but CMutex will not
// diagnose it).  The resulting counter value of 2 causes a deadlock in
// __libcpp_condvar_wait: Release() decrements the count to 1 instead of 0,
// leaving the mutex held while the caller blocks on the condvar semaphore.
//
// NonRecursiveMutexImpl fixes this with a binary-semaphore idiom:
//   Acquire() — fast path when unlocked; CSemaphore::Down() when already held.
//   Release() — transfers ownership to one waiter (via Up()); otherwise
//               clears m_locked.  Does NOT call Yield(); the cooperative
//               scheduler context-switches only at the Down() in the caller.
//
// Zero-initialised storage is a valid initial state (per _LIBCPP_MUTEX_INITIALIZER):
//   m_locked  = false  — unlocked
//   m_waiters = 0      — no blocked tasks
//   m_sem              — count 0, no waiting tasks
//
// Size on ARM64: bool(1)+pad(3)+int(4)+CSemaphore(24) = 32 B
// Size on ARM32: bool(1)+pad(3)+int(4)+CSemaphore(12) = 20 B
// Both fit in __libcpp_mutex_t::__storage[32].
//

#pragma once

#include <__external_threading>
#include <circle/sched/semaphore.h>

struct NonRecursiveMutexImpl
{
    bool       m_locked;   // true while the mutex is held
    int        m_waiters;  // number of tasks blocked in Acquire()
    CSemaphore m_sem;      // count 0 from zero-init; used only for blocking

    void Acquire()
    {
        if (!m_locked)
        {
            m_locked = true;
            return;
        }
        ++m_waiters;
        m_sem.Down(); // yields; Release() transfers ownership before Up()
    }

    bool TryAcquire()
    {
        if (!m_locked)
        {
            m_locked = true;
            return true;
        }
        return false;
    }

    void Release()
    {
        if (m_waiters > 0)
        {
            --m_waiters;
            // m_locked stays true — ownership transferred to the first waiter.
            m_sem.Up();
        }
        else
        {
            m_locked = false;
        }
    }
};

static_assert(sizeof(NonRecursiveMutexImpl) <=
                  sizeof(std::__libcpp_mutex_t::__storage),
              "NonRecursiveMutexImpl does not fit in __libcpp_mutex_t storage");
static_assert(alignof(NonRecursiveMutexImpl) <=
                  alignof(std::__libcpp_mutex_t),
              "NonRecursiveMutexImpl alignment exceeds __libcpp_mutex_t alignment");

inline NonRecursiveMutexImpl *as_nonrecursive_mutex(std::__libcpp_mutex_t *__m)
{
    return reinterpret_cast<NonRecursiveMutexImpl *>(__m->__storage);
}
