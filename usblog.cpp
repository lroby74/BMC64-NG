




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
#include <circle/machineinfo.h>
#include <circle/timer.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define USBLOG_FILE "/USB-LOG.TXT"




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




static FILE *apri_in_coda(void) {
  static char tieni[USBLOG_TIENI + 1];
  unsigned tenuti = 0;
  FILE *vecchio = fopen(USBLOG_FILE, "r");
  if (vecchio != NULL) {
    long dim = 0;
    if (fseek(vecchio, 0, SEEK_END) == 0) {
      dim = ftell(vecchio);
    }
    long da = dim > USBLOG_TIENI ? dim - USBLOG_TIENI : 0;
    if (fseek(vecchio, da, SEEK_SET) == 0) {
      tenuti = (unsigned)fread(tieni, 1, USBLOG_TIENI, vecchio);
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

  FILE *fp = fopen(USBLOG_FILE, "w");
  if (fp == NULL) {
    return NULL;
  }
  if (tenuti > 0) {
    fwrite(tieni, 1, tenuti, fp);
  }
  return fp;
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
