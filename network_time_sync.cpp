#include "network_time_sync.h"

#include <circle/logger.h>
#include <circle/net/dnsclient.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/ntpclient.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/synchronizationevent.h>
#include <circle/timer.h>

#include <reent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern "C" {
#include "third_party/common/circle.h"
}

bool ConfigureSystemTimeZone(int offset_minutes) {
  int absolute_offset = offset_minutes;
  if (absolute_offset < 0) {
    absolute_offset = -absolute_offset;
  }

  char timezone[16];
  snprintf(timezone, sizeof(timezone), "UTC%c%d:%02d",
           offset_minutes > 0 ? '-' : '+', absolute_offset / 60,
           absolute_offset % 60);
  if (_setenv_r(_REENT, "TZ", timezone, 1) != 0) {
    return false;
  }
  _tzset_r(_REENT);
  return CTimer::Get()->SetTimeZone(offset_minutes);
}




static int base_offset_minutes;
static int dst_mode;

static long GiorniDal1970(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

static long UltimaDomenica(int anno, unsigned mese) {
  long giorni = GiorniDal1970(anno, mese, 31);
  return giorni - ((giorni % 7 + 7 + 4) % 7);
}

static bool OraLegaleEuropea(unsigned utc) {
  if (utc < 1000000000u) {
    return false;
  }
  time_t t = utc;
  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);
  int anno = tm_utc.tm_year + 1900;
  long inizio = UltimaDomenica(anno, 3) * 86400L + 3600;
  long fine = UltimaDomenica(anno, 10) * 86400L + 3600;
  return (long)utc >= inizio && (long)utc < fine;
}









static bool ApplicaOraLegaleA(unsigned utc) {
  int offset = base_offset_minutes;
  if (dst_mode == 1 || (dst_mode == 2 && OraLegaleEuropea(utc))) {
    offset += 60;
  }
  if (CTimer::Get()->GetTimeZone() == offset) {
    return true;
  }
  return ConfigureSystemTimeZone(offset);
}

static bool ApplicaOraLegale(void) {
  return ApplicaOraLegaleA(CTimer::Get()->GetUniversalTime());
}

static bool OraDallaRete(unsigned utc) {
  ApplicaOraLegaleA(utc);
  return CTimer::Get()->SetTime(utc, FALSE);
}

bool ConfigureDaylightSaving(int offset_minutes, int mode) {
  base_offset_minutes = offset_minutes;
  dst_mode = mode;
  return ConfigureSystemTimeZone(offset_minutes) && ApplicaOraLegale();
}







static volatile int s_ntp_acceso = 1;
static char s_ntp_server[128] = "pool.ntp.org";
static char s_ntp_riserva[128] = "";
static volatile int s_ntp_minuti = 15;
static char s_ntp_dettaglio[96] = "";
static volatile unsigned s_ntp_ultimo_utc = 0;
static volatile unsigned s_ntp_ultimo_locale = 0;



static bool OraArrivata(unsigned timestamp, const char *nome) {
  if (timestamp == 0) {
    snprintf(s_ntp_dettaglio, sizeof(s_ntp_dettaglio), "No answer from %s",
             nome);
    CLogger::Get()->Write("ntp", LogWarning,
                          "Cannot get time from NTP server: %s", nome);
    return false;
  }
  if (!OraDallaRete(timestamp)) {
    snprintf(s_ntp_dettaglio, sizeof(s_ntp_dettaglio), "Cannot set the clock");
    CLogger::Get()->Write("ntp", LogWarning, "Cannot set system time from NTP");
    return false;
  }
  s_ntp_ultimo_utc = timestamp;
  s_ntp_ultimo_locale = CTimer::Get()->GetLocalTime();
  snprintf(s_ntp_dettaglio, sizeof(s_ntp_dettaglio), "%s", nome);
  CLogger::Get()->Write("ntp", LogNotice, "System time set from NTP: %u UTC (%s)",
                        timestamp, nome);
  return true;
}

namespace {

CSynchronizationEvent s_ntp_sveglia;
volatile int s_ntp_stato = CIRCLE_NTP_WAITING_NETWORK;


bool ProvaServer(CNetSubSystem *network, const char *nome) {
  CIPAddress server;
  CDNSClient dns(network);
  if (!dns.Resolve(nome, &server)) {
    snprintf(s_ntp_dettaglio, sizeof(s_ntp_dettaglio), "Cannot resolve %s",
             nome);
    CLogger::Get()->Write("ntp", LogWarning, "Cannot resolve NTP server: %s",
                          nome);
    return false;
  }
  CNTPClient client(network);
  return OraArrivata(client.GetTime(server), nome);
}



void Aspetta(unsigned secondi) {
  if (secondi == 0) {
    s_ntp_sveglia.Wait();
  } else {
    while (secondi > 0 && !s_ntp_sveglia.GetState()) {
      const unsigned pezzo = secondi > 3600U ? 3600U : secondi;
      s_ntp_sveglia.WaitWithTimeout(pezzo * 1000000U);
      secondi -= pezzo;
    }
  }
  s_ntp_sveglia.Clear();
}

class NetworkTimeSyncTask : public CTask {
public:
  explicit NetworkTimeSyncTask(CNetSubSystem *network) : mNetwork(network) {
    SetName("ntpwait");
  }

  void Run(void) override {
    while (!mNetwork->IsRunning()) {
      CScheduler::Get()->Sleep(1);
    }

    for (;;) {
      s_ntp_sveglia.Clear();
      if (!s_ntp_acceso) {
        s_ntp_stato = CIRCLE_NTP_OFF;
        Aspetta(0);
        continue;
      }
      s_ntp_stato = CIRCLE_NTP_SYNCING;
      char server[sizeof(s_ntp_server)], riserva[sizeof(s_ntp_riserva)];
      snprintf(server, sizeof(server), "%s", s_ntp_server);
      snprintf(riserva, sizeof(riserva), "%s", s_ntp_riserva);
      const bool ok = (server[0] != '\0' && ProvaServer(mNetwork, server)) ||
                      (riserva[0] != '\0' && ProvaServer(mNetwork, riserva));
      s_ntp_stato = ok ? CIRCLE_NTP_SYNCED : CIRCLE_NTP_FAILED;


      Aspetta(ok ? (unsigned)s_ntp_minuti * 60U : 300U);
    }
  }

private:
  CNetSubSystem *mNetwork;
};

}  // namespace

void StartNetworkTimeSync(CNetSubSystem *network) {
  new NetworkTimeSyncTask(network);
}


extern "C" void circle_ntp_imposta(int acceso, const char *server,
                                   const char *riserva, int minuti) {
  s_ntp_acceso = acceso ? 1 : 0;
  snprintf(s_ntp_server, sizeof(s_ntp_server), "%s", server ? server : "");
  snprintf(s_ntp_riserva, sizeof(s_ntp_riserva), "%s", riserva ? riserva : "");
  s_ntp_minuti = minuti == 0 || minuti == 15 || minuti == 360 ||
                         minuti == 720 || minuti == 1440
                     ? minuti
                     : 15;
  if (!s_ntp_acceso) {
    s_ntp_stato = CIRCLE_NTP_OFF;
  } else if (s_ntp_stato == CIRCLE_NTP_OFF) {
    s_ntp_stato = CIRCLE_NTP_WAITING_NETWORK;
  }
  printf("ntp: settings: %s server=%s fallback=%s interval=%d min\n",
         s_ntp_acceso ? "on" : "off", s_ntp_server, s_ntp_riserva,
         s_ntp_minuti);
  s_ntp_sveglia.Set();
}

extern "C" void circle_ntp_adesso(void) {
  printf("ntp: sync now\n");
  s_ntp_sveglia.Set();
}

extern "C" int circle_ntp_stato(char *dettaglio, int n,
                                unsigned *ultimo_locale) {
  if (dettaglio != nullptr && n > 0) {
    snprintf(dettaglio, (size_t)n, "%s", s_ntp_dettaglio);
  }
  if (ultimo_locale != nullptr) {
    *ultimo_locale = s_ntp_ultimo_locale;
  }
  return s_ntp_stato;
}