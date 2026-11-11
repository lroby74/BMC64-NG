



















#include "wic64/engine.h"
#include "wic64/persistence.h"
#include "network/http/circle_connection.h"
#include "network/io/tcp_stream.h"
#include "network/tls/web_profile.h"
#include "platform/hardware_identity.h"
#include "vice_network.h"
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <circle/spinlock.h>
#include <circle/string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "third_party/common/circle.h"
}

namespace bmx {
CNetSubSystem *GetActiveNetworkSubsystem() { return ViceNetworkGetSubsystem(); }
}

namespace {
using namespace bmx;



CSpinLock lucchetto;
char ssid_del_menu[33];
wic64::Identity identita_da_scrivere;
bool identita_cambiata = false;

bool RetePronta(CNetSubSystem *net) {
    return net && net->IsRunning() && !net->GetConfig()->GetIPAddress()->IsNull();
}

class Wic64Task : public CTask, private wic64::Platform {
public:
    Wic64Task()
        : CTask(128U * 1024U),
          web_clock_(network::tls::WebCaEpoch(), Monotonic, network::tls::NetworkUtcSeconds, nullptr),
          engine_(*this, InitialIdentity()),
          service_(malloc, free, &engine_) {
        SetName("wic64-transport");
        wic64::InstallService(&service_);
    }
    void Run(void) override {
        for (;;) {
            const uint64_t start = Now();
            service_.Step(RetePronta(GetActiveNetworkSubsystem()));
            service_.RecordStep(Now() - start);
            CScheduler::Get()->MsSleep(1U);
        }
    }
private:
    static wic64::Identity InitialIdentity() {
        wic64::Identity identity{};
        char serial[17];

        if (!platform::ReadBoardSerial(serial)) strcpy(serial, "0000000000000000");
        wic64::DeriveMac(serial, identity.mac);
        snprintf(identity.token, sizeof identity.token, "%s", "0123456789ab");
        wic64::LoadIdentity(identity);
        return identity;
    }
    uint64_t Now() override { return network::io::NetworkMonotonicMicroseconds(); }
    static uint64_t Monotonic(void *) { return network::io::NetworkMonotonicMicroseconds(); }
    network::http::Connection *Http() override {


        const auto options = network::tls::WebClientOptions(
            network::tls::LowerBoundClock::Seconds, &web_clock_,
            network::tls::CertificateTimePolicy::AllowNotYetValid);
        return new network::http::CircleConnection(GetActiveNetworkSubsystem(), &options);
    }
    network::io::TcpStream *Tcp() override { return network::io::CreateNetworkTcpStream(); }
    wic64::NetworkSnapshot Snapshot() override {
        wic64::NetworkSnapshot result;
        CNetSubSystem *net = GetActiveNetworkSubsystem();
        const int stato = circle_get_network_status();
        result.configured = net != nullptr;
        result.ready = RetePronta(net);
        if (result.ready) {
            CString ip;
            net->GetConfig()->GetIPAddress()->Format(&ip);
            snprintf(result.ip, sizeof result.ip, "%s", static_cast<const char *>(ip));
        }
        result.wifi = stato >= CIRCLE_NETWORK_WIFI_CONFIG_MISSING && stato < CIRCLE_NETWORK_STATUS_COUNT;

        if (result.ready && stato == CIRCLE_NETWORK_WIFI_CONNECTED) {
            lucchetto.Acquire();
            snprintf(result.ssid, sizeof result.ssid, "%s", ssid_del_menu);
            lucchetto.Release();
        }
        return result;
    }
    bool SaveIdentity(const wic64::Identity &identity) override {
        lucchetto.Acquire();
        identita_da_scrivere = identity;
        identita_cambiata = true;
        lucchetto.Release();
        return true;
    }
    network::tls::LowerBoundClock web_clock_;
    wic64::Engine engine_;
    wic64::Service service_;
};
}


extern "C" void bmc_wic64_avvia(void) {
    static Wic64Task *task = nullptr;
    if (!task) task = new Wic64Task();
}


extern "C" void bmc_wic64_ssid(const char *ssid) {
    lucchetto.Acquire();
    snprintf(ssid_del_menu, sizeof ssid_del_menu, "%s", ssid ? ssid : "");
    lucchetto.Release();
}




extern "C" void bmc_wic64_menu_aperto(void) {
    wic64::Identity identita;
    lucchetto.Acquire();
    const bool scrivere = identita_cambiata;
    identita = identita_da_scrivere;
    identita_cambiata = false;
    lucchetto.Release();
    if (!scrivere || wic64::PersistIdentity(identita)) return;
    lucchetto.Acquire();
    if (!identita_cambiata) {
        identita_da_scrivere = identita;
        identita_cambiata = true;
    }
    lucchetto.Release();
}
