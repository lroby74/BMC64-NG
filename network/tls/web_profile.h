#ifndef BMX_NETWORK_TLS_WEB_PROFILE_H
#define BMX_NETWORK_TLS_WEB_PROFILE_H
#include "client_stream.h"
namespace bmx { namespace network { namespace tls {


class LowerBoundClock {
public:
    using Read = uint64_t (*)(void *);
    LowerBoundClock(uint64_t epoch, Read monotonic_us, Read trusted_utc, void *context);
    uint64_t Now();
    static uint64_t Seconds(void *clock);
private:
    uint64_t epoch_, start_, last_;
    Read monotonic_, trusted_utc_;
    void *context_;
};
uint64_t WebCaEpoch();


ClientOptions WebClientOptions(uint64_t (*utc)(void *), void *context,
                               CertificateTimePolicy policy);
} } }
#endif
