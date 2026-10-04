




// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#include "crtprova.h"

#include <circle/timer.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if RASPPI >= 5
#include "pi5kms/pi5_kms.h"
#endif


















static const struct {
  const char *nome;
  const char *timings;
  const char *nota;
} MODI[] = {
  {"amiga262", "1920 1 180 192 334 262 1 14 15 22 0 0 0 50 0 41200000 1",
   "50,1254 Hz - la frequenza del VIC-II PAL, ma 262 righe visibili"},
  {"md288",    "1920 1 48 192 240 288 1 6 3 16 0 0 0 50 0 37560000 1",
   "50,0000 Hz - 288 righe, ci stanno tutte le nostre 272"},
  {"ms256",    "1920 1 140 200 260 256 1 18 10 26 0 0 0 50 0 39060000 1",
   "50,0000 Hz - 256 righe"},
  {"nes240",   "1920 1 158 200 314 240 1 23 6 43 0 0 0 50.01 0 40450000 1",
   "50,0183 Hz - 240 righe"},
  {0, 0, 0}
};

extern "C" const char *crtprova_modo(const char *nome) {
  if (nome == 0) {
    return 0;
  }
  for (int i = 0; MODI[i].nome != 0; i++) {
    if (strcmp(nome, MODI[i].nome) == 0) {
      return MODI[i].timings;
    }
  }
  return 0;
}

extern "C" const char *crtprova_nome(int i) {
  for (int k = 0; MODI[k].nome != 0; k++) {
    if (k == i) {
      return MODI[k].nome;
    }
  }
  return 0;
}

#if RASPPI >= 5








static void disegna(uint8_t *pixel, unsigned larghezza, unsigned altezza,
                    unsigned passo) {
  const uint16_t NERO = 0x0000, BIANCO = 0xFFFF, GRIGIO = 0x8410;
  const uint16_t ROSSO = 0xF800, VERDE = 0x07E0, BLU = 0x001F;

  for (unsigned y = 0; y < altezza; y++) {
    uint16_t *riga = (uint16_t *)(pixel + y * passo);
    for (unsigned x = 0; x < larghezza; x++) {
      uint16_t c = NERO;
      if (y < 2 || y >= altezza - 2 || x < 8 || x >= larghezza - 8) {
        c = BIANCO;
      } else if (y == altezza / 2 || x == larghezza / 2) {
        c = BIANCO;
      } else if ((x % 64) == 0 || (y % 16) == 0) {
        c = GRIGIO;
      }
      riga[x] = c;
    }
  }


  unsigned y0 = altezza - altezza / 5;
  for (unsigned y = y0; y < altezza - 4; y++) {
    uint16_t *riga = (uint16_t *)(pixel + y * passo);
    for (unsigned x = 16; x < larghezza - 16; x++) {
      unsigned quale = (x - 16) * 4 / (larghezza - 32);
      riga[x] = quale == 0 ? ROSSO : quale == 1 ? VERDE : quale == 2 ? BLU : BIANCO;
    }
  }
}








#define CRT_REFERTO "/CRT-PROVA.TXT"
#define CRT_REFERTO_MAX 2048
static char s_crt_referto[CRT_REFERTO_MAX];
static unsigned s_crt_usati;

static void crt_riga(const char *fmt, ...) {
  char buf[224];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf("[CRT] %s\r\n", buf);
  unsigned n = strlen(buf);
  if (s_crt_usati + n + 2 < CRT_REFERTO_MAX) {
    memcpy(s_crt_referto + s_crt_usati, buf, n);
    s_crt_usati += n;
    s_crt_referto[s_crt_usati++] = '\n';
    s_crt_referto[s_crt_usati] = '\0';
  }
}




static void crt_deposita(void) {
  FILE *fp = fopen(CRT_REFERTO, "w");
  if (fp == NULL) {
    return;
  }
  fwrite(s_crt_referto, 1, s_crt_usati, fp);
  fclose(fp);
}

extern "C" int crtprova_esegui(const char *nome_o_timings, int secondi) {
  if (nome_o_timings == 0 || *nome_o_timings == '\0') {
    return 0;
  }
  s_crt_usati = 0;
  s_crt_referto[0] = '\0';
  crt_riga("PROVA DEL MODO CRT - crt_prova=%s, %d secondi",
           nome_o_timings, secondi);
  crt_deposita();
  const char *timings = crtprova_modo(nome_o_timings);
  if (timings == 0) {

    timings = nome_o_timings;
  }
  crt_riga("modeline: %s", timings);
  crt_deposita();

  bmxkms::Mode modo;
  if (!pi5kms::ResolveBmcMode(2, 87, timings, 0, &modo)) {
    crt_riga("FERMATO: ResolveBmcMode non legge la modeline");
    crt_deposita();
    return 0;
  }
  const unsigned htot = modo.width + modo.h_front_porch + modo.h_sync +
                        modo.h_back_porch;
  const unsigned vtot = modo.height + modo.v_front_porch + modo.v_sync +
                        modo.v_back_porch;
  if (htot == 0 || vtot == 0) {
    crt_riga("FERMATO: totali a zero (htot=%u vtot=%u)", htot, vtot);
    crt_deposita();
    return 0;
  }
  const unsigned hz_riga = modo.pixel_clock / htot;
  const unsigned mhz_quadro = (unsigned)((uint64_t)modo.pixel_clock * 1000 /
                                         ((uint64_t)htot * vtot));
  crt_riga("%ux%u, clock %u, riga %u Hz, quadro %u,%03u Hz",
           modo.width, modo.height, modo.pixel_clock, hz_riga,
           mhz_quadro / 1000, mhz_quadro % 1000);
  crt_riga("sto per chiedere il modo al controllore (SetMode)");
  crt_deposita();

  if (!pi5kms::SetMode(modo)) {
    crt_riga(">>> SetMode HA DETTO NO. Il modo non e' stato impostato.");
    crt_riga("    (e' questa la riga che spiega il CRT nero)");
    crt_deposita();
    return 0;
  }

  bmxkms::Framebuffer fb;
  if (!pi5kms::CreateFramebuffer(modo.width, modo.height, 16, &fb)) {
    crt_riga("FERMATO: CreateFramebuffer non da' memoria");
    crt_deposita();
    return 0;
  }
  disegna(fb.pixels, fb.width, fb.height, fb.pitch);
  pi5kms::FlushFramebuffer(fb);
  if (!pi5kms::ConfigureScanout(fb)) {
    crt_riga("FERMATO: ConfigureScanout ha detto no");
    crt_deposita();
    pi5kms::DestroyFramebuffer(&fb);
    return 0;
  }
  crt_riga("SetMode ok, quadro acceso: guarda lo schermo per %d s", secondi);
  crt_deposita();

  for (int i = 0; i < secondi * 10; i++) {
    CTimer::SimpleMsDelay(100);
  }

  pi5kms::DestroyFramebuffer(&fb);
  crt_riga("fine del quadro, l'avvio prosegue");
  crt_deposita();
  return 1;
}

#else

extern "C" int crtprova_esegui(const char *nome_o_timings, int secondi) {
  (void)secondi;




  FILE *fp = fopen("/CRT-PROVA.TXT", "w");
  if (fp != NULL) {
    fprintf(fp, "PROVA DEL MODO CRT - crt_prova=%s\n",
            nome_o_timings != 0 ? nome_o_timings : "");
    fprintf(fp, "NON FATTA: questo non e' un Pi 5.\n");
    fprintf(fp, "Sul Pi 4 il modo su misura lo imposta il firmware da\n");
    fprintf(fp, "config.txt (hdmi_timings), non il programma.\n");
    fclose(fp);
  }
  return 0;
}

#endif
