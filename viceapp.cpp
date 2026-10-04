//
// viceapp.cpp
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "viceapp.h"

#include "usblog.h"
#include "vice_network.h"
#include "network_time_sync.h"



extern "C" int fbl_stato_scalatore(void);
extern "C" int fbl_stato_v3d(void);
extern "C" void circle_diag_schermo(const char *quando);
extern "C" void circle_diag_audio(const char *quando);
extern "C" void bmc_righe_boot_scrivi(FILE *fp);
extern "C" int circle_fb_display_chiesto(void);
extern "C" int circle_composito_chiesto(void);
#include "third_party/common/circle.h"
#include <circle/sound/hdmisoundbasedevice.h>


extern "C" int vidtrial_pending(void);
#include <circle/usb/usbdevicefactory.h>
#include <circle/synchronize.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/timer.h>
#include "fbl.h"
#include "uscitaaudio.h"



extern "C" const char *bmc64_version_string(void);

static const unsigned int WIFI_SCAN_DURATION_US = 4000000;
static const unsigned int WIFI_CONNECT_TIMEOUT_US = 30000000;

#if defined(RASPI_SCPU64)

#include "bootstat_scpu64.h"
#elif defined(RASPI_C64)
#include "bootstat_c64.h"
#elif defined(RASPI_C128)
#include "bootstat_c128.h"
#elif defined(RASPI_VIC20)
#include "bootstat_vic20.h"
#elif defined(RASPI_PLUS4)
#include "bootstat_plus4.h"
#elif defined(RASPI_PLUS4EMU)
#include "bootstat_plus4emu.h"
#elif defined(RASPI_PET)
#include "bootstat_pet.h"
#else
  #error Unknown RASPI_ variant
#endif

//
// ViceApp impl
//

bool ViceApp::Initialize(void) {
  if (!mSerial.Initialize(115200)) {
    return false;
  }

  // Initialize our replacement newlib stdio. Give it
  // a pointer to our serial device so we can use printf
  // to serial as soon as possible.
  CGlueStdioInit(mViceOptions.SerialEnabled() ? &mSerial : nullptr);

  if (!mInterrupt.Initialize()) {
    return false;
  }

  return true;
}

int ViceApp::circle_get_machine_timing() {
  // See circle.h for valid values
  return mViceOptions.GetMachineTiming();
}













extern unsigned long bmc_fw_quadro_misurato_us(void);
static int cicli_composito(int cicli_hdmi, int hz, int randy) {
  static int fatti[2] = {0, 0};
  const int i = hz == 60 ? 1 : 0;
  if (fatti[i] != 0) {
    return fatti[i];
  }
  const double nominale = 1000000.0 / hz;
  const unsigned long p = bmc_fw_quadro_misurato_us();




  if (p == 0 && !bmc_vice_partito) {
    return randy;
  }
  if (p == 0 || p < nominale * 0.98 || p > nominale * 1.02) {
    fatti[i] = randy;
    printf("boot: composito: ritorno di quadro %lu us, fuori misura: "
           "cicli di Randy %d\r\n", p, randy);
  } else {
    fatti[i] = (int)((double)cicli_hdmi * nominale / (double)p + 0.5);
    const unsigned long mhz = (unsigned long)(1000000000000.0 / (double)p);
    printf("boot: composito: ritorno di quadro %lu us (%lu.%04lu Hz), "
           "cicli %d (Randy %d)\r\n", p, mhz / 1000000UL,
           (mhz % 1000000UL) / 100UL, fatti[i], randy);
  }
  return fatti[i];
}

#if defined(RASPI_PLUS4) | defined(RASPI_PLUS4EMU)
int ViceApp::circle_cycles_per_second() {
  int timing = circle_get_machine_timing();
  if (timing == MACHINE_TIMING_NTSC_HDMI || timing == MACHINE_TIMING_NTSC_DPI) {
    // 60hz
    return 1792080;
  } else if (circle_get_machine_timing() == MACHINE_TIMING_NTSC_COMPOSITE) {
    // Actual C64's NTSC Composite frequency is 59.826 but the Pi's vertical
    // sync frequency on composite is 60.053. See c64.h for how this is
    // calculated. This keeps audio buffer to a minimum using ReSid.
    return cicli_composito(1792080, 60, 1793672);
  } else if (timing == MACHINE_TIMING_NTSC_CUSTOM_HDMI || timing == MACHINE_TIMING_NTSC_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else if (timing == MACHINE_TIMING_PAL_HDMI) {
    // 50hz
    return 1778400;
  } else if (timing == MACHINE_TIMING_PAL_COMPOSITE) {
    // Actual C64's PAL Composite frequency is 50.125 but the Pi's vertical
    // sync frequency on composite is 50.0816. See c64.h for how this is
    // calculated.  This keep audio buffer to a minimum using ReSid.
    return cicli_composito(1778400, 50, 1781245);
  } else if (timing == MACHINE_TIMING_PAL_CUSTOM_HDMI || timing == MACHINE_TIMING_PAL_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else {
    return 1778400;
  }
}
#elif defined(RASPI_VIC20)
int ViceApp::circle_cycles_per_second() {
  int timing = circle_get_machine_timing();
  if (timing == MACHINE_TIMING_NTSC_HDMI) {
    // 60hz
    return 1017900;
  } else if (timing == MACHINE_TIMING_NTSC_COMPOSITE) {
    // Actual C64's NTSC Composite frequency is 59.826 but the Pi's vertical
    // sync frequency on composite is 60.053. See c64.h for how this is
    // calculated. This keeps audio buffer to a minimum using ReSid.
    return cicli_composito(1017900, 60, 1018804);
  } else if (timing == MACHINE_TIMING_NTSC_CUSTOM_HDMI || timing == MACHINE_TIMING_NTSC_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else if (timing == MACHINE_TIMING_PAL_HDMI) {
    // 50hz
    return 1107600;
  } else if (timing == MACHINE_TIMING_PAL_COMPOSITE) {
    // Actual C64's PAL Composite frequency is 50.125 but the Pi's vertical
    // sync frequency on composite is 50.0816. See c64.h for how this is
    // calculated.  This keep audio buffer to a minimum using ReSid.
    return cicli_composito(1107600, 50, 1109372);
  } else if (timing == MACHINE_TIMING_PAL_CUSTOM_HDMI || timing == MACHINE_TIMING_PAL_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else {
    return 1017900;
  }
}
#elif defined(RASPI_C64) | defined(RASPI_C128)
int ViceApp::circle_cycles_per_second() {
  int timing = circle_get_machine_timing();
  if (timing == MACHINE_TIMING_NTSC_HDMI) {
    // 60hz
    return 1025700;
  } else if (timing == MACHINE_TIMING_NTSC_COMPOSITE) {
    // Actual C64's NTSC Composite frequency is 59.826 but the Pi's vertical
    // sync frequency on composite is 60.053. See c64.h for how this is
    // calculated. This keeps audio buffer to a minimum using ReSid.
    return cicli_composito(1025700, 60, 1026611);
  } else if (timing == MACHINE_TIMING_NTSC_CUSTOM_HDMI || timing == MACHINE_TIMING_NTSC_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else if (timing == MACHINE_TIMING_PAL_HDMI) {
    // 50hz
    return 982800;
  } else if (timing == MACHINE_TIMING_PAL_COMPOSITE) {
    // Actual C64's PAL Composite frequency is 50.125 but the Pi's vertical
    // sync frequency on composite is 50.0816. See c64.h for how this is
    // calculated.  This keep audio buffer to a minimum using ReSid.
    return cicli_composito(982800, 50, 984404);
  } else if (timing == MACHINE_TIMING_PAL_CUSTOM_HDMI || timing == MACHINE_TIMING_PAL_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else {
    return 982800;
  }
}
#elif defined(RASPI_PET)
int ViceApp::circle_cycles_per_second() {
  int timing = circle_get_machine_timing();
  if (timing == MACHINE_TIMING_NTSC_HDMI) {
    // 60hz
    return 1013760;
  } else if (timing == MACHINE_TIMING_NTSC_COMPOSITE) {
    // Actual C64's NTSC Composite frequency is 59.826 but the Pi's vertical
    // sync frequency on composite is 60.053. See c64.h for how this is
    // calculated. This keeps audio buffer to a minimum using ReSid.
    return cicli_composito(1013760, 60, 1014661);
  } else if (timing == MACHINE_TIMING_NTSC_CUSTOM_HDMI || timing == MACHINE_TIMING_NTSC_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else if (timing == MACHINE_TIMING_PAL_HDMI) {
    // 50hz
    return 1001600;
  } else if (timing == MACHINE_TIMING_PAL_COMPOSITE) {
    // Actual C64's PAL Composite frequency is 50.125 but the Pi's vertical
    // sync frequency on composite is 50.0816. See c64.h for how this is
    // calculated.  This keep audio buffer to a minimum using ReSid.
    return cicli_composito(1001600, 50, 1003202);
  } else if (timing == MACHINE_TIMING_PAL_CUSTOM_HDMI || timing == MACHINE_TIMING_PAL_CUSTOM_DPI) {
    return mViceOptions.GetCyclesPerSecond();
  } else {
    return 1000000;
  }
}
#else
  #error Unknown RASPI_ variant
#endif

//
// ViceScreenApp impl
//

bool ViceScreenApp::Initialize(void) {
  if (!ViceApp::Initialize()) {
    return false;
  }

  if (mViceOptions.SerialEnabled()) {
     if (!mLogger.Initialize(&mSerial)) {
        return false;
     }
  } else {
     if (!mLogger.Initialize(&mNullDevice)) {
        return false;
     }
  }

  mLogger.Write(GetKernelName(), LogNotice, "%s",
                bmc64_version_string());

  if (!mEmulatorCore->Init(&mViceOptions)) {
    return false;
  }

  if (!mTimer.Initialize()) {
    return false;
  }

  if (!mGPIOManager.Initialize()) {
    return false;
  }

#ifndef BMC64_USE_HDMI_SOUND


  //




  if (mViceOptions.GetAudioOut() == VCHIQSoundDestinationHeadphones ||
      mViceOptions.AudioForceVCHIQ()) {
    if (!mVCHIQ.Initialize()) {
      return false;
    }
  }
#endif

  SetupGPIO();

  FrameBufferLayer::Initialize();

  return true;
}

// Setup GPIO pins for scanning keyboard, button or joysticks.
void ViceScreenApp::SetupGPIOForInput() {
  // PA - Set to output-low for when scanning each
  // row. Otherwise set to input-pullup.
  // Note: Lines 0 and 7 are swapped. The order here is
  // from keyboard connector pins 20 down to 13.

  // Connector Pin 20 - PA7
  gpioPins[7] =
      new CGPIOPin(26, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 19 - PA1
  gpioPins[1] =
      new CGPIOPin(20, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 18 - PA2
  gpioPins[2] =
      new CGPIOPin(19, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 17 - PA3
  gpioPins[3] =
      new CGPIOPin(16, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 16 - PA4
  gpioPins[4] =
      new CGPIOPin(13, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 15 - PA5
  gpioPins[5] =
      new CGPIOPin(6, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 14 - PA6
  gpioPins[6] =
      new CGPIOPin(12, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 13 - PA0
  gpioPins[0] =
      new CGPIOPin(5, GPIOModeInputPullUp, &mGPIOManager);

  // PB - Always input-pullup for read during kbd scan or joy port 1
  // Note: Lines 3 and 7 are swapped. The order here is from
  // keyboard connector pins 12 down to 5

  // Connector Pin 12 - PB 0
  gpioPins[8] =
      new CGPIOPin(8, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 11 - PB 1
  gpioPins[9] =
      new CGPIOPin(25, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 10 - PB 2
  gpioPins[10] =
      new CGPIOPin(24, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 9 - PB 7
  gpioPins[15] =
      new CGPIOPin(22, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 8 - PB 4
  gpioPins[12] =
      new CGPIOPin(23, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 7 - PB 5
  gpioPins[13] =
      new CGPIOPin(27, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 6 - PB 6
  gpioPins[14] =
      new CGPIOPin(17, GPIOModeInputPullUp, &mGPIOManager);
  // Connector Pin 5 - PB 3
  gpioPins[11] =
      new CGPIOPin(18, GPIOModeInputPullUp, &mGPIOManager);

  // A few more special pins
  gpioPins[GPIO_KBD_RESTORE_INDEX] =
      new CGPIOPin(GPIO_KBD_RESTORE, GPIOModeInputPullUp, &mGPIOManager);
  gpioPins[GPIO_JS1_SELECT_INDEX] =
      new CGPIOPin(GPIO_JS1_SELECT, GPIOModeInputPullUp, &mGPIOManager);
  gpioPins[GPIO_JS2_SELECT_INDEX] =
      new CGPIOPin(GPIO_JS2_SELECT, GPIOModeInputPullUp, &mGPIOManager);

  gpioPins[NO_FIXED_PURPOSE_1_INDEX] =
      new CGPIOPin(2, GPIOModeInputPullUp, &mGPIOManager);
  gpioPins[NO_FIXED_PURPOSE_2_INDEX] =
      new CGPIOPin(3, GPIOModeInputPullUp, &mGPIOManager);
  gpioPins[NO_FIXED_PURPOSE_3_INDEX] =
      new CGPIOPin(9, GPIOModeInputPullUp, &mGPIOManager);
  gpioPins[NO_FIXED_PURPOSE_4_INDEX] =
      new CGPIOPin(10, GPIOModeInputPullUp, &mGPIOManager);

  // Convenience arrays for joysticks
  config_1_joystickPins1[JOY_UP] = gpioPins[GPIO_CONFIG_1_JOY_1_UP_INDEX];
  config_1_joystickPins1[JOY_DOWN] = gpioPins[GPIO_CONFIG_1_JOY_1_DOWN_INDEX];
  config_1_joystickPins1[JOY_LEFT] = gpioPins[GPIO_CONFIG_1_JOY_1_LEFT_INDEX];
  config_1_joystickPins1[JOY_RIGHT] = gpioPins[GPIO_CONFIG_1_JOY_1_RIGHT_INDEX];
  config_1_joystickPins1[JOY_FIRE] = gpioPins[GPIO_CONFIG_1_JOY_1_FIRE_INDEX];

  config_1_joystickPins2[JOY_UP] = gpioPins[GPIO_CONFIG_1_JOY_2_UP_INDEX];
  config_1_joystickPins2[JOY_DOWN] = gpioPins[GPIO_CONFIG_1_JOY_2_DOWN_INDEX];
  config_1_joystickPins2[JOY_LEFT] = gpioPins[GPIO_CONFIG_1_JOY_2_LEFT_INDEX];
  config_1_joystickPins2[JOY_RIGHT] = gpioPins[GPIO_CONFIG_1_JOY_2_RIGHT_INDEX];
  config_1_joystickPins2[JOY_FIRE] = gpioPins[GPIO_CONFIG_1_JOY_2_FIRE_INDEX];

  config_0_joystickPins1[JOY_UP] = gpioPins[GPIO_CONFIG_0_JOY_1_UP_INDEX];
  config_0_joystickPins1[JOY_DOWN] = gpioPins[GPIO_CONFIG_0_JOY_1_DOWN_INDEX];
  config_0_joystickPins1[JOY_LEFT] = gpioPins[GPIO_CONFIG_0_JOY_1_LEFT_INDEX];
  config_0_joystickPins1[JOY_RIGHT] = gpioPins[GPIO_CONFIG_0_JOY_1_RIGHT_INDEX];
  config_0_joystickPins1[JOY_FIRE] = gpioPins[GPIO_CONFIG_0_JOY_1_FIRE_INDEX];

  config_0_joystickPins2[JOY_UP] = gpioPins[GPIO_CONFIG_0_JOY_2_UP_INDEX];
  config_0_joystickPins2[JOY_DOWN] = gpioPins[GPIO_CONFIG_0_JOY_2_DOWN_INDEX];
  config_0_joystickPins2[JOY_LEFT] = gpioPins[GPIO_CONFIG_0_JOY_2_LEFT_INDEX];
  config_0_joystickPins2[JOY_RIGHT] = gpioPins[GPIO_CONFIG_0_JOY_2_RIGHT_INDEX];
  config_0_joystickPins2[JOY_FIRE] = gpioPins[GPIO_CONFIG_0_JOY_2_FIRE_INDEX];

  config_2_joystickPins[JOY_UP] = gpioPins[GPIO_CONFIG_2_WAVESHARE_UP_INDEX];
  config_2_joystickPins[JOY_DOWN] = gpioPins[GPIO_CONFIG_2_WAVESHARE_DOWN_INDEX];
  config_2_joystickPins[JOY_LEFT] = gpioPins[GPIO_CONFIG_2_WAVESHARE_LEFT_INDEX];
  config_2_joystickPins[JOY_RIGHT] = gpioPins[GPIO_CONFIG_2_WAVESHARE_RIGHT_INDEX];
  config_2_joystickPins[JOY_FIRE] = gpioPins[GPIO_CONFIG_2_WAVESHARE_B_INDEX];
  config_2_joystickPins[JOY_POTX] = gpioPins[GPIO_CONFIG_2_WAVESHARE_A_INDEX];
  config_2_joystickPins[JOY_POTY] = gpioPins[GPIO_CONFIG_2_WAVESHARE_Y_INDEX];

  config_3_joystickPins1[JOY_UP] = gpioPins[GPIO_CONFIG_3_JOY_1_UP_INDEX];
  config_3_joystickPins1[JOY_DOWN] = gpioPins[GPIO_CONFIG_3_JOY_1_DOWN_INDEX];
  config_3_joystickPins1[JOY_LEFT] = gpioPins[GPIO_CONFIG_3_JOY_1_LEFT_INDEX];
  config_3_joystickPins1[JOY_RIGHT] = gpioPins[GPIO_CONFIG_3_JOY_1_RIGHT_INDEX];
  config_3_joystickPins1[JOY_FIRE] = gpioPins[GPIO_CONFIG_3_JOY_1_FIRE_INDEX];

  config_3_joystickPins2[JOY_UP] = gpioPins[GPIO_CONFIG_3_JOY_2_UP_INDEX];
  config_3_joystickPins2[JOY_DOWN] = gpioPins[GPIO_CONFIG_3_JOY_2_DOWN_INDEX];
  config_3_joystickPins2[JOY_LEFT] = gpioPins[GPIO_CONFIG_3_JOY_2_LEFT_INDEX];
  config_3_joystickPins2[JOY_RIGHT] = gpioPins[GPIO_CONFIG_3_JOY_2_RIGHT_INDEX];
  config_3_joystickPins2[JOY_FIRE] = gpioPins[GPIO_CONFIG_3_JOY_2_FIRE_INDEX];

  config_3_userportPins[USERPORT_PB0] = gpioPins[GPIO_CONFIG_3_USERPORT_PB0_INDEX];
  config_3_userportPins[USERPORT_PB1] = gpioPins[GPIO_CONFIG_3_USERPORT_PB1_INDEX];
  config_3_userportPins[USERPORT_PB2] = gpioPins[GPIO_CONFIG_3_USERPORT_PB2_INDEX];
  config_3_userportPins[USERPORT_PB3] = gpioPins[GPIO_CONFIG_3_USERPORT_PB3_INDEX];
  config_3_userportPins[USERPORT_PB4] = gpioPins[GPIO_CONFIG_3_USERPORT_PB4_INDEX];
  config_3_userportPins[USERPORT_PB5] = gpioPins[GPIO_CONFIG_3_USERPORT_PB5_INDEX];
  config_3_userportPins[USERPORT_PB6] = gpioPins[GPIO_CONFIG_3_USERPORT_PB6_INDEX];
  config_3_userportPins[USERPORT_PB7] = gpioPins[GPIO_CONFIG_3_USERPORT_PB7_INDEX];
}

// Setup GPIO pins for DPI
void ViceScreenApp::SetupGPIOForDPI() {
  for (int i=0; i< 28; i++) {
    DPIPins[i] =
      new CGPIOPin(i, GPIOModeAlternateFunction2, &mGPIOManager);
  }
}

void ViceScreenApp::SetupGPIO() {
  if (mViceOptions.DPIEnabled()) {
     SetupGPIOForDPI();
  } else {
     SetupGPIOForInput();
  }
}

//
// ViceStdioApp impl
//








static void diag_echo_file(FILE *out, const char *path, const char *keys[]) {
  FILE *in = fopen(path, "r");
  if (in == NULL) {
    fprintf(out, "  (%s non leggibile)\n", path);
    return;
  }
  char line[512];
  while (fgets(line, sizeof(line) - 1, in)) {
    if (feof(in)) {
      break;
    }
    for (int i = 0; keys[i] != NULL; i++) {
      if (strstr(line, keys[i]) != NULL && line[0] != '#') {
        int n = strlen(line);
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n')) {
          line[--n] = '\0';
        }
        fprintf(out, "  %s\n", line);
        break;
      }
    }
  }
  fclose(in);
}



#define DIAG_KEEP 32768







extern "C" void circle_diag_schermo(const char *quando) {
  FILE *fp = fopen("/BMC64-DIAG.TXT", "a");
  if (fp == NULL) {
    return;
  }
  fprintf(fp, "\n-- lo schermo di BMX, %s: scalatore %d, shader V3D %d --\n",
          quando != NULL ? quando : "?", fbl_stato_scalatore(),
          fbl_stato_v3d());

  fprintf(fp, "   core 0 nella scheda a VICE partito: %d%s%s\n",
          bmc_core0_nella_scheda,
          bmc_core0_nella_scheda != 0 ? " volte, l'ultima: " : "",
          bmc_core0_nella_scheda != 0 && bmc_core0_nella_scheda_cosa != nullptr
              ? bmc_core0_nella_scheda_cosa : "");


  {
    extern unsigned kbd_tasti_arrivati, kbd_tasti_col_gs;
    extern long kbd_ultimo_tasto;
    fprintf(fp, "   tastiera: %u tasti arrivati, %u col C64 GS "
                "(l'ultimo 0x%lx)\n",
            kbd_tasti_arrivati, kbd_tasti_col_gs,
            (unsigned long)kbd_ultimo_tasto);
  }
  bmc_righe_boot_scrivi(fp);
  fclose(fp);


  printf("[DIAG] scritta: %s\n", quando != NULL ? quando : "?");
}






extern "C" void circle_diag_audio(const char *quando) {
  static const char *chi[] = { "niente", "VCHIQ", "HDMI diretto", "DAC USB" };
  FILE *fp = fopen("/BMC64-DIAG.TXT", "a");

  if (fp == NULL) {
    return;
  }
  fprintf(fp, "\n-- audio, %s --\n", quando != NULL ? quando : "?");
  fprintf(fp, "   uscita            %s, avvio %d, a %u Hz\n",
          chi[raspi_snd_dev < 4 ? raspi_snd_dev : 0], raspi_snd_play,
          raspi_snd_hz);




  fprintf(fp, "   canali            il VICE ne manda %u%s\n", raspi_snd_canali,
          raspi_snd_dev != 1 ? ""
          : raspi_snd_canali == 2 ? ", al VCHIQ due"
                                  : ", al VCHIQ due (raddoppiato)");



  fprintf(fp, "   SID 2 sul core 2  %lu pezzi calcolati\n", bmc_giri[2]);


  fprintf(fp, "   SID sul core 3    %lu pezzi calcolati\n", bmc_sid3_lavori);
  if (raspi_snd_dev == 1) {



    const unsigned trascorsi = CTimer::GetClockTicks() - raspi_vchiq_inizio;
    const unsigned long consumati = raspi_vchiq_byte > raspi_vchiq_coda
                                        ? raspi_vchiq_byte - raspi_vchiq_coda
                                        : 0;
    const unsigned long al_secondo =
        trascorsi > 0 ? (unsigned long)((unsigned long long)consumati *
                                        1000000ULL / trascorsi)
                      : 0;
    fprintf(fp, "   VC4 consuma       %lu byte/s in %u,%u s (atteso 176400)\n",
            al_secondo, trascorsi / 1000000, (trascorsi / 100000) % 10);
    fprintf(fp, "   attese coda piena %lu  (atteso 0)\n", raspi_vchiq_attese);
  }



  if (raspi_snd_dev == 2) {
    fprintf(fp, "   porta HDMI        %u\n", raspi_snd_porta);
  }
#if RASPPI >= 4
  for (unsigned porta = 0; porta < 2; porta++) {
    uintptr base, pacchetti, mai;
    unsigned dreq;
    CHDMISoundBaseDevice::GetPortLayout(porta, &base, &pacchetti, &mai, &dreq);
    fprintf(fp, "   registri HDMI%u    hdmi 0x%lx  packet 0x%lx  mai 0x%lx  dreq %u\n",
            porta, (unsigned long)base, (unsigned long)pacchetti,
            (unsigned long)mai, dreq);
  }
#endif
  fprintf(fp, "   volume            %u%% (%d centesimi di dB)\n",
          raspi_snd_vol_pct, raspi_snd_vol_cb);
  fprintf(fp, "   picco del VICE    %u su 32767%s\n", raspi_snd_picco,
          raspi_snd_picco > 20000 ? "  <-- vicino al fondo scala" : "");
  fprintf(fp, "   campioni mandati  %lu\n", raspi_snd_written);
  fprintf(fp, "   coda vuota        %lu volte  (i buchi)\n", raspi_snd_vuoti);
  fprintf(fp, "   campioni buttati  %lu  (coda piena)\n", raspi_snd_scartati);
  fprintf(fp, "   campioni tagliati %lu  (dal guadagno del volume)%s\n",
          raspi_snd_tagliati,
          raspi_snd_tagliati > 0 ? "  <-- il volume storce il suono" : "");
  fprintf(fp, "   riaperture        %u  (l'ultima ha detto %d)\n",
          raspi_snd_restarts, raspi_snd_restart);
  fprintf(fp, "   cambi d'uscita    %u  (DAC sparito %u volte)\n",
          raspi_snd_cambi, raspi_snd_assenze);
  fclose(fp);
}




void ViceStdioApp::ScriviListeDopoLaUsb(void) {
  usblog_elenco("dopo l'avvio della USB");
  circle_diag_schermo("dopo l'avvio della USB");
  circle_diag_audio("dopo l'avvio della USB");
}

void ViceStdioApp::WriteBootDiag(void) {



  static char keep[DIAG_KEEP + 1];
  unsigned kept = 0;
  {






    FILE *old = fopen("/BMC64-DIAG.TXT", "r");
    if (old != NULL) {
      long dim = 0;
      if (fseek(old, 0, SEEK_END) == 0) {
        dim = ftell(old);
      }
      long da = dim > DIAG_KEEP ? dim - DIAG_KEEP : 0;
      if (fseek(old, da, SEEK_SET) == 0) {
        kept = fread(keep, 1, DIAG_KEEP, old);
      }
      fclose(old);
      keep[kept] = '\0';
      if (da > 0 && kept > 0) {

        char *inizio = strstr(keep, "\n========");
        if (inizio == NULL) {
          inizio = strchr(keep, '\n');
        }
        if (inizio != NULL) {
          unsigned via = (unsigned)(inizio + 1 - keep);
          memmove(keep, inizio + 1, kept - via);
          kept -= via;
        } else {
          kept = 0;
        }
      }
    }
  }
  keep[kept] = '\0';

  FILE *fp = fopen("/BMC64-DIAG.TXT", "w");
  if (fp == NULL) {
    return;
  }
  if (kept > 0) {
    fwrite(keep, 1, kept, fp);
  }
  fprintf(fp, "\n========================================\n");
  fprintf(fp, "BMC64 - diagnosi di avvio\n");
  fprintf(fp, "kernel            %s\n", GetKernelName());
  fprintf(fp, "machine_timing    %u\n", mViceOptions.GetMachineTiming());
  fprintf(fp, "enable_dpi        %d\n", mViceOptions.DPIEnabled() ? 1 : 0);
  fprintf(fp, "audio_out         %d (0=auto 1=jack 2=hdmi 3=usb)\n",
          (int)mViceOptions.GetAudioOut());
  fprintf(fp, "audio vchiq forz. %d\n", mViceOptions.AudioForceVCHIQ() ? 1 : 0);
  fprintf(fp, "modello del Pi    %s\n", CMachineInfo::Get()->GetMachineName());

  fprintf(fp, "revisione         %06x\n",
          (unsigned)CMachineInfo::Get()->GetRevisionRaw());
  fprintf(fp, "jack cuffie       %s\n",
          uscita_audio_ha_il_jack(CMachineInfo::Get()->GetMachineModel())
              ? "si'" : "no (audio_out=analog suona dall'HDMI)");
  fprintf(fp, "\n-- video --\n");
  fprintf(fp, "display firmware  %u x %u\n", raspi_disp_w, raspi_disp_h);
  fprintf(fp, "display aperto    %u  di %u\n", raspi_fb_display,
          raspi_fb_displays);
  fprintf(fp, "display chiesto   %d  (fb_display, Main HDMI Output)\n",
          circle_fb_display_chiesto());
  fprintf(fp, "composito AV      %s\n",
          !circle_composito_chiesto() ? "no"
          : fbl_stato_scalatore()
              ? "si' (composito=1: HDMI spenta, jack, strati sullo "
                "scalatore della GPU)"
              : "si' (composito=1: HDMI spenta, jack, schermo al firmware, "
                "composto dal core 3)");
  fprintf(fp, "framebuffer chies %u x %u\n", raspi_fb_ask_w, raspi_fb_ask_h);
  fprintf(fp, "framebuffer avuto %u x %u\n", raspi_fb_got_w, raspi_fb_got_h);
  fprintf(fp, "pitch / size      %u / %u byte\n", raspi_fb_pitch,
          raspi_fb_size);
  fprintf(fp, "pagine            %u\n", raspi_fb_pages);
  fprintf(fp, "scalatore (kms)   %d  (1 = lo schermo di BMX sull'HVS, 0 = ripiego sul framebuffer del firmware)\n",
          fbl_stato_scalatore());
  fprintf(fp, "\n-- lo schermo di BMX: le righe \"boot:\" del driver --\n");
  bmc_righe_boot_scrivi(fp);

  fprintf(fp, "\n-- cosa e' stato CHIESTO al firmware --\n");
  {
    static const char *cfg_keys[] = {
      "hdmi_group", "hdmi_mode", "hdmi_timings", "hdmi_cvt", "hdmi_drive",
      "sdtv_mode", "dpi_group", "dpi_mode", "dpi_timings",
      "dpi_output_format", "enable_dpi_lcd", "display_default_lcd",
      "kernel=", "arm_freq", NULL
    };
    static const char *cmd_keys[] = { "machine_timing", "hdmi_group",
                                      "v3dcrt", NULL };
    fprintf(fp, "config.txt:\n");
    diag_echo_file(fp, "/config.txt", cfg_keys);
    fprintf(fp, "cmdline.txt:\n");
    diag_echo_file(fp, "/cmdline.txt", cmd_keys);
  }

  fprintf(fp, "\nprova modo video in corso: %s\n",
          vidtrial_pending() ? "SI" : "no");
  fclose(fp);
}

void ViceStdioApp::InitBootStat() {
  FILE *fp;
#if defined(RASPI_SCPU64)
  fp = fopen("/SCPU64/bootstat.txt", "r");
#elif defined(RASPI_C64)
  fp = fopen("/C64/bootstat.txt", "r");
#elif defined(RASPI_C128)
  fp = fopen("/C128/bootstat.txt", "r");
#elif defined(RASPI_VIC20)
  fp = fopen("/VIC20/bootstat.txt", "r");
#elif defined(RASPI_PLUS4)
  fp = fopen("/PLUS4/bootstat.txt", "r");
#elif defined(RASPI_PLUS4EMU)
  fp = NULL;
#elif defined(RASPI_PET)
  fp = fopen("/PET/bootstat.txt", "r");
#else
  #error Unknown RASPI_ variant
#endif

  if (fp == NULL) {
    printf("Could not find bootstat. Using default list.\n");

    CGlueStdioInitBootStat(dflt_bootStatNum, dflt_bootStatWhat,
                           dflt_bootStatFile, dflt_bootStatSize);

    return;
  }

  char line[80];
  int num = 0;
  while (fgets(line, 79, fp)) {
    if (feof(fp))
      break;
    if (strlen(line) == 0)
      continue;
    if (line[0] == '#')
      continue;
    char *what = strtok(line, ",");
    if (what == NULL)
      continue;
    char *file = strtok(NULL, ",");
    if (file == NULL)
      continue;
    char *size = strtok(NULL, ",");
    if (size == NULL)
      continue;
    if (size[strlen(size) - 1] == '\n') {
      size[strlen(size) - 1] = '\0';
    }

    if (num >= MAX_BOOTSTAT_LINES) {
      printf("Warning: bootstat.txt too long, max %d entries\n",
             MAX_BOOTSTAT_LINES);
      break;
    }

    if (strcmp(what, "stat") == 0) {
      if (strcmp(file, "d1541II") == 0) {
        // Ignore legacy d1541II faking found file without a fully
        // qualified path.
        printf("Ignoring d1541II in bootstat.txt\n");
        continue;
      }
      mBootStatWhat[num] = BOOTSTAT_WHAT_STAT;
    } else if (strcmp(what, "fail") == 0) {
      if (strcmp(file,"rpi_pos.vkm") == 0) {
        // Ignore legacy mistake blocking rpi_pos.vkm
        printf("Ignoring rpi_pos.vkm in bootstat.txt\n");
        continue;
      }
      mBootStatWhat[num] = BOOTSTAT_WHAT_FAIL;
    } else {
      printf("Ignoring unknown bootstat.txt '%s'\n", what);
      continue;
    }

    // These never get freed...
    mBootStatFile[num] = (char *)malloc(MAX_BOOTSTAT_FLEN);
    strncpy(mBootStatFile[num], file, MAX_BOOTSTAT_FLEN - 1);
    mBootStatFile[num][MAX_BOOTSTAT_FLEN - 1] = '\0';
    mBootStatSize[num] = atoi(size);

    num++;
  }

  fclose(fp);

  CGlueStdioInitBootStat(num, mBootStatWhat, (const char **)mBootStatFile,
                         mBootStatSize);
}

void ViceStdioApp::DisableBootStat() {
  CGlueStdioInitBootStat(0, nullptr, nullptr, nullptr);
}


static bool LeggiIndirizzo(const char *testo, u8 *byte) {
  unsigned a, b, c, d;
  char dopo = 0;
  int n = sscanf(testo, "%u.%u.%u.%u%c", &a, &b, &c, &d, &dopo);
  if (n < 4 || (n == 5 && dopo != '\n' && dopo != '\r' && dopo != ' ') ||
      a > 255 || b > 255 || c > 255 || d > 255) {
    return false;
  }
  byte[0] = a; byte[1] = b; byte[2] = c; byte[3] = d;
  return true;
}

void ViceStdioApp::LoadNetworkDevice() {
  memset(mStaticAddress, 0, sizeof(mStaticAddress));
  const char *settings_path;
#if defined(RASPI_SCPU64)
  settings_path = "/settings-scpu64.txt";
#elif defined(RASPI_C64)
  settings_path = "/settings.txt";
#elif defined(RASPI_C128)
  settings_path = "/settings-c128.txt";
#elif defined(RASPI_VIC20)
  settings_path = "/settings-vic20.txt";
#elif defined(RASPI_PLUS4EMU)
  settings_path = "/settings-plus4emu.txt";
#elif defined(RASPI_PLUS4)
  settings_path = "/settings-plus4.txt";
#elif defined(RASPI_PET)
  settings_path = "/settings-pet.txt";
#else
  return;
#endif

  FILE *settings = fopen(settings_path, "r");
  if (settings == nullptr) {
    return;
  }

  char line[64];
  while (fgets(line, sizeof(line), settings) != nullptr) {
    int network_device;
    if (sscanf(line, "network_device=%d", &network_device) == 1 &&
        network_device >= 0 && network_device <= 2) {
      mNetworkDevice = network_device;
      continue;
    }

    int timezone_offset_minutes;
    if (sscanf(line, "timezone_offset_minutes=%d",
               &timezone_offset_minutes) == 1 &&
        timezone_offset_minutes >= -12 * 60 &&
        timezone_offset_minutes <= 14 * 60) {
      mTimezoneOffsetMinutes = timezone_offset_minutes;
      continue;
    }

    int valore;
    if (sscanf(line, "timezone_dst=%d", &valore) == 1 &&
        valore >= 0 && valore <= 2) {
      mTimezoneDst = valore;
      continue;
    }
    if (sscanf(line, "network_address_mode=%d", &valore) == 1) {
      mAddressMode = valore == 1 ? 1 : 0;
      continue;
    }
    static const char *const chiavi[4] = {
        "network_ip=", "network_netmask=", "network_gateway=", "network_dns="};
    for (int i = 0; i < 4; i++) {
      size_t lunga = strlen(chiavi[i]);
      if (strncmp(line, chiavi[i], lunga) == 0 &&
          !LeggiIndirizzo(line + lunga, mStaticAddress[i])) {
        memset(mStaticAddress[i], 0, 4);
      }
    }
  }
  fclose(settings);
}



CNetSubSystem *ViceStdioApp::NuovaRete(TNetDeviceType tipo) {
  const u8 *ip = 0, *maschera = 0, *gateway = 0, *dns = 0;
  static const u8 maschera24[4] = {255, 255, 255, 0};
  if (mAddressMode == 1 && (mStaticAddress[0][0] | mStaticAddress[0][1] |
                            mStaticAddress[0][2] | mStaticAddress[0][3])) {
    ip = mStaticAddress[0];
    maschera = (mStaticAddress[1][0] | mStaticAddress[1][1] |
                mStaticAddress[1][2] | mStaticAddress[1][3])
                   ? mStaticAddress[1] : maschera24;
    gateway = mStaticAddress[2];
    dns = mStaticAddress[3];
    mLogger.Write(GetKernelName(), LogNotice, "Networking: static IP %u.%u.%u.%u",
                  ip[0], ip[1], ip[2], ip[3]);
  }
  return new CNetSubSystem(ip, maschera, gateway, dns, "BMC64-NG", tipo);
}

void ViceStdioApp::InitializeNetwork() {
  if (mNetworkDevice == 0) {
    SetNetworkStatus(CIRCLE_NETWORK_DISABLED);
    mLogger.Write(GetKernelName(), LogNotice, "Networking not enabled");
    return;
  }

  if (mNetworkDevice == 1 &&
      ViceNetworkHasOnboardEthernet(mMachineInfo.GetMachineModel())) {
    SetNetworkStatus(CIRCLE_NETWORK_ETHERNET_INITIALIZING);
    mNet = NuovaRete(NetDeviceTypeEthernet);
    if (!mNet->Initialize(FALSE)) {
      SetNetworkStatus(CIRCLE_NETWORK_ETHERNET_INIT_FAILED);
      mLogger.Write(GetKernelName(), LogError,
                    "Cannot initialize Ethernet network stack");
      delete mNet;
      mNet = nullptr;
    } else {
      ViceNetworkSetSubsystem(mNet);
      StartNetworkTimeSync(mNet);
      SetNetworkStatus(CIRCLE_NETWORK_ETHERNET_WAITING_FOR_DHCP);
      mLogger.Write(GetKernelName(), LogNotice, "Networking: Ethernet initialized");
    }
    return;
  }

  if (mNetworkDevice == 1) {
    mLogger.Write(GetKernelName(), LogNotice,
                  "Ethernet selected, but this Raspberry Pi has no onboard Ethernet; disabling networking");
    mNetworkDevice = 0;
    SetNetworkStatus(CIRCLE_NETWORK_DISABLED);
    return;
  }

  if (!ViceNetworkHasOnboardWifi(mMachineInfo.GetMachineModel())) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_UNAVAILABLE);
    mLogger.Write(GetKernelName(), LogError,
                  "Wi-Fi selected, but this Raspberry Pi has no onboard WLAN");
    return;
  }

  CString firmwarePath;
  CString configPath;
  firmwarePath.Format("%s:/firmware/", mViceOptions.GetDiskVolume());
  configPath.Format("%s:/wpa_supplicant.conf", mViceOptions.GetDiskVolume());

  if (!ViceNetworkHasWifiFirmware((const char *)firmwarePath)) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_FIRMWARE_MISSING);
    mLogger.Write(GetKernelName(), LogError,
                  "Wi-Fi firmware is missing from %s",
                  (const char *)firmwarePath);
    return;
  }

  FIL configFile;
  if (f_open(&configFile, (const char *)configPath, FA_READ) != FR_OK) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_CONFIG_MISSING);
    mLogger.Write(GetKernelName(), LogError,
                  "Wi-Fi enabled but WPA config is missing: %s",
                  (const char *)configPath);
    return;
  }
  f_close(&configFile);

  SetNetworkStatus(CIRCLE_NETWORK_WIFI_DEVICE_INITIALIZING);
  mWLAN = new CBcm4343Device((const char *)firmwarePath);
  if (!mWLAN->Initialize()) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_DEVICE_INIT_FAILED);
    mLogger.Write(GetKernelName(), LogError, "Cannot initialize WLAN");
    delete mWLAN;
    mWLAN = nullptr;
    return;
  }

  mNet = NuovaRete(NetDeviceTypeWLAN);
  if (!mNet->Initialize(FALSE)) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_NETWORK_INIT_FAILED);
    mLogger.Write(GetKernelName(), LogError, "Cannot initialize WLAN network stack");
    delete mNet;
    mNet = nullptr;
    delete mWLAN;
    mWLAN = nullptr;
    return;
  }
  ViceNetworkSetSubsystem(mNet);
  StartNetworkTimeSync(mNet);
  SetNetworkStatus(CIRCLE_NETWORK_WIFI_WPA_INITIALIZING);
  mLogger.Write(GetKernelName(), LogNotice, "Networking: Wi-Fi initialized");

  mWPASupplicant = new CWPASupplicant((const char *)configPath);
  if (!mWPASupplicant->Initialize()) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_WPA_INIT_FAILED);
    mLogger.Write(GetKernelName(), LogError, "Cannot initialize WPA supplicant");
    delete mWPASupplicant;
    mWPASupplicant = nullptr;
  } else {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_CONNECTING);
  }
}

void ViceStdioApp::SetNetworkStatus(int status) {
  if (mNetworkStatus == status) {
    return;
  }
  mNetworkStatus = status;
  ViceNetworkNotifyStatusChanged();
}

int ViceStdioApp::GetNetworkStatus(void) const {
  if (mNet != nullptr && mNet->IsRunning()) {
    CString address;
    mNet->GetConfig()->GetIPAddress()->Format(&address);
    if (strcmp((const char *)address, "0.0.0.0") != 0) {
      return mNetworkDevice == 1 ? CIRCLE_NETWORK_ETHERNET_CONNECTED
                                 : CIRCLE_NETWORK_WIFI_CONNECTED;
    }
  }

  return mNetworkStatus;
}

int ViceStdioApp::WifiIsRunning(void) const {
  return ViceNetworkHasOnboardWifi(mMachineInfo.GetMachineModel()) && mWLAN != nullptr;
}

int ViceStdioApp::ConnectWifi(void) {
  if (!ViceNetworkHasOnboardWifi(mMachineInfo.GetMachineModel())) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_UNAVAILABLE);
    return 0;
  }

  if (mWLAN == nullptr) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_DEVICE_NOT_INITIALIZED);
    mLogger.Write(GetKernelName(), LogError,
                  "Wi-Fi connection requested without WLAN");
    return 0;
  }

  CString config_path;
  config_path.Format("%s:/wpa_supplicant.conf", mViceOptions.GetDiskVolume());
  SetNetworkStatus(CIRCLE_NETWORK_WIFI_CONNECTING);
  mLogger.Write(GetKernelName(), LogNotice,
                "Wi-Fi reconnecting with %s", (const char *)config_path);
  delete mWPASupplicant;
  mWPASupplicant = new CWPASupplicant((const char *)config_path);
  if (!mWPASupplicant->Initialize()) {
    SetNetworkStatus(CIRCLE_NETWORK_WIFI_WPA_INIT_FAILED);
    mLogger.Write(GetKernelName(), LogError,
                  "Cannot restart WPA supplicant");
    delete mWPASupplicant;
    mWPASupplicant = nullptr;
    return 0;
  }

  unsigned int connect_started_at = CTimer::GetClockTicks();
  while (!CWPASupplicant::IsConnected()) {
    if ((unsigned int)(CTimer::GetClockTicks() - connect_started_at) >=
        WIFI_CONNECT_TIMEOUT_US) {
      mLogger.Write(GetKernelName(), LogError,
                    "Wi-Fi connection timed out after %u seconds",
                    WIFI_CONNECT_TIMEOUT_US / 1000000);
      SetNetworkStatus(CIRCLE_NETWORK_WIFI_CONNECTION_TIMEOUT);
      return 0;
    }
    CScheduler::Get()->MsSleep(100);
  }
  mLogger.Write(GetKernelName(), LogNotice, "Wi-Fi connected");
  SetNetworkStatus(CIRCLE_NETWORK_WIFI_CONNECTED);
  return 1;
}

int ViceStdioApp::ScanWifiAccessPoints(struct wifi_access_point *access_points,
                                       unsigned int max_access_points) {
  if (!ViceNetworkHasOnboardWifi(mMachineInfo.GetMachineModel())) {
    return 0;
  }



  if (mWLAN == nullptr) {
    CString firmwarePath;
    firmwarePath.Format("%s:/firmware/", mViceOptions.GetDiskVolume());
    if (!ViceNetworkHasWifiFirmware((const char *)firmwarePath)) {
      mLogger.Write(GetKernelName(), LogError,
                    "Wi-Fi scan: firmware is missing from %s",
                    (const char *)firmwarePath);
      return 0;
    }
    mWLAN = new CBcm4343Device((const char *)firmwarePath);
    if (!mWLAN->Initialize()) {
      mLogger.Write(GetKernelName(), LogError, "Wi-Fi scan: cannot initialize WLAN");
      delete mWLAN;
      mWLAN = nullptr;
      return 0;
    }
    mLogger.Write(GetKernelName(), LogNotice, "Wi-Fi scan: WLAN started for the scan");
  }

  uint8_t buffer[FRAME_BUFFER_SIZE];
  unsigned int result_length;
  while (mWLAN->ReceiveScanResult(buffer, &result_length)) {
  }

  if (!mWLAN->Control("escan %u", 3)) {
    mLogger.Write(GetKernelName(), LogError, "Cannot start Wi-Fi scan");
    return 0;
  }

  unsigned int count = 0;
  unsigned int result_messages = 0;
  unsigned int scan_started_at = CTimer::GetClockTicks();
  do {
    count = ViceNetworkCollectWifiScanResults(mWLAN, access_points, max_access_points,
                                   count, &result_messages);
    CScheduler::Get()->MsSleep(100);
  } while ((unsigned int)(CTimer::GetClockTicks() - scan_started_at) <
           WIFI_SCAN_DURATION_US);

  mWLAN->Control("escan 0");
  count = ViceNetworkCollectWifiScanResults(mWLAN, access_points, max_access_points,
                                 count, &result_messages);
  mLogger.Write(GetKernelName(), LogNotice,
                "Wi-Fi scan received %u results, found %u access points",
                result_messages, count);
  return count;
}
bool ViceStdioApp::Initialize(void) {
  ViceNetworkSetStdioApp(this);
  if (!ViceScreenApp::Initialize()) {
    return false;
  }

  if (!mEMMC.Initialize()) {


    mLogger.Write(GetKernelName(), LogError,
                  "EMMC: Initialize() ha detto no, non si prosegue");
    return false;
  }

  int partition = mViceOptions.GetDiskPartition();
  int ss = 0;
  if (partition > 4) {
    // User is forcing a start sector by specifying
    // a partition above 4. Tell glue code partition
    // is 5 and this will set the start sector to what
    // they provided when the disk is mounted.
    ss = partition;
    partition = 5;
  }

  // When mounting, fatfs gets ":" appended.  But StdioInit
  // does not.
  const char *volumeName = mViceOptions.GetDiskVolume();
  char fatFsVol[VOLUME_NAME_LEN];
  strncpy(fatFsVol, volumeName, VOLUME_NAME_LEN - 2);
  strcat(fatFsVol, ":");

  CGlueStdioSetPartitionForVolume(volumeName, partition, ss);

  if (f_mount(&mFileSystemSD, fatFsVol, 1) != FR_OK) {
    mLogger.Write(GetKernelName(), LogError, "Cannot mount partition: %s",
                  fatFsVol);
    return false;
  }






  f_mount(&mFileSystemFLP1, "FLP1:", 0);
  f_mount(&mFileSystemFLP2, "FLP2:", 0);

  WriteBootDiag();




  usblog_avvio();
  InitBootStat();


  bmc_core0_dove = "rete (WiFi, Ethernet)";
  {






    PeripheralEntry();
    u32 hi = read32(ARM_SYSTIMER_CHI);
    u32 lo = read32(ARM_SYSTIMER_CLO);
    if (hi != read32(ARM_SYSTIMER_CHI)) {
      hi = read32(ARM_SYSTIMER_CHI);
      lo = read32(ARM_SYSTIMER_CLO);
    }
    const u32 rsts = read32(ARM_PM_RSTS);
    PeripheralExit();
    const unsigned long soc_ms = ((((unsigned long)hi) << 32) | lo) / 1000;
    const unsigned long nostro_ms = (unsigned long)(CTimer::GetClockTicks64() / 1000);
    char riga[176];
    snprintf(riga, sizeof riga,
             "orologio del SoC %lu ms, nostro %lu ms: prima di noi %ld ms; RSTS 0x%08x",
             soc_ms, nostro_ms, (long)soc_ms - (long)nostro_ms, (unsigned)rsts);
    passo_nota(riga);
  }
  passo_nota("core 0: rete, inizio");
  LoadNetworkDevice();
  if (!ConfigureDaylightSaving(mTimezoneOffsetMinutes, mTimezoneDst)) {
    mLogger.Write(GetKernelName(), LogWarning, "Cannot configure timezone");
  }
  InitializeNetwork();
  passo_nota("core 0: rete, fatta");
  bmc_core0_dove = "lancio del VICE";

  // Now that emmc is initialized, launch
  // the emulator main loop on CORE 1 before USBHCII.
  int timing_int = mViceOptions.GetMachineTiming();
  if (timing_int == MACHINE_TIMING_NTSC_HDMI ||
      timing_int == MACHINE_TIMING_NTSC_CUSTOM_HDMI ||
      timing_int == MACHINE_TIMING_NTSC_COMPOSITE ||
      timing_int == MACHINE_TIMING_NTSC_DPI ||
      timing_int == MACHINE_TIMING_NTSC_CUSTOM_DPI) {
    strcpy(mTimingOption, "-ntsc");
  } else {
    strcpy(mTimingOption, "-pal");
  }
















  const bool dopo_usb = mViceOptions.ViceDopoUsb();
#ifdef ARM_ALLOW_MULTI_CORE
  if (!dopo_usb) {
    passo_nota("core 0: lancio del VICE, poi la USB");

    bmc_vice_partito = 1;
    DataMemBarrier();
    mEmulatorCore->LaunchEmulator(mTimingOption);
  } else {
    passo_nota("core 0: prima la USB, poi il VICE");
  }
#endif

#if RASPPI >= 4





  CUSBDeviceFactory::EnableAudioClass(
      mViceOptions.GetAudioOut() == VCHIQSoundDestinationUSB);
#endif

  // This takes 1.5 seconds to init.
  bmc_core0_dove = "USB: inizializzazione (1,5 s)";
  if (!mUSBHCII.Initialize()) {
    return false;
  }


















  bmc_dopo_usb_da_scrivere = 1;
  bmc_usb_pronta = 1;
  DataMemBarrier();
  bmc_core0_dove = "USB fatta";
#ifdef ARM_ALLOW_MULTI_CORE
  if (dopo_usb) {
    passo_nota("core 0: USB fatta, lancio del VICE");
    bmc_vice_partito = 1;
    DataMemBarrier();
    mEmulatorCore->LaunchEmulator(mTimingOption);
  }
#endif

  return true;
}

void ViceStdioApp::Cleanup(void) {
  ViceNetworkSetStdioApp(nullptr);
  delete mWPASupplicant;
  ViceNetworkSetSubsystem(nullptr);
  delete mNet;
  delete mWLAN;

  // When mounting, fatfs gets ":" appended.  But StdioInit
  // does not.
  const char *volumeName = mViceOptions.GetDiskVolume();
  char fatFsVol[VOLUME_NAME_LEN];
  strncpy(fatFsVol, volumeName, VOLUME_NAME_LEN - 2);
  strcat(fatFsVol, ":");

  if (f_mount(0, fatFsVol, 0) != FR_OK) {
    mLogger.Write(GetKernelName(), LogError, "Cannot unmount drive");
  }
  ViceScreenApp::Cleanup();
}

void ViceStdioApp::circle_find_usb(int (*usb)[3]) {
  CDevice* usb1 = CDeviceNameService::Get()->GetDevice ("umsd1", TRUE);
  (*usb)[0] = usb1 ? 1 : 0;
  CDevice* usb2 = CDeviceNameService::Get()->GetDevice ("umsd2", TRUE);
  (*usb)[1] = usb2 ? 1 : 0;
  CDevice* usb3 = CDeviceNameService::Get()->GetDevice ("umsd3", TRUE);
  (*usb)[2] = usb3 ? 1 : 0;
}

int ViceStdioApp::circle_mount_usb(int usb) {
  int status;
  switch (usb) {
     case 0:
       status = f_mount(&mFileSystemUSB1, "USB:", 1);
       break;
     case 1:





       status = f_mount(&mFileSystemUSB2, "USB2:", 1);
       break;
     case 2:
       status = f_mount(&mFileSystemUSB3, "USB3:", 1);
       break;
     default: return 0;
  }

  if (status != FR_OK) {
    mLogger.Write(GetKernelName(), LogError, "Cannot mount usb %d", usb);
    return 0;
  }

  return 1;
}

int ViceStdioApp::circle_unmount_usb(int usb) {
  int status;
  switch (usb) {
     case 0:
       status = f_mount(0, "USB:", 1);
       break;
     case 1:
       status = f_mount(0, "USB2:", 1);
       break;
     case 2:
       status = f_mount(0, "USB3:", 1);
       break;
     default: return 0;
  }

  if (status != FR_OK) {
    mLogger.Write(GetKernelName(), LogError, "Cannot unmount usb %d", usb);
    return 0;
  }

  return 1;
}




void ViceStdioApp::circle_find_floppy(int (*flp)[2]) {
  (*flp)[0] = CDeviceNameService::Get()->GetDevice("ufd1", TRUE) ? 1 : 0;
  (*flp)[1] = CDeviceNameService::Get()->GetDevice("ufd2", TRUE) ? 1 : 0;
}






