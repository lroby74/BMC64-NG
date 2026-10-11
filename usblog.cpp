




// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#include "usblog.h"

#include <circle/device.h>
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/timer.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define USBLOG_FILE "/USB-LOG.TXT"

#define NETLOG_FILE "/NET-LOG.TXT"
#define NETLOG_TIENI 8192




#define USBLOG_TIENI 8192

static int s_aperto = 0;



class ScrittoreSuFile : public CDevice {
public:
  explicit ScrittoreSuFile(FILE *fp) : m_fp(fp) {}

  int Write(const void *pBuffer, size_t nCount) override {
    if (m_fp == 0) {
      return -1;
    }
    return (int)fwrite(pBuffer, 1, nCount, m_fp);
  }

private:
  FILE *m_fp;
};





static FILE *apri_tagliato(const char *nome, char *tieni, unsigned quanto) {
  unsigned tenuti = 0;
  FILE *vecchio = fopen(nome, "r");
  if (vecchio != NULL) {
    long dim = 0;
    if (fseek(vecchio, 0, SEEK_END) == 0) {
      dim = ftell(vecchio);
    }
    long da = dim > (long)quanto ? dim - (long)quanto : 0;
    if (fseek(vecchio, da, SEEK_SET) == 0) {
      tenuti = (unsigned)fread(tieni, 1, quanto, vecchio);
    }
    fclose(vecchio);
    tieni[tenuti] = '\0';
    if (da > 0) {

      const char *inizio = strstr(tieni, "\n==== avvio");
      if (inizio != NULL) {
        unsigned salta = (unsigned)(inizio - tieni) + 1;
        memmove(tieni, tieni + salta, tenuti - salta + 1);
        tenuti -= salta;
      }
    }
  }

  FILE *fp = fopen(nome, "w");
  if (fp == NULL) {
    return NULL;
  }
  if (tenuti > 0) {
    fwrite(tieni, 1, tenuti, fp);
  }
  return fp;
}


static FILE *apri_in_coda(void) {
  static char tieni[USBLOG_TIENI + 1];
  return apri_tagliato(USBLOG_FILE, tieni, USBLOG_TIENI);
}








static char s_ultima_riga[200];

static int riga_usb(const char *r, unsigned n) {
  for (unsigned i = 0; i + 3 <= n; i++) {
    if ((r[i] == 'u' || r[i] == 'U') && (r[i + 1] == 's' || r[i + 1] == 'S') &&
        (r[i + 2] == 'b' || r[i + 2] == 'B')) {
      return 1;
    }
    if (i + 6 <= n && (strncmp(r + i, "umouse", 6) == 0 ||
                       strncmp(r + i, "xhci", 4) == 0)) {
      return 1;
    }
  }
  return 0;
}





static int copia_righe(FILE *fp, const char *motivo,
                       int (*filtro)(const char *, unsigned), char *ultima,
                       unsigned dim_ultima, const char *titolo) {
  static char reg[LOGGER_BUFSIZE + 1];
  CLogger *log = CLogger::Get();
  if (log == 0) {
    return 0;
  }
  const int m = log->Read(reg, LOGGER_BUFSIZE, FALSE);
  if (m <= 0) {
    return 0;
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
  if (ultima[0] != '\0') {
    char *q = strstr(reg, ultima);
    if (q != NULL) {
      char *a_capo = strchr(q, '\n');
      p = a_capo != NULL ? a_capo + 1 : reg + w;
    }
  }
  int scritte = 0;
  while (*p != '\0') {
    char *a_capo = strchr(p, '\n');
    unsigned n = a_capo != NULL ? (unsigned)(a_capo - p) : (unsigned)strlen(p);
    if (n > 0 && filtro(p, n)) {
      if (fp != NULL) {
        if (scritte == 0) {
          fprintf(fp, "\n-- %s (%s) --\n", titolo, motivo);
        }
        fwrite(p, 1, n, fp);
        fputc('\n', fp);
      }
      scritte++;
    }
    if (n > 0 && fp != NULL) {
      unsigned k = n < dim_ultima - 1 ? n : dim_ultima - 1;
      memcpy(ultima, p, k);
      ultima[k] = '\0';
    }
    p = a_capo != NULL ? a_capo + 1 : p + n;
  }
  return scritte;
}


static int copia_righe_circle(FILE *fp, const char *motivo) {
  return copia_righe(fp, motivo, riga_usb, s_ultima_riga, sizeof(s_ultima_riga),
                     "i messaggi USB di Circle");
}

extern "C" void usblog_circle(const char *motivo) {
  if (!s_aperto || copia_righe_circle(NULL, "") == 0) {
    return;
  }
  FILE *fp = fopen(USBLOG_FILE, "a");
  if (fp == NULL) {
    return;
  }
  copia_righe_circle(fp, motivo != NULL ? motivo : "adesso");
  fclose(fp);
}

extern "C" void usblog_avvio(void) {
  FILE *fp = apri_in_coda();
  if (fp == NULL) {
    return;
  }
  s_aperto = 1;

  CMachineInfo *info = CMachineInfo::Get();
  fprintf(fp, "\n==== avvio, %lu ms dall'accensione ====\n",
          (unsigned long)(CTimer::GetClockTicks64() / 1000U));
  if (info != 0) {
    fprintf(fp, "scheda            %s\n", info->GetMachineName());
    fprintf(fp, "revisione         %06x\n", (unsigned)info->GetRevisionRaw());
  }
  fprintf(fp, "\n-- i dispositivi che il sistema conosce --\n");
  {
    ScrittoreSuFile scrittore(fp);
    CDeviceNameService *nomi = CDeviceNameService::Get();
    if (nomi != 0) {
      nomi->ListDevices(&scrittore, TRUE);
    }
  }
  fclose(fp);
}

extern "C" void usblog_elenco(const char *motivo) {
  if (!s_aperto) {
    return;
  }
  FILE *fp = fopen(USBLOG_FILE, "a");
  if (fp == NULL) {
    return;
  }
  fprintf(fp, "\n-- i dispositivi adesso (%s) --\n",
          motivo != NULL ? motivo : "cambiato qualcosa");
  {
    ScrittoreSuFile scrittore(fp);
    CDeviceNameService *nomi = CDeviceNameService::Get();
    if (nomi != 0) {
      nomi->ListDevices(&scrittore, TRUE);
    }
  }
  copia_righe_circle(fp, motivo != NULL ? motivo : "cambiato qualcosa");
  fclose(fp);
}

extern "C" void usblog_riga(const char *formato, ...) {
  if (!s_aperto || formato == NULL) {
    return;
  }
  FILE *fp = fopen(USBLOG_FILE, "a");
  if (fp == NULL) {
    return;
  }
  va_list ap;
  va_start(ap, formato);
  vfprintf(fp, formato, ap);
  va_end(ap);
  fputc('\n', fp);
  fclose(fp);
}










extern "C" const char *circle_nome_in_rete(void);

static char s_ultima_rete[200];
static int s_rete_aperto = 0;


static int ha_sorgente(const char *r, unsigned n, const char *s) {
  const unsigned k = (unsigned)strlen(s);
  for (unsigned i = 0; i + k + 2 <= n; i++) {
    if ((i == 0 || r[i - 1] == ' ') && strncmp(r + i, s, k) == 0 &&
        r[i + k] == ':' && r[i + k + 1] == ' ') {
      return 1;
    }
  }
  return 0;
}

static int contiene(const char *r, unsigned n, const char *s) {
  const unsigned k = (unsigned)strlen(s);
  for (unsigned i = 0; i + k <= n; i++) {
    if (strncmp(r + i, s, k) == 0) {
      return 1;
    }
  }
  return 0;
}

static int riga_rete(const char *r, unsigned n) {
  return ha_sorgente(r, n, "genet") || ha_sorgente(r, n, "macb") ||
         ha_sorgente(r, n, "dhcp") || ha_sorgente(r, n, "wlan") ||
         contiene(r, n, "Ethernet") || contiene(r, n, "Wi-Fi") ||
         contiene(r, n, "WLAN") || contiene(r, n, "WPA ") ||
         contiene(r, n, "static IP");
}

extern "C" void netlog_circle(const char *motivo) {
  if (copia_righe(NULL, "", riga_rete, s_ultima_rete, sizeof(s_ultima_rete),
                  "") == 0) {
    return;
  }
  FILE *fp;
  if (!s_rete_aperto) {
    static char tieni[NETLOG_TIENI + 1];
    fp = apri_tagliato(NETLOG_FILE, tieni, NETLOG_TIENI);
    if (fp == NULL) {
      return;
    }
    s_rete_aperto = 1;
    CMachineInfo *info = CMachineInfo::Get();
    fprintf(fp, "\n==== avvio di %s (%s, revisione %06x) ====\n",
            circle_nome_in_rete(), info != 0 ? info->GetMachineName() : "?",
            info != 0 ? (unsigned)info->GetRevisionRaw() : 0u);
  } else {
    fp = fopen(NETLOG_FILE, "a");
    if (fp == NULL) {
      return;
    }
  }
  copia_righe(fp, motivo != NULL ? motivo : "adesso", riga_rete, s_ultima_rete,
              sizeof(s_ultima_rete), "la rete");
  fclose(fp);
}
