#include "circle_tcp_stream.h"
#include <circle/timer.h>
namespace bmx {
CNetSubSystem *GetActiveNetworkSubsystem();
namespace network { namespace io {
TcpStream *CreateNetworkTcpStream() {
    return new CircleTcpStream(GetActiveNetworkSubsystem());
}
uint64_t NetworkMonotonicMicroseconds() { return CTimer::GetClockTicks64(); }
}}}
