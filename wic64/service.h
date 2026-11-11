#ifndef BMX_WIC64_SERVICE_H
#define BMX_WIC64_SERVICE_H
#include "wic64/transport.h"
#include "wic64/config.h"
#if defined(RASPI_COMPILE)
#include <circle/spinlock.h>
#else
#include <mutex>
#endif
namespace bmx { namespace wic64 {
struct Request {
    bmx_wic64_token token;
    uint8_t command, protocol;
    const uint8_t *payload;
    size_t length;
    unsigned timeout;
    char server[BMX_WIC64_SERVER_SIZE];
    bmx_wic64_reporting reporting;
};

class Backend {
public:
    virtual ~Backend() {}
    virtual int Process(const Request &request, bool first, bool network_ready) = 0;
    virtual void Cancel() = 0;
    virtual void Reset() = 0;
    virtual void Release() = 0;
    virtual void Tick() = 0;
    virtual bmx_wic64_reply Reply() const = 0;
    virtual bmx_wic64_stats Stats() const { return {}; }
};

class Service {
public:
    typedef void *(*Allocate)(size_t);
    typedef void (*Free)(void *);
    Service(Allocate allocate, Free free, Backend *backend = nullptr);
    ~Service();
    int Submit(uint8_t command, uint8_t protocol, const void *payload,
               size_t length, unsigned timeout, bmx_wic64_token *token,
               const char *server = nullptr, const bmx_wic64_reporting *reporting = nullptr);
    int Poll(bmx_wic64_token token, bmx_wic64_reply *reply = nullptr);
    void Cancel(bmx_wic64_token token);
    void Release(bmx_wic64_token token);
    void Reset(bool enabled);
    bool Take(Request *request);
    void Complete(bmx_wic64_token token, int result);
    void Step(bool network_ready);
    void RecordStep(uint64_t microseconds);
    void ReadStats(bmx_wic64_stats *stats);
private:
    Service(const Service &) = delete;
    Service &operator=(const Service &) = delete;
    void Lock();
    void Unlock();
    bool Matches(bmx_wic64_token token) const;
    uint8_t *Clear();
#if defined(RASPI_COMPILE)
    CSpinLock lock_;
#else
    std::mutex lock_;
#endif
    enum State { Idle, Queued, Running, Done, Abandoned } state_;
    bool enabled_;
    uint64_t device_, connection_, sequence_;
    Request request_;
    int result_;
    Allocate allocate_;
    Free free_;
    Backend *backend_;
    Request running_;
    bmx_wic64_reply reply_;
    bool processing_, published_;
    uint64_t owner_device_;
    bmx_wic64_stats stats_{};
};

void InstallService(Service *service);
} }
#endif
