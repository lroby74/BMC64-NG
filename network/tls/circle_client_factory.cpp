#include <assert.h>
#include <circle-mbedtls/tlssimplesupport.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>

#include "client_stream.h"
#include "network/io/circle_tcp_stream.h"
namespace bmx {
namespace network {
namespace tls {
namespace {
unsigned active_streams = 0;
class ManagedClientStream : public ClientStream {
   public:
    ManagedClientStream(io::TcpStream* tcp, CircleMbedTLS::CTLSSimpleSupport* support)
        : ClientStream(tcp, support) {
        ++active_streams;
    }
    ~ManagedClientStream() override { --active_streams; }
};
}
ClientStream* CreateNetworkTlsStream(CNetSubSystem* network) {
#ifndef NDEBUG
    const bool scheduler_owner = CScheduler::IsActive() && CScheduler::IsOwnerCore();
    assert(scheduler_owner);
#endif
    if (!network || !network->IsRunning() || active_streams >= kMaximumClientStreams) return 0;


    static CircleMbedTLS::CTLSSimpleSupport support(network);
    if (support.GetNet() != network) return 0;
    io::TcpStream* tcp = new io::CircleTcpStream(network);
    if (!tcp) return 0;
    ClientStream* stream = new ManagedClientStream(tcp, &support);
    if (!stream) delete tcp;
    return stream;
}
uint64_t NetworkUtcSeconds(void*) {
    return CTimer::Get() ? CTimer::Get()->GetUniversalTime() : 0;
}
}
}
}
