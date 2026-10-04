
































#include "shaderprova.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <circle/bcmframebuffer.h>
#include <circle/bcmpropertytags.h>
#include <circle/machineinfo.h>
#include <circle/timer.h>
#include <circle/util.h>

#include "v3dcrt/v3d_crt.h"

#define REFERTO "/SHADER-PROVA.TXT"
#define REFERTO_MAX 3072

static char s_referto[REFERTO_MAX];
static unsigned s_usati;


static void riga(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf("[SHADER] %s\n", buf);
  unsigned n = strlen(buf);
  if (s_usati + n + 2 < REFERTO_MAX) {
    memcpy(s_referto + s_usati, buf, n);
    s_usati += n;
    s_referto[s_usati++] = '\n';
    s_referto[s_usati] = '\0';
  }
}




static void deposita(void) {
  FILE *fp = fopen(REFERTO, "w");
  if (fp == NULL) {
    printf("[SHADER] non riesco a scrivere %s\n", REFERTO);
    return;
  }
  fwrite(s_referto, 1, s_usati, fp);
  fclose(fp);
}




static void misura_display(unsigned *w, unsigned *h) {
  *w = 640;
  *h = 480;
  CBcmPropertyTags Tags;
  TPropertyTagDisplayDimensions Dimensions;
  if (Tags.GetTag(PROPTAG_GET_DISPLAY_DIMENSIONS, &Dimensions,
                  sizeof Dimensions)) {
    if (Dimensions.nWidth >= 64 && Dimensions.nHeight >= 64) {
      *w = Dimensions.nWidth;
      *h = Dimensions.nHeight;
    }
  }
}

extern "C" void shaderprova_esegui(const char *modo, int secondi) {
  if (modo == 0 || modo[0] == 0) {
    return;
  }
  s_usati = 0;
  s_referto[0] = '\0';

  riga("PROVA DELLA V3D - shader_prova=%s", modo);
  riga("scheda: %s, revisione %06x",
       CMachineInfo::Get()->GetMachineName(),
       (unsigned)CMachineInfo::Get()->GetRevisionRaw());
  riga("driver compilato per RASPPI=%d", RASPPI);
  deposita();

  v3dcrt::BootTestMode prova = v3dcrt::ParseBootTestMode(modo);
  if (prova == v3dcrt::kBootTestOff) {
    riga("modo sconosciuto. I modi sono:");
    riga("  mmu solid source qpu qpufill");
    riga("  fragment_artifact fragment_replay fragment_lifecycle");
    riga("  fragment_scanout fragment_fullscreen fragment_source");
    deposita();
    return;
  }

  unsigned dw, dh;
  misura_display(&dw, &dh);




  riga("sto per chiedere un framebuffer %ux%u a 16 bit", dw, dh);
  deposita();
  CBcmFrameBuffer *fb = new CBcmFrameBuffer(dw, dh, 16);
  if (fb == 0 || !fb->Initialize()) {
    riga("FERMATO: il firmware non da' quel framebuffer");
    deposita();
    delete fb;
    return;
  }
  unsigned char *pixel = (unsigned char *)(uintptr_t)fb->GetBuffer();
  unsigned passo = fb->GetPitch();
  riga("bersaglio %ux%u passo %u, RGB565: ok", dw, dh, passo);


  memset(pixel, 0, (size_t)passo * dh);

  riga("sto per configurare e accendere la V3D");
  deposita();
  v3dcrt::Configure(  true,   true,
                    v3dcrt::kShaderCrt, prova,
                      true,
                    v3dcrt::kFragmentPackageDefault,
                    v3dcrt::kRenderResolutionSource);
  if (!v3dcrt::Initialize()) {
    riga("FERMATO: la V3D non si e' accesa (Initialize falso)");
    deposita();
    delete fb;
    return;
  }
  riga("V3D accesa. disponibile=%d", v3dcrt::IsAvailable() ? 1 : 0);
  deposita();

  bool presentato = false;
  v3dcrt::OutputFramebuffer bersaglio = {
        0,
      pixel,
      dw,
      dh,
      passo,
        16,
      v3dcrt::kPixelFormatRgb565,
        dw,
        dh,
        {0, 0, dw, dh},
        false,
      &presentato,
        false,
        0};

  riga("sto per far girare la prova \"%s\"", modo);
  deposita();
  bool esito = v3dcrt::RunBootTest(bersaglio);
  riga("prova \"%s\": %s", modo, esito ? "RIUSCITA" : "FALLITA");
  riga("presentato=%d  sequenza=%u", presentato ? 1 : 0,
       (unsigned)v3dcrt::LastFrameSequence());
  riga("guarda lo schermo adesso: resta cosi' per %d s", secondi);
  deposita();

  if (secondi < 1) secondi = 1;
  if (secondi > 300) secondi = 300;
  for (int i = 0; i < secondi; i++) {
    CTimer::SimpleMsDelay(1000);
  }

  v3dcrt::Shutdown();
  delete fb;
  riga("V3D spenta, l'avvio prosegue");
  deposita();
}
