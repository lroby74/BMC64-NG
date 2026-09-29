//
// mutex.cpp
//
// libc++ external threading — mutex implementation for the Circle bare-metal
// framework.
//

#include <__external_threading>
#include "mutex_impl.h"
#include <circle/sched/mutex.h>
#include <new>

_LIBCPP_BEGIN_NAMESPACE_STD


// ---------------------------------------------------------------------------
// Non-recursive mutex (backed by NonRecursiveMutexImpl, not CMutex)
//
// Initialised via _LIBCPP_MUTEX_INITIALIZER (zero-init), which is a valid
// initial state for NonRecursiveMutexImpl.  No explicit init function exists
// in the libc++ API for non-recursive mutexes.
// ---------------------------------------------------------------------------

int __libcpp_mutex_lock(__libcpp_mutex_t *__m)
{
    as_nonrecursive_mutex(__m)->Acquire();
    return 0;
}

bool __libcpp_mutex_trylock(__libcpp_mutex_t *__m)
{
    return as_nonrecursive_mutex(__m)->TryAcquire();
}

int __libcpp_mutex_unlock(__libcpp_mutex_t *__m)
{
    as_nonrecursive_mutex(__m)->Release();
    return 0;
}

int __libcpp_mutex_destroy(__libcpp_mutex_t *__m)
{
    as_nonrecursive_mutex(__m)->~NonRecursiveMutexImpl();
    return 0;
}

// ---------------------------------------------------------------------------
// Recursive mutex (backed directly by CMutex)
//
// Explicitly initialised via __libcpp_recursive_mutex_init() using
// placement new so the constructor runs properly.
// ---------------------------------------------------------------------------

static_assert(sizeof(CMutex) <= sizeof(__libcpp_recursive_mutex_t::__storage),
              "CMutex does not fit in __libcpp_recursive_mutex_t storage");
static_assert(alignof(CMutex) <= alignof(__libcpp_recursive_mutex_t),
              "CMutex alignment exceeds __libcpp_recursive_mutex_t alignment");

static CMutex *as_recursive_mutex(__libcpp_recursive_mutex_t *__m)
{
    return reinterpret_cast<CMutex *>(__m->__storage);
}

int __libcpp_recursive_mutex_init(__libcpp_recursive_mutex_t *__m)
{
    new (__m->__storage) CMutex();
    return 0;
}

int __libcpp_recursive_mutex_lock(__libcpp_recursive_mutex_t *__m)
{
    as_recursive_mutex(__m)->Acquire();
    return 0;
}

bool __libcpp_recursive_mutex_trylock(__libcpp_recursive_mutex_t *__m)
{
    return as_recursive_mutex(__m)->TryAcquire();
}

int __libcpp_recursive_mutex_unlock(__libcpp_recursive_mutex_t *__m)
{
    as_recursive_mutex(__m)->Release();
    return 0;
}

int __libcpp_recursive_mutex_destroy(__libcpp_recursive_mutex_t *__m)
{
    as_recursive_mutex(__m)->~CMutex();
    return 0;
}

_LIBCPP_END_NAMESPACE_STD
