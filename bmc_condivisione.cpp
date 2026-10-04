













#include <circle/logger.h>
#include <circle/macaddress.h>
#include <circle/netdevice.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <circle/timer.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "vice_network.h"

#include "third_party/circle-stdlib/include/wrap_fatfs.h"
#include "third_party/circle-stdlib/libs/circle-newlib/libgloss/circle/cglueio.h"
#include "third_party/circle-stdlib/libs/circle-newlib/libgloss/circle/filetable.h"

extern "C" {
#include "smb2.h"
#include "libsmb2.h"
#include "bmc_smb.h"
#include "bmc_sock.h"
#include "bmc_nomi.h"
#include "bmc_wsd.h"
#include "bmc_nas.h"
int circle_get_network_ip_address(char *address, unsigned int address_size);
int bmc_file_in_uso(const char *path, int is_dir);
}

volatile int bmc_condivisione_attiva;

extern const char *volatile bmc_smb_fase;

static struct smb2_server server_smb;
static volatile int compito_vivo;
static char utente_smb[32];
static char password_smb[64];
static unsigned char ip_nostro[4];
static unsigned char mac_nostro[6];





static void aggiungi_nomi(struct smb2_server *server, fd_set *rfds,
                          fd_set *wfds, int *maxfd) {
  (void)server;
  (void)wfds;
  bmc_nomi_fdset(rfds, maxfd);
  bmc_wsd_fdset(rfds, maxfd);
}

static void servi_nomi(struct smb2_server *server, fd_set *rfds,
                       fd_set *wfds) {
  (void)server;
  (void)wfds;
  bmc_nomi_servi(rfds);
  bmc_wsd_servi(rfds);
}

static void scrivi_log(const char *testo) {
  CLogger::Get()->Write("smb", LogNotice, "%s", testo);
}



static int fuso_adesso(void) {
  return CTimer::Get()->GetTimeZone();
}

namespace {





#define PILA_CONDIVISIONE 0x40000

class CCondivisione : public CTask {
public:
  CCondivisione() : CTask(PILA_CONDIVISIONE) { SetName("smb"); }

  void Run(void) override {
    bmc_smb_fase = "i nomi (NBNS, LLMNR, mDNS)";
    bmc_nomi_apri(ip_nostro, "BMC64-NG", scrivi_log);
    bmc_smb_fase = "WS-Discovery (3702, 5357)";
    bmc_wsd_apri(ip_nostro, mac_nostro, "BMC64-NG", scrivi_log);
    bmc_smb_fase = "il server in ascolto (445)";
    int err = smb2_serve_port(&server_smb, 4, bmc_smb_new_client, NULL);
    bmc_smb_fase = "il server si chiude";
    bmc_wsd_chiudi();
    bmc_nomi_chiudi();
    bmc_smb_close_all();
    bmc_sock_reset_all();
    CLogger::Get()->Write("smb", LogNotice, "server fermato (%d)", err);
    bmc_smb_fase = err == 0 ? "fermato" : "fermato con errore";
    compito_vivo = 0;
  }
};

}

extern "C" int circle_smb_start(const char *user, const char *password) {
  if (compito_vivo) {
    return 1;
  }


  if (ViceNetworkGetSubsystem() == nullptr) {
    CLogger::Get()->Write("smb", LogWarning, "nessuna rete: non parte");
    return 0;
  }



  bmc_nas_scollega();
  snprintf(utente_smb, sizeof(utente_smb), "%s", user ? user : "bmc64");
  snprintf(password_smb, sizeof(password_smb), "%s",
           password ? password : "bmc64");

  struct bmc_smb_config cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.volume = "SD:";
  cfg.share = "sdcard";
  cfg.hostname = "BMC64-NG";
  cfg.user = utente_smb;
  cfg.password = password_smb;
  cfg.tz_offset_minutes = CTimer::Get()->GetTimeZone();
  cfg.tz_offset = fuso_adesso;
  cfg.in_use = bmc_file_in_uso;
  cfg.log = scrivi_log;

  char testo[32];
  unsigned a = 0, b = 0, c = 0, d = 0;
  memset(ip_nostro, 0, sizeof(ip_nostro));
  if (circle_get_network_ip_address(testo, sizeof(testo)) &&
      sscanf(testo, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
    ip_nostro[0] = a; ip_nostro[1] = b; ip_nostro[2] = c; ip_nostro[3] = d;
  }

  memset(mac_nostro, 0, sizeof(mac_nostro));
  CNetDevice *scheda = CNetDevice::GetNetDevice(NetDeviceTypeAny);
  if (scheda != nullptr && scheda->GetMACAddress() != nullptr) {
    scheda->GetMACAddress()->CopyTo(mac_nostro);
  }

  memset(&server_smb, 0, sizeof(server_smb));
  bmc_smb_setup(&server_smb, &cfg);
  server_smb.port = 445;
  server_smb.extra_fdset = aggiungi_nomi;
  server_smb.extra_service = servi_nomi;
  compito_vivo = 1;
  new CCondivisione;
  bmc_condivisione_attiva = 1;
  CLogger::Get()->Write("smb", LogNotice, "server avviato, utente %s",
                        utente_smb);
  return 1;
}

extern "C" void circle_smb_stop(void) {
  bmc_condivisione_attiva = 0;
  if (!compito_vivo) {
    return;
  }
  server_smb.stop_requested = 1;
  unsigned inizio = CTimer::GetClockTicks();
  while (compito_vivo && CTimer::GetClockTicks() - inizio < 3000000) {
    CScheduler::Get()->Yield();
  }
}

const char *volatile bmc_smb_fase = nullptr;

extern "C" const char *circle_smb_fase(void) {
  const char *f = bmc_smb_fase;
  return f != nullptr ? f : "spento";
}

extern "C" int circle_smb_active(void) {
  return compito_vivo;
}

extern "C" int circle_smb_open_files(void) {
  return compito_vivo ? bmc_smb_open_count() : 0;
}



extern "C" void circle_smb_giro(unsigned microsecondi) {
  unsigned inizio = CTimer::GetClockTicks();
  do {
    CScheduler::Get()->Yield();
  } while (CTimer::GetClockTicks() - inizio < microsecondi);
}










extern "C" int bmc_sock_chiudi(int fd) {
  using namespace _CircleStdlib;
  {
    FileTable::FileTableLock lucchetto;
    CircleFile *f = FileTable::GetFile(fd);
    if (f == nullptr || !f->IsOpen()) {
      errno = EBADF;
      return -1;
    }
    CGlueIO *g = f->GetGlueIO();
    g->DecrementRefCount();
    if (g->GetRefCount() == 0) {
      f->CloseGlueIO();
    }
  }
  CScheduler::Get()->Yield();
  return 0;
}



extern "C" int getlogin_r(char *buf, size_t bufsize) {
  if (buf == nullptr || bufsize == 0) {
    return -1;
  }
  snprintf(buf, bufsize, "%s", utente_smb[0] ? utente_smb : "bmc64");
  return 0;
}

extern "C" int gethostname(char *name, size_t len) {
  if (name == nullptr || len == 0) {
    return -1;
  }
  snprintf(name, len, "%s", "BMC64-NG");
  return 0;
}
