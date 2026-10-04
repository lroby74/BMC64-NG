#include "network_time_sync.h"

#include <circle/logger.h>
#include <circle/net/dnsclient.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/ntpclient.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>

#include <reent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

namespace {

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
      CIPAddress server;
      CDNSClient dns(mNetwork);
      if (!dns.Resolve("pool.ntp.org", &server)) {
        CLogger::Get()->Write("ntp", LogWarning,
                              "Cannot resolve NTP server: pool.ntp.org");
        CScheduler::Get()->Sleep(300);
        continue;
      }

      CNTPClient client(mNetwork);
      unsigned timestamp = client.GetTime(server);
      if (timestamp == 0) {
        CLogger::Get()->Write("ntp", LogWarning,
                              "Cannot get time from NTP server: pool.ntp.org");
        CScheduler::Get()->Sleep(300);
        continue;
      }

      if (OraDallaRete(timestamp)) {
        CLogger::Get()->Write("ntp", LogNotice,
                              "System time set from NTP: %u UTC", timestamp);
      } else {
        CLogger::Get()->Write("ntp", LogWarning,
                              "Cannot set system time from NTP");
      }
      CScheduler::Get()->Sleep(900);
    }
  }

private:
  CNetSubSystem *mNetwork;
};

}  // namespace

void StartNetworkTimeSync(CNetSubSystem *network) {
  new NetworkTimeSyncTask(network);
}