














#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/macaddress.h>
#include <circle/netdevice.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <circle/timer.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
















extern "C" const char *circle_nome_in_rete(void);

#ifndef SMBLOG_FILE
#define SMBLOG_FILE "/SMB-LOG.TXT"
#endif
#define SMBLOG_RAM 6144
#define SMBLOG_MAX_ERRORI 50
#define SMBLOG_TIENI 32768

static char smblog_ram[SMBLOG_RAM];
static char smblog_copia[SMBLOG_RAM];
static unsigned smblog_usati = 0;
static unsigned smblog_errori = 0;
static unsigned smblog_tagliati = 0;
static int smblog_testa = 0;
static unsigned long smblog_aperta_s = 0;
static char smblog_ultima_emmc[160];

static void smblog_metti(const char *p, unsigned n) {
  if (smblog_usati + n > SMBLOG_RAM) {
    smblog_tagliati++;
    return;
  }
  memcpy(smblog_ram + smblog_usati, p, n);
  smblog_usati += n;
}

static void smblog_riga(const char *testo) {
  smblog_metti(testo, (unsigned)strlen(testo));
  smblog_metti("\n", 1);
}

static const char *smblog_nome_fr(int fr) {
  static const char *const nomi[] = {
      "FR_OK",           "FR_DISK_ERR",          "FR_INT_ERR",
      "FR_NOT_READY",    "FR_NO_FILE",           "FR_NO_PATH",
      "FR_INVALID_NAME", "FR_DENIED",            "FR_EXIST",
      "FR_INVALID_OBJECT", "FR_WRITE_PROTECTED", "FR_INVALID_DRIVE",
      "FR_NOT_ENABLED",  "FR_NO_FILESYSTEM",     "FR_MKFS_ABORTED",
      "FR_TIMEOUT",      "FR_LOCKED",            "FR_NOT_ENOUGH_CORE",
      "FR_TOO_MANY_OPEN_FILES", "FR_INVALID_PARAMETER"};
  if (fr < 0 || fr >= (int)(sizeof(nomi) / sizeof(nomi[0]))) {
    return "codice sconosciuto";
  }
  return nomi[fr];
}

static int smblog_contiene(const char *p, unsigned n, const char *s) {
  const unsigned k = (unsigned)strlen(s);
  for (unsigned i = 0; i + k <= n; i++) {
    if (memcmp(p + i, s, k) == 0) {
      return 1;
    }
  }
  return 0;
}




static void smblog_righe_emmc(int copia) {
  static char reg[LOGGER_BUFSIZE + 1];
  CLogger *log = CLogger::Get();
  if (log == 0) {
    return;
  }
  const int m = log->Read(reg, LOGGER_BUFSIZE, FALSE);
  if (m <= 0) {
    return;
  }
  reg[m] = '\0';
  int w = 0;
  for (int i = 0; i < m; i++) {
    if (reg[i] == 27) {
      while (i < m && reg[i] != 'm') {
        i++;
      }
      continue;
    }
    if (reg[i] == '\n' || (reg[i] >= 32 && reg[i] < 127)) {
      reg[w++] = reg[i];
    }
  }
  reg[w] = '\0';
  char *p = reg;
  if (smblog_ultima_emmc[0] != '\0') {
    char *q = strstr(reg, smblog_ultima_emmc);
    if (q != NULL) {
      char *a_capo = strchr(q, '\n');
      p = a_capo != NULL ? a_capo + 1 : reg + w;
    }
  }
  while (*p != '\0') {
    char *a_capo = strchr(p, '\n');
    const unsigned n = a_capo != NULL ? (unsigned)(a_capo - p) : (unsigned)strlen(p);
    if (n > 0 && (smblog_contiene(p, n, "emmc:") || smblog_contiene(p, n, "sdhost:"))) {
      if (copia) {
        smblog_metti("   ", 3);
        smblog_metti(p, n);
        smblog_metti("\n", 1);
      }
      const unsigned k = n < sizeof(smblog_ultima_emmc) - 1 ? n : sizeof(smblog_ultima_emmc) - 1;
      memcpy(smblog_ultima_emmc, p, k);
      smblog_ultima_emmc[k] = '\0';
    }
    p = a_capo != NULL ? a_capo + 1 : p + n;
  }
}


static void smblog_inizio(void) {
  smblog_errori = 0;
  smblog_testa = 0;
  smblog_aperta_s = (unsigned long)(CTimer::GetClockTicks64() / 1000000U);
  smblog_righe_emmc(0);
}



static void smblog_raccogli(const char *testo) {
  if (strncmp(testo, "read ", 5) != 0 && strncmp(testo, "write ", 6) != 0) {
    return;
  }
  smblog_errori++;
  if (smblog_errori > SMBLOG_MAX_ERRORI) {
    return;
  }
  char riga[800];
  if (!smblog_testa) {
    smblog_testa = 1;
    CMachineInfo *info = CMachineInfo::Get();
    snprintf(riga, sizeof(riga),
             "\n==== condivisione su %s (%s), aperta a %lu s dall'accensione ====",
             circle_nome_in_rete(), info != 0 ? info->GetMachineName() : "?",
             smblog_aperta_s);
    smblog_riga(riga);
  }
  const unsigned long ms = (unsigned long)(CTimer::GetClockTicks64() / 1000U);
  const char *f = strstr(testo, "-> fr ");
  const int fr = f != NULL ? atoi(f + 6) : -1;
  snprintf(riga, sizeof(riga), "[%lu.%03lu s] %s  (%s)", ms / 1000, ms % 1000, testo,
           fr == 0 ? "FR_OK ma meno byte" : smblog_nome_fr(fr));
  smblog_riga(riga);
  smblog_righe_emmc(1);
}

static int smblog_da_scrivere(void) {
  return smblog_usati > 0 || smblog_tagliati > 0;
}


static void smblog_scrivi(int chiusura) {
  const unsigned n = smblog_usati;
  const unsigned tagliati = smblog_tagliati;
  char coda[160];
  coda[0] = '\0';
  if (chiusura && smblog_testa) {
    snprintf(coda, sizeof(coda),
             "---- condivisione chiusa: %u errori di lettura o scrittura%s ----\n",
             smblog_errori, smblog_errori > SMBLOG_MAX_ERRORI ? " (qui i primi 50)" : "");
    smblog_testa = 0;
  }
  if (n == 0 && tagliati == 0 && coda[0] == '\0') {
    return;
  }
  memcpy(smblog_copia, smblog_ram, n);
  smblog_usati = 0;
  smblog_tagliati = 0;
  long dim = 0;
  FILE *fp = fopen(SMBLOG_FILE, "r");
  if (fp != NULL) {
    if (fseek(fp, 0, SEEK_END) == 0) {
      dim = ftell(fp);
    }
    fclose(fp);
  }
  fp = fopen(SMBLOG_FILE, dim > SMBLOG_TIENI ? "w" : "a");
  if (fp == NULL) {
    return;
  }
  if (dim > SMBLOG_TIENI) {
    fprintf(fp, "(il file superava %d byte: ricomincia)\n", SMBLOG_TIENI);
  }
  fwrite(smblog_copia, 1, n, fp);
  if (tagliati > 0) {
    fprintf(fp, "   (%u righe non entrate in memoria)\n", tagliati);
  }
  if (coda[0] != '\0') {
    fputs(coda, fp);
  }
  fclose(fp);
}


static void scrivi_log(const char *testo) {
  CLogger::Get()->Write("smb", LogNotice, "%s", testo);
  smblog_raccogli(testo);
}
















extern "C" const char *circle_nome_in_rete(void) {
  switch (CMachineInfo::Get()->GetMachineModel()) {
  case MachineModel4B:
    return "BMC64-NG-4";
  case MachineModel400:
    return "BMC64-NG-400";
  case MachineModelCM4:
  case MachineModelCM4S:
    return "BMC64-NG-CM4";
  case MachineModel5:
    return "BMC64-NG-5";
  case MachineModel500:

    return ((CMachineInfo::Get()->GetRevisionRaw() >> 20) & 7) >= 6
               ? "BMC64-NG-500P"
               : "BMC64-NG-500";
  case MachineModelCM5:
  case MachineModelCM5Lite:
    return "BMC64-NG-CM5";
  default:
    return "BMC64-NG";
  }
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
    bmc_nomi_apri(ip_nostro, circle_nome_in_rete(), scrivi_log);
    bmc_smb_fase = "WS-Discovery (3702, 5357)";
    bmc_wsd_apri(ip_nostro, mac_nostro, circle_nome_in_rete(), scrivi_log);
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
  cfg.hostname = circle_nome_in_rete();
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
  smblog_inizio();
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
  smblog_scrivi(1);
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
  if (smblog_da_scrivere()) {
    smblog_scrivi(0);
  }
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
  snprintf(name, len, "%s", circle_nome_in_rete());
  return 0;
}
