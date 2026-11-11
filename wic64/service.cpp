#include "wic64/service.h"
#include <string.h>
namespace bmx { namespace wic64 {
namespace { Service *active; }
void InstallService(Service *service) { active = service; }
Service::Service(Allocate allocate, Free free, Backend *backend)
    : lock_(), state_(Idle), enabled_(false), device_(1), connection_(1),
      sequence_(0), request_(), result_(BMX_WIC64_PENDING),
      allocate_(allocate), free_(free), backend_(backend), running_(), reply_(),
      processing_(false), published_(false), owner_device_(1) {}
Service::~Service() { free_(const_cast<uint8_t *>(request_.payload)); }
void Service::Lock() {
#ifdef RASPI_COMPILE
    lock_.Acquire();
#else
    lock_.lock();
#endif
}
void Service::Unlock() {
#ifdef RASPI_COMPILE
    lock_.Release();
#else
    lock_.unlock();
#endif
}
bool Service::Matches(bmx_wic64_token t) const {
    return state_ != Idle && t.device == device_ &&
           t.connection == connection_ && t.request == request_.token.request;
}
uint8_t *Service::Clear() {
    uint8_t *p = const_cast<uint8_t *>(request_.payload);
    request_ = Request();
    state_ = Idle;
    return p;
}
int Service::Submit(uint8_t command, uint8_t protocol, const void *payload,
                    size_t length, unsigned timeout, bmx_wic64_token *token,
                    const char *server, const bmx_wic64_reporting *reporting) {
    if (!token || length > BMX_WIC64_MAX_REQUEST || (length && !payload) ||
        timeout < 1 || timeout > 255) return BMX_WIC64_INVALID;
    if (reporting && !bmx_wic64_reporting_valid(reporting)) return BMX_WIC64_INVALID;
    if (server && !bmx_wic64_server_valid(server)) return BMX_WIC64_INVALID;
    Lock();
    const uint64_t generation = device_;
    int status = !enabled_ ? BMX_WIC64_UNAVAILABLE :
                 state_ != Idle ? BMX_WIC64_BUSY : BMX_WIC64_PENDING;
    Unlock();
    if (status != BMX_WIC64_PENDING) return status;
    uint8_t *copy = length ? static_cast<uint8_t *>(allocate_(length)) : nullptr;
    if (length && !copy) return BMX_WIC64_NO_MEMORY;
    if (length) memcpy(copy, payload, length);
    Lock();
    if (!enabled_ || device_ != generation) status = BMX_WIC64_STALE;
    else if (state_ != Idle) status = BMX_WIC64_BUSY;
    else {

        if (command == 0x21 || command == 0x2e) ++connection_;
        request_ = Request{{device_, connection_, ++sequence_}, command,
                           protocol, copy, length, timeout, {0}, BMX_WIC64_REPORTING_DEFAULT};
        if (reporting) request_.reporting = *reporting;
        strcpy(request_.server, server ? server : "http://x.wic64.net/");
        *token = request_.token;
        state_ = Queued;
    }
    Unlock();
    if (status != BMX_WIC64_PENDING) free_(copy);
    return status;
}
int Service::Poll(bmx_wic64_token token, bmx_wic64_reply *reply) {
    Lock();
    int result = !Matches(token) ? BMX_WIC64_STALE :
                 state_ == Done ? result_ : BMX_WIC64_PENDING;
    if (reply && result == BMX_WIC64_REPLY) *reply = reply_;
    Unlock();
    return result;
}
void Service::Reset(bool enabled) {
    Lock();
    ++device_; ++connection_;
    enabled_ = enabled;
    uint8_t *retired = nullptr;
    if (state_ == Running || state_ == Abandoned) state_ = Abandoned;
    else retired = Clear();
    Unlock();
    free_(retired);
}
void Service::Cancel(bmx_wic64_token token) {
    Lock();
    uint8_t *retired = nullptr;
    if (Matches(token)) {
        ++connection_;
        if (state_ == Running) state_ = Abandoned;
        else retired = Clear();
    }
    Unlock();
    free_(retired);
}
void Service::Release(bmx_wic64_token token) {
    Lock();
    uint8_t *retired = Matches(token) && state_ == Done ? Clear() : nullptr;
    Unlock();
    free_(retired);
}
bool Service::Take(Request *request) {
    if (!request) return false;
    Lock();
    bool available = state_ == Queued;
    if (available) { *request = request_; state_ = Running; }
    Unlock();
    return available;
}
void Service::Complete(bmx_wic64_token token, int result) {
    Lock();
    uint8_t *retired = nullptr;
    if (token.request == request_.token.request) {
        if (state_ == Abandoned) retired = Clear();
        else if (state_ == Running && Matches(token)) {
            result_ = result;
            state_ = Done;
        }
    }
    Unlock();
    free_(retired);
}
void Service::Step(bool network_ready) {
    if (backend_) {
        Lock();
        const uint64_t device = device_;
        const State state = state_;
        Unlock();
        if (processing_ && (state == Abandoned || device != owner_device_)) {
            backend_->Cancel();
            Complete(running_.token, BMX_WIC64_STALE);
            processing_ = false;
        }
        if (published_ && (state != Done || device != owner_device_)) {
            backend_->Release(); published_ = false;
        }
        if (device != owner_device_) {
            backend_->Reset(); owner_device_ = device;
        }
        backend_->Tick();
        if (published_) return;
        bool first = false;
        if (!processing_) {
            if (!Take(&running_)) return;



            if (running_.token.device != owner_device_) {
                backend_->Reset(); owner_device_ = running_.token.device;
            }
            processing_ = first = true;
        }
        const int result = backend_->Process(running_, first, network_ready);
        if (result != BMX_WIC64_PENDING) {

            const bmx_wic64_reply reply = backend_->Reply();
            Lock(); reply_ = reply; Unlock();
            Complete(running_.token, result);
            processing_ = false; published_ = true;
        }
        return;
    }
    Request request;
    if (!Take(&request)) return;

    Complete(request.token, network_ready ? BMX_WIC64_UNSUPPORTED :
                                          BMX_WIC64_UNAVAILABLE);
}
void Service::RecordStep(uint64_t us) {
    bmx_wic64_stats next = backend_ ? backend_->Stats() : bmx_wic64_stats{};
    Lock();
    next.step_max_us = us > stats_.step_max_us ? us : stats_.step_max_us;
    stats_ = next;
    Unlock();
}
void Service::ReadStats(bmx_wic64_stats *stats) {
    if (!stats) return;
    Lock(); *stats = stats_;
    stats->request_bytes = request_.length;
    stats->resets = device_ - 1;
    Unlock();
}
} }
using namespace bmx::wic64;
extern "C" {
void bmx_wic64_read_stats(bmx_wic64_stats *stats) {
    if (!stats) return;
    if (active) active->ReadStats(stats); else memset(stats, 0, sizeof *stats);
}
int bmx_wic64_submit(uint8_t c, uint8_t p, const void *data, size_t n,
                     unsigned timeout, bmx_wic64_token *token) {
    return active ? active->Submit(c, p, data, n, timeout, token) :
                    BMX_WIC64_UNAVAILABLE;
}
int bmx_wic64_poll(bmx_wic64_token token) {
    return active ? active->Poll(token) : BMX_WIC64_STALE;
}
int bmx_wic64_submit_with_server(uint8_t c, uint8_t p, const void *data,
    size_t n, unsigned timeout, const char *server, const bmx_wic64_reporting *reporting, bmx_wic64_token *token) {
    return active ? active->Submit(c, p, data, n, timeout, token, server, reporting) : BMX_WIC64_UNAVAILABLE;
}
int bmx_wic64_poll_reply(bmx_wic64_token token, bmx_wic64_reply *reply) {
    return active ? active->Poll(token, reply) : BMX_WIC64_STALE;
}
void bmx_wic64_cancel(bmx_wic64_token token) { if (active) active->Cancel(token); }
void bmx_wic64_release(bmx_wic64_token token) { if (active) active->Release(token); }
void bmx_wic64_reset(int enabled) { if (active) active->Reset(enabled != 0); }
}
