//
// thread.cpp
//
// libc++ external threading — thread, TLS, and execute-once implementation
// for the Circle bare-metal framework.
//

#include <__external_threading>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <new>
#include <cstdint>
#include <cstring>

_LIBCPP_BEGIN_NAMESPACE_STD

// ---------------------------------------------------------------------------
// Thread-local storage state
// ---------------------------------------------------------------------------
unsigned constexpr MAX_TLS_KEYS = 32;

static void (*s_destructors[MAX_TLS_KEYS])(void *) = {};
static unsigned s_next_key = 0;

// ---------------------------------------------------------------------------
// Per-task TLS state — stored in TASK_USER_DATA_LIBCXX (slot 2).
//
// tls_block: heap allocation for the hardware TLS block; TPIDR_EL0/TPIDR is
//            set to point here on every context switch into this task.
// kv:        key-value slots for __libcpp_tls_get/set (formerly raw void*[]).
// ---------------------------------------------------------------------------

struct TaskTLSData
{
    void *tls_block;
    void *kv[MAX_TLS_KEYS];
};

// ---------------------------------------------------------------------------
// Linker-exported TLS section boundaries (defined in circle.ld).
// Used to compute the hardware TLS block size and .tdata initialisation image.
// ---------------------------------------------------------------------------

extern "C" char __tdata_start[];
extern "C" char __tdata_end[];
extern "C" char __tbss_start[];
extern "C" char __tbss_end[];

// ---------------------------------------------------------------------------
// Thread Control Block size — precedes .tdata/.tbss in the per-task TLS block.
// AArch64 variant-1 TLS: 16-byte TCB.  ARM32: 8-byte TCB.
// ---------------------------------------------------------------------------

#ifdef __aarch64__
unsigned constexpr TLS_TCB_SIZE = 16;
#else
unsigned constexpr TLS_TCB_SIZE = 8;
#endif

// ---------------------------------------------------------------------------
// alloc_tls_block — allocate and initialise a fresh per-task hardware TLS
// block.
//
// Layout:
//   [0 .. TLS_TCB_SIZE-1]             TCB (zeroed)
//   [TLS_TCB_SIZE .. +tdata_size-1]   .tdata image (copied from linker section)
//   [TLS_TCB_SIZE + tdata_size .. end] .tbss (zeroed)
//
// The total size matches the span __tdata_start...__tbss_end so that every
// tprel offset baked into the binary lands at the correct slot in the block.
// ---------------------------------------------------------------------------

static void *alloc_tls_block()
{
    std::size_t const tdata_size =
        static_cast<std::size_t>(__tdata_end - __tdata_start);
    std::size_t const span =
        static_cast<std::size_t>(__tbss_end - __tdata_start);
    std::size_t const total = TLS_TCB_SIZE + span;

    u8 *const block = new u8[total]();   // value-initialises (zeroes) entire block
    if (tdata_size > 0)
    {
        std::memcpy(block + TLS_TCB_SIZE, __tdata_start, tdata_size);
    }
    return block;
}

// ---------------------------------------------------------------------------
// CLibCXXTask — CTask subclass that runs a std::thread entry function
//
// __libcpp_thread_t::__opaque points at a heap-allocated JoinHandle (below),
// NOT at the CTask directly.  This keeps join() and detach() safe after the
// scheduler has already deleted the CTask via its unconditional
// `delete pTask` in GetNextTask().
//
// JoinHandle ownership rules:
//   - join()  : reads task (may be nullptr), then deletes the handle.
//   - detach(): sets detached = true; if task is already nullptr, deletes
//               handle immediately; otherwise ~CLibCXXTask() will delete it.
//   - ~CLibCXXTask(): always nulls task; deletes handle when detached or
//   joined.
// ---------------------------------------------------------------------------

class CLibCXXTask;

struct JoinHandle
{
    CLibCXXTask *task;
    uintptr_t id; // Stable cached thread ID
    bool detached;
    bool joined;
};

class CLibCXXTask : public CTask
{
public:
    // Threads with C++ exception handling need a larger stack to accommodate
    // unwinder state.
    CLibCXXTask(void *(*func)(void *), void *arg, JoinHandle *handle)
        : CTask(TASK_STACK_SIZE * 4), m_func(func), m_arg(arg), m_handle(handle)
    {
        TaskTLSData *const tls = new TaskTLSData{};
        tls->tls_block = alloc_tls_block();
        SetUserData(tls, TASK_USER_DATA_LIBCXX);
    }

    ~CLibCXXTask() override
    {
        m_handle->task = nullptr;
        if (m_handle->detached || m_handle->joined)
            delete m_handle;
    }

    void Run() override
    {
        TaskTLSData *const tls =
            static_cast<TaskTLSData *>(GetUserData(TASK_USER_DATA_LIBCXX));
        if (tls)
        {
            // Set the hardware thread pointer once; Circle's TaskSwitch
            // saves/restores TPIDR on every subsequent context switch.
#ifdef __aarch64__
            asm volatile("msr tpidr_el0, %0" : : "r"(tls->tls_block));
#else
            asm volatile("mcr p15, 0, %0, c13, c0, 2" : : "r"(tls->tls_block));
            asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(tls->tls_block));
#endif
        }

        m_func(m_arg);

        if (tls)
        {
            int constexpr PTHREAD_DESTRUCTOR_ITERATIONS = 4;
            bool destructors_called = true;
            for (int i = 0;
                 i < PTHREAD_DESTRUCTOR_ITERATIONS && destructors_called; ++i)
            {
                destructors_called = false;
                for (unsigned k = 0; k < s_next_key; ++k)
                {
                    if (tls->kv[k] && s_destructors[k])
                    {
                        void *const val = tls->kv[k];
                        tls->kv[k] = nullptr;
                        s_destructors[k](val);
                        destructors_called = true;
                    }
                }
            }
            delete[] static_cast<u8 *>(tls->tls_block);
            delete tls;
            SetUserData(nullptr, TASK_USER_DATA_LIBCXX);
        }
    }

private:
    void *(*m_func)(void *);
    void *m_arg;
    JoinHandle *m_handle;
};

// ---------------------------------------------------------------------------
// Thread
// ---------------------------------------------------------------------------

int __libcpp_thread_create(__libcpp_thread_t *__t, void *(*__func)(void *),
                           void *__arg)
{
    // First-time initialisation: give the main task its own TLS block and
    // set the hardware thread pointer; Circle's TaskSwitch will save/restore it.
    static bool s_initialized = false;
    if (!s_initialized)
    {
        s_initialized = true;
        CTask *const main_task = CScheduler::Get()->GetCurrentTask();
        TaskTLSData *const tls = new TaskTLSData{};
        tls->tls_block = alloc_tls_block();
        main_task->SetUserData(tls, TASK_USER_DATA_LIBCXX);
#ifdef __aarch64__
        asm volatile("msr tpidr_el0, %0" : : "r"(tls->tls_block));
#else
        asm volatile("mcr p15, 0, %0, c13, c0, 2" : : "r"(tls->tls_block));
        asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(tls->tls_block));
#endif
    }

    JoinHandle *const h = new JoinHandle{nullptr, 0, false, false};
    CLibCXXTask *const task = new CLibCXXTask(__func, __arg, h);
    h->task = task;
    h->id = reinterpret_cast<uintptr_t>(task);
    __t->__opaque = h;
    return 0;
}

__libcpp_thread_id __libcpp_thread_get_current_id()
{
    return reinterpret_cast<uintptr_t>(CScheduler::Get()->GetCurrentTask());
}

__libcpp_thread_id __libcpp_thread_get_id(__libcpp_thread_t const *__t)
{
    JoinHandle const *const h = static_cast<JoinHandle const *>(__t->__opaque);
    return h->id;
}

int __libcpp_thread_join(__libcpp_thread_t *__t)
{
    JoinHandle *const h = static_cast<JoinHandle *>(__t->__opaque);
    if (h->task == nullptr)
    {
        delete h;
    }
    else
    {
        h->joined = true;
        h->task->WaitForTermination();
    }
    __t->__opaque = nullptr;
    return 0;
}

int __libcpp_thread_detach(__libcpp_thread_t *__t)
{
    JoinHandle *const h = static_cast<JoinHandle *>(__t->__opaque);
    h->detached = true;
    if (h->task == nullptr)
        delete h;
    // else ~CLibCXXTask() will delete h when the task eventually terminates
    __t->__opaque = nullptr;
    return 0;
}

void __libcpp_thread_yield()
{
    CScheduler::Get()->Yield();
}

void __libcpp_thread_sleep_for(chrono::nanoseconds const &__ns)
{
    long long const us = __ns.count() / 1000LL;
    if (us > 0)
        CScheduler::Get()->usSleep(static_cast<unsigned>(us));
    else
        CScheduler::Get()->Yield();
}

// ---------------------------------------------------------------------------
// Execute once
//
// Three-state flag: 0 = not started, 1 = in progress, 2 = done.
//
// If __init_routine() yields internally (e.g. via memory allocation or any
// Circle API that calls Yield()), concurrent callers spin-yield in the inner
// loop until the flag leaves state 1.
//
// If __init_routine() throws (exceptional call per the C++ spec), the flag is
// reset to 0 and the exception propagates to the caller.  The outer loop
// ensures that any task that exits the inner spin because the flag dropped
// back to 0 retries from the top rather than returning success with an
// uninitialised state.
// ---------------------------------------------------------------------------

int __libcpp_execute_once(__libcpp_exec_once_flag *__flag,
                          void (*__init_routine)())
{
    while (true)
    {
        if (*__flag == 2)
            return 0;

        if (*__flag == 0)
        {
            *__flag = 1;
            try
            {
                __init_routine();
            }
            catch (...)
            {
                *__flag = 0; // allow a subsequent call to retry
                throw;
            }
            *__flag = 2;
            return 0;
        }

        // *__flag == 1: another task is initialising; spin-yield until it
        // either succeeds (flag → 2) or throws (flag → 0).
        while (*__flag == 1)
            CScheduler::Get()->Yield();
    }
}

// ---------------------------------------------------------------------------
// Thread-local storage (key-value layer used by libc++ internals)
//
// Each task stores a TaskTLSData in TASK_USER_DATA_LIBCXX; the kv[] array
// within it maps key index → per-task value pointer.
// Keys are assigned with a monotonic counter; destructors are in s_destructors.
// ---------------------------------------------------------------------------

int __libcpp_tls_create(__libcpp_tls_key *__key, void (*__at_exit)(void *))
{
    unsigned const k = s_next_key++;
    s_destructors[k] = __at_exit;
    *__key = k;
    return 0;
}

void *__libcpp_tls_get(__libcpp_tls_key __key)
{
    TaskTLSData const *const tls = static_cast<TaskTLSData const *>(
        CScheduler::Get()->GetCurrentTask()->GetUserData(TASK_USER_DATA_LIBCXX));
    return tls ? tls->kv[__key] : nullptr;
}

int __libcpp_tls_set(__libcpp_tls_key __key, void *__p)
{
    TaskTLSData *const tls = static_cast<TaskTLSData *>(
        CScheduler::Get()->GetCurrentTask()->GetUserData(TASK_USER_DATA_LIBCXX));
    if (tls)
    {
        tls->kv[__key] = __p;
    }
    return 0;
}

_LIBCPP_END_NAMESPACE_STD

// ---------------------------------------------------------------------------
// __cxa_thread_atexit
//
// Required by the compiler to register destructors for thread_local variables.
// Uses the TLS mechanism above to store a linked list of destructors per thread.
// Nodes are prepended so the list is processed LIFO (reverse construction
// order), as required by the C++ standard.
// ---------------------------------------------------------------------------

struct CXAThreadAtexitNode
{
    void (*dtor)(void *);
    void *obj;
    void *dso_symbol;
    CXAThreadAtexitNode *next;
};

static std::__libcpp_tls_key s_cxa_thread_atexit_key;
static std::__libcpp_exec_once_flag s_cxa_thread_atexit_flag = _LIBCPP_EXEC_ONCE_INITIALIZER;

static void cxa_thread_atexit_dtor(void *arg)
{
    CXAThreadAtexitNode *node = static_cast<CXAThreadAtexitNode *>(arg);
    while (node)
    {
        node->dtor(node->obj);
        CXAThreadAtexitNode *const next = node->next;
        delete node;
        node = next;
    }
}

static void cxa_thread_atexit_init()
{
    std::__libcpp_tls_create(&s_cxa_thread_atexit_key, cxa_thread_atexit_dtor);
}

extern "C" int __cxa_thread_atexit(void (*dtor)(void *), void *obj, void *dso_symbol)
{
    std::__libcpp_execute_once(&s_cxa_thread_atexit_flag, cxa_thread_atexit_init);

    CXAThreadAtexitNode *const node =
        new (std::nothrow) CXAThreadAtexitNode{dtor, obj, dso_symbol, nullptr};
    if (!node)
    {
        return -1;
    }

    CXAThreadAtexitNode *const head =
        static_cast<CXAThreadAtexitNode *>(
            std::__libcpp_tls_get(s_cxa_thread_atexit_key));
    node->next = head;
    std::__libcpp_tls_set(s_cxa_thread_atexit_key, node);

    return 0;
}
