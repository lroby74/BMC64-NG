#include "web_profile.h"
namespace bmx { namespace network { namespace tls {
namespace {
#include "web_ca_bundle.inc"
const Certificate authorities[] = {
    {reinterpret_cast<const uint8_t *>(kWebCaPem), sizeof kWebCaPem}
};
}
LowerBoundClock::LowerBoundClock(uint64_t epoch, Read monotonic_us, Read trusted_utc, void *context)
    : epoch_(epoch), start_(monotonic_us ? monotonic_us(context) : 0), last_(epoch),
      monotonic_(monotonic_us), trusted_utc_(trusted_utc), context_(context) {}
uint64_t LowerBoundClock::Now() {
    if (!epoch_ || !monotonic_) return 0;
    const uint64_t ticks = monotonic_(context_);
    if (ticks < start_) return 0;
    const uint64_t elapsed = (ticks - start_) / 1000000;
    if (elapsed > UINT64_MAX - epoch_) return 0;
    uint64_t value = epoch_ + elapsed;
    const uint64_t trusted = trusted_utc_ ? trusted_utc_(context_) : 0;
    if (trusted > value) { value = trusted; epoch_ = trusted; start_ = ticks; }
    if (value > last_) last_ = value;
    return last_;
}
uint64_t LowerBoundClock::Seconds(void *clock) {
    return clock ? static_cast<LowerBoundClock *>(clock)->Now() : 0;
}
uint64_t WebCaEpoch() { return kWebCaEpoch; }
ClientOptions WebClientOptions(uint64_t (*utc)(void *), void *context,
                               CertificateTimePolicy policy) {
    return {authorities, 1, utc, context, kWebCaEpoch, policy};
}
} } }
