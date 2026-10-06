/*
 * menu.c
 *
 * Written by
 *  Randy Rossi <randy.rossi@gmail.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "menu.h"

#include <math.h>
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// RASPI Includes
#include "emux_api.h"
#include "../smb/src/bmc_nas.h"
#include "crt_preset.h"
#include "demo.h"
#include "joy.h"
#include "kbd.h"
#include "text.h"
#include "menu_confirm_osd.h"
#include "menu_reset_osd.h"
#include "menu_tape_osd.h"
#include "menu_timing.h"
#include "menu_usb.h"
#include "menu_wifi.h"
#include "menu_keyset.h"
#include "menu_switch.h"
#include "menu_gpio.h"
#include "overlay.h"
#include "raspi_util.h"
#include "ui.h"
#include "usb_gamepad_defaults.h"

extern void reboot(void);












static int uscita_avvisata = 0;

static int salva_prima_di_uscire(void) {
  if (uscita_avvisata || emux_prepare_shutdown() == 0) {
    return 0;
  }
  uscita_avvisata = 1;
  if (ui_enabled) {
    ui_error("REU or EasyFlash file NOT saved. Choose again to quit anyway.");
  } else {
    overlay_avviso("NOT SAVED - AGAIN TO QUIT");
  }
  return -1;
}








static void prima_del_reset(const char *cosa) {
  fflush(NULL);
  passo_nota(cosa);
  circle_sleep(500000);
}

static void riavvia(void) {
  if (salva_prima_di_uscire() == 0) {
    prima_del_reset("riavvio: mezzo secondo alla scheda, poi il reset");
    reboot();
  }
}

static void spegni(void) {
  if (salva_prima_di_uscire() == 0) {
    prima_del_reset("spegnimento: mezzo secondo alla scheda, poi il reset");
    circle_poweroff();
  }
}





#define NOME_PROGETTO "BMC64-NG"






#define VERSIONE_BMC64_NG "1.1.1"

#ifdef RASPI_LITE
#define VARIANT_STRING "-Lite"
#else
#define VARIANT_STRING ""
#endif







const char *bmc64_version_string(void) {
  return NOME_PROGETTO VARIANT_STRING " (" __DATE__ " " __TIME__ ")";
}

#define DEFAULT_VICII_H_STRETCH 1200
#define DEFAULT_VICII_V_STRETCH 1000

#define DEFAULT_VIC_H_STRETCH 1450
#define DEFAULT_VIC_V_STRETCH 1000

#define DEFAULT_VDC_H_STRETCH 1450
#define DEFAULT_VDC_V_STRETCH 1000



#define OC_MSG "The new CPU clock only takes effect after a restart. " \
               "config.txt will be rewritten. Reboot now?"
#define GPU_MSG "The new GPU clock only takes effect after a restart. " \
                "config.txt will be rewritten. Reboot now?"





#define SWITCH_MSG "The Pi restarts with the new machine. " \
                   "The card will start it on any Pi 4, 400, " \
                   "5 or 500. Reboot now?"



#define AUDIO_OUT_MSG "The sound device is chosen at boot. cmdline.txt " \
                      "is rewritten and the Pi reboots. Auto means: try " \
                      "HDMI first, fall back to the headphone jack if " \
                      "the display takes no audio. Jack needs a Pi that " \
                      "has one (not the 400, the CM4 or any Pi 5)."

#define POWEROFF_MSG "Cuts the power. Press the keyboard power key " \
                     "(Fn+F10) to switch it back on." 

#define REBOOTPI_MSG "Restarts the Raspberry from scratch. Anything not " \
                     "saved is lost."


#define DUE_HDMI_MSG "Two HDMI needs a restart: the C128 comes back " \
                     "with the VDC on the other HDMI. No CRT shaders " \
                     "in this mode. Reboot now?"


#define HDMI_PRINCIPALE_MSG "The main screen and the HDMI sound go to the " \
                            "chosen port after a restart. HDMI 1: no CRT " \
                            "shader. C128 with Two HDMI: the VIC-II goes " \
                            "there, the VDC to the other one. Reboot now?"



#define SWITCH_TIMER_MSG "Without the timer, a machine whose video mode " \
                         "your display cannot show stays BLACK and does " \
                         "NOT come back by itself: config.txt must then " \
                         "be fixed on a PC. Turn the timer off?"

#define SWITCH_FAIL_MSG "Something went wrong. File a bug with the error " \
                        "code above. You may have to manually edit " \
                        "config.txt and/or cmdline.txt to restore boot."

// For filename filters
typedef enum {
   FILTER_NONE,
   FILTER_DISK,
   FILTER_CART,
   FILTER_TAPE,
   FILTER_SNAP,
   FILTER_DIRS,
   FILTER_PRGS,
   FILTER_IDE64,





   FILTER_D81,


   FILTER_SID,


   FILTER_REU,



   FILTER_D64,
} FileFilter;

// These can be saved




static struct menu_item *sid_titolo_item;
static struct menu_item *sid_autore_item;
static struct menu_item *sid_diritti_item;
static struct menu_item *sid_brano_item;



static void sid_aggiorna_righe(void);

struct menu_item *port_1_menu_item;
struct menu_item *port_2_menu_item;
struct menu_item *port_3_menu_item;
struct menu_item *port_4_menu_item;
int usb_pref[MAX_USB_DEVICES];
int usb_x_axis[MAX_USB_DEVICES];
int usb_y_axis[MAX_USB_DEVICES];
float usb_x_thresh[MAX_USB_DEVICES];
float usb_y_thresh[MAX_USB_DEVICES];
int usb_button_assignments[MAX_USB_DEVICES][MAX_USB_BUTTONS];
int usb_button_bits[MAX_USB_BUTTONS]; // never change



static int real_drive_wanted[4];




static struct menu_item *realdrive_log_item = NULL;
static struct menu_item *realdrive_respiro_item = NULL;
static struct menu_item *realdrive_veloce_item = NULL;




static const int realdrive_respiri[] = { 0, 1000, 2000, 5000 };
#define REALDRIVE_RESPIRI_NUM ((int)(sizeof(realdrive_respiri) / sizeof(realdrive_respiri[0])))
long keyset_codes[2][7];
long key_bindings[6];




#define DRIVE_MOD_1541II 1542
#define DRIVE_MOD_1551   1551
#define DRIVE_MOD_1571   1571
#define DRIVE_MOD_1571CR 1573
#define DRIVE_MOD_1581   1581

static int estensione_e(const char *nome, const char *est) {
  const char *p = strrchr(nome, '.');
  if (p == NULL) {
    return 0;
  }
  p++;
  while (*p && *est) {
    if ((*p | 32) != (*est | 32)) {
      return 0;
    }
    p++;
    est++;
  }
  return *p == '\0' && *est == '\0';
}




static int modello_per_immagine(const char *nome, int adesso);

static void modello_secondo_immagine(const char *nome, int unita) {












  emux_leave_real_drive(unita);
  emux_use_emulated_drive(unita);







  int ora = -1;
  int serve;
  emux_get_int_1(Setting_DriveNType, &ora, unita);
  if (ora < 0) {
    return;
  }
  serve = modello_per_immagine(nome, ora);
  if (serve != ora) {
    emux_set_int_1(Setting_DriveNType, serve, unita);
  }
}



static int modello_per_immagine(const char *nome, int adesso) {
  if (estensione_e(nome, "d81")) {
    return (adesso == DRIVE_MOD_1581) ? adesso : DRIVE_MOD_1581;
  }
  if (estensione_e(nome, "d71") || estensione_e(nome, "g71")) {
    return (adesso == DRIVE_MOD_1571 || adesso == DRIVE_MOD_1571CR)
             ? adesso : DRIVE_MOD_1571;
  }
  if (estensione_e(nome, "d64") || estensione_e(nome, "g64") ||
      estensione_e(nome, "d67") || estensione_e(nome, "p64") ||
      estensione_e(nome, "x64")) {
    switch (adesso) {
      case 1540:
      case 1541:
      case DRIVE_MOD_1541II:
      case DRIVE_MOD_1551:
      case 1570:
      case DRIVE_MOD_1571:
      case DRIVE_MOD_1571CR:
        return adesso;
      default:
        return DRIVE_MOD_1541II;
    }
  }
  return adesso;
}


char attached_disk_name[4][MAX_STR_VAL_LEN];

// Lower byte is BTN_ASSIGN_ constant. Upper byte is port or other arg.
unsigned int gpio_bindings[NUM_GPIO_PINS];

struct menu_item *drive_sounds_item;
struct menu_item *drive_sounds_vol_item;
struct menu_item *audio_out_item;
struct menu_item *tape_sounds_item;
struct menu_item *tape_sounds_vol_item;





static struct menu_item *menu_radice = NULL;

struct menu_item *hotkey_cf1_item;
struct menu_item *hotkey_cf3_item;
struct menu_item *hotkey_cf5_item;
struct menu_item *hotkey_cf7_item;
struct menu_item *hotkey_tf1_item;
struct menu_item *hotkey_tf3_item;
struct menu_item *hotkey_tf5_item;
struct menu_item *hotkey_tf7_item;
struct menu_item *volume_item;
struct menu_item *statusbar_item;
struct menu_item *statusbar_padding_item;
struct menu_item *statusbar_info_item;

static unsigned long vidtrial_deadline = 0;
static int vidtrial_armed = 0;
#define VIDTRIAL_SECONDS 15


static const char *overclock_label(int level);
void show_overclock_menu(void);
void show_gpuclock_menu(void);
struct menu_item *tape_reset_with_machine_item;
struct menu_item *vkbd_transparency_item;

struct menu_item *palette_item[2];
struct menu_item *brightness_item[2];
struct menu_item *contrast_item[2];
struct menu_item *gamma_item[2];
struct menu_item *tint_item[2];
struct menu_item *saturation_item[2];

struct menu_item *warp_item;
struct menu_item *reset_confirm_item;
struct menu_item *drive_flush_item;

static struct menu_item *switch_timer_item;
struct menu_item *gpio_config_item;
struct menu_item *active_display_item;
struct menu_item *secondo_hdmi_item;
struct menu_item *hdmi_principale_item;



static int display_all_avvio = 0;
static int doppio_in_cmdline = -1;
static void doppio_hdmi_riavvia(int salva);
static void doppio_hdmi_cmdline(int voglio);
static void due_hdmi_nel_giro(void);



static struct menu_item *network_device_item;
static struct menu_item *network_status_item;
static struct menu_item *network_ip_address_item;
static struct menu_item *network_modem_address_item;
static struct menu_item *timezone_offset_item;
static struct menu_item *wifi_settings_item;
static struct menu_item *wifi_ssid_item;
static struct menu_item *wifi_security_item;
static struct menu_item *wifi_country_item;
static struct menu_item *wifi_connect_item;
static struct menu_item *wifi_psk_item;

static struct menu_item *spinner_sens_item;
extern void emu_spinner_set_sensitivity(int percento);


static char wifi_country_saved[3] = "US";
static struct menu_item *network_address_mode_item;
static struct menu_item *network_static_item[4];
static struct menu_item *timezone_dst_item;
static struct menu_item *smb_password_item;
static struct menu_item *network_modem_item;
static const char *const network_static_keys[4] = {
    "network_ip", "network_netmask", "network_gateway", "network_dns"};
static void update_network_address_items(void);
static char *fullpath(DirType dir_type, char *name);
static char wifi_psk[MAX_STR_VAL_LEN];


static char wifi_psk_letta[MAX_STR_VAL_LEN];
static int saved_network_device;
static int network_device_was_selected;
static int network_reboot_prompted;

static const int acia_network_addresses[] = CIRCLE_ACIA_NETWORK_ADDRESS_VALUES;
static const char *const acia_network_address_labels[] =
  CIRCLE_ACIA_NETWORK_ADDRESS_LABELS;

static void configure_timezone_offsets(struct menu_item *item) {
  int index = 0;

  for (int offset = -12 * 60; offset <= 14 * 60; offset += 30) {
    int absolute_offset = offset < 0 ? -offset : offset;
    snprintf(item->choices[index], MAX_MENU_STR, "UTC%c%02d:%02d",
             offset < 0 ? '-' : '+', absolute_offset / 60,
             absolute_offset % 60);
    item->choice_ints[index++] = offset;

    if (offset == 330 || offset == 510 || offset == 750) {
      int quarter_hour_offset = offset + 15;
      snprintf(item->choices[index], MAX_MENU_STR, "UTC+%02d:%02d",
               quarter_hour_offset / 60, quarter_hour_offset % 60);
      item->choice_ints[index++] = quarter_hour_offset;
    }
  }

  item->num_choices = index;
}

static int timezone_offset_index(int offset) {
  for (int index = 0; index < timezone_offset_item->num_choices; index++) {
    if (timezone_offset_item->choice_ints[index] == offset) {
      return index;
    }
  }
  return 24;
}

static int acia_network_address_index(int address) {
  int index;
  for (index = 0;
       index < (int)(sizeof(acia_network_addresses) /
                     sizeof(acia_network_addresses[0])); index++) {
    if (acia_network_addresses[index] == address) {
      return index;
    }
  }
  return CIRCLE_ACIA_NETWORK_ADDRESS_DEFAULT;
}

static const char *const network_status_labels[CIRCLE_NETWORK_STATUS_COUNT] = {
    "Disabled",
    "Starting Ethernet",
    "Check Ethernet cable",
    "Ethernet connected",
    "Ethernet unavailable",
    "Ethernet init failed",
    "Wi-Fi config missing",
    "Starting Wi-Fi",
    "Wi-Fi device failed",
    "Wi-Fi network failed",
    "Starting Wi-Fi WPA",
    "Wi-Fi WPA failed",
    "Wi-Fi connecting",
    "Wi-Fi connected",
    "Wi-Fi timeout",
    "Wi-Fi unavailable",
    "Wi-Fi device missing",
    "Wi-Fi unsupported",
    "Missing Wi-Fi firmware"
};

static const char *network_status_label(int status) {
  if (status < 0 || status >= CIRCLE_NETWORK_STATUS_COUNT) {
    return "Unknown";
  }
  return network_status_labels[status];
}


static struct menu_item *temperatura_item;









static void riga_cpu(char *riga, size_t n, unsigned gpu) {
  if (gpu != 0) {
    snprintf(riga, n, "CPU %u C  %u MHz  GPU %u MHz", circle_get_temperature(),
             menu_arm_hz_pieno() / 1000000, gpu);
  } else {
    snprintf(riga, n, "CPU %u C   %u MHz", circle_get_temperature(),
             menu_arm_hz_pieno() / 1000000);
  }
}










static struct menu_item *ora_item;


static void civile_da_giorni(long z, int *anno, unsigned *mese,
                             unsigned *giorno) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  *giorno = doy - (153 * mp + 2) / 5 + 1;
  *mese = mp < 10 ? mp + 3 : mp - 9;
  *anno = (int)((long)yoe + era * 400 + (*mese <= 2));
}

static void riga_ora(char *riga, size_t n, int valida, unsigned locale) {
  static const char *const giorni[7] = {"Thu", "Fri", "Sat", "Sun",
                                        "Mon", "Tue", "Wed"};
  static const char *const mesi[12] = {"Jan", "Feb", "Mar", "Apr",
                                       "May", "Jun", "Jul", "Aug",
                                       "Sep", "Oct", "Nov", "Dec"};
  int anno;
  unsigned mese, giorno, sec;
  if (!valida) {
    snprintf(riga, n, "Clock: waiting for network time");
    return;
  }
  civile_da_giorni((long)(locale / 86400U), &anno, &mese, &giorno);
  sec = locale % 86400U;
  snprintf(riga, n, "%s %02u %s %04d  %02u:%02u:%02u",
           giorni[(locale / 86400U) % 7], giorno, mesi[mese - 1], anno,
           sec / 3600, (sec / 60) % 60, sec % 60);
}

static void aggiorna_ora(void) {
  unsigned locale = 0;
  int valida = circle_get_local_time(&locale);
  riga_ora(ora_item->name, MAX_MENU_STR, valida, locale);
}

void menu_update_network_status(void);

static void network_status_changed(void) {
  menu_update_network_status();
}





void menu_update_temperatura(void) {





  static unsigned long ultimo;
  static int mai = 1;
  unsigned long adesso;

  if (temperatura_item == NULL) {
    return;
  }
  adesso = circle_get_ticks();
  if (!mai && adesso - ultimo < 1000000UL) {
    return;
  }
  mai = 0;
  ultimo = adesso;
  riga_cpu(temperatura_item->name, MAX_MENU_STR, circle_get_gpu_mhz());
  if (ora_item != NULL) {
    aggiorna_ora();
  }
}























void menu_update_network_status(void) {
  if (network_status_item == NULL) {
    return;
  }

  char status[MAX_STR_VAL_LEN];
  int status_code = circle_get_network_status();
    strncpy(status, network_status_label(status_code), sizeof(status) - 1);
    status[sizeof(status) - 1] = '\0';
    strncpy(network_status_item->displayed_value, status,
      sizeof(network_status_item->displayed_value) - 1);
    network_status_item->displayed_value[
        sizeof(network_status_item->displayed_value) - 1] = '\0';

    if (network_ip_address_item != NULL) {
      char address[MAX_STR_VAL_LEN];
      const char *ip_address = " ";
      if (circle_get_network_ip_address(address, sizeof(address))) {
        ip_address = address;
      }
      strncpy(network_ip_address_item->str_value, ip_address,
        sizeof(network_ip_address_item->str_value) - 1);
      network_ip_address_item->str_value[
    sizeof(network_ip_address_item->str_value) - 1] = '\0';
      strncpy(network_ip_address_item->displayed_value, ip_address,
        sizeof(network_ip_address_item->displayed_value) - 1);
      network_ip_address_item->displayed_value[
    sizeof(network_ip_address_item->displayed_value) - 1] = '\0';
    }
    update_network_address_items();
}



static void update_network_address_items(void) {
  if (network_address_mode_item == NULL) {
    return;
  }
  int statico = network_address_mode_item->value == 1;
  for (int i = 0; i < 4; i++) {
    struct menu_item *campo = network_static_item[i];
    if (campo == NULL) {
      continue;
    }
    campo->disabled = !statico;
    if (!statico) {
      char valore[MAX_STR_VAL_LEN];
      if (circle_get_network_config(i, valore, sizeof(valore))) {
        strncpy(campo->str_value, valore, campo->max_length);
        campo->str_value[campo->max_length] = '\0';
        campo->value = strlen(campo->str_value);
      }
    }
  }
}





static void condivisione_chiusa(struct menu_item *old_root,
                                struct menu_item *new_root) {
  circle_smb_stop();
}

static void condividi_scheda(void) {
  char ip[MAX_STR_VAL_LEN];
  if (!circle_get_network_ip_address(ip, sizeof(ip))) {
    ui_error("No network: set it in Network");
    return;
  }
  emux_flush_disks_now(fullpath);
  const char *password = "bmc64";
  if (smb_password_item != NULL && smb_password_item->str_value[0] != '\0') {
    password = smb_password_item->str_value;
  }
  if (!circle_smb_start("bmc64", password)) {
    ui_error("Cannot start SD Card Sharing");
    return;
  }
  char riga[MAX_MENU_STR];
  struct menu_item *root = ui_push_menu(36, 10);
  root->on_popped_off = condivisione_chiusa;
  ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                     "SD Card Sharing is ON")->disabled = 1;
  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                     "On the PC open:")->disabled = 1;
  ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                     "\\\\BMC64-NG\\sdcard")->disabled = 1;
  snprintf(riga, sizeof(riga), "\\\\%s\\sdcard", ip);
  ui_menu_add_button(MENU_ID_DO_NOTHING, root, riga)->disabled = 1;
  snprintf(riga, sizeof(riga), "User bmc64  Password %s", password);
  ui_menu_add_button(MENU_ID_DO_NOTHING, root, riga)->disabled = 1;
  ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                     "Emulation is paused")->disabled = 1;
  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_SD_SHARING_STOP, root, "Stop Sharing");
  ui_select_first_interactive_item();
}

static void update_wifi_menu_enabled(void) {
  if (network_device_item != NULL &&
      ((network_device_item->value == 1 && !circle_has_onboard_ethernet()) ||
       (network_device_item->value == 2 && !circle_has_onboard_wifi()))) {
    network_device_item->value = 0;
    saved_network_device = 0;
  }


  int enabled = circle_has_onboard_wifi();
  update_network_address_items();
  if (wifi_settings_item) wifi_settings_item->disabled = !enabled;
  if (wifi_ssid_item) wifi_ssid_item->disabled = !enabled;
  if (wifi_security_item) wifi_security_item->disabled = !enabled;
  if (wifi_country_item) wifi_country_item->disabled = !enabled;
  if (wifi_connect_item) wifi_connect_item->disabled = !enabled;
}

struct menu_item *use_scaling_params_item[2];

struct menu_item *h_center_item[2];
struct menu_item *v_center_item[2];
struct menu_item *h_border_item[2];
struct menu_item *v_border_item[2];
struct menu_item *h_stretch_item[2];
static struct menu_item *meter_snd_item;
static struct menu_item *meter_snd2_item;
static struct menu_item *meter_geo_item;
static struct menu_item *meter_geo2_item;
static struct menu_item *meter_geo3_item;
struct menu_item *v_stretch_item[2];
int h_integer_stretch[2];
int v_integer_stretch[2];
int use_h_integer_stretch[2];
int use_v_integer_stretch[2];









static int switch_voce_visibile(const struct machine_entry *e) {
  if (e->custom) {
    return 0;
  }
  if (e->video_out == BMC64_VIDEO_OUT_COMPOSITE &&
      circle_board_type() != 0x11) {
    return 0;
  }
  return 1;
}










struct geo_schermo {
  int presente;
  int hc, vc, hb, vb, hs, vs;
};
static struct geo_schermo geo_comp[2];
static struct geo_schermo geo_hdmi;

static int geo_std(void) { return is_ntsc() ? 1 : 0; }


static int geo_comp_legge(const char *name, int value) {
  static const char *chiavi[6] = {"h_center_c", "v_center_c", "h_border_c",
                                  "v_border_c", "h_stretch_c", "v_stretch_c"};
  const size_t n = strlen(name);
  int std, i;
  if (n < 2) {
    return 0;
  }
  std = name[n - 1] == 'p' ? 0 : (name[n - 1] == 'n' ? 1 : -1);
  if (std < 0) {
    return 0;
  }
  for (i = 0; i < 6; i++) {
    if (strlen(chiavi[i]) == n - 1 && strncmp(name, chiavi[i], n - 1) == 0) {
      int *campo[6] = {&geo_comp[std].hc, &geo_comp[std].vc, &geo_comp[std].hb,
                       &geo_comp[std].vb, &geo_comp[std].hs, &geo_comp[std].vs};
      *campo[i] = value;
      geo_comp[std].presente |= 1 << i;
      return 1;
    }
  }
  return 0;
}

static void geo_scrivi(FILE *fp, const char *suf, const struct geo_schermo *g) {
  fprintf(fp, "h_center_%s=%d\n", suf, g->hc);
  fprintf(fp, "v_center_%s=%d\n", suf, g->vc);
  fprintf(fp, "h_border_%s=%d\n", suf, g->hb);
  fprintf(fp, "v_border_%s=%d\n", suf, g->vb);
  fprintf(fp, "h_stretch_%s=%d\n", suf, g->hs);
  fprintf(fp, "v_stretch_%s=%d\n", suf, g->vs);
}

static void geo_dalle_voci(struct geo_schermo *g) {
  g->presente = 0x3F;
  g->hc = h_center_item[0]->value;
  g->vc = v_center_item[0]->value;
  g->hb = h_border_item[0]->value;
  g->vb = v_border_item[0]->value;
  g->hs = h_stretch_item[0]->value;
  g->vs = v_stretch_item[0]->value;
}



static void geo_salva(FILE *fp) {
  struct geo_schermo adesso;
  int s;
  geo_dalle_voci(&adesso);
  if (is_composite() && geo_hdmi.presente == 0x3F) {
    geo_scrivi(fp, "0", &geo_hdmi);
    geo_comp[geo_std()] = adesso;
  } else {
    geo_scrivi(fp, "0", &adesso);
  }
  for (s = 0; s < 2; s++) {
    if (geo_comp[s].presente == 0x3F) {
      geo_scrivi(fp, s ? "cn" : "cp", &geo_comp[s]);
    }
  }
}

struct menu_item *pip_location_item;
struct menu_item *pip_swapped_item;

struct menu_item *c40_80_column_item;
struct menu_item *dir_convention_item;

struct menu_item *scaling_interp_item;



struct menu_item* s_enable_shader_item;
struct menu_item* s_crt_preset_item;
struct menu_item* s_curvature_item;
struct menu_item* s_curvature_x_item;
struct menu_item* s_curvature_y_item;
struct menu_item* s_skew_x_item;
struct menu_item* s_skew_y_item;
struct menu_item* s_trapezoid_item;
struct menu_item* s_rotation_item;
struct menu_item* s_overscan_item;
struct menu_item* s_convergence_item;
struct menu_item* s_red_offset_x_item;
struct menu_item* s_red_offset_y_item;
struct menu_item* s_blue_offset_x_item;
struct menu_item* s_blue_offset_y_item;
struct menu_item* s_convergence_radial_strength_item;
struct menu_item* s_horizontal_filtering_item;
struct menu_item* s_sigma_x_item;
struct menu_item* s_edge_blur_item;
struct menu_item* s_edge_blur_strength_item;
struct menu_item* s_edge_blur_radius_item;
struct menu_item* s_mask_enable_item;
struct menu_item* s_mask_item;
struct menu_item* s_mask_brightness_item;
struct menu_item* s_bloom_item;
struct menu_item* s_output_response_item;
struct menu_item* s_response_mode_item;
struct menu_item* s_level_mapping_item;
struct menu_item* s_scanlines_item;
struct menu_item* s_multisample_item;
struct menu_item* s_scanline_weight_item;
struct menu_item* s_scanline_gap_brightness_item;
struct menu_item* s_bloom_factor_item;
struct menu_item* s_vignette_item;
struct menu_item* s_vignette_strength_item;
struct menu_item* s_vignette_scale_item;
struct menu_item* s_vignette_softness_item;
struct menu_item* s_uneven_illumination_item;
struct menu_item* s_uneven_illumination_strength_item;
struct menu_item* s_uneven_illumination_scale_item;
struct menu_item* s_horizontal_jitter_item;
struct menu_item* s_horizontal_jitter_strength_item;
struct menu_item* s_horizontal_jitter_frequency_item;
struct menu_item* s_horizontal_jitter_speed_item;
struct menu_item* s_composite_artifacts_item;
struct menu_item* s_composite_chroma_blur_item;
struct menu_item* s_composite_luma_sharpen_item;
struct menu_item* s_composite_color_bleed_item;
struct menu_item* s_glass_reflection_item;
struct menu_item* s_glass_reflection_angle_item;
struct menu_item* s_glass_reflection_width_item;
struct menu_item* s_glass_reflection_position_item;
struct menu_item* s_rounded_screen_mask_item;
struct menu_item* s_rounded_corner_radius_item;
struct menu_item* s_rounded_border_softness_item;
struct menu_item* s_edge_glow_item;
struct menu_item* s_edge_glow_strength_item;
struct menu_item* s_edge_glow_width_item;
struct menu_item* s_noise_item;
struct menu_item* s_luminance_noise_item;
struct menu_item* s_chroma_noise_item;
struct menu_item* s_noise_speed_item;
struct menu_item* s_input_gamma_item;
struct menu_item* s_output_gamma_item;
struct menu_item* s_response_saturation_item;
struct menu_item* s_black_level_item;
struct menu_item* s_white_clip_item;

static void refresh_crt_shader_runtime(void);

struct crt_preset_binding {
  const char *key;
  struct menu_item **item;
};

#define CRT_PRESET_BIND(key, item) {key, &item},
static const struct crt_preset_binding s_crt_preset_bindings[] = {
#include "crt_preset_fields.inc"
};
#undef CRT_PRESET_BIND

#define CRT_PRESET_FIELD_COUNT \
  (sizeof(s_crt_preset_bindings) / sizeof(s_crt_preset_bindings[0]))
#define CRT_PRESET_DIR "/crt"
#define CRT_PRESET_EXTENSION ".crt"
#define CRT_PRESET_CURRENT_CHOICE 0

static char s_crt_preset_paths[MAX_CHOICES][MAX_STR_VAL_LEN];
static int s_crt_preset_applied_choice = CRT_PRESET_CURRENT_CHOICE;

static int unit;
static int joyswap;
static int statusbar_forced;

// Held here, exported for menu_usb to read
int pot_x_high_value;
int pot_x_low_value;
int pot_y_high_value;
int pot_y_low_value;

// Property names for load/save files
static char usb_btn_name[MAX_USB_DEVICES][16];
static char usb_pref_name[MAX_USB_DEVICES][16];
static char usb_x_name[MAX_USB_DEVICES][16];
static char usb_y_name[MAX_USB_DEVICES][16];
static char usb_x_t_name[MAX_USB_DEVICES][16];
static char usb_y_t_name[MAX_USB_DEVICES][16];


const int num_d81_ext = 1;
static char d81_filt_ext[1][5] = {".d81"};

const int num_d64_ext = 1;
static char d64_filt_ext[1][5] = {".d64"};

const int num_sid_ext = 1;
static char sid_filt_ext[1][5] = {".sid"};

const int num_reu_ext = 1;
static char reu_filt_ext[1][5] = {".reu"};

const int num_disk_ext = 15;
static char disk_filt_ext[15][5] = {".d64", ".d67", ".d71", ".d80", ".d81",
                                    ".d82", ".d1m", ".d2m", ".d4m", ".g64",
                                    ".g71", ".g41", ".p64", ".x64", ".dhd"};

const int num_tape_ext = 2;
static char tape_filt_ext[2][5] = {".t64", ".tap"};

const int num_cart_ext = 2;
static char cart_filt_ext[2][5] = {".crt", ".bin"};

const int num_snap_ext = 1;
char snap_filt_ext[1][5];

const int num_prg_ext = 1;
const char prg_filt_ext[1][5] = {".prg"};

const int num_ide64_ext = 2;
const char ide64_filt_ext[2][5] = {".cfa", ".hdd"};

#define TEST_FILTER_MACRO(funcname, numvar, filtarray)                         \
  static int funcname(char *name) {                                            \
    int include = 0;                                                           \
    int len = strlen(name);                                                    \
    int i;                                                                     \
    for (i = 0; i < numvar; i++) {                                             \
      if (len > 4 && !strcasecmp(name + len - 4, filtarray[i])) {              \
        include = 1;                                                           \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    return include;                                                            \
  }

// What directories to initialize file search dialogs with for
// each type of file.
// TODO: Make these start dirs configurable.
static const char default_volume_name[8] = "SD:";
static const char default_dir_names[NUM_DIR_TYPES][16] = {
    "/", "/disks", "/tapes", "/carts", "/snapshots", "/roms", "/", "/sids",
    "/REU", "/prg"};

// Keep track of the current volume
static char current_volume_name[8] = "";
// Keep track of current directory for each type of file.
static char current_dir_names[NUM_DIR_TYPES][256];
// Set to the sub dir name for this type.
static char machine_sub_dir[16];


static char files_sub_dir[16];
// Keep track of last iec dirs for each drive
static char last_iec_dir[4][256];

static int usb1_mounted;
static int usb2_mounted;
static int usb3_mounted;

// Temp storage for full path name concatenations.


static char full_path_str[MAX_STR_VAL_LEN * 3];

// Keep track of last known position in the file list.
static int current_dir_pos[NUM_DIR_TYPES];

TEST_FILTER_MACRO(test_disk_name, num_disk_ext, disk_filt_ext);
TEST_FILTER_MACRO(test_tape_name, num_tape_ext, tape_filt_ext);
TEST_FILTER_MACRO(test_cart_name, num_cart_ext, cart_filt_ext);
TEST_FILTER_MACRO(test_snap_name, num_snap_ext, snap_filt_ext);
TEST_FILTER_MACRO(test_prg_name, num_prg_ext, prg_filt_ext);
TEST_FILTER_MACRO(test_ide64_name, num_ide64_ext, ide64_filt_ext);
TEST_FILTER_MACRO(test_d81_name, num_d81_ext, d81_filt_ext);
TEST_FILTER_MACRO(test_d64_name, num_d64_ext, d64_filt_ext);
TEST_FILTER_MACRO(test_sid_name, num_sid_ext, sid_filt_ext);
TEST_FILTER_MACRO(test_reu_name, num_reu_ext, reu_filt_ext);

static void rtrim(char *txt) {
  if (!txt) return;
  int p=strlen(txt)-1;
  while (isspace(txt[p])) { txt[p] = '\0'; p--; }
}

static char* ltrim(char *txt) {
  if (!txt) return NULL;
  int p=0;
  while (isspace(txt[p])) { p++; }
  return txt+p;
}

static void get_key_and_value(char *line, char **key, char **value) {
   for (int i=0;i<strlen(line);i++) {
      if (line[i] == '=') {
         line[i] = '\0';
         *key = ltrim(&line[0]);
         rtrim(*key);
         *value = ltrim(&line[i+1]);
         rtrim(*value);
         return;
      }
   }
   *key = 0;
   *value = 0;
}

static void unquote_wifi_value(char *value) {
  size_t length = strlen(value);
  if (length >= 2 && value[0] == '"' && value[length - 1] == '"') {
    memmove(value, value + 1, length - 2);
    value[length - 2] = '\0';
  }
}


static int wifi_paese_valido(const char *c) {
  return strlen(c) == 2 && c[0] >= 'A' && c[0] <= 'Z' &&
         c[1] >= 'A' && c[1] <= 'Z';
}






static int wifi_psk_esadecimale(const char *p) {
  int i;
  if (strlen(p) != 64) {
    return 0;
  }
  for (i = 0; i < 64; i++) {
    if (!((p[i] >= '0' && p[i] <= '9') || (p[i] >= 'a' && p[i] <= 'f') ||
          (p[i] >= 'A' && p[i] <= 'F'))) {
      return 0;
    }
  }
  return 1;
}

static void load_wifi_settings(void) {
  FILE *fp = fopen("/wpa_supplicant.conf", "r");
  if (fp == NULL) {
    return;
  }

  char line[MAX_STR_VAL_LEN];
  while (fgets(line, sizeof(line), fp) != NULL) {
    char *name;
    char *value;
    get_key_and_value(line, &name, &value);
    if (name == NULL || value == NULL) {
      continue;
    }
    unquote_wifi_value(value);
    if (strcmp(name, "ssid") == 0) {
      strncpy(wifi_ssid_item->str_value, value, wifi_ssid_item->max_length);
      wifi_ssid_item->str_value[wifi_ssid_item->max_length] = '\0';
    } else if (strcmp(name, "psk") == 0) {
      strncpy(wifi_psk, value, sizeof(wifi_psk) - 1);
      wifi_psk[sizeof(wifi_psk) - 1] = '\0';
      strcpy(wifi_psk_letta, wifi_psk);
    } else if (strcmp(name, "key_mgmt") == 0) {
      wifi_security_item->value = strcmp(value, "NONE") == 0 ? 1 : 0;
    } else if (strcmp(name, "country") == 0) {
      strncpy(wifi_country_item->str_value, value,
              wifi_country_item->max_length);
      wifi_country_item->str_value[wifi_country_item->max_length] = '\0';
      if (wifi_paese_valido(wifi_country_item->str_value)) {
        strcpy(wifi_country_saved, wifi_country_item->str_value);
      }
    }
  }
  fclose(fp);

  wifi_ssid_item->value = strlen(wifi_ssid_item->str_value);
  wifi_country_item->value = strlen(wifi_country_item->str_value);
}

static int save_wifi_settings(void) {
  FILE *fp = fopen("/wpa_supplicant.conf", "w");
  if (fp == NULL) {
    return 1;
  }

  fprintf(fp, "country=%s\n\n",
          wifi_paese_valido(wifi_country_item->str_value)
              ? wifi_country_item->str_value : wifi_country_saved);
  fprintf(fp, "network={\n");
  fprintf(fp, "\tssid=\"%s\"\n", wifi_ssid_item->str_value);
  if (wifi_security_item->value == 0) {
    if (wifi_psk_esadecimale(wifi_psk)) {
      fprintf(fp, "\tpsk=%s\n", wifi_psk);
    } else {
      fprintf(fp, "\tpsk=\"%s\"\n", wifi_psk);
    }
    fprintf(fp, "\tkey_mgmt=WPA-PSK\n");
  } else {
    fprintf(fp, "\tkey_mgmt=NONE\n");
  }
  fprintf(fp, "}\n");
  fclose(fp);
  return 0;
}








static void wifi_paese_cambiato(void) {
  char vecchio[2048];
  char nuovo[2048 + 32];
  size_t n, k = 0;
  int scritto = 0;
  const char *c = wifi_country_item->str_value;
  FILE *fp;

  if (!wifi_paese_valido(c) || strcmp(c, wifi_country_saved) == 0) {
    return;
  }
  fp = fopen("/wpa_supplicant.conf", "r");
  if (fp == NULL) {
    return;
  }
  n = fread(vecchio, 1, sizeof(vecchio) - 1, fp);
  if (!feof(fp)) {
    fclose(fp);
    ui_error("wpa_supplicant.conf too big");
    return;
  }
  fclose(fp);
  vecchio[n] = '\0';
  for (const char *r = vecchio; *r; ) {
    const char *fine = strchr(r, '\n');
    size_t lung = fine ? (size_t)(fine - r) + 1 : strlen(r);
    if (k + lung + 16 > sizeof(nuovo)) {
      ui_error("wpa_supplicant.conf too big");
      return;
    }
    if (strncmp(r, "country=", 8) == 0) {
      k += sprintf(nuovo + k, "country=%s\n", c);
      scritto = 1;
    } else {
      memcpy(nuovo + k, r, lung);
      k += lung;
    }
    r += lung;
  }
  if (!scritto) {
    memmove(nuovo + 11, nuovo, k);
    memcpy(nuovo, "country=", 8);
    nuovo[8] = c[0];
    nuovo[9] = c[1];
    nuovo[10] = '\n';
    k += 11;
  }
  fp = fopen("/wpa_supplicant.conf", "w");
  if (fp == NULL || fwrite(nuovo, 1, k, fp) != k) {
    if (fp != NULL) {
      fclose(fp);
    }
    ui_error("Cannot save WiFi country");
    return;
  }
  fclose(fp);
  strcpy(wifi_country_saved, c);
  ui_info("WiFi country saved, reboot to apply");
}

static void show_wifi_connect_dialog(void) {
  struct menu_item *root = ui_push_menu(42, 6);
  struct menu_item *prompt = ui_menu_add_button(
      MENU_ID_DO_NOTHING, root, "Enter WiFi PSK");
  prompt->disabled = 1;
  ui_menu_add_divider(root);




  wifi_psk_item = ui_menu_add_text_field_limit(
      MENU_WIFI_PSK, root, "WiFi PSK", wifi_psk, 64);


  if (wifi_psk_item != NULL) {
    wifi_psk_item->textfield_gap = 3;





    wifi_psk_item->textfield_masked =
        wifi_psk[0] != '\0' && strcmp(wifi_psk, wifi_psk_letta) == 0;
  }
  ui_menu_add_button(MENU_WIFI_CONNECT_NOW, root, "Save & Reboot");
  ui_select_first_interactive_item();
  ui_render_single_frame();
}





static void accoda(char *dst, size_t max, const char *src) {
  size_t n = strlen(dst);
  if (src == NULL || n + 1 >= max) {
    return;
  }
  strncpy(dst + n, src, max - n - 1);
  dst[max - 1] = '\0';
}



static int finisce_con_slash(const char *dir) {
  size_t n = strlen(dir);
  return n > 0 && dir[n - 1] == '/';
}

static char *fullpath(DirType dir_type, char *name) {
  full_path_str[0] = '\0';
  accoda(full_path_str, sizeof(full_path_str), current_volume_name);
  accoda(full_path_str, sizeof(full_path_str), current_dir_names[dir_type]);
  // Put a trailing slash unless we are at the root
  if (!finisce_con_slash(current_dir_names[dir_type])) {
    accoda(full_path_str, sizeof(full_path_str), "/");
  }
  accoda(full_path_str, sizeof(full_path_str), name);
  return full_path_str;
}

// Remove one directory from the end of path
static void remove_dir(char *path) {
  int i;
  // Remove last directory from current_dir_names
  i = strlen(path) - 1;
  while (path[i] != '/' && i > 0)
    i--;
  path[i] = '\0';
  if (strlen(path) == 0) {
    strcpy(path, "/");
  }
}













static void nas_torna_alla_scheda(void);

static DIR *apri_la_cartella(DirType dir_type) {
  char ricordata[sizeof(current_dir_names[0])];
  DIR *dp;

  strcpy(ricordata, current_dir_names[dir_type]);
  dp = opendir(fullpath(dir_type, ""));
  while (dp == NULL && current_dir_names[dir_type][0] != '\0' &&
         strcmp(current_dir_names[dir_type], "/") != 0) {
    remove_dir(current_dir_names[dir_type]);
    dp = opendir(fullpath(dir_type, ""));
  }
  if (dp == NULL) {
    strcpy(current_dir_names[dir_type], ricordata);
  }
  return dp;
}


static int numero_usb(const char *volume) {
  if (strcmp(volume, "USB:") == 0) return 0;
  if (strcmp(volume, "USB2:") == 0) return 1;
  if (strcmp(volume, "USB3:") == 0) return 2;
  return -1;
}


static int numero_floppy(const char *volume) {
  if (strcmp(volume, "FLP1:") == 0) return 0;
  if (strcmp(volume, "FLP2:") == 0) return 1;
  return -1;
}

static void usb_segna_montata(int usb, int montata) {
  if (usb == 0) usb1_mounted = montata;
  if (usb == 1) usb2_mounted = montata;
  if (usb == 2) usb3_mounted = montata;
}





















#define RICERCA_MAX 30
static char ricerca_testo[RICERCA_MAX + 1];
static char ricerca_bozza[RICERCA_MAX + 1];
static int ricerca_riga;





static int ricerca_dentro;
static int ricerca_da_se;
static int ricerca_sul_primo;
static struct menu_item *ricerca_root;
static struct menu_item *ricerca_item_riga;
static struct menu_item *ricerca_item_primo;


static char ricerca_scelto[MAX_STR_VAL_LEN];
static int ricerca_scelto_dir = -1;

static void ricerca_spegni(void) {
  ricerca_dentro = 0;
  ricerca_testo[0] = '\0';
  ricerca_bozza[0] = '\0';
  ricerca_riga = 0;
  ricerca_sul_primo = 0;
}




static int ricerca_combacia(const char *pat, const char *nome) {
  for (;;) {
    if (*pat == '*') {
      while (*pat == '*') {
        pat++;
      }
      if (*pat == '\0') {
        return 1;
      }
      for (; *nome != '\0'; nome++) {
        if (ricerca_combacia(pat, nome)) {
          return 1;
        }
      }
      return 0;
    }
    if (*pat == '\0') {
      return *nome == '\0';
    }
    if (*nome == '\0') {
      return 0;
    }
    if (*pat != '?' &&
        toupper((unsigned char)*pat) != toupper((unsigned char)*nome)) {
      return 0;
    }
    pat++;
    nome++;
  }
}





static int nome_passa_il_filtro(const char *nome, int filter) {
  if (filter == FILTER_DISK) {
    return test_disk_name(nome);
  } else if (filter == FILTER_TAPE) {
    return test_tape_name(nome);
  } else if (filter == FILTER_CART) {
    return test_cart_name(nome);
  } else if (filter == FILTER_SNAP) {
    return test_snap_name(nome);
  } else if (filter == FILTER_PRGS) {
    return test_prg_name(nome);
  } else if (filter == FILTER_IDE64) {
    return test_ide64_name(nome);
  } else if (filter == FILTER_D81) {
    return test_d81_name(nome);
  } else if (filter == FILTER_D64) {
    return test_d64_name(nome);
  } else if (filter == FILTER_SID) {
    return test_sid_name(nome);
  } else if (filter == FILTER_REU) {
    return test_reu_name(nome);
  } else if (filter == FILTER_DIRS) {
    return 0;
  } else if (filter == FILTER_NONE) {
    return 1;
  }
  return 0;
}



static int ricerca_passa(const char *nome) {
  char pat[RICERCA_MAX + 3];
  size_t n;

  if (ricerca_testo[0] == '\0') {
    return 1;
  }
  if (ricerca_dentro) {
    pat[0] = '*';
    strcpy(pat + 1, ricerca_testo);
  } else {
    strcpy(pat, ricerca_testo);
  }
  n = strlen(pat);
  if (pat[n - 1] != '*') {
    pat[n] = '*';
    pat[n + 1] = '\0';
  }
  return ricerca_combacia(pat, nome);
}






static void ricerca_scegli_il_modo(DIR *dp, int filter) {
  struct dirent *ep;
  int trovato = 0;

  ricerca_dentro = 0;
  if (ricerca_testo[0] == '\0' || dp == NULL) {
    return;
  }
  while ((ep = readdir(dp)) != NULL) {
    if (ep->d_name[0] == '.' && ep->d_name[1] == '_') {
      continue;
    }
    if (ep->d_type == DT_DIR) {
      if (ricerca_passa(ep->d_name)) {
        trovato = 1;
        break;
      }
    } else if (nome_passa_il_filtro(ep->d_name, filter) &&
               ricerca_passa(ep->d_name)) {
      trovato = 1;
      break;
    }
  }
  rewinddir(dp);
  ricerca_dentro = !trovato;
}



static int lista_col_nome_nuovo(int menu_id) {
  return menu_id == MENU_SAVE_SNAP_FILE ||
         menu_id == MENU_REU_SAVE_IMAGE_AS_FILE ||
         (menu_id >= MENU_CREATE_D64_FILE && menu_id <= MENU_CREATE_TAP_FILE);
}


static int ricerca_indice(struct menu_item *root, struct menu_item *voce) {
  struct menu_item *p;
  int i = 0;

  for (p = root->first_child; p != NULL; p = p->next, i++) {
    if (p == voce) {
      return i;
    }
  }
  return -1;
}


static int ricerca_ritrova_scelto(struct menu_item *root, int dir_type) {
  struct menu_item *p;
  int i = 0;

  if (ricerca_scelto_dir != dir_type || ricerca_scelto[0] == '\0') {
    return -1;
  }
  ricerca_scelto_dir = -1;
  for (p = root->first_child; p != NULL; p = p->next, i++) {
    if (p->sub_id == MENU_SUB_PICK_FILE && p->type != TEXTFIELD &&
        strcmp(p->str_value, ricerca_scelto) == 0) {
      return i;
    }
  }
  return -1;
}

// Clears the file menu and populates it with files.
static void list_files(struct menu_item *parent,
                       DirType dir_type, FileFilter filter,
                       int menu_id) {
  DIR *dp;
  struct dirent *ep;
  int i;
  int include;
  char volume_perso[48];
  const char *avviso_perso = "CANNOT READ THIS DRIVE";









  volume_perso[0] = '\0';
  dp = apri_la_cartella(dir_type);
  if (dp == NULL && strcmp(current_volume_name, default_volume_name) != 0) {
    int usb = numero_usb(current_volume_name);
    int flp = numero_floppy(current_volume_name);

    if (usb >= 0 && circle_mount_usb(usb)) {
      usb_segna_montata(usb, 1);
      dp = apri_la_cartella(dir_type);
    }


    if (dp == NULL && flp >= 0 && circle_floppy_stato(flp) == -1) {

      snprintf(volume_perso, sizeof(volume_perso),
               "*** %s NO DISK ***", current_volume_name);
      avviso_perso = "NO DISK IN THE DRIVE";
      printf("[VOL] %s senza dischetto: si torna su %s\n", current_volume_name,
             default_volume_name);
      strcpy(current_volume_name, default_volume_name);
      dp = apri_la_cartella(dir_type);
    } else if (dp == NULL) {
      snprintf(volume_perso, sizeof(volume_perso),
               "*** %s NON SI LEGGE ***", current_volume_name);
      printf("[VOL] %s non si legge: si torna su %s\n", current_volume_name,
             default_volume_name);
      if (usb >= 0) {
        usb_segna_montata(usb, 0);
      }
      nas_torna_alla_scheda();
      strcpy(current_volume_name, default_volume_name);
      dp = apri_la_cartella(dir_type);
    }
  }

  // Current directory item, also action to change disk drive
  struct menu_item* cur_dir = ui_menu_add_button(
     menu_id, parent, fullpath(dir_type,""));
  cur_dir->sub_id = MENU_SUB_SELECT_VOLUME;
  cur_dir->symbol = 31;  // left arrow
  ui_menu_add_divider(parent);
  if (volume_perso[0] != '\0') {
    struct menu_item *avviso =
        ui_menu_add_button(MENU_ID_DO_NOTHING, parent, volume_perso);
    if (avviso != NULL) {
      avviso->disabled = 1;
    }
    overlay_avviso(avviso_perso);
  }
  if (dp == NULL) {


    struct menu_item *avviso =
        ui_menu_add_button(MENU_ID_DO_NOTHING, parent, "*** NON SI LEGGE ***");
    if (avviso != NULL) {
      avviso->disabled = 1;
    }
    printf("[VOL] %s non si legge nemmeno lui\n", current_volume_name);
    return;
  }


  if (ricerca_riga) {
    struct menu_item *riga = ui_menu_add_text_field_limit(
        menu_id, parent, "Search:", ricerca_bozza, RICERCA_MAX);
    if (riga != NULL) {
      riga->sub_id = MENU_SUB_SEARCH;
      ricerca_item_riga = riga;
    }
  }

  // When we are picking dirs, include a button to select the current dir.
  if (filter == FILTER_DIRS) {
    struct menu_item *new_button =
         ui_menu_add_button(menu_id, parent, "(Use this dir)");
    new_button->sub_id = MENU_SUB_PICK_DIR;
    ui_menu_add_divider(parent);
  }

  // Put together a string that represents the root of this volume
  char current_root[16];
  current_root[0] = '\0';
  accoda(current_root, sizeof(current_root), current_volume_name);
  accoda(current_root, sizeof(current_root), "/");

  if (strcmp(fullpath(dir_type,""), current_root) != 0) {
    ui_menu_add_button(menu_id, parent, "..")->sub_id = MENU_SUB_UP_DIR;
  }

  // Make two buckets
  struct menu_item dirs_root;
  memset(&dirs_root, 0, sizeof(struct menu_item));
  dirs_root.type = FOLDER;
  dirs_root.is_expanded = 1;
  dirs_root.name[0] = '\0';

  struct menu_item files_root;
  memset(&files_root, 0, sizeof(struct menu_item));
  files_root.type = FOLDER;
  files_root.is_expanded = 1;
  files_root.name[0] = '\0';





  int quante_voci = 0;
  int trovati = 0;
  ricerca_scegli_il_modo(dp, filter);
  unsigned long memoria_prima = circle_memoria_libera();
  ui_memoria_finita = 0;

  if (dp != NULL) {
    while (ep = readdir(dp)) {
      if (ui_memoria_finita) {
        break;
      }




      if (ep->d_name[0] == '.' && ep->d_name[1] == '_') {
        continue;
      }
      quante_voci++;
      if (ep->d_type == DT_DIR) {
        if (ricerca_passa(ep->d_name)) {
          struct menu_item *cartella = ui_menu_add_button_with_value(
              menu_id, &dirs_root, ep->d_name, 0, ep->d_name, "(dir)");
          if (cartella != NULL) {
            cartella->sub_id = MENU_SUB_ENTER_DIR;
            trovati++;
          }
        }
      } else {
        include = nome_passa_il_filtro(ep->d_name, filter);
        if (include && !ricerca_passa(ep->d_name)) {
          include = 0;
        }
        if (include) {
          // Button name will be filename but it will be truncated
          // due to menu width.  Actual filename will be stored in
          // str_value which is never displayed except for text fields.
          struct menu_item *new_button =
              ui_menu_add_button(menu_id, &files_root, ep->d_name);

          if (new_button != NULL) {
            new_button->sub_id = MENU_SUB_PICK_FILE;
            strncpy(new_button->str_value, ep->d_name, MAX_STR_VAL_LEN - 1);
            trovati++;
          }
        }
      }
    }

    (void)closedir(dp);
  }



  {
    unsigned long memoria_dopo = circle_memoria_libera();
    printf("[DIR] %s: %d voci, %lu KB usati, %lu KB liberi%s\n",
           fullpath(dir_type, ""), quante_voci,
           (memoria_prima - memoria_dopo) / 1024, memoria_dopo / 1024,
           ui_memoria_finita ? "  >>> MEMORIA FINITA <<<" : "");
  }
  if (ui_memoria_finita) {


    struct menu_item *avviso =
        ui_menu_add_button(MENU_ID_DO_NOTHING, parent,
                           "*** TROPPI FILE: ELENCO TAGLIATO ***");
    if (avviso != NULL) {
      avviso->disabled = 1;
    }
    overlay_avviso("TOO MANY FILES IN FOLDER");
  }



  if (ricerca_testo[0] != '\0' && trovati == 0) {
    struct menu_item *niente =
        ui_menu_add_button(MENU_ID_DO_NOTHING, parent, "NO FILES FOUND");
    if (niente != NULL) {
      niente->disabled = 1;
    }
  }

  struct menu_item *dfc = dirs_root.first_child;
  merge_sort(&dfc);
  dirs_root.first_child = dfc;

  struct menu_item *ffc = files_root.first_child;
  merge_sort(&ffc);
  files_root.first_child = ffc;


  ricerca_item_primo = dirs_root.first_child != NULL ? dirs_root.first_child
                                                     : files_root.first_child;

  // Transfer ownership of dirs children first, then files. Childless
  // parents are on the stack.
  ui_add_all(&dirs_root, parent);
  ui_add_all(&files_root, parent);

  assert(dirs_root.first_child == NULL);
  assert(files_root.first_child == NULL);
}

static void files_cursor_listener(struct menu_item* parent,
                                  int new_pos) {


  if (ricerca_riga) {
    return;
  }
  // dir type is in value field
  current_dir_pos[parent->value] = new_pos;
}











static int hdmi_principale_adesso(void) {
  return circle_fb_display_chiesto() == 1 ? 1 : 0;
}

static void rimetti_voci_riavvio(int pos) {
  if (audio_out_item != NULL && pos != audio_out_item->render_index) {
    audio_out_item->value = circle_uscita_audio();
  }
  if (hdmi_principale_item != NULL &&
      pos != hdmi_principale_item->render_index) {
    hdmi_principale_item->value = hdmi_principale_adesso();
  }
}



static void chiedi_riavvio_voce(struct menu_item *item) {
  char titolo[MAX_MENU_STR];

  snprintf(titolo, sizeof(titolo), "Reboot for %s?",
           item->choices[item->value]);
  ui_confirm_wrapped(titolo,
                     item->id == MENU_AUDIO_OUT ? AUDIO_OUT_MSG
                                                : HDMI_PRINCIPALE_MSG,
                     item->value, item->id);
}

static void main_menu_cursor_listener(struct menu_item* parent, int new_pos) {
  (void) parent;

  rimetti_voci_riavvio(new_pos);
  menu_update_network_status();
  int network_device_is_selected =
      network_device_item != NULL &&
      new_pos == network_device_item->render_index;

  if (network_device_was_selected && !network_device_is_selected &&
      network_device_item->value != saved_network_device &&
      !network_reboot_prompted) {
    network_reboot_prompted = 1;
    ui_confirm_wrapped_labels("Network settings changed",
        "Network settings have changed. You need to reboot for them to take effect. Reboot now?",
      0, MENU_NETWORK_ENABLED, "Yes", "No");
  }

  network_device_was_selected = network_device_is_selected;
}

static struct menu_item *show_files(DirType dir_type, FileFilter filter,
                                   int menu_id, int reset_cur_pos) {



  const int da_se = ricerca_da_se;
  ricerca_da_se = 0;
  if (!da_se) {
    ricerca_spegni();
  }
  ricerca_item_riga = NULL;
  ricerca_item_primo = NULL;

  // Show files
  struct menu_item *file_root = ui_push_menu(-1, -1);
  ricerca_root = file_root;

  // Keep the type of files this list is for in value field.
  file_root->value = dir_type;

  file_root->cursor_listener_func = files_cursor_listener;

  if (lista_col_nome_nuovo(menu_id)) {
    struct menu_item *file_name_item = ui_menu_add_text_field(
       menu_id, file_root, "Enter name:", "");
    file_name_item->sub_id = MENU_SUB_PICK_FILE;
  }
  list_files(file_root, dir_type, filter, menu_id);

  if (da_se) {


    struct menu_item *dove = ricerca_item_riga;
    if (ricerca_sul_primo && ricerca_item_primo != NULL) {
      dove = ricerca_item_primo;
    }
    if (dove != NULL) {
      int i = ricerca_indice(file_root, dove);
      if (i > 0) {
        ui_set_cur_pos(i);
      }
    } else {
      ui_set_cur_pos(current_dir_pos[dir_type]);
    }
  } else if (reset_cur_pos) {
     current_dir_pos[dir_type] = 0;
  } else {
     int scelto = ricerca_ritrova_scelto(file_root, dir_type);
     if (scelto >= 0) {
       current_dir_pos[dir_type] = scelto;
     }
     // Position cursor to last known location for this dir type.
     ui_set_cur_pos(current_dir_pos[dir_type]);
  }
  return file_root;
}






static void tasto_apre_i_file(DirType dir_type, FileFilter filter,
                              int menu_id) {
  struct menu_item *root;



  if (ui_enabled) {
    ui_dismiss_osd_if_active();
    return;
  }
  root = show_files(dir_type, filter, menu_id, 0);
  if (root != NULL) {
    root->on_popped_off = glob_osd_popped;
  }
  ui_enable_osd();
}

















static int plus4emu_due_modelli(int unita, int tasto) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
    return 0;
  }
  if ((unita == 8 || unita == 9) && tasto >= 1 && tasto <= 3) {






    char avviso[32];
    int modello = (tasto == 1) ? 0 : (tasto == 2 ? 1541 : 1551);
    emux_set_int_1(Setting_DriveNType, modello, unita);
    emux_refresh_drive_items(unita);
    if (tasto == 1) {
      snprintf(avviso, sizeof(avviso), "DRIVE %d: NONE", unita);
      overlay_avviso(avviso);
    } else {







      snprintf(avviso, sizeof(avviso), "%s: ATTACH A .D64 %s",
               tasto == 2 ? "1541" : "1551", unita == 9 ? "ON 9" : "DISK");
      overlay_avviso(avviso);
      unit = unita;
      tasto_apre_i_file(DIR_DISKS, FILTER_D64, MENU_DISK_FILE);
    }
  } else if ((unita == 8 || unita == 9) && tasto == 4) {









    overlay_avviso(unita == 9 ? "1581: ATTACH A .D81 ON 9"
                              : "1581: ATTACH A .D81 DISK");
    unit = unita;




    tasto_apre_i_file(DIR_DISKS, FILTER_D81, MENU_DISK_FILE);
  } else {








    overlay_avviso("1=NONE 2=1541 3=1551 4=D81/1581");
  }
  return 1;
}




static const char *nome_del_modello(int modello) {
  switch (modello) {
    case 0:                return "NONE";
    case 1541:             return "1541";
    case DRIVE_MOD_1541II: return "1541-II";
    case DRIVE_MOD_1551:   return "1551";
    case 1570:             return "1570";
    case DRIVE_MOD_1571:   return "1571";
    case DRIVE_MOD_1571CR: return "1571CR";
    case DRIVE_MOD_1581:   return "1581";
    default:               return "?";
  }
}






static void avviso_del_modello(int unita, const char *nome) {
  char riga[32];
  snprintf(riga, sizeof(riga), "DRIVE %d: %s", unita, nome);
  overlay_avviso(riga);
}

static void tasto_mette_il_modello(int unita, int tasto, int modello) {
  if (plus4emu_due_modelli(unita, tasto)) {
    return;
  }







  if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4 && tasto == 4) {
    modello = DRIVE_MOD_1551;
  }




  passo_prima("lasciare il drive vero");
  emux_leave_real_drive(unita);
  passo_fatto("lasciare il drive vero");
  emux_use_emulated_drive(unita);
  passo_prima("mettere il modello (legge la ROM dalla scheda)");
  emux_set_int_1(Setting_DriveNType, modello, unita);
  passo_fatto("mettere il modello (legge la ROM dalla scheda)");
  emux_refresh_drive_items(unita);
  avviso_del_modello(unita, nome_del_modello(modello));
}




static void tasto_apre_la_cartuccia(void) {
  switch (emux_machine_class) {
    case BMC64_MACHINE_CLASS_C64:
    case BMC64_MACHINE_CLASS_SCPU64:
    case BMC64_MACHINE_CLASS_C128:
      tasto_apre_i_file(DIR_CARTS, FILTER_CART, MENU_C64_CART_FILE);
      break;
    case BMC64_MACHINE_CLASS_VIC20:
      tasto_apre_i_file(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_DETECT_FILE);
      break;
    case BMC64_MACHINE_CLASS_PLUS4:
      tasto_apre_i_file(DIR_CARTS, FILTER_CART, MENU_PLUS4_CART_FILE);
      break;
    case BMC64_MACHINE_CLASS_PLUS4EMU:







      tasto_apre_i_file(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C1_LO_FILE);
      break;
    default:

      break;
  }
}




static void tasto_stacca_la_cartuccia(void) {


  emux_sid_lascia_la_porta();
  if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU) {
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C1_LO);
    return;
  }
  emux_detach_cart(0);
}


static void show_about() {




  struct menu_item *about_root = ui_push_menu(-1, -1);
  char title[32];
  char desc[32];

  switch (emux_machine_class) {
  case BMC64_MACHINE_CLASS_C64:
    snprintf (title, 31, "%s",
              NOME_PROGETTO VARIANT_STRING " Ver. " VERSIONE_BMC64_NG);
    strncpy (desc, "A Bare Metal C64 Emulator", 31);
    break;
  case BMC64_MACHINE_CLASS_SCPU64:
    snprintf (title, 31, "%s",
              NOME_PROGETTO VARIANT_STRING " Ver. " VERSIONE_BMC64_NG);
    strncpy (desc, "A Bare Metal SCPU64 Emulator", 31);
    break;
  case BMC64_MACHINE_CLASS_C128:
    snprintf (title, 31, "%s",
              NOME_PROGETTO VARIANT_STRING " Ver. " VERSIONE_BMC64_NG);
    strncpy (desc, "A Bare Metal C128 Emulator", 31);
    break;
  case BMC64_MACHINE_CLASS_VIC20:
    snprintf (title, 31, "%s",
              NOME_PROGETTO VARIANT_STRING " Ver. " VERSIONE_BMC64_NG);
    strncpy (desc, "A Bare Metal VIC20 Emulator", 31);
    break;
  case BMC64_MACHINE_CLASS_PLUS4:
  case BMC64_MACHINE_CLASS_PLUS4EMU:
    snprintf (title, 31, "%s",
              NOME_PROGETTO VARIANT_STRING " Ver. " VERSIONE_BMC64_NG);
    strncpy (desc, "A Bare Metal PLUS/4 Emulator", 31);
    break;
  case BMC64_MACHINE_CLASS_PET:
    snprintf (title, 31, "%s",
              NOME_PROGETTO VARIANT_STRING " Ver. " VERSIONE_BMC64_NG);
    strncpy (desc, "A Bare Metal PET Emulator", 31);
    break;
  default:
    strncpy (title, "ERROR", 31);
    strncpy (desc, "Unknown Emulator", 31);
    break;
  }

  ui_menu_add_button(MENU_TEXT, about_root, title);
  ui_menu_add_button(MENU_TEXT, about_root, desc);




  ui_menu_add_button(MENU_TEXT, about_root, "For Raspberry Pi");
  ui_menu_add_button(MENU_TEXT, about_root, "4 / 5 / 400 / 500 & 500+");






  char engine[32];
  snprintf(engine, 31, "Engine: %s", emux_get_engine_version());
  ui_menu_add_button(MENU_TEXT, about_root, engine);

  ui_menu_add_divider(about_root);




  ui_menu_add_button(MENU_TEXT, about_root, "By Cla-Bob Systems");
  ui_menu_add_button(MENU_TEXT, about_root, "  Bob  Roberto");
  ui_menu_add_button(MENU_TEXT, about_root, "       the hardware, the tests");
  ui_menu_add_button(MENU_TEXT, about_root, "       and the decisions");
  ui_menu_add_button(MENU_TEXT, about_root, "  Cla  Claude Code");
  ui_menu_add_button(MENU_TEXT, about_root, "       the code, written from");
  ui_menu_add_button(MENU_TEXT, about_root, "       what Bob measures");

  ui_menu_add_divider(about_root);
  ui_menu_add_button(MENU_TEXT, about_root, "Forked from BMC64 by R. Rossi:");



  ui_menu_add_button(MENU_FONT_TEST, about_root,
                     "https://github.com/randyrossi/bmc64");
}







static void font_test_append(char *dst, int *len, unsigned int cp) {
  if (cp < 0x80) {
    dst[(*len)++] = (char)cp;
  } else {
    dst[(*len)++] = (char)(0xC0 | (cp >> 6));
    dst[(*len)++] = (char)(0x80 | (cp & 0x3F));
  }
}




static void font_test_row(struct menu_item *root, unsigned int base) {
  char line[MAX_MENU_STR];
  int len = snprintf(line, sizeof(line), "%02X ", base);
  unsigned int c;
  for (c = base; c < base + 16; c++) {
    font_test_append(line, &len, c);
  }
  line[len] = '\0';
  ui_menu_add_button(MENU_TEXT, root, line);
}






static void show_font_test() {
  struct menu_item *root = ui_push_menu(-1, -1);
  unsigned int base;

  ui_menu_add_button(MENU_TEXT, root, "MENU FONT TEST");
  ui_menu_add_button(MENU_TEXT, root, "Bescii Mono  U+0000 - U+00FF");

  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_TEXT, root, "ASCII 0x20 - 0x7F");
  for (base = 0x20; base < 0x80; base += 16) {
    font_test_row(root, base);
  }

  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_TEXT, root, "C1 controls 0x80 - 0x9F (box)");
  for (base = 0x80; base < 0xA0; base += 16) {
    font_test_row(root, base);
  }

  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_TEXT, root, "Latin-1 0xA0 - 0xFF");
  for (base = 0xA0; base < 0x100; base += 16) {
    font_test_row(root, base);
  }

  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_TEXT, root,
                     "Sample: " "caf\xC3\xA9" " " "\xC2\xA3" "5 "
                     "\xC2\xB1" " " "\xC2\xBD");
  ui_menu_add_button(MENU_TEXT, root,
                     "> U+00FF: " "\xCE\x94" " " "\xE2\x82\xAC" " box");
  ui_menu_add_button(MENU_TEXT, root, "bad utf-8: \xFF\xFE box");






  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_TEXT, root, "Type here:");
  ui_menu_add_text_field_limit(MENU_TEXT, root, ">", "", 32);
}

static void show_license() {
  int i;
  struct menu_item *license_root = ui_push_menu(-1, -1);
  for (i = 0; i < 510; i++) {
    ui_menu_add_button(MENU_TEXT, license_root, license[i]);
  }
}

static void configure_usb(int dev) {
  struct menu_item *usb_root = ui_push_menu(-1, -1);
  build_usb_menu(dev, usb_root);
}

static void configure_keyset(int num) {
  struct menu_item *keyset_root = ui_push_menu(-1, -1);
  build_keyset_menu(num, keyset_root);
}

static void configure_timing() {
  struct menu_item *timing_root = ui_push_menu(-1, -1);
  build_timing_menu(timing_root);
}

static void configure_gpio() {
  struct menu_item *gpio_root = ui_push_menu(-1, -1);
  build_gpio_menu(gpio_root);
}










static struct menu_item *nas_ip_item;
static struct menu_item *nas_user_item;
static struct menu_item *nas_password_item;
static struct menu_item *nas_protocol_item;
static struct menu_item *nas_path_item;

static char nas_dir_prima[NUM_DIR_TYPES][256];








static const char *const nas_path_chiavi[5] = {
  "c64", "c128", "vic20", "pet", "plus4"};
static char nas_path_letti[5][MAX_STR_VAL_LEN];

static int nas_famiglia(void) {
  switch (emux_machine_class) {
  case BMC64_MACHINE_CLASS_C128:
    return 1;
  case BMC64_MACHINE_CLASS_VIC20:
    return 2;
  case BMC64_MACHINE_CLASS_PET:
    return 3;
  case BMC64_MACHINE_CLASS_PLUS4:
  case BMC64_MACHINE_CLASS_PLUS4EMU:
    return 4;
  default:
    return 0;
  }
}



static const char *nas_percorso(void) {
  const char *p = nas_path_item != NULL ? nas_path_item->str_value : "";
  while (*p == '\\' || *p == '/' || *p == ' ') {
    p++;
  }
  return p;
}


static const char *nas_condivisione(void) {
  static char c[MAX_STR_VAL_LEN];
  const char *p = nas_percorso();
  size_t n = strcspn(p, "\\/");
  if (n >= sizeof(c)) {
    n = sizeof(c) - 1;
  }
  memcpy(c, p, n);
  c[n] = '\0';
  return c;
}



static const char *nas_cartella_macchina(void) {
  static char c[MAX_STR_VAL_LEN];
  const char *p = nas_percorso();
  size_t i = 0;
  p += strcspn(p, "\\/");
  while (*p != '\0' && i + 2 < sizeof(c)) {
    if (*p == '\\' || *p == '/') {
      while (*p == '\\' || *p == '/') {
        p++;
      }
      if (*p != '\0') {
        c[i++] = '/';
      }
    } else {
      c[i++] = *p++;
    }
  }
  c[i] = '\0';
  if (i == 0) {
    strcpy(c, "/");
  }
  return c;
}

static void nas_applica(void) {
  if (nas_ip_item == NULL) {
    return;
  }
  bmc_nas_imposta(nas_ip_item->str_value, nas_user_item->str_value,
                  nas_password_item->str_value, nas_protocol_item->value);
  bmc_nas_imposta_condivisione(nas_condivisione());
}

static void nas_carica(void) {
  char riga[256];
  FILE *fp;

  if (nas_ip_item == NULL) {
    return;
  }
  memset(nas_path_letti, 0, sizeof(nas_path_letti));
  fp = fopen("/nas.txt", "r");
  while (fp != NULL && fgets(riga, sizeof(riga), fp) != NULL) {
    char *v = strchr(riga, '=');
    struct menu_item *campo = NULL;

    if (v == NULL) {
      continue;
    }
    *v++ = '\0';
    v[strcspn(v, "\r\n")] = '\0';
    if (strcmp(riga, "nas_ip") == 0) {
      campo = nas_ip_item;
    } else if (strcmp(riga, "nas_user") == 0) {
      campo = nas_user_item;
    } else if (strcmp(riga, "nas_password") == 0) {
      campo = nas_password_item;
    } else if (strcmp(riga, "nas_smb") == 0) {
      int n = atoi(v);
      if (n >= 0 && n < nas_protocol_item->num_choices) {
        nas_protocol_item->value = n;
      }
    } else if (strncmp(riga, "nas_path_", 9) == 0) {
      for (int i = 0; i < 5; i++) {
        if (strcmp(riga + 9, nas_path_chiavi[i]) == 0) {
          snprintf(nas_path_letti[i], sizeof(nas_path_letti[i]), "%s", v);
        }
      }
    }
    if (campo != NULL) {
      strncpy(campo->str_value, v, campo->max_length);
      campo->str_value[campo->max_length] = '\0';
      campo->value = strlen(campo->str_value);
    }
  }
  if (fp != NULL) {
    fclose(fp);
  }


  snprintf(nas_path_item->str_value, sizeof(nas_path_item->str_value), "%s",
           nas_path_letti[nas_famiglia()]);
  nas_path_item->str_value[nas_path_item->max_length] = '\0';
  nas_path_item->value = strlen(nas_path_item->str_value);


  nas_password_item->textfield_masked =
      nas_password_item->str_value[0] != '\0';
  nas_applica();
}

static int nas_salva(void) {
  FILE *fp;

  if (nas_ip_item == NULL) {
    return 0;
  }
  fp = fopen("/nas.txt", "w");
  if (fp == NULL) {
    return 1;
  }
  fprintf(fp, "nas_ip=%s\n", nas_ip_item->str_value);
  fprintf(fp, "nas_user=%s\n", nas_user_item->str_value);
  fprintf(fp, "nas_password=%s\n", nas_password_item->str_value);
  fprintf(fp, "nas_smb=%d\n", nas_protocol_item->value);


  snprintf(nas_path_letti[nas_famiglia()], sizeof(nas_path_letti[0]), "%s",
           nas_path_item->str_value);
  for (int i = 0; i < 5; i++) {
    if (nas_path_letti[i][0] != '\0') {
      fprintf(fp, "nas_path_%s=%s\n", nas_path_chiavi[i], nas_path_letti[i]);
    }
  }
  fclose(fp);
  return 0;
}



static int nas_collega_dal_menu(void) {
  int ok;

  nas_applica();
  if (bmc_nas_collegato()) {
    return 1;
  }
  ui_info("Connecting to the NAS...");
  ok = bmc_nas_collega() == 0;
  ui_pop_menu();
  if (!ok) {
    ui_error("NAS: %s", bmc_nas_errore());
  }
  return ok;
}

static void nas_prova(void) {
  unsigned long long dim = 0;
  int cartella = 0;
  char p[MAX_STR_VAL_LEN + 8], vista[MAX_STR_VAL_LEN];
  int i;

  nas_applica();
  bmc_nas_scollega();
  if (!nas_collega_dal_menu()) {
    return;
  }
  snprintf(p, sizeof(p), "NAS:%s", nas_cartella_macchina());
  snprintf(vista, sizeof(vista), "%s", nas_percorso());
  for (i = 0; vista[i] != '\0'; i++) {
    if (vista[i] == '/') {
      vista[i] = '\\';
    }
  }
  if (bmc_nas_stat(p, &dim, &cartella) == 0 && cartella) {
    ui_info("NAS connected (%s).\n%s is there.", bmc_nas_versione_usata(),
            vista);
  } else {
    ui_error("NAS connected, but %s is missing.", vista);
  }
}




static int nas_entra_nel_volume(void) {
  if (!nas_collega_dal_menu()) {
    return 0;
  }
  if (strcmp(current_volume_name, "NAS:") != 0) {
    memcpy(nas_dir_prima, current_dir_names, sizeof(nas_dir_prima));
    for (int t = 0; t < NUM_DIR_TYPES; t++) {
      strcpy(current_dir_names[t], nas_cartella_macchina());
    }
  }
  strcpy(current_volume_name, "NAS:");
  return 1;
}


static void nas_torna_alla_scheda(void) {
  if (strcmp(current_volume_name, "NAS:") == 0) {
    memcpy(current_dir_names, nas_dir_prima, sizeof(nas_dir_prima));
  }
}

// Show a pop up menu with the available drive volumes.
// The item's id will be passed along to every item created
// here. The action to perform is dicatated by sub_id.
static void filesystem_change_volume(struct menu_item *item) {
  struct menu_item *vol_root = ui_push_menu(12, 8);
  struct menu_item *item2;

  // SD card is always available
  item2 = ui_menu_add_button(item->id, vol_root, "SD");
  item2->sub_id = MENU_SUB_CHANGE_VOLUME;
  item2->value = MENU_VOLUME_SD;

  int available[3];
  circle_find_usb(&available);

  if (available[0]) {
    item2 = ui_menu_add_button(item->id, vol_root, "USB1");
    item2->sub_id = MENU_SUB_CHANGE_VOLUME;
    item2->value = MENU_VOLUME_USB1;
  }
  if (available[1]) {
    item2 = ui_menu_add_button(item->id, vol_root, "USB2");
    item2->sub_id = MENU_SUB_CHANGE_VOLUME;
    item2->value = MENU_VOLUME_USB2;
  }
  if (available[2]) {
    item2 = ui_menu_add_button(item->id, vol_root, "USB3");
    item2->sub_id = MENU_SUB_CHANGE_VOLUME;
    item2->value = MENU_VOLUME_USB3;
  }


  int floppy[2];
  circle_find_floppy(&floppy);
  if (floppy[0]) {
    item2 = ui_menu_add_button(item->id, vol_root, "FLP1");
    item2->sub_id = MENU_SUB_CHANGE_VOLUME;
    item2->value = MENU_VOLUME_FLP1;
  }
  if (floppy[1]) {
    item2 = ui_menu_add_button(item->id, vol_root, "FLP2");
    item2->sub_id = MENU_SUB_CHANGE_VOLUME;
    item2->value = MENU_VOLUME_FLP2;
  }


  if (bmc_nas_configurato()) {
    item2 = ui_menu_add_button(item->id, vol_root, "NAS");
    item2->sub_id = MENU_SUB_CHANGE_VOLUME;
    item2->value = MENU_VOLUME_NAS;
  }
}

static void drive_change_rom() {
  struct menu_item *root = ui_push_menu(12, 8);







  switch (emux_machine_class) {
  case BMC64_MACHINE_CLASS_PET:
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_2031, root, "2031...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_2040, root, "2040...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_3040, root, "3040...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_4040, root, "4040...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1001, root, "1001...");
    break;
  case BMC64_MACHINE_CLASS_PLUS4:
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1541, root, "1541...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1541II, root, "1541II...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1551, root, "1551...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1571, root, "1571...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1581, root, "1581...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_CMDHD, root, "CMDHD...");
    break;
  default:

    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1541, root, "1541...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1541II, root, "1541II...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1571, root, "1571...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_1581, root, "1581...");
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM_CMDHD, root, "CMDHD...");
    break;
  }
}

static void ui_set_hotkeys() {
  kbd_set_hotkey_function(0, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(1, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(2, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(3, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(4, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(5, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(6, 0, BTN_ASSIGN_UNDEF);
  kbd_set_hotkey_function(7, 0, BTN_ASSIGN_UNDEF);

  // Apply hotkey selections to keyboard handler
  if (hotkey_cf1_item->value > 0) {
    kbd_set_hotkey_function(
        0, KEYCODE_F1, hotkey_cf1_item->choice_ints[hotkey_cf1_item->value]);
  }
  if (hotkey_cf3_item->value > 0) {
    kbd_set_hotkey_function(
        1, KEYCODE_F3, hotkey_cf3_item->choice_ints[hotkey_cf3_item->value]);
  }
  if (hotkey_cf5_item->value > 0) {
    kbd_set_hotkey_function(
        2, KEYCODE_F5, hotkey_cf5_item->choice_ints[hotkey_cf5_item->value]);
  }
  if (hotkey_cf7_item->value > 0) {
    kbd_set_hotkey_function(
        3, KEYCODE_F7, hotkey_cf7_item->choice_ints[hotkey_cf7_item->value]);
  }
  if (hotkey_tf1_item->value > 0) {
    kbd_set_hotkey_function(
        4, KEYCODE_F1, hotkey_tf1_item->choice_ints[hotkey_tf1_item->value]);
  }
  if (hotkey_tf3_item->value > 0) {
    kbd_set_hotkey_function(
        5, KEYCODE_F3, hotkey_tf3_item->choice_ints[hotkey_tf3_item->value]);
  }
  if (hotkey_tf5_item->value > 0) {
    kbd_set_hotkey_function(
        6, KEYCODE_F5, hotkey_tf5_item->choice_ints[hotkey_tf5_item->value]);
  }
  if (hotkey_tf7_item->value > 0) {
    kbd_set_hotkey_function(
        7, KEYCODE_F7, hotkey_tf7_item->choice_ints[hotkey_tf7_item->value]);
  }
}



static int is_mouse_device(int device) {
   return device == JOYDEV_MOUSE || device == JOYDEV_MOUSE_MICROMYS;
}





static int is_vice_mouse_type(int device) {
   return is_mouse_device(device) || device == JOYDEV_SPINNER;
}

// If any joystick is set to mouse, enable it in the emulator.
// FCIII apparently doesn't like the mouse enabled unless necessary
static void set_need_mouse() {
   int need_mouse = 0;
   int index;
   // Only ports 1 and 2 can be assigned a mouse.
   if (port_1_menu_item) {
      index = port_1_menu_item->value;
      if (is_mouse_device(port_1_menu_item->choice_ints[index])) {
         need_mouse = 1;
      }
   }
   if (port_2_menu_item) {
      index = port_2_menu_item->value;



      if (is_mouse_device(port_2_menu_item->choice_ints[index])) {
         need_mouse = 1;
      }
   }
   emux_set_int(Setting_Mouse, need_mouse);
}

// Sets joydev port 'p' (1-4) to JOYDEV_* value 'value' and makes sure
// all other ports get the mouse turned off if this port got a mouse.
static void set_joy_item_to_value(int p, int value) {
    joydevs[p-1].device = value;
    if (is_vice_mouse_type(value)) {
      // If any other port has mouse, set it to none.
      for (int l = 0; l < MAX_JOY_PORTS; l++) {
         if (l == (p-1)) continue;

         struct menu_item* other;
         switch (l) {
            case 0:
               other = port_1_menu_item; break;
            case 1:
               other = port_2_menu_item; break;
            case 2:
               other = port_3_menu_item; break;
            case 3:
               other = port_4_menu_item; break;
            default:
               assert(0);
         }
         if (other && is_vice_mouse_type(other->choice_ints[other->value])) {
           emux_set_joy_port_device(l+1, JOYDEV_NONE);
           joydevs[l].device = JOYDEV_NONE;
           other->value = 0;
         }
      }
    }
    emux_set_joy_port_device(p, value);
}











static int validated_port_choice(struct menu_item* it) {
  if (it == NULL) {
    return JOYDEV_NONE;
  }
  if (it->value < 0 || it->value >= it->num_choices ||
      it->choice_disabled[it->value]) {
    it->value = 0;
  }
  return it->choice_ints[it->value];
}

void ui_set_joy_items() {
  int joydev;
  int i;
  for (joydev = 0; joydev < MAX_JOY_PORTS; joydev++) {
    struct menu_item *dst;

    if (joydevs[joydev].port == 1) {
      dst = port_1_menu_item;
    } else if (joydevs[joydev].port == 2) {
      dst = port_2_menu_item;
    } else if (joydevs[joydev].port == 3) {
      dst = port_3_menu_item;
    } else if (joydevs[joydev].port == 4) {
      dst = port_4_menu_item;
    } else {
      continue;
    }

    if (!dst)
      continue;

    // Find which choice matches the device selected and
    // make sure the menu item matches
    for (i = 0; i < dst->num_choices; i++) {
      if (dst->choice_ints[i] == joydevs[joydev].device) {
        dst->value = i;
        break;
      }
    }
  }

  if (port_1_menu_item) {
     set_joy_item_to_value(1, validated_port_choice(port_1_menu_item));
  }
  if (port_2_menu_item) {
     set_joy_item_to_value(2, validated_port_choice(port_2_menu_item));
  }
  if (port_3_menu_item) {
     set_joy_item_to_value(3, validated_port_choice(port_3_menu_item));
  }
  if (port_4_menu_item) {
     set_joy_item_to_value(4, validated_port_choice(port_4_menu_item));
  }
  set_need_mouse();
}

static int do_use_int_scaling(int layer, int silent) {
  int canvas_index;
  if (layer == FB_LAYER_VIC) {
    canvas_index = VIC_INDEX;
  } else if (layer == FB_LAYER_VDC) {
    canvas_index = VDC_INDEX;
  } else {
    if (!silent)
       ui_error("Bad display num");
    return 0;
  }




  if (h_border_item[canvas_index] == NULL ||
      v_border_item[canvas_index] == NULL ||
      h_stretch_item[canvas_index] == NULL ||
      v_stretch_item[canvas_index] == NULL) {
    return 0;
  }

  int fbw, fbh, sx, sy;
  int display_num = canvas_index;
  // For the PET, 1st display is 40 column models, 2nd is 80 column models
  if (emux_machine_class == BMC64_MACHINE_CLASS_PET) {
     int cols;
     emux_get_int(Setting_VideoSize, &cols);
     display_num = cols == 40 ? 0 : 1;
  }
  circle_get_scaling_params(display_num, &fbw, &fbh, &sx, &sy);

  int dpw, dph, tmp;
  circle_get_fbl_dimensions(layer,
                            &dpw, &dph,
                            &tmp, &tmp,
                            &tmp, &tmp,
                            &tmp, &tmp);


  if (fbw <= 0 || fbh <= 0 || sx <= 0 || sy <= 0) {
     if (!silent)
        ui_error("Bad or missing params");
     return 0;
  }

  if (fbw % 2 != 0) {
     if (!silent)
        ui_error("fbw must be even");
     return 0;
  }

  if (fbh % 2 != 0) {
     if (!silent)
        ui_error("fbh must be even");
     return 0;
  }

  if (sx > dpw) {
     if (!silent)
        ui_error("sx too large for display");
     return 0;
  }

  if (sy > dph) {
     if (!silent)
        ui_error("sy too large for display");
     return 0;
  }

  h_integer_stretch[canvas_index] = sx;
  v_integer_stretch[canvas_index] = sy;

  h_border_item[canvas_index]->value =
     (fbw - canvas_state[canvas_index].gfx_w) / 2;
  if (h_border_item[canvas_index]->value >
         h_border_item[canvas_index]->max) {
     if (!silent)
        ui_error("fbw too large");
     h_border_item[canvas_index]->value =
        h_border_item[canvas_index]->max;
     return 0;
  } else if (h_border_item[canvas_index]->value <
                h_border_item[canvas_index]->min) {
     if (!silent)
        ui_error("fbw too small");
     h_border_item[canvas_index]->value =
        h_border_item[canvas_index]->min;
     return 0;
  }

  v_border_item[canvas_index]->value =
     (fbh - canvas_state[canvas_index].gfx_h) / 2;
  if (v_border_item[canvas_index]->value >
     v_border_item[canvas_index]->max) {
     if (!silent)
        ui_error("fbh too large");
     v_border_item[canvas_index]->value =
        v_border_item[canvas_index]->max;
     return 0;
  } else if (v_border_item[canvas_index]->value <
                v_border_item[canvas_index]->min) {
     if (!silent)
        ui_error("fbh too small");
     v_border_item[canvas_index]->value =
        v_border_item[canvas_index]->min;
     return 0;
  }

  h_stretch_item[canvas_index]->value =
     ceil((double)h_integer_stretch[canvas_index] * 1000.0 / (double)dph);
  v_stretch_item[canvas_index]->value =
     ceil((double)v_integer_stretch[canvas_index] * 1000.0 / (double)dph);

  use_h_integer_stretch[canvas_index] = 1;
  use_v_integer_stretch[canvas_index] = 1;
  return 1;
}




static void geo_composito_all_avvio(void) {
  const int std = geo_std();
  if (!is_composite()) {
    return;
  }
  geo_dalle_voci(&geo_hdmi);
  if (geo_comp[std].presente == 0x3F) {
    h_center_item[0]->value = geo_comp[std].hc;
    v_center_item[0]->value = geo_comp[std].vc;
    h_border_item[0]->value = geo_comp[std].hb;
    v_border_item[0]->value = geo_comp[std].vb;
    h_stretch_item[0]->value = geo_comp[std].hs;
    v_stretch_item[0]->value = geo_comp[std].vs;
    use_h_integer_stretch[VIC_INDEX] = 0;
    use_v_integer_stretch[VIC_INDEX] = 0;
  } else {






    h_center_item[0]->value = 0;
    if (!do_use_int_scaling(FB_LAYER_VIC, 1 /* silent */)) {
      printf("[VID] composito: i bordi dagli scaling_params non si possono "
             "applicare, restano quelli di settings\n");
    } else {







      use_h_integer_stretch[VIC_INDEX] = 0;
      use_v_integer_stretch[VIC_INDEX] = 0;
    }
  }
}

static void next_integer_scaling(int layer,
                                 int canvas_index,
                                 int dimension) {
  int dpw, dph, fbw, fbh, sw, sh, dw, dh;
  circle_get_fbl_dimensions(layer,
                            &dpw, &dph,
                            &fbw, &fbh,
                            &sw, &sh,
                            &dw, &dh);

  int dim = dimension == 0 ? sw : sh;
  int scaled_dim = dimension == 0 ? dw : dh;
  int max = dimension == 0 ? dpw : dph;

  int scale = scaled_dim / dim;
  scale = scale + 1;

  scaled_dim = dim * scale;
  if (scaled_dim > max) {
     // Start back at 1.
     if (dimension == 0)
        scaled_dim = sw;
     else
        scaled_dim = sh;
  }

  // Now express the scale as a ratio of the display height for the menu
  // This won't be the actual value that determines the final dimension
  // due to rounding errors.  'scaled dim' is what will be sent to
  // fbl.
  int menu_stretch_value = ceil((double)scaled_dim * 1000.0 / (double)dph);

  if (dimension == 0) {
     h_stretch_item[canvas_index]->value = menu_stretch_value;
     h_integer_stretch[canvas_index] = scaled_dim;
     use_h_integer_stretch[canvas_index] = 1;
  } else {
     v_stretch_item[canvas_index]->value = menu_stretch_value;
     v_integer_stretch[canvas_index] = scaled_dim;
     use_v_integer_stretch[canvas_index] = 1;
  }
}

static int save_settings() {
  FILE *fp;
  const char *settings_filename;
  switch (emux_machine_class) {
  case BMC64_MACHINE_CLASS_C64:
    settings_filename = "/settings.txt";
    break;
  case BMC64_MACHINE_CLASS_SCPU64:
    settings_filename = "/settings-scpu64.txt";
    break;
  case BMC64_MACHINE_CLASS_C128:
    settings_filename = "/settings-c128.txt";
    break;
  case BMC64_MACHINE_CLASS_VIC20:
    settings_filename = "/settings-vic20.txt";
    break;
  case BMC64_MACHINE_CLASS_PLUS4:
    settings_filename = "/settings-plus4.txt";
    break;
  case BMC64_MACHINE_CLASS_PLUS4EMU:
    settings_filename = "/settings-plus4emu.txt";
    break;
  case BMC64_MACHINE_CLASS_PET:
    settings_filename = "/settings-pet.txt";
    break;
  default:
    printf("ERROR: Unhandled machine\n");
    return 1;
  }

  fp = fopen(settings_filename, "w");

  int r = emux_save_settings();
  if (r < 0) {
    printf("resource_save failed with %d\n", r);


    if (fp != NULL) {
      fclose(fp);
    }
    return 1;
  }

  if (fp == NULL)
    return 1;

  if (port_1_menu_item) {
    fprintf(fp, "port_1=%d\n", port_1_menu_item->value);
  }
  if (port_2_menu_item) {
    fprintf(fp, "port_2=%d\n", port_2_menu_item->value);
  }
  if (port_3_menu_item) {
    fprintf(fp, "port_3=%d\n", port_3_menu_item->value);
  }
  if (port_4_menu_item) {
    fprintf(fp, "port_4=%d\n", port_4_menu_item->value);
  }
  if (spinner_sens_item) {
    fprintf(fp, "spinner_sensitivity=%d\n", spinner_sens_item->value);
  }

  for (int k = 0;k < MAX_USB_DEVICES; k++) {
    fprintf(fp, "usb_%d=%d\n", k, usb_pref[k]);
    fprintf(fp, "usb_x_%d=%d\n", k, usb_x_axis[k]);
    fprintf(fp, "usb_y_%d=%d\n", k, usb_y_axis[k]);
    fprintf(fp, "usb_x_t_%d=%d\n", k, (int)(usb_x_thresh[k] * 100.0f));
    fprintf(fp, "usb_y_t_%d=%d\n", k, (int)(usb_y_thresh[k] * 100.0f));
  }





  if (palette_item[0] != NULL) {
    fprintf(fp, "palette=%d\n", palette_item[0]->value);
  }
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
      palette_item[1] != NULL) {
    fprintf(fp, "palette2=%d\n", palette_item[1]->value);
  }

  for (int k = 0; k < MAX_USB_DEVICES; k++) {
    for (int i = 0; i < MAX_USB_BUTTONS; i++) {
      fprintf(fp, "usb_btn_%d=%d\n", k, usb_button_assignments[k][i]);
    }
  }
  fprintf(fp, "hotkey_cf1=%d\n", hotkey_cf1_item->value);
  fprintf(fp, "hotkey_cf3=%d\n", hotkey_cf3_item->value);
  fprintf(fp, "hotkey_cf5=%d\n", hotkey_cf5_item->value);
  fprintf(fp, "hotkey_cf7=%d\n", hotkey_cf7_item->value);
  fprintf(fp, "hotkey_tf1=%d\n", hotkey_tf1_item->value);
  fprintf(fp, "hotkey_tf3=%d\n", hotkey_tf3_item->value);
  fprintf(fp, "hotkey_tf5=%d\n", hotkey_tf5_item->value);
  fprintf(fp, "hotkey_tf7=%d\n", hotkey_tf7_item->value);
  // Can't change the 'overlay_*' names, legacy.
  fprintf(fp, "overlay=%d\n", statusbar_item->value);
  fprintf(fp, "overlay_padding=%d\n", statusbar_padding_item->value);
  fprintf(fp, "overlay_info=%d\n", statusbar_info_item->value);
  fprintf(fp, "vkbd_trans=%d\n", vkbd_transparency_item->value);
  fprintf(fp, "tapereset=%d\n", tape_reset_with_machine_item->value);
  fprintf(fp, "reset_confirm=%d\n", reset_confirm_item->value);
  if (drive_flush_item != NULL) {
    fprintf(fp, "drive_flush=%d\n", drive_flush_item->value);
  }
  fprintf(fp, "scaling_interp=%d\n", scaling_interp_item->value);
  fprintf(fp, "gpio_config=%d\n", gpio_config_item->choice_ints[gpio_config_item->value]);
  if (network_device_item != NULL) {
    fprintf(fp, "network_device=%d\n", network_device_item->value);
    saved_network_device = network_device_item->value;
    network_reboot_prompted = 0;
  }
  if (timezone_offset_item != NULL) {
    fprintf(fp, "timezone_offset_minutes=%d\n",
            timezone_offset_item->choice_ints[timezone_offset_item->value]);
  }
  if (timezone_dst_item != NULL) {
    fprintf(fp, "timezone_dst=%d\n", timezone_dst_item->value);
  }
  if (smb_password_item != NULL && smb_password_item->str_value[0] != '\0') {
    fprintf(fp, "smb_password=%s\n", smb_password_item->str_value);
  }
  if (network_address_mode_item != NULL) {
    fprintf(fp, "network_address_mode=%d\n", network_address_mode_item->value);
    for (int i = 0; i < 4; i++) {
      if (network_static_item[i] != NULL &&
          network_static_item[i]->str_value[0] != '\0') {
        fprintf(fp, "%s=%s\n", network_static_keys[i],
                network_static_item[i]->str_value);
      }
    }
  }
  if (network_modem_address_item != NULL) {
    fprintf(fp, "network_modem_address=%d\n",
            network_modem_address_item->value);
  }
  if (network_modem_item != NULL) {
    fprintf(fp, "network_modem=%d\n", network_modem_item->value);
  }

  nas_salva();

  geo_salva(fp);
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     fprintf(fp, "h_center_1=%d\n", h_center_item[1]->value);
     fprintf(fp, "v_center_1=%d\n", v_center_item[1]->value);
     fprintf(fp, "h_border_1=%d\n", h_border_item[1]->value);
     fprintf(fp, "v_border_1=%d\n", v_border_item[1]->value);
     fprintf(fp, "h_stretch_1=%d\n", h_stretch_item[1]->value);
     fprintf(fp, "v_stretch_1=%d\n", v_stretch_item[1]->value);
  }

  int drive_type;

  emux_get_int_1(Setting_DriveNType, &drive_type, 8);
  fprintf(fp, "drive_type_8=%d\n", drive_type);
  emux_get_int_1(Setting_DriveNType, &drive_type, 9);
  fprintf(fp, "drive_type_9=%d\n", drive_type);
  emux_get_int_1(Setting_DriveNType, &drive_type, 10);
  fprintf(fp, "drive_type_10=%d\n", drive_type);
  emux_get_int_1(Setting_DriveNType, &drive_type, 11);
  fprintf(fp, "drive_type_11=%d\n", drive_type);




  for (int u = 8; u <= 11; u++) {





    fprintf(fp, "real_drive_%d=%d\n", u, emux_is_real_drive(u));
  }

  if (realdrive_veloce_item != NULL) {
    fprintf(fp, "real_drive_fast=%d\n", realdrive_veloce_item->value);
  }
  if (realdrive_respiro_item != NULL) {
    fprintf(fp, "real_drive_pause=%d\n",
            realdrive_respiro_item->choice_ints[realdrive_respiro_item->value]);
  }
  if (realdrive_log_item != NULL) {
    fprintf(fp, "real_drive_log=%d\n", realdrive_log_item->value);
  }

  fprintf(fp, "pot_x_high=%d\n", pot_x_high_value);
  fprintf(fp, "pot_x_low=%d\n", pot_x_low_value);
  fprintf(fp, "pot_y_high=%d\n", pot_y_high_value);
  fprintf(fp, "pot_y_low=%d\n", pot_y_low_value);

  fprintf(fp, "keyset_1_up=%d\n", keyset_codes[0][KEYSET_UP]);
  fprintf(fp, "keyset_1_down=%d\n", keyset_codes[0][KEYSET_DOWN]);
  fprintf(fp, "keyset_1_left=%d\n", keyset_codes[0][KEYSET_LEFT]);
  fprintf(fp, "keyset_1_right=%d\n", keyset_codes[0][KEYSET_RIGHT]);
  fprintf(fp, "keyset_1_fire=%d\n", keyset_codes[0][KEYSET_FIRE]);
  fprintf(fp, "keyset_1_potx=%d\n", keyset_codes[0][KEYSET_POTX]);
  fprintf(fp, "keyset_1_poty=%d\n", keyset_codes[0][KEYSET_POTY]);

  fprintf(fp, "keyset_2_up=%d\n", keyset_codes[1][KEYSET_UP]);
  fprintf(fp, "keyset_2_down=%d\n", keyset_codes[1][KEYSET_DOWN]);
  fprintf(fp, "keyset_2_left=%d\n", keyset_codes[1][KEYSET_LEFT]);
  fprintf(fp, "keyset_2_right=%d\n", keyset_codes[1][KEYSET_RIGHT]);
  fprintf(fp, "keyset_2_fire=%d\n", keyset_codes[1][KEYSET_FIRE]);
  fprintf(fp, "keyset_2_potx=%d\n", keyset_codes[1][KEYSET_POTX]);
  fprintf(fp, "keyset_2_poty=%d\n", keyset_codes[1][KEYSET_POTY]);

  fprintf(fp, "key_binding_1=%d\n", key_bindings[0]);
  fprintf(fp, "key_binding_2=%d\n", key_bindings[1]);
  fprintf(fp, "key_binding_3=%d\n", key_bindings[2]);
  fprintf(fp, "key_binding_4=%d\n", key_bindings[3]);
  fprintf(fp, "key_binding_5=%d\n", key_bindings[4]);
  fprintf(fp, "key_binding_6=%d\n", key_bindings[5]);

  fprintf(fp, "volume=%d\n", volume_item->value);



  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     fprintf(fp, "active_display=%d\n", active_display_item->value);
     fprintf(fp, "second_hdmi=%d\n", secondo_hdmi_item->value);
  }
  fprintf(fp, "dir_convention=%d\n", dir_convention_item->value);
  fprintf(fp, "use_int_scaling_0=%d\n", use_scaling_params_item[0]->value);
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     fprintf(fp, "use_int_scaling_1=%d\n", use_scaling_params_item[1]->value);
  }

  for (int i = 0 ; i < NUM_GPIO_PINS; i++) {
     fprintf (fp, "custom_gpio=%d,%d\n", i, gpio_bindings[i]);
  }

  fprintf(fp,"s_curvature=%d\n", s_curvature_item->value);
  fprintf(fp,"s_curvature_x=%d\n", s_curvature_x_item->value);
  fprintf(fp,"s_curvature_y=%d\n", s_curvature_y_item->value);
  fprintf(fp,"s_skew_x=%d\n", s_skew_x_item->value);
  fprintf(fp,"s_skew_y=%d\n", s_skew_y_item->value);
  fprintf(fp,"s_trapezoid=%d\n", s_trapezoid_item->value);
  fprintf(fp,"s_rotation=%d\n", s_rotation_item->value);
  fprintf(fp,"s_overscan=%d\n", s_overscan_item->value);
  fprintf(fp,"s_convergence=%d\n", s_convergence_item->value);
  fprintf(fp,"s_red_offset_x=%d\n", s_red_offset_x_item->value);
  fprintf(fp,"s_red_offset_y=%d\n", s_red_offset_y_item->value);
  fprintf(fp,"s_blue_offset_x=%d\n", s_blue_offset_x_item->value);
  fprintf(fp,"s_blue_offset_y=%d\n", s_blue_offset_y_item->value);
  fprintf(fp,"s_convergence_radial_strength=%d\n",
          s_convergence_radial_strength_item->value);
  fprintf(fp,"s_horizontal_filtering=%d\n", s_horizontal_filtering_item->value);
  fprintf(fp,"s_sigma_x=%d\n", s_sigma_x_item->value);
  fprintf(fp,"s_edge_blur=%d\n", s_edge_blur_item->value);
  fprintf(fp,"s_edge_blur_strength=%d\n", s_edge_blur_strength_item->value);
  fprintf(fp,"s_edge_blur_radius=%d\n", s_edge_blur_radius_item->value);
  fprintf(fp,"s_sharper=%d\n",
          (!s_horizontal_filtering_item->value || s_sigma_x_item->value < 50) ? 1 : 0);
  fprintf(fp,"s_mask=%d\n",
          s_mask_enable_item->value ? s_mask_item->value + 1 : 0);
  fprintf(fp,"s_mask_enable=%d\n", s_mask_enable_item->value);
  fprintf(fp,"s_mask_type=%d\n", s_mask_item->value);
  fprintf(fp,"s_mask_brightness=%d\n", s_mask_brightness_item->value);
  fprintf(fp,"s_scanlines=%d\n", s_scanlines_item->value);
  fprintf(fp,"s_multisample=%d\n", s_multisample_item->value);
  fprintf(fp,"s_scanline_weight=%d\n", s_scanline_weight_item->value);
  fprintf(fp,"s_scanline_gap_brightness=%d\n", s_scanline_gap_brightness_item->value);
  fprintf(fp,"s_bloom=%d\n", s_bloom_item->value);
  fprintf(fp,"s_bloom_factor=%d\n", s_bloom_factor_item->value);
  fprintf(fp,"s_vignette=%d\n", s_vignette_item->value);
  fprintf(fp,"s_vignette_strength=%d\n", s_vignette_strength_item->value);
  fprintf(fp,"s_vignette_scale=%d\n", s_vignette_scale_item->value);
  fprintf(fp,"s_vignette_softness=%d\n", s_vignette_softness_item->value);
  fprintf(fp,"s_uneven_illumination=%d\n", s_uneven_illumination_item->value);
  fprintf(fp,"s_uneven_illumination_strength=%d\n",
          s_uneven_illumination_strength_item->value);
  fprintf(fp,"s_uneven_illumination_scale=%d\n",
          s_uneven_illumination_scale_item->value);
  fprintf(fp,"s_horizontal_jitter=%d\n", s_horizontal_jitter_item->value);
  fprintf(fp,"s_horizontal_jitter_strength=%d\n",
          s_horizontal_jitter_strength_item->value);
  fprintf(fp,"s_horizontal_jitter_frequency=%d\n",
          s_horizontal_jitter_frequency_item->value);
  fprintf(fp,"s_horizontal_jitter_speed=%d\n",
          s_horizontal_jitter_speed_item->value);
  fprintf(fp,"s_composite_artifacts=%d\n", s_composite_artifacts_item->value);
  fprintf(fp,"s_composite_chroma_blur=%d\n",
          s_composite_chroma_blur_item->value);
  fprintf(fp,"s_composite_luma_sharpen=%d\n",
          s_composite_luma_sharpen_item->value);
  fprintf(fp,"s_composite_color_bleed=%d\n",
          s_composite_color_bleed_item->value);
  fprintf(fp,"s_glass_reflection=%d\n", s_glass_reflection_item->value);
  fprintf(fp,"s_glass_reflection_angle=%d\n",
          s_glass_reflection_angle_item->value);
  fprintf(fp,"s_glass_reflection_width=%d\n",
          s_glass_reflection_width_item->value);
  fprintf(fp,"s_glass_reflection_position=%d\n",
          s_glass_reflection_position_item->value);
  fprintf(fp,"s_rounded_screen_mask=%d\n", s_rounded_screen_mask_item->value);
  fprintf(fp,"s_rounded_corner_radius=%d\n",
          s_rounded_corner_radius_item->value);
  fprintf(fp,"s_rounded_border_softness=%d\n",
          s_rounded_border_softness_item->value);
  fprintf(fp,"s_edge_glow=%d\n", s_edge_glow_item->value);
  fprintf(fp,"s_edge_glow_strength=%d\n", s_edge_glow_strength_item->value);
  fprintf(fp,"s_edge_glow_width=%d\n", s_edge_glow_width_item->value);
  fprintf(fp,"s_noise=%d\n", s_noise_item->value);
  fprintf(fp,"s_luminance_noise=%d\n", s_luminance_noise_item->value);
  fprintf(fp,"s_chroma_noise=%d\n", s_chroma_noise_item->value);
  fprintf(fp,"s_noise_speed=%d\n", s_noise_speed_item->value);
  fprintf(fp,"s_gamma=%d\n",
          s_output_response_item->value ? s_response_mode_item->value + 1 : 0);
  fprintf(fp,"s_output_response=%d\n", s_output_response_item->value);
  fprintf(fp,"s_response_mode=%d\n", s_response_mode_item->value);
  fprintf(fp,"s_level_mapping=%d\n", s_level_mapping_item->value);
  fprintf(fp,"s_input_gamma=%d\n", s_input_gamma_item->value);
  fprintf(fp,"s_output_gamma=%d\n", s_output_gamma_item->value);
  fprintf(fp,"s_response_saturation=%d\n", s_response_saturation_item->value);
  fprintf(fp,"s_black_level=%d\n", s_black_level_item->value);
  fprintf(fp,"s_white_clip=%d\n", s_white_clip_item->value);

  emux_save_additional_settings(fp);

  fclose(fp);
  if (network_device_item != NULL && network_device_item->value == 2 &&
      save_wifi_settings()) {
    return 1;
  }
  emux_log_settings_file(settings_filename);

  return 0;
}

// Make joydev reflect menu choice
static void ui_set_joy_devs() {
  if (port_1_menu_item) {
    joydevs[0].device = validated_port_choice(port_1_menu_item);
  }

  if (port_2_menu_item) {
    joydevs[1].device = validated_port_choice(port_2_menu_item);
  }

  if (port_3_menu_item) {
    joydevs[2].device = validated_port_choice(port_3_menu_item);
  }

  if (port_4_menu_item) {
    joydevs[3].device = validated_port_choice(port_4_menu_item);
  }
}

static int load_settings() {

  int tmp_value;

  emux_get_int(Setting_DriveSoundEmulation, &drive_sounds_item->value);
  emux_get_int(Setting_DriveSoundEmulationVolume, &drive_sounds_vol_item->value);
  if (tape_sounds_item != NULL) {
    emux_get_int(Setting_TapeSoundEmulation, &tape_sounds_item->value);
    emux_get_int(Setting_TapeSoundEmulationVolume,
                 &tape_sounds_vol_item->value);
  }

  brightness_item[0]->value = emux_get_color_brightness(0);
  contrast_item[0]->value = emux_get_color_contrast(0);
  gamma_item[0]->value = emux_get_color_gamma(0);
  tint_item[0]->value = emux_get_color_tint(0);
  saturation_item[0]->value = emux_get_color_saturation(0);

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
    brightness_item[1]->value = emux_get_color_brightness(1);
    contrast_item[1]->value = emux_get_color_contrast(1);
    gamma_item[1]->value = emux_get_color_gamma(1);
    tint_item[1]->value = emux_get_color_tint(1);
    saturation_item[1]->value = emux_get_color_saturation(1);
    emux_get_int(Setting_C128ColumnKey, &c40_80_column_item->value);
  }

  // Default pot values for buttons
  pot_x_high_value = 192;
  pot_x_low_value = 64;
  pot_y_high_value = 192;
  pot_y_low_value = 64;

  FILE *fp;
  switch (emux_machine_class) {
  case BMC64_MACHINE_CLASS_C64:
    fp = fopen("/settings.txt", "r");
    break;
  case BMC64_MACHINE_CLASS_SCPU64:
    fp = fopen("/settings-scpu64.txt", "r");
    break;
  case BMC64_MACHINE_CLASS_C128:
    fp = fopen("/settings-c128.txt", "r");
    break;
  case BMC64_MACHINE_CLASS_VIC20:
    fp = fopen("/settings-vic20.txt", "r");
    break;
  case BMC64_MACHINE_CLASS_PLUS4:
    fp = fopen("/settings-plus4.txt", "r");
    break;
  case BMC64_MACHINE_CLASS_PLUS4EMU:
    fp = fopen("/settings-plus4emu.txt", "r");
    break;
  case BMC64_MACHINE_CLASS_PET:
    fp = fopen("/settings-pet.txt", "r");
    break;
  default:
    printf("ERROR: Unhandled machine\n");
    return 0;
  }

  if (wifi_ssid_item != NULL) {
    load_wifi_settings();
  }

  if (fp == NULL)
    return 0;

  char name_value[256];
  size_t len;
  int value;
  int usb_btn_i[MAX_USB_DEVICES];
  memset(usb_btn_i, 0, sizeof(usb_btn_i));

  while (1) {
    char *line = fgets(name_value, 255, fp);
    if (feof(fp) || line == NULL) break;

    strcpy(name_value, line);

    char *name;
    char *value_str;
    get_key_and_value(name_value, &name, &value_str);
    if (!name || !value_str ||
       strlen(name) == 0 ||
          strlen(value_str) == 0) {
       continue;
    }

    value = atoi(value_str);

    if (emux_handle_loaded_setting(name, value_str, value)) {
       continue;
    }

    if (port_1_menu_item && strcmp(name, "port_1") == 0) {
      port_1_menu_item->value = value;
    } else if (port_2_menu_item && strcmp(name, "port_2") == 0) {
      port_2_menu_item->value = value;
    } else if (port_3_menu_item && strcmp(name, "port_3") == 0) {
      port_3_menu_item->value = value;
    } else if (port_4_menu_item && strcmp(name, "port_4") == 0) {
      port_4_menu_item->value = value;
    } else if (spinner_sens_item && strcmp(name, "spinner_sensitivity") == 0) {
      if (value >= 0 && value < spinner_sens_item->num_choices) {
        spinner_sens_item->value = value;
        emu_spinner_set_sensitivity(spinner_sens_item->choice_ints[value]);
      }
    } else if (palette_item[0] != NULL && strcmp(name, "palette") == 0) {





      palette_item[0]->value = value;
      if (value >= palette_item[0]->num_choices) {
         palette_item[0]->value = palette_item[0]->num_choices - 1;
      }
    } else if (palette_item[1] != NULL && strcmp(name, "palette2") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      palette_item[1]->value = value;
      if (value >= palette_item[1]->num_choices) {
         palette_item[1]->value = palette_item[1]->num_choices - 1;
      }
    } else if (strcmp(name, "alt_f12") == 0) {
      // Old. Equivalent to cf7 = Menu
      hotkey_cf7_item->value = HOTKEY_CHOICE_MENU;
    } else if (strcmp(name, "overlay") == 0) { // legacy name
      statusbar_item->value = value;
    } else if (strcmp(name, "overlay_padding") == 0) { // legacy name
      statusbar_padding_item->value = value;
    } else if (strcmp(name, "overlay_info") == 0) {
      statusbar_info_item->value = value;
    } else if (strcmp(name, "overclock") == 0) {



    } else if (strcmp(name, "vkbd_trans") == 0) {
      vkbd_transparency_item->value = value;
    } else if (strcmp(name, "tapereset") == 0) {
      tape_reset_with_machine_item->value = value;
    } else if (strcmp(name, "pot_x_high") == 0) {
      pot_x_high_value = value;
    } else if (strcmp(name, "pot_x_low") == 0) {
      pot_x_low_value = value;
    } else if (strcmp(name, "pot_y_high") == 0) {
      pot_y_high_value = value;
    } else if (strcmp(name, "pot_y_low") == 0) {
      pot_y_low_value = value;
    } else if (strcmp(name, "hotkey_cf1") == 0) {
      hotkey_cf1_item->value = value;
    } else if (strcmp(name, "hotkey_cf3") == 0) {
      hotkey_cf3_item->value = value;
    } else if (strcmp(name, "hotkey_cf5") == 0) {
      hotkey_cf5_item->value = value;
    } else if (strcmp(name, "hotkey_cf7") == 0) {
      hotkey_cf7_item->value = value;
    } else if (strcmp(name, "hotkey_tf1") == 0) {
      hotkey_tf1_item->value = value;
    } else if (strcmp(name, "hotkey_tf3") == 0) {
      hotkey_tf3_item->value = value;
    } else if (strcmp(name, "hotkey_tf5") == 0) {
      hotkey_tf5_item->value = value;
    } else if (strcmp(name, "hotkey_tf7") == 0) {
      hotkey_tf7_item->value = value;
    } else if (strcmp(name, "reset_confirm") == 0) {
      reset_confirm_item->value = value;
    } else if (strcmp(name, "drive_flush") == 0) {
      if (drive_flush_item != NULL && value >= 0 &&
          value < drive_flush_item->num_choices) {
        drive_flush_item->value = value;
      }
    } else if (strcmp(name, "scaling_interp") == 0) {
      scaling_interp_item->value = value;
    } else if (strcmp(name, "gpio_config") == 0) {
      // We save/restore the choice int and map back to
      // the value as index into the choices for this
      // param.
      switch(value) {
        case GPIO_CONFIG_NAV_JOY:
           gpio_config_item->value = 1;
           break;
        case GPIO_CONFIG_KYB_JOY:
           gpio_config_item->value = 2;
           break;
        case GPIO_CONFIG_WAVESHARE:
           gpio_config_item->value = 3;
           break;
        case GPIO_CONFIG_USERPORT:
           gpio_config_item->value = 4;
           break;
        case GPIO_CONFIG_CUSTOM:
           gpio_config_item->value = 5;
           break;
        default:
           // Disabled
           gpio_config_item->value = 0;
           break;
      }

      // Force disabled if kernel options says so.
      if (!circle_gpio_enabled()) {
         gpio_config_item->value = 0;
      }

      // Make sure pins are configured properly after load
      circle_reset_gpio(emu_get_gpio_config());
    } else if (network_device_item != NULL &&
               strcmp(name, "network_device") == 0) {
      if (value >= 0 && value < network_device_item->num_choices) {
        network_device_item->value = value;
        saved_network_device = value;
      }
    } else if (network_modem_address_item != NULL &&
               strcmp(name, "network_modem_address") == 0) {
      if (value >= 0 && value < network_modem_address_item->num_choices &&
          circle_set_acia_network_address(acia_network_addresses[value])) {
        network_modem_address_item->value = value;
      }
    } else if (network_modem_item != NULL &&
               strcmp(name, "network_modem") == 0) {
      network_modem_item->value = value == 1 ? 1 : 0;
      circle_set_acia_network_enabled(network_modem_item->value);
    } else if (timezone_offset_item != NULL &&
               strcmp(name, "timezone_offset_minutes") == 0) {
      timezone_offset_item->value = timezone_offset_index(value);
    } else if (timezone_dst_item != NULL &&
               strcmp(name, "timezone_dst") == 0) {
      if (value >= 0 && value <= 2) {
        timezone_dst_item->value = value;
      }
    } else if (smb_password_item != NULL &&
               strcmp(name, "smb_password") == 0) {
      int n = 0;
      while (value_str[n] != '\0' && value_str[n] != '\n' &&
             value_str[n] != '\r' && n < smb_password_item->max_length) {
        smb_password_item->str_value[n] = value_str[n];
        n++;
      }
      smb_password_item->str_value[n] = '\0';
      smb_password_item->value = n;
    } else if (network_address_mode_item != NULL &&
               strcmp(name, "network_address_mode") == 0) {
      network_address_mode_item->value = value == 1 ? 1 : 0;
    } else if (network_address_mode_item != NULL &&
               strncmp(name, "network_", 8) == 0 &&
               (strcmp(name, "network_ip") == 0 ||
                strcmp(name, "network_netmask") == 0 ||
                strcmp(name, "network_gateway") == 0 ||
                strcmp(name, "network_dns") == 0)) {
      for (int i = 0; i < 4; i++) {
        struct menu_item *campo = network_static_item[i];
        if (campo != NULL && strcmp(name, network_static_keys[i]) == 0) {
          int n = 0;
          while (value_str[n] != '\0' && value_str[n] != '\n' &&
                 value_str[n] != '\r' && value_str[n] != ' ' &&
                 n < campo->max_length) {
            campo->str_value[n] = value_str[n];
            n++;
          }
          campo->str_value[n] = '\0';
          campo->value = n;
        }
      }
    } else if (strcmp(name, "drive_type_8") == 0) {
      emux_set_int_1(Setting_DriveNType, value, 8);
    } else if (strcmp(name, "drive_type_9") == 0) {
      emux_set_int_1(Setting_DriveNType, value, 9);
    } else if (strcmp(name, "drive_type_10") == 0) {
      emux_set_int_1(Setting_DriveNType, value, 10);
    } else if (strcmp(name, "drive_type_11") == 0) {
      emux_set_int_1(Setting_DriveNType, value, 11);
    } else if (strcmp(name, "real_drive_fast") == 0) {


      if (realdrive_veloce_item != NULL) {
        realdrive_veloce_item->value = value ? 1 : 0;
        emux_set_real_drive_veloce(realdrive_veloce_item->value);
      }
    } else if (strcmp(name, "real_drive_pause") == 0) {
      if (realdrive_respiro_item != NULL) {
        int b;
        for (b = 0; b < REALDRIVE_RESPIRI_NUM; b++) {
          if (realdrive_respiri[b] == value) {
            realdrive_respiro_item->value = b;
            emux_set_real_drive_respiro(value);
            break;
          }
        }
      }
    } else if (strcmp(name, "real_drive_log") == 0) {
      if (realdrive_log_item != NULL) {
        realdrive_log_item->value = value ? 1 : 0;
      }
    } else if (strncmp(name, "real_drive_", 11) == 0) {
      int u = atoi(name + 11);
      if (u >= 8 && u <= 11) {
        real_drive_wanted[u - 8] = value;
      }
    } else if (strcmp(name, "keyset_1_up") == 0) {
      keyset_codes[0][KEYSET_UP] = value;
    } else if (strcmp(name, "keyset_1_down") == 0) {
      keyset_codes[0][KEYSET_DOWN] = value;
    } else if (strcmp(name, "keyset_1_left") == 0) {
      keyset_codes[0][KEYSET_LEFT] = value;
    } else if (strcmp(name, "keyset_1_right") == 0) {
      keyset_codes[0][KEYSET_RIGHT] = value;
    } else if (strcmp(name, "keyset_1_fire") == 0) {
      keyset_codes[0][KEYSET_FIRE] = value;
    } else if (strcmp(name, "keyset_1_potx") == 0) {
      keyset_codes[0][KEYSET_POTX] = value;
    } else if (strcmp(name, "keyset_1_poty") == 0) {
      keyset_codes[0][KEYSET_POTY] = value;
    } else if (strcmp(name, "keyset_2_up") == 0) {
      keyset_codes[1][KEYSET_UP] = value;
    } else if (strcmp(name, "keyset_2_down") == 0) {
      keyset_codes[1][KEYSET_DOWN] = value;
    } else if (strcmp(name, "keyset_2_left") == 0) {
      keyset_codes[1][KEYSET_LEFT] = value;
    } else if (strcmp(name, "keyset_2_right") == 0) {
      keyset_codes[1][KEYSET_RIGHT] = value;
    } else if (strcmp(name, "keyset_2_fire") == 0) {
      keyset_codes[1][KEYSET_FIRE] = value;
    } else if (strcmp(name, "keyset_2_potx") == 0) {
      keyset_codes[1][KEYSET_POTX] = value;
    } else if (strcmp(name, "keyset_2_poty") == 0) {
      keyset_codes[1][KEYSET_POTY] = value;
    } else if (strcmp(name, "key_binding_1") == 0) {
      key_bindings[0] = value;
    } else if (strcmp(name, "key_binding_2") == 0) {
      key_bindings[1] = value;
    } else if (strcmp(name, "key_binding_3") == 0) {
      key_bindings[2] = value;
    } else if (strcmp(name, "key_binding_4") == 0) {
      key_bindings[3] = value;
    } else if (strcmp(name, "key_binding_5") == 0) {
      key_bindings[4] = value;
    } else if (strcmp(name, "key_binding_6") == 0) {
      key_bindings[5] = value;
    } else if (strcmp(name, "h_center_0") == 0) {
      h_center_item[0]->value = value;
    } else if (strcmp(name, "v_center_0") == 0) {
      v_center_item[0]->value = value;
    } else if (strcmp(name, "h_border_trim_0") == 0) {
      // LEGACY NAME : menu value = max_border_w * value / 100.
      h_border_item[0]->value =
         h_border_item[0]->max * (1.0d - (value / 100.0d));
      // If this exists, we're going to default use_scaling_params to
      // 0 so we don't clobber user settings. This will never happen
      // again after the user saves at least once.
      use_scaling_params_item[0]->value = 0;
    } else if (strcmp(name, "v_border_trim_0") == 0) {
      // LEGACY NAME : menu value = max_border_h * value / 100.
      v_border_item[0]->value =
         v_border_item[0]->max * (1.0d - (value / 100.0d));
      // If this exists, we're going to default use_scaling_params to
      // 0 so we don't clobber user settings. This will never happen
      // again after the user saves at least once.
      use_scaling_params_item[0]->value = 0;
    } else if (strcmp(name, "aspect_0") == 0) {
      // LEGACY NAME : aspect * 10 = h_stretch
      h_stretch_item[0]->value = value * 10;
    } else if (strcmp(name, "h_border_0") == 0) {
      h_border_item[0]->value = value;
    } else if (strcmp(name, "v_border_0") == 0) {
      v_border_item[0]->value = value;
    } else if (strcmp(name, "h_stretch_0") == 0) {
      h_stretch_item[0]->value = value;
    } else if (strcmp(name, "v_stretch_0") == 0) {
      v_stretch_item[0]->value = value;
    } else if (geo_comp_legge(name, value)) {

    } else if (strcmp(name, "h_center_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      h_center_item[1]->value = value;
    } else if (strcmp(name, "v_center_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      v_center_item[1]->value = value;
    } else if (strcmp(name, "h_border_trim_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      // LEGACY NAME : menu value = max_border_w * value / 100.
      h_border_item[1]->value = h_border_item[1]->max * (1.0d - (value / 100.0d));
      // If this exists, we're going to default use_scaling_params to
      // 0 so we don't clobber user settings. This will never happen
      // again after the user saves at least once.
      use_scaling_params_item[1]->value = 0;
    } else if (strcmp(name, "v_border_trim_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      // LEGACY NAME : menu value = max_border_h * value / 100.
      v_border_item[1]->value = v_border_item[1]->max * (1.0d - (value / 100.0d));
      // If this exists, we're going to default use_scaling_params to
      // 0 so we don't clobber user settings. This will never happen
      // again after the user saves at least once.
      use_scaling_params_item[1]->value = 0;
    } else if (strcmp(name, "aspect_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      // LEGACY NAME : aspect * 10 = h_stretch
      h_stretch_item[1]->value = value * 10;
    } else if (strcmp(name, "h_border_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      h_border_item[1]->value = value;
    } else if (strcmp(name, "v_border_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      v_border_item[1]->value = value;
    } else if (strcmp(name, "h_stretch_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      h_stretch_item[1]->value = value;
    } else if (strcmp(name, "v_stretch_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      v_stretch_item[1]->value = value;
    } else if (strcmp(name, "second_hdmi") == 0 &&
               emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      secondo_hdmi_item->value = value ? 1 : 0;
      due_hdmi_nel_giro();
    } else if (strcmp(name, "active_display") == 0 &&
               emux_machine_class == BMC64_MACHINE_CLASS_C128) {


      if (value >= 0 && value <= MENU_ACTIVE_DISPLAY_DUE_HDMI) {
         active_display_item->value = value;
      }
    } else if (strcmp(name, "volume") == 0) {
      volume_item->value = value;
    } else if (strcmp(name, "dir_convention") == 0) {
      dir_convention_item->value = value;
    } else if (strcmp(name, "use_int_scaling_0") == 0) {
      use_scaling_params_item[0]->value = value;
    } else if (strcmp(name, "use_int_scaling_1") == 0 && emux_machine_class == BMC64_MACHINE_CLASS_C128) {
      use_scaling_params_item[1]->value = value;
    } else if (strcmp(name, "s_curvature") == 0) {
      s_curvature_item->value = value;
    } else if (strcmp(name, "s_curvature_x") == 0) {
      s_curvature_x_item->value = value;
    } else if (strcmp(name, "s_curvature_y") == 0) {
      s_curvature_y_item->value = value;
    } else if (strcmp(name, "s_skew_x") == 0) {
      s_skew_x_item->value = value;
    } else if (strcmp(name, "s_skew_y") == 0) {
      s_skew_y_item->value = value;
    } else if (strcmp(name, "s_trapezoid") == 0) {
      s_trapezoid_item->value = value;
    } else if (strcmp(name, "s_rotation") == 0) {
      s_rotation_item->value = value;
    } else if (strcmp(name, "s_overscan") == 0) {
      s_overscan_item->value = value;
    } else if (strcmp(name, "s_convergence") == 0) {
      s_convergence_item->value = value;
    } else if (strcmp(name, "s_red_offset_x") == 0) {
      s_red_offset_x_item->value = value;
    } else if (strcmp(name, "s_red_offset_y") == 0) {
      s_red_offset_y_item->value = value;
    } else if (strcmp(name, "s_blue_offset_x") == 0) {
      s_blue_offset_x_item->value = value;
    } else if (strcmp(name, "s_blue_offset_y") == 0) {
      s_blue_offset_y_item->value = value;
    } else if (strcmp(name, "s_convergence_radial_strength") == 0) {
      s_convergence_radial_strength_item->value = value;
    } else if (strcmp(name, "s_horizontal_filtering") == 0) {
      s_horizontal_filtering_item->value = value;
    } else if (strcmp(name, "s_sigma_x") == 0) {
      s_sigma_x_item->value = value;
    } else if (strcmp(name, "s_edge_blur") == 0) {
      s_edge_blur_item->value = value;
    } else if (strcmp(name, "s_edge_blur_strength") == 0) {
      s_edge_blur_strength_item->value = value;
    } else if (strcmp(name, "s_edge_blur_radius") == 0) {
      s_edge_blur_radius_item->value = value;
    } else if (strcmp(name, "s_sharper") == 0) {
      s_horizontal_filtering_item->value = 1;
      s_sigma_x_item->value = value ? 20 : 50;
    } else if (strcmp(name, "s_mask") == 0) {
      s_mask_enable_item->value = value != 0;
      if (value > 0) {
        s_mask_item->value = value - 1;
      }
    } else if (strcmp(name, "s_mask_enable") == 0) {
      s_mask_enable_item->value = value;
    } else if (strcmp(name, "s_mask_type") == 0) {
      s_mask_item->value = value;
    } else if (strcmp(name, "s_mask_brightness") == 0) {
      s_mask_brightness_item->value = value;
    } else if (strcmp(name, "s_scanlines") == 0) {
      s_scanlines_item->value = value;
    } else if (strcmp(name, "s_multisample") == 0) {
      s_multisample_item->value = value;
    } else if (strcmp(name, "s_scanline_weight") == 0) {
      s_scanline_weight_item->value = value;
    } else if (strcmp(name, "s_scanline_gap_brightness") == 0) {
      s_scanline_gap_brightness_item->value = value;
    } else if (strcmp(name, "s_bloom") == 0) {
      s_bloom_item->value = value;
    } else if (strcmp(name, "s_bloom_factor") == 0) {
      s_bloom_factor_item->value = value;
    } else if (strcmp(name, "s_vignette") == 0) {
      s_vignette_item->value = value;
    } else if (strcmp(name, "s_vignette_strength") == 0) {
      s_vignette_strength_item->value = value;
    } else if (strcmp(name, "s_vignette_scale") == 0) {
      s_vignette_scale_item->value = value;
    } else if (strcmp(name, "s_vignette_softness") == 0) {
      s_vignette_softness_item->value = value;
    } else if (strcmp(name, "s_uneven_illumination") == 0) {
      s_uneven_illumination_item->value = value;
    } else if (strcmp(name, "s_uneven_illumination_strength") == 0) {
      s_uneven_illumination_strength_item->value = value;
    } else if (strcmp(name, "s_uneven_illumination_scale") == 0) {
      s_uneven_illumination_scale_item->value = value;
    } else if (strcmp(name, "s_horizontal_jitter") == 0) {
      s_horizontal_jitter_item->value = value;
    } else if (strcmp(name, "s_horizontal_jitter_strength") == 0) {
      s_horizontal_jitter_strength_item->value = value;
    } else if (strcmp(name, "s_horizontal_jitter_frequency") == 0) {
      s_horizontal_jitter_frequency_item->value = value;
    } else if (strcmp(name, "s_horizontal_jitter_speed") == 0) {
      s_horizontal_jitter_speed_item->value = value;
    } else if (strcmp(name, "s_composite_artifacts") == 0) {
      s_composite_artifacts_item->value = value;
    } else if (strcmp(name, "s_composite_chroma_blur") == 0) {
      s_composite_chroma_blur_item->value = value;
    } else if (strcmp(name, "s_composite_luma_sharpen") == 0) {
      s_composite_luma_sharpen_item->value = value;
    } else if (strcmp(name, "s_composite_color_bleed") == 0) {
      s_composite_color_bleed_item->value = value;
    } else if (strcmp(name, "s_glass_reflection") == 0) {
      s_glass_reflection_item->value = value;
    } else if (strcmp(name, "s_glass_reflection_angle") == 0) {
      s_glass_reflection_angle_item->value = value;
    } else if (strcmp(name, "s_glass_reflection_width") == 0) {
      s_glass_reflection_width_item->value = value;
    } else if (strcmp(name, "s_glass_reflection_position") == 0) {
      s_glass_reflection_position_item->value = value;
    } else if (strcmp(name, "s_rounded_screen_mask") == 0) {
      s_rounded_screen_mask_item->value = value;
    } else if (strcmp(name, "s_rounded_corner_radius") == 0) {
      s_rounded_corner_radius_item->value = value;
    } else if (strcmp(name, "s_rounded_border_softness") == 0) {
      s_rounded_border_softness_item->value = value;
    } else if (strcmp(name, "s_edge_glow") == 0) {
      s_edge_glow_item->value = value;
    } else if (strcmp(name, "s_edge_glow_strength") == 0) {
      s_edge_glow_strength_item->value = value;
    } else if (strcmp(name, "s_edge_glow_width") == 0) {
      s_edge_glow_width_item->value = value;
    } else if (strcmp(name, "s_noise") == 0) {
      s_noise_item->value = value;
    } else if (strcmp(name, "s_luminance_noise") == 0) {
      s_luminance_noise_item->value = value;
    } else if (strcmp(name, "s_chroma_noise") == 0) {
      s_chroma_noise_item->value = value;
    } else if (strcmp(name, "s_noise_speed") == 0) {
      s_noise_speed_item->value = value;
    } else if (strcmp(name, "s_gamma") == 0) {
      s_output_response_item->value = value != 0;
      if (value > 0) {
        s_response_mode_item->value = value == 2 ? 1 : 0;
      }
    } else if (strcmp(name, "s_output_response") == 0) {
      s_output_response_item->value = value;
    } else if (strcmp(name, "s_response_mode") == 0) {
      s_response_mode_item->value = value;
    } else if (strcmp(name, "s_level_mapping") == 0) {
      s_level_mapping_item->value = value;
    } else if (strcmp(name, "s_input_gamma") == 0) {
      s_input_gamma_item->value = value;
    } else if (strcmp(name, "s_output_gamma") == 0) {
      s_output_gamma_item->value = value;
    } else if (strcmp(name, "s_response_saturation") == 0) {
      s_response_saturation_item->value = value;
    } else if (strcmp(name, "s_black_level") == 0) {
      s_black_level_item->value = value;
    } else if (strcmp(name, "s_white_clip") == 0) {
      s_white_clip_item->value = value;
    } else if (strcmp(name, "custom_gpio") == 0) {
      char* token = strtok (value_str, ",");
      if (token != NULL) {
         int pin_index = atoi(token);
         if (pin_index >=0 && pin_index < NUM_GPIO_PINS) {
            token = strtok (NULL, ",");
            unsigned int binding_value = token ? atoi(token) : 0;
            gpio_bindings[pin_index] = binding_value;
         }
      }
    } else {
      for (int k=0; k < MAX_USB_DEVICES; k++) {
       if (strcmp(name, usb_btn_name[k]) == 0) {
         if (value >= NUM_BUTTON_ASSIGNMENTS) {
            value = NUM_BUTTON_ASSIGNMENTS - 1;
         }
         usb_button_assignments[k][usb_btn_i[k]] = value;
         usb_btn_i[k]++;
         if (usb_btn_i[k] >= MAX_USB_BUTTONS) {
           usb_btn_i[k] = 0;
         }
       } else if (strcmp(name, usb_pref_name[k]) == 0) {
         usb_pref[k] = value;
       } else if (strcmp(name, usb_x_name[k]) == 0) {
         usb_x_axis[k] = value;
       } else if (strcmp(name, usb_y_name[k]) == 0) {
         usb_y_axis[k] = value;
       } else if (strcmp(name, usb_x_t_name[k]) == 0) {
         usb_x_thresh[k] = ((float)value) / 100.0f;
       } else if (strcmp(name, usb_y_t_name[k]) == 0) {
         usb_y_thresh[k] = ((float)value) / 100.0f;
       }
      }
    }
  }
  fclose(fp);







  {
    struct menu_item* allarga[] = {
      h_stretch_item[0], v_stretch_item[0],
      emux_machine_class == BMC64_MACHINE_CLASS_C128 ? h_stretch_item[1] : NULL,
      emux_machine_class == BMC64_MACHINE_CLASS_C128 ? v_stretch_item[1] : NULL,
    };
    int si;
    for (si = 0; si < 4; si++) {
      struct menu_item* it = allarga[si];
      if (it == NULL) continue;
      int lo = it->min > 0 ? it->min : 1;
      if (it->value < lo) it->value = lo;
      if (it->max > lo && it->value > it->max) it->value = it->max;
    }
  }





  {
    struct menu_item* bordi[] = {
      h_border_item[0], v_border_item[0],
      emux_machine_class == BMC64_MACHINE_CLASS_C128 ? h_border_item[1] : NULL,
      emux_machine_class == BMC64_MACHINE_CLASS_C128 ? v_border_item[1] : NULL,
    };
    int bi;
    for (bi = 0; bi < 4; bi++) {
      struct menu_item* it = bordi[bi];
      if (it == NULL) continue;
      if (it->value < it->min) it->value = it->min;
      if (it->value > it->max) it->value = it->max;
    }
  }

  update_wifi_menu_enabled();
  emux_load_settings_done();

  emux_video_color_setting_changed(0);
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
    emux_video_color_setting_changed(1);
  }
  return 1;
}

// Swap ports 1 & 2
void menu_swap_joysticks() {
  if (port_1_menu_item &&
      is_mouse_device(port_1_menu_item->choice_ints[port_1_menu_item->value])) {
     emux_set_joy_port_device(1, JOYDEV_NONE);
  }

  if (port_2_menu_item &&
      is_mouse_device(port_2_menu_item->choice_ints[port_2_menu_item->value])) {
     emux_set_joy_port_device(2, JOYDEV_NONE);
  }

  int tmp = joydevs[0].device;
  joydevs[0].device = joydevs[1].device;
  joydevs[1].device = tmp;
  joyswap = 1 - joyswap;
  overlay_joyswap_changed(joyswap);
  ui_set_joy_items();
}

static struct menu_item *menu_trova_reu(void);





int menu_attacca_cartuccia(int menu_id, char *percorso) {
  struct menu_item *reu = menu_trova_reu();






  if (emux_sid_quanti_brani() > 0) {
    emux_sid_lascia_la_porta();
    emux_sid_ferma();
    sid_aggiorna_righe();
    overlay_avviso("SID PLAYER OFF");
  }
  if (reu != NULL && reu->value) {
    reu->value = 0;
    if (reu->on_value_changed) {
      reu->on_value_changed(reu);
    }
    overlay_avviso("REU OFF");
  }
  return emux_attach_cart(menu_id, percorso);
}

static void attach_cart(int menu_id, struct menu_item *item) {
  menu_attacca_cartuccia(menu_id, fullpath(DIR_CARTS, item->str_value));
}

// Reset current_dir_names according to preference.
static void set_current_dir_names() {
  int i;

  switch (dir_convention_item->value) {
     case MENU_DIR_CONVENTION_FOLDER_EMU:
        for (i = 0; i < NUM_DIR_TYPES; i++) {
          strcpy(current_dir_names[i], default_dir_names[i]);
          strcat(current_dir_names[i], files_sub_dir);
        }
        strcpy(current_dir_names[DIR_ROOT], "/");


        strcpy(current_dir_names[DIR_SNAPS], default_dir_names[DIR_SNAPS]);
        strcat(current_dir_names[DIR_SNAPS], machine_sub_dir);
        break;
     case MENU_DIR_CONVENTION_EMU_FOLDER:
        for (i = 0; i < NUM_DIR_TYPES; i++) {
          strcpy(current_dir_names[i], files_sub_dir);
          strcat(current_dir_names[i], default_dir_names[i]);
        }
        strcpy(current_dir_names[DIR_ROOT], files_sub_dir);
        strcpy(current_dir_names[DIR_SNAPS], machine_sub_dir);
        strcat(current_dir_names[DIR_SNAPS], default_dir_names[DIR_SNAPS]);
        break;
     default:
        assert(0);
        break;
  }

  // These don't change
  strcpy(current_dir_names[DIR_ROMS], machine_sub_dir);
  strcpy(current_dir_names[DIR_IEC], "/");


  strcpy(current_dir_names[DIR_REU], "/REU");
}


//




void menu_autostart_nome(char *nome) {
  char *dove;





  modello_secondo_immagine(nome, 8);

  dove = fullpath(DIR_PRGS, nome);
  ui_info("Starting...");
  if (emux_autostart_file(dove) < 0) {
    ui_pop_menu();



    ui_error("Failed to autostart:\n%s", dove);
  } else {
    ui_pop_all_and_toggle();
  }
}

static void select_file(struct menu_item *item) {
  switch (item->id) {

     case MENU_REU_ATTACH_IMAGE_FILE:
       if (emux_handle_reu_image_change(
               fullpath(DIR_REU, item->str_value)) == 0) {
         ui_pop_all_and_toggle();

         overlay_avviso("REU IMAGE LOADED");
       } else {
         ui_error("Failed to attach REU image");
       }
       return;
     case MENU_REU_SAVE_IMAGE_AS_FILE: {
       char *path;
       int esito;
       if (item->type == TEXTFIELD && strlen(item->str_value) == 0) {
         ui_error("Empty filename");
         return;
       }
       path = fullpath(DIR_REU, item->str_value);
       esito = emux_save_reu_image(path);
       if (esito == 0) {
         ui_pop_all_and_toggle();
       } else if (esito == -2) {
         ui_error("RAM Expansion is not enabled");
       } else if (esito == -3) {
         ui_error("RAM Expansion memory is unavailable");
       } else {
         printf("REU image save failed (%d): %s\n", esito, path);
         ui_error("Unable to create REU image\n%s", path);
       }
       return;
     }
     case MENU_IEC_DIR:
       emux_set_iec_dir(unit, fullpath(DIR_IEC, ""));


       last_iec_dir[unit-8][0] = '\0';
       accoda(last_iec_dir[unit-8], sizeof(last_iec_dir[unit-8]),
              fullpath(DIR_IEC, ""));
       ui_pop_menu();
       return;
     case MENU_LOAD_SNAP_FILE:
       ui_info("Loading...");
       if (emux_load_state(fullpath(DIR_SNAPS, item->str_value)) < 0) {
         ui_pop_menu();
         ui_error("Load snapshot failed");
       } else {
         ui_pop_all_and_toggle();
       }
       return;
     case MENU_DISK_FILE: {



       modello_secondo_immagine(item->str_value, unit);
       // Perform the attach
       ui_info("Attaching...");
       if (emux_attach_disk_image(unit, fullpath(DIR_DISKS, item->str_value)) <
           0) {
         ui_pop_menu();
         ui_error("Failed to attach disk image");
	 attached_disk_name[unit-8][0] = '\0';
       } else {
         ui_pop_all_and_toggle();
	 strcpy (attached_disk_name[unit-8], item->str_value);
       }
       return;
     }
     case MENU_DRIVE_ROM_FILE_1541:
     case MENU_DRIVE_ROM_FILE_1541II:
     case MENU_DRIVE_ROM_FILE_1551:
     case MENU_DRIVE_ROM_FILE_1571:
     case MENU_DRIVE_ROM_FILE_1581:
     case MENU_DRIVE_ROM_FILE_CMDHD:
     case MENU_DRIVE_ROM_FILE_2031:
     case MENU_DRIVE_ROM_FILE_2040:
     case MENU_DRIVE_ROM_FILE_3040:
     case MENU_DRIVE_ROM_FILE_4040:
     case MENU_DRIVE_ROM_FILE_1001:
       {

         int esito = emux_handle_rom_change(item, fullpath);
         // Two pops necessary here.
         ui_pop_menu();
         ui_pop_menu();
         if (esito < 0) {
           ui_error("ROM not loaded - kept the old one");
         }
       }
       return;
     case MENU_TAPE_FILE:
       ui_info("Attaching...");
       if (emux_attach_tape_image(fullpath(DIR_TAPES, item->str_value)) < 0) {
         ui_pop_menu();
         ui_error("Failed to attach tape image");
       } else {
         ui_pop_all_and_toggle();
       }
       return;
     // NOTE: ROMs can't be fullpath or VICE complains.
     case MENU_KERNAL_FILE:
     case MENU_BASIC_FILE:
     case MENU_CHARGEN_FILE:
     case MENU_C128_LOAD_KERNAL_FILE:
     case MENU_C128_LOAD_BASIC_HI_FILE:
     case MENU_C128_LOAD_BASIC_LO_FILE:
     case MENU_C128_LOAD_CHARGEN_FILE:
     case MENU_C128_LOAD_64_KERNAL_FILE:
     case MENU_C128_LOAD_64_BASIC_FILE:
       {
         int esito = emux_handle_rom_change(item, fullpath);
         ui_pop_all_and_toggle();
         if (esito < 0) {
           ui_error("ROM not loaded - kept the old one");
         }
       }
       return;
     case MENU_SID_FILE:



       ui_info("Loading SID...");
       if (emux_sid_carica(fullpath(DIR_SIDS, item->str_value)) < 0) {
         ui_pop_menu();
         ui_error("Not a valid SID file");
       } else {
         sid_aggiorna_righe();
         overlay_avviso(emux_sid_chi_suona(0));
         ui_pop_all_and_toggle();
       }
       return;
     case MENU_AUTOSTART_FILE:
       menu_autostart_nome(item->str_value);
       return;
     case MENU_LOADPRG_FILE:
       {
         char *dove;



         modello_secondo_immagine(item->str_value, 8);
         dove = fullpath(DIR_PRGS, item->str_value);
         ui_info("Loading...");
         if (emux_load_prg_file(dove) < 0) {
           ui_pop_menu();
           ui_error("Failed to load:\n%s", dove);
         } else {
           ui_pop_all_and_toggle();
         }
       }
       return;
     case MENU_C64_CART_FILE:
     case MENU_C64_CART_8K_FILE:
     case MENU_C64_CART_16K_FILE:
     case MENU_C64_CART_ULTIMAX_FILE:
     case MENU_VIC20_CART_DETECT_FILE:
     case MENU_VIC20_CART_GENERIC_FILE:
     case MENU_VIC20_CART_16K_2000_FILE:
     case MENU_VIC20_CART_16K_4000_FILE:
     case MENU_VIC20_CART_16K_6000_FILE:
     case MENU_VIC20_CART_8K_A000_FILE:
     case MENU_VIC20_CART_4K_B000_FILE:
     case MENU_VIC20_CART_BEHRBONZ_FILE:
     case MENU_VIC20_CART_UM_FILE:
     case MENU_VIC20_CART_FP_FILE:
     case MENU_VIC20_CART_MEGACART_FILE:
     case MENU_VIC20_CART_FINAL_EXPANSION_FILE:
     case MENU_PLUS4_CART_FILE:
     case MENU_PLUS4_CART_C0_LO_FILE:
     case MENU_PLUS4_CART_C0_HI_FILE:
     case MENU_PLUS4_CART_C1_LO_FILE:
     case MENU_PLUS4_CART_C1_HI_FILE:
     case MENU_PLUS4_CART_C2_LO_FILE:
     case MENU_PLUS4_CART_C2_HI_FILE:
       attach_cart(item->id, item);
       return;
    case MENU_IDE64_IMAGE_1_FILE:
    case MENU_IDE64_IMAGE_2_FILE:
    case MENU_IDE64_IMAGE_3_FILE:
    case MENU_IDE64_IMAGE_4_FILE:


      if (emux_machine_class == BMC64_MACHINE_CLASS_C64 ||
          emux_machine_class == BMC64_MACHINE_CLASS_SCPU64) {
        if (emux_handle_ide64_image_change(
                item->id - MENU_IDE64_IMAGE_1_FILE + 1,
                fullpath(DIR_DISKS, item->str_value)) == 0) {
          ui_pop_all_and_toggle();
        } else {
          ui_error("Failed to set IDE64 image");
        }
      }
      return;
     default:
       break;
  }

  // Handle saving snapshots.
  if (item->id == MENU_SAVE_SNAP_FILE) {
    char *fname = item->str_value;
    if (item->type == TEXTFIELD) {
      // Scrub the filename before passing it along
      fname = item->str_value;
      if (strlen(fname) == 0) {
        ui_error("Empty filename");
        return;
      } else if (strlen(fname) > MAX_FN_NAME) {
        ui_error("Too long");
        return;
      }
      char *dot = strchr(fname, '.');
      if (dot == NULL) {
        if (strlen(fname) + 4 <= MAX_FN_NAME) {
          strcat(fname, snap_filt_ext[0]);
        } else {
          ui_error("Too long");
          return;
        }
      } else {
        char l1 = tolower(dot[1]);
        char l2 = tolower(dot[2]);
        char l3 = tolower(dot[3]);
        if (l1 != snap_filt_ext[0][1] ||
            l2 != snap_filt_ext[0][2] ||
            l3 != snap_filt_ext[0][3] || dot[4] != '\0') {
          if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU) {
             ui_error("Need .P4S extension");
          } else {
             ui_error("Need .VSF extension");
          }
          return;
        }
      }
    }
    ui_info("Saving...");
    if (emux_save_state(fullpath(DIR_SNAPS, fname)) < 0) {
      ui_pop_menu();
      ui_error("Save snapshot failed");
    } else {





      ricerca_scelto_dir = DIR_SNAPS;
      strncpy(ricerca_scelto, fname, sizeof(ricerca_scelto) - 1);
      ricerca_scelto[sizeof(ricerca_scelto) - 1] = '\0';
      ui_pop_all_and_toggle();
    }
  }

  // Handle creating empty disk
  else if (item->id >= MENU_CREATE_D64_FILE &&
           item->id <= MENU_CREATE_DHD_FILE) {
    emux_create_disk(item, fullpath);
  }

  // Handle creating empty tape
  else if (item->id == MENU_CREATE_TAP_FILE) {
    emux_create_tape(item, fullpath);
  }
}

// Utility to determine current dir index from a menu file item
static int menu_file_item_to_dir_index(struct menu_item *item) {
  int index;
  switch (item->id) {
  case MENU_LOAD_SNAP_FILE:
  case MENU_SAVE_SNAP_FILE:
    return DIR_SNAPS;
  case MENU_DISK_FILE:
  case MENU_IDE64_IMAGE_1_FILE:
  case MENU_IDE64_IMAGE_2_FILE:
  case MENU_IDE64_IMAGE_3_FILE:
  case MENU_IDE64_IMAGE_4_FILE:
  case MENU_CREATE_D64_FILE:
  case MENU_CREATE_D67_FILE:
  case MENU_CREATE_D71_FILE:
  case MENU_CREATE_D80_FILE:
  case MENU_CREATE_D81_FILE:
  case MENU_CREATE_D82_FILE:
  case MENU_CREATE_D1M_FILE:
  case MENU_CREATE_D2M_FILE:
  case MENU_CREATE_D4M_FILE:
  case MENU_CREATE_G64_FILE:
  case MENU_CREATE_G71_FILE:
  case MENU_CREATE_P64_FILE:
  case MENU_CREATE_X64_FILE:
  case MENU_CREATE_DHD_FILE:
    return DIR_DISKS;
  case MENU_TAPE_FILE:
  case MENU_CREATE_TAP_FILE:
    return DIR_TAPES;
  case MENU_C64_CART_FILE:
  case MENU_C64_CART_8K_FILE:
  case MENU_C64_CART_16K_FILE:
  case MENU_C64_CART_ULTIMAX_FILE:
  case MENU_VIC20_CART_DETECT_FILE:
  case MENU_VIC20_CART_GENERIC_FILE:
  case MENU_VIC20_CART_16K_2000_FILE:
  case MENU_VIC20_CART_16K_4000_FILE:
  case MENU_VIC20_CART_16K_6000_FILE:
  case MENU_VIC20_CART_8K_A000_FILE:
  case MENU_VIC20_CART_4K_B000_FILE:
  case MENU_VIC20_CART_BEHRBONZ_FILE:
  case MENU_VIC20_CART_UM_FILE:
  case MENU_VIC20_CART_FP_FILE:
  case MENU_VIC20_CART_MEGACART_FILE:
  case MENU_VIC20_CART_FINAL_EXPANSION_FILE:
  case MENU_PLUS4_CART_FILE:
  case MENU_PLUS4_CART_C0_LO_FILE:
  case MENU_PLUS4_CART_C0_HI_FILE:
  case MENU_PLUS4_CART_C1_LO_FILE:
  case MENU_PLUS4_CART_C1_HI_FILE:
  case MENU_PLUS4_CART_C2_LO_FILE:
  case MENU_PLUS4_CART_C2_HI_FILE:
    return DIR_CARTS;


  case MENU_REU_ATTACH_IMAGE_FILE:
  case MENU_REU_SAVE_IMAGE_AS_FILE:
    return DIR_REU;
  case MENU_KERNAL_FILE:
  case MENU_BASIC_FILE:
  case MENU_CHARGEN_FILE:
  case MENU_DRIVE_ROM_FILE_1541:
  case MENU_DRIVE_ROM_FILE_1541II:
  case MENU_DRIVE_ROM_FILE_1551:
  case MENU_DRIVE_ROM_FILE_1571:
  case MENU_DRIVE_ROM_FILE_1581:
  case MENU_DRIVE_ROM_FILE_CMDHD:
  case MENU_DRIVE_ROM_FILE_2031:
  case MENU_DRIVE_ROM_FILE_2040:
  case MENU_DRIVE_ROM_FILE_3040:
  case MENU_DRIVE_ROM_FILE_4040:
  case MENU_DRIVE_ROM_FILE_1001:
  case MENU_C128_LOAD_KERNAL_FILE:
  case MENU_C128_LOAD_BASIC_HI_FILE:
  case MENU_C128_LOAD_BASIC_LO_FILE:
  case MENU_C128_LOAD_CHARGEN_FILE:
  case MENU_C128_LOAD_64_KERNAL_FILE:
  case MENU_C128_LOAD_64_BASIC_FILE:
    return DIR_ROMS;



  case MENU_SID_FILE:
    return DIR_SIDS;
  case MENU_AUTOSTART_FILE:
  case MENU_LOADPRG_FILE:
    return DIR_PRGS;
  case MENU_IEC_DIR:
    return DIR_IEC;
  default:
    return -1;
  }
}

// Utility function to re-list same type of files given
// a file item.



static struct menu_item *relist_files_after_dir_change(struct menu_item *item) {
  struct menu_item *root = NULL;

  switch (item->id) {
  case MENU_LOAD_SNAP_FILE:
    root = show_files(DIR_SNAPS, FILTER_SNAP, item->id, 1);
    break;
  case MENU_SAVE_SNAP_FILE:
    root = show_files(DIR_SNAPS, FILTER_SNAP, item->id, 1);
    break;
  case MENU_DISK_FILE:
  case MENU_IDE64_IMAGE_1_FILE:
  case MENU_IDE64_IMAGE_2_FILE:
  case MENU_IDE64_IMAGE_3_FILE:
  case MENU_IDE64_IMAGE_4_FILE:
  case MENU_CREATE_D64_FILE:
  case MENU_CREATE_D67_FILE:
  case MENU_CREATE_D71_FILE:
  case MENU_CREATE_D80_FILE:
  case MENU_CREATE_D81_FILE:
  case MENU_CREATE_D82_FILE:
  case MENU_CREATE_D1M_FILE:
  case MENU_CREATE_D2M_FILE:
  case MENU_CREATE_D4M_FILE:
  case MENU_CREATE_G64_FILE:
  case MENU_CREATE_G71_FILE:
  case MENU_CREATE_P64_FILE:
  case MENU_CREATE_X64_FILE:
  case MENU_CREATE_DHD_FILE:
    root = show_files(DIR_DISKS,
           item->id >= MENU_IDE64_IMAGE_1_FILE &&
               item->id <= MENU_IDE64_IMAGE_4_FILE
             ? FILTER_NONE
             : FILTER_DISK,
           item->id, 1);
    break;
  case MENU_TAPE_FILE:
  case MENU_CREATE_TAP_FILE:
    root = show_files(DIR_TAPES, FILTER_TAPE, item->id, 1);
    break;
  case MENU_C64_CART_FILE:
    root = show_files(DIR_CARTS, FILTER_CART, item->id, 1);
    break;
  case MENU_REU_ATTACH_IMAGE_FILE:

    root = show_files(DIR_REU, FILTER_REU, item->id, 1);
    break;
  case MENU_REU_SAVE_IMAGE_AS_FILE:
    root = show_files(DIR_REU, FILTER_NONE, item->id, 1);
    break;
  case MENU_C64_CART_8K_FILE:
  case MENU_C64_CART_16K_FILE:
  case MENU_C64_CART_ULTIMAX_FILE:
  case MENU_VIC20_CART_DETECT_FILE:
  case MENU_VIC20_CART_GENERIC_FILE:
  case MENU_VIC20_CART_16K_2000_FILE:
  case MENU_VIC20_CART_16K_4000_FILE:
  case MENU_VIC20_CART_16K_6000_FILE:
  case MENU_VIC20_CART_8K_A000_FILE:
  case MENU_VIC20_CART_4K_B000_FILE:
  case MENU_VIC20_CART_BEHRBONZ_FILE:
  case MENU_VIC20_CART_UM_FILE:
  case MENU_VIC20_CART_FP_FILE:
  case MENU_VIC20_CART_MEGACART_FILE:
  case MENU_VIC20_CART_FINAL_EXPANSION_FILE:
  case MENU_PLUS4_CART_FILE:
  case MENU_PLUS4_CART_C0_LO_FILE:
  case MENU_PLUS4_CART_C0_HI_FILE:
  case MENU_PLUS4_CART_C1_LO_FILE:
  case MENU_PLUS4_CART_C1_HI_FILE:
  case MENU_PLUS4_CART_C2_LO_FILE:
  case MENU_PLUS4_CART_C2_HI_FILE:
    root = show_files(DIR_CARTS, FILTER_NONE, item->id, 1);
    break;
  case MENU_KERNAL_FILE:
  case MENU_BASIC_FILE:
  case MENU_CHARGEN_FILE:
  case MENU_C128_LOAD_KERNAL_FILE:
  case MENU_C128_LOAD_BASIC_HI_FILE:
  case MENU_C128_LOAD_BASIC_LO_FILE:
  case MENU_C128_LOAD_CHARGEN_FILE:
  case MENU_C128_LOAD_64_KERNAL_FILE:
  case MENU_C128_LOAD_64_BASIC_FILE:
  case MENU_DRIVE_ROM_FILE_1541:
  case MENU_DRIVE_ROM_FILE_1541II:
  case MENU_DRIVE_ROM_FILE_1551:
  case MENU_DRIVE_ROM_FILE_1571:
  case MENU_DRIVE_ROM_FILE_1581:
  case MENU_DRIVE_ROM_FILE_CMDHD:
  case MENU_DRIVE_ROM_FILE_2031:
  case MENU_DRIVE_ROM_FILE_2040:
  case MENU_DRIVE_ROM_FILE_3040:
  case MENU_DRIVE_ROM_FILE_4040:
  case MENU_DRIVE_ROM_FILE_1001:
    root = show_files(DIR_ROMS, FILTER_NONE, item->id, 1);
    break;
  case MENU_AUTOSTART_FILE:
    root = show_files(DIR_PRGS, FILTER_NONE, item->id, 1);
    break;
  case MENU_LOADPRG_FILE:
    root = show_files(DIR_PRGS, FILTER_PRGS, item->id, 1);
    break;
  case MENU_IEC_DIR:
    root = show_files(DIR_IEC, FILTER_DIRS, item->id, 1);
    break;
  case MENU_SID_FILE:
    root = show_files(DIR_SIDS, FILTER_SID, item->id, 1);
    break;
  default:
    break;
  }
  return root;
}












static void rilista_stessa_finestra(struct menu_item *item, int quanti_pop) {
  int era_osd = ui_osd_attivo();
  struct menu_item *nuovo;
  int i;

  for (i = 0; i < quanti_pop; i++) {
    ui_pop_menu();
  }
  nuovo = relist_files_after_dir_change(item);
  if (era_osd && nuovo != NULL) {
    nuovo->on_popped_off = glob_osd_popped;
    ui_enable_osd();
  }
}

static void up_dir(struct menu_item *item) {
  int i;
  int dir_index = menu_file_item_to_dir_index(item);
  if (dir_index < 0)
    return;
  // Remove last directory from current_dir_names
  i = strlen(current_dir_names[dir_index]) - 1;
  while (current_dir_names[dir_index][i] != '/' && i > 0)
    i--;
  current_dir_names[dir_index][i] = '\0';
  if (strlen(current_dir_names[dir_index]) == 0) {
    strcpy(current_dir_names[dir_index], "/");
  }
  rilista_stessa_finestra(item, 1);
}

static void enter_dir(struct menu_item *item) {
  int dir_index = menu_file_item_to_dir_index(item);
  if (dir_index < 0)
    return;


  if (!finisce_con_slash(current_dir_names[dir_index])) {
    accoda(current_dir_names[dir_index],
           sizeof(current_dir_names[dir_index]), "/");
  }
  accoda(current_dir_names[dir_index],
         sizeof(current_dir_names[dir_index]), item->str_value);
  rilista_stessa_finestra(item, 1);
}




static int ricerca_tasto(struct menu_item *cur, char ch) {
  struct menu_item *riga = NULL;
  size_t n;

  if (cur == NULL || lista_col_nome_nuovo(cur->id) ||
      menu_file_item_to_dir_index(cur) < 0) {
    return 0;
  }
  if (cur->sub_id != MENU_SUB_PICK_FILE && cur->sub_id != MENU_SUB_PICK_DIR &&
      cur->sub_id != MENU_SUB_UP_DIR && cur->sub_id != MENU_SUB_ENTER_DIR &&
      cur->sub_id != MENU_SUB_SELECT_VOLUME &&
      cur->sub_id != MENU_SUB_SEARCH) {
    return 0;
  }

  if (ch != '\b' && ((unsigned char)ch < 0x20 || (unsigned char)ch > 0x7e)) {
    return 0;
  }


  if (ricerca_riga && ricerca_root != NULL && ricerca_item_riga != NULL &&
      ricerca_indice(ricerca_root, cur) >= 0 &&
      ricerca_indice(ricerca_root, ricerca_item_riga) >= 0) {
    riga = ricerca_item_riga;
  }

  if (cur->sub_id == MENU_SUB_SEARCH) {

    if (ch != '\b' || riga == NULL || cur->str_value[0] != '\0') {
      return 0;
    }
    ricerca_spegni();
    ricerca_da_se = 1;
    rilista_stessa_finestra(cur, 1);
    return 1;
  }

  if (riga != NULL) {


    n = strlen(riga->str_value);
    if (ch == '\b') {
      if (n > 0) {
        riga->str_value[n - 1] = '\0';
      }
    } else if (n < RICERCA_MAX) {
      riga->str_value[n] = ch;
      riga->str_value[n + 1] = '\0';
    }
    riga->value = strlen(riga->str_value);
    ui_to_top();
    ui_set_cur_pos(ricerca_indice(ricerca_root, riga));
    return 1;
  }

  if (ch == '\b') {
    return 0;
  }

  ricerca_bozza[0] = ch;
  ricerca_bozza[1] = '\0';
  ricerca_riga = 1;
  ricerca_sul_primo = 0;
  ricerca_da_se = 1;
  rilista_stessa_finestra(cur, 1);
  return 1;
}



static void ricerca_applica(struct menu_item *riga) {
  char testo[RICERCA_MAX + 1];
  size_t n;

  strncpy(testo, riga->str_value, RICERCA_MAX);
  testo[RICERCA_MAX] = '\0';
  n = strlen(testo);
  while (n > 0 && testo[n - 1] == ' ') {
    testo[--n] = '\0';
  }
  if (n == 0) {
    ricerca_spegni();
  } else {
    strcpy(ricerca_testo, testo);
    strcpy(ricerca_bozza, testo);
    ricerca_riga = 1;
    ricerca_sul_primo = 1;
  }
  ricerca_da_se = 1;
  rilista_stessa_finestra(riga, 1);
}


static void ricerca_ricorda_scelto(struct menu_item *item) {
  if (ricerca_testo[0] == '\0' || item->type == TEXTFIELD ||
      item->sub_id != MENU_SUB_PICK_FILE) {
    return;
  }
  ricerca_scelto_dir = menu_file_item_to_dir_index(item);
  strncpy(ricerca_scelto, item->str_value, sizeof(ricerca_scelto) - 1);
  ricerca_scelto[sizeof(ricerca_scelto) - 1] = '\0';
}

static void toggle_warp(int value) {
  emux_set_warp(value);
  overlay_warp_changed(value);
  warp_item->value = value;
}

// Tell videoarch the new settings made from the menu.
static void do_video_settings(int layer) {





  double lpad = 0;
  double rpad = 0;
  double tpad = 0;
  double bpad = 0;
  int zlayer = 0;

  struct menu_item* hcenter_item;
  struct menu_item* vcenter_item;
  struct menu_item* hborder_item;
  struct menu_item* vborder_item;
  struct menu_item* h_str_item;
  struct menu_item* v_str_item;
  int h_int_stretch;
  int v_int_stretch;
  int use_h_int_stretch;
  int use_v_int_stretch;

  int canvas_index;
  if (layer == FB_LAYER_VIC) {
     canvas_index = VIC_INDEX;
  } else if (layer == FB_LAYER_VDC) {
     canvas_index = VDC_INDEX;
  } else {
     return;
  }

  hcenter_item = h_center_item[canvas_index];
  vcenter_item = v_center_item[canvas_index];
  hborder_item = h_border_item[canvas_index];
  vborder_item = v_border_item[canvas_index];
  h_str_item = h_stretch_item[canvas_index];
  v_str_item = v_stretch_item[canvas_index];
  h_int_stretch = h_integer_stretch[canvas_index];
  v_int_stretch = v_integer_stretch[canvas_index];
  use_h_int_stretch = use_h_integer_stretch[canvas_index];
  use_v_int_stretch = use_v_integer_stretch[canvas_index];







  const int menu_ready = hcenter_item != NULL && vcenter_item != NULL &&
                         hborder_item != NULL && vborder_item != NULL &&
                         h_str_item != NULL && v_str_item != NULL;
  const int default_h_stretch =
      emux_machine_class == BMC64_MACHINE_CLASS_VIC20 ? DEFAULT_VIC_H_STRETCH
                                                     : DEFAULT_VICII_H_STRETCH;

  int hc = menu_ready ? hcenter_item->value : 0;
  int vc = menu_ready ? vcenter_item->value : 0;
  int vid_hc = hc;
  int vid_vc = vc;

  if (menu_ready && active_display_item != NULL &&
      emux_machine_class == BMC64_MACHINE_CLASS_C128) {


     if ((active_display_item->value == MENU_ACTIVE_DISPLAY_VICII && layer == FB_LAYER_VIC) ||
         (active_display_item->value == MENU_ACTIVE_DISPLAY_VDC && layer == FB_LAYER_VDC) ||
         active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI) {
        lpad = 0; rpad = 0; tpad = 0; bpad = 0; zlayer = layer == FB_LAYER_VIC ? 0 : 1;
     } else if (active_display_item->value == MENU_ACTIVE_DISPLAY_SIDE_BY_SIDE) {
        // VIC on the left, VDC on the right, always, no swapping
        use_h_int_stretch = 0;
        use_v_int_stretch = 0;
        if (layer == FB_LAYER_VIC) {
            lpad = 0; rpad = .50d; tpad = 0; bpad = 0; zlayer = 0;
        } else {
            lpad = .50d; rpad = 0; tpad = 0; bpad = 0; zlayer = 1;
        }
        // Always ignore centering in this mode
        vid_hc = 0;
        vid_vc = 0;
     } else if (active_display_item->value == MENU_ACTIVE_DISPLAY_PIP) {
        if ((layer == FB_LAYER_VIC && pip_swapped_item->value == 0) ||
            (layer == FB_LAYER_VDC && pip_swapped_item->value == 1)) {
            // full screen for this layer
            lpad = 0; rpad = 0; tpad = 0; bpad = 0; zlayer = 0;
        } else {
            use_h_int_stretch = 0;
            use_v_int_stretch = 0;
            zlayer = 1;
            if (pip_location_item->value == MENU_PIP_TOP_LEFT) {
              // top left quad
              lpad = .05d; rpad = .65d; tpad = .05d; bpad = .65d;
            } else if (pip_location_item->value == MENU_PIP_TOP_RIGHT) {
              // top right quad
              lpad = .65d; rpad = .05d; tpad = .05d; bpad = .65d;
            } else if (pip_location_item->value == MENU_PIP_BOTTOM_RIGHT) {
              // bottom right quad
              lpad = .65d; rpad = .05d; tpad = .65d; bpad = .05d;
            } else if (pip_location_item->value == MENU_PIP_BOTTOM_LEFT) {
              // bottom left quad
              lpad = .05d; rpad = .65d; tpad = .65d; bpad = .05d;
            }
            // Always ignore centering in this mode
            vid_hc = 0;
            vid_vc = 0;
        }
    } else {
        return;
    }
  } else {
     // Only 1 display for this machine. Full screen.
     lpad = 0; rpad = 0; tpad = 0; bpad = 0; zlayer = 0;
  }

  int h = menu_ready ? hborder_item->value : 0;
  int v = menu_ready ? vborder_item->value : 0;
  double hs = (double)(menu_ready ? h_str_item->value : default_h_stretch)
              / 1000.0d;
  double vs = (double)(menu_ready ? v_str_item->value : DEFAULT_VICII_V_STRETCH)
              / 1000.0d;

  double vid_hstretch = hs;
  if (menu_ready && active_display_item != NULL &&
      emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
          active_display_item->value == MENU_ACTIVE_DISPLAY_SIDE_BY_SIDE) {
     // For side-by-side, it makes more sense to fill horizontal then scale
     // vertical since we just cut horizontal in half. So pass in negative
     // hstretch.
     vid_hstretch = -hs;
  }

  // Tell videoarch about these changes
  emux_apply_video_adjustments(layer, vid_hc, vid_vc,
     h, v,
     vid_hstretch, vs,
     h_int_stretch, v_int_stretch,
     use_h_int_stretch, use_v_int_stretch,
     lpad, rpad, tpad, bpad, zlayer);

  if (layer == FB_LAYER_VIC) {
     // Make UI match VIC settings except for padding.
     emux_apply_video_adjustments(
        FB_LAYER_UI, hc, vc,
        h, v,
        hs, vs,
        h_integer_stretch[0], v_integer_stretch[0],
        use_h_integer_stretch[0], use_v_integer_stretch[0],
        0, 0, 0, 0, 3);
  }
}




static void sid_aggiorna_righe(void) {
  int quanti = emux_sid_quanti_brani();
  int ora;

  if (sid_titolo_item == NULL) {
    return;
  }
  if (quanti <= 0) {



    snprintf(sid_titolo_item->name, MAX_MENU_STR, "(no SID loaded)");
    sid_autore_item->name[0] = '\0';
    sid_diritti_item->name[0] = '\0';
    sid_brano_item->name[0] = '\0';
    return;
  }
  snprintf(sid_titolo_item->name, MAX_MENU_STR, "%s", emux_sid_chi_suona(0));
  snprintf(sid_autore_item->name, MAX_MENU_STR, "by %s",
           emux_sid_chi_suona(1));
  snprintf(sid_diritti_item->name, MAX_MENU_STR, "%s", emux_sid_chi_suona(2));
  ora = emux_sid_quale_brano();
  if (ora < 1) {
    ora = 1;
  }
  snprintf(sid_brano_item->name, MAX_MENU_STR, "Tune %d of %d", ora, quanti);
}




static void tasto_sid(void) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_C64) {
    overlay_avviso("NO SID PLAYER ON THIS MACHINE");
    return;
  }







  if (ui_enabled) {
    ui_dismiss_osd_if_active();
    return;
  }
  ui_pop_all_and_toggle();
  show_files(DIR_SIDS, FILTER_SID, MENU_SID_FILE, 0);



  ui_esc_chiude_tutto = 1;
}

static void menu_machine_reset(int type, int pop) {
  // The IEC dir may have been changed by the emulated machine. On reset,
  // we reset back to the last dir set by the user.
  emux_set_iec_dir(8, last_iec_dir[0]);
  emux_set_iec_dir(9, last_iec_dir[1]);
  emux_set_iec_dir(10, last_iec_dir[2]);
  emux_set_iec_dir(11, last_iec_dir[3]);




  emux_real_drive_molla();
  emux_reset(type);
  if (pop) {
     ui_pop_all_and_toggle();
  }
}



static void reset_shader_params() {
  s_curvature_item->value = 0;
  s_curvature_x_item->value = 10;
  s_curvature_y_item->value = 15;
  s_skew_x_item->value = 0;
  s_skew_y_item->value = 0;
  s_trapezoid_item->value = 0;
  s_rotation_item->value = 0;
  s_overscan_item->value = 0;
  s_convergence_item->value = 0;
  s_red_offset_x_item->value = 25;
  s_red_offset_y_item->value = 0;
  s_blue_offset_x_item->value = -25;
  s_blue_offset_y_item->value = 0;
  s_convergence_radial_strength_item->value = 25;
  s_horizontal_filtering_item->value = 1;
  s_sigma_x_item->value = 50;
  s_edge_blur_item->value = 0;
  s_edge_blur_strength_item->value = 30;
  s_edge_blur_radius_item->value = 70;
  s_mask_enable_item->value = 0;
  s_mask_item->value = 0;
  s_mask_brightness_item->value = 70;
  s_scanlines_item->value = 1;
  s_multisample_item->value = 1;
  s_scanline_weight_item->value = 60;
  s_scanline_gap_brightness_item->value = 12;
  s_bloom_item->value = 1;
  s_bloom_factor_item->value = 150;
  s_vignette_item->value = 0;
  s_vignette_strength_item->value = 25;
  s_vignette_scale_item->value = 75;
  s_vignette_softness_item->value = 45;
  s_uneven_illumination_item->value = 0;
  s_uneven_illumination_strength_item->value = 15;
  s_uneven_illumination_scale_item->value = 25;
  s_horizontal_jitter_item->value = 0;
  s_horizontal_jitter_strength_item->value = 10;
  s_horizontal_jitter_frequency_item->value = 18;
  s_horizontal_jitter_speed_item->value = 0;
  s_composite_artifacts_item->value = 0;
  s_composite_chroma_blur_item->value = 25;
  s_composite_luma_sharpen_item->value = 10;
  s_composite_color_bleed_item->value = 15;
  s_glass_reflection_item->value = 0;
  s_glass_reflection_angle_item->value = -20;
  s_glass_reflection_width_item->value = 25;
  s_glass_reflection_position_item->value = 35;
  s_rounded_screen_mask_item->value = 0;
  s_rounded_corner_radius_item->value = 20;
  s_rounded_border_softness_item->value = 15;
  s_edge_glow_item->value = 0;
  s_edge_glow_strength_item->value = 15;
  s_edge_glow_width_item->value = 20;
  s_noise_item->value = 0;
  s_luminance_noise_item->value = 10;
  s_chroma_noise_item->value = 8;
  s_noise_speed_item->value = 0;
  s_output_response_item->value = 1;
  s_response_mode_item->value = 1;
  s_level_mapping_item->value = BMX_OUTPUT_LEVEL_MAPPING_CUBIC;
  s_input_gamma_item->value = 240;
  s_output_gamma_item->value = 220;
  s_response_saturation_item->value = 100;
  s_black_level_item->value = 0;
  s_white_clip_item->value = 100;
}

static void set_shader_items_disabled(struct menu_item **items,
                                      unsigned int count,
                                      int disabled) {
  for (unsigned int i = 0; i < count; ++i) {
    items[i]->disabled = disabled;
  }
}

static int crt_shader_preview_layer(void) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_C128 ||
      active_display_item == NULL) {
    return FB_LAYER_VIC;
  }

  if (active_display_item->value == MENU_ACTIVE_DISPLAY_VDC ||
      (active_display_item->value == MENU_ACTIVE_DISPLAY_PIP &&
       pip_swapped_item != NULL && pip_swapped_item->value != 0)) {
    return FB_LAYER_VDC;
  }
  return FB_LAYER_VIC;
}

static int crt_shader_display_mode_supported(void) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_C128 ||
      active_display_item == NULL ||
      active_display_item->value == MENU_ACTIVE_DISPLAY_VICII) {
    return 1;
  }

  if (active_display_item->value == MENU_ACTIVE_DISPLAY_VDC) {
    // Legacy EGL applies shader state only to the VIC layer. Board-specific
    // backends advertise support for this specific C128 base layer.
    return circle_shader_backend_available_for_layer(FB_LAYER_VDC);
  }
  return 0;
}

static void reveal_crt_shader_preview(void) {
  ui_canvas_reveal_temp(crt_shader_preview_layer());
}

static void mark_crt_shader_preview_hidden(void) {
  if (crt_shader_preview_layer() == FB_LAYER_VDC) {
    vdc_showing = 0;
  } else {
    vic_showing = 0;
  }
}

static void sanity_check_shader_params(void) {
  if (s_response_saturation_item->value < 0) {
    s_response_saturation_item->value = 0;
  } else if (s_response_saturation_item->value > 100) {
    s_response_saturation_item->value = 100;
  }
  if (s_level_mapping_item->value < BMX_OUTPUT_LEVEL_MAPPING_LINEAR ||
      s_level_mapping_item->value > BMX_OUTPUT_LEVEL_MAPPING_TOE_SHOULDER) {
    s_level_mapping_item->value = BMX_OUTPUT_LEVEL_MAPPING_CUBIC;
  }

  struct menu_item *all_items[] = {
    s_curvature_item, s_curvature_x_item, s_curvature_y_item,
    s_skew_x_item, s_skew_y_item, s_trapezoid_item, s_rotation_item,
    s_overscan_item, s_convergence_item, s_red_offset_x_item,
    s_red_offset_y_item, s_blue_offset_x_item, s_blue_offset_y_item,
    s_convergence_radial_strength_item, s_horizontal_filtering_item,
    s_sigma_x_item, s_edge_blur_item, s_edge_blur_strength_item,
    s_edge_blur_radius_item, s_scanlines_item, s_multisample_item,
    s_scanline_weight_item, s_scanline_gap_brightness_item,
    s_mask_enable_item, s_mask_item, s_mask_brightness_item,
    s_bloom_item, s_bloom_factor_item, s_vignette_item,
    s_vignette_strength_item, s_vignette_scale_item,
    s_vignette_softness_item, s_uneven_illumination_item,
    s_uneven_illumination_strength_item, s_uneven_illumination_scale_item,
    s_horizontal_jitter_item, s_horizontal_jitter_strength_item,
    s_horizontal_jitter_frequency_item, s_horizontal_jitter_speed_item,
    s_composite_artifacts_item,
    s_composite_chroma_blur_item, s_composite_luma_sharpen_item,
    s_composite_color_bleed_item, s_glass_reflection_item,
    s_glass_reflection_angle_item, s_glass_reflection_width_item,
    s_glass_reflection_position_item, s_rounded_screen_mask_item,
    s_rounded_corner_radius_item, s_rounded_border_softness_item,
    s_edge_glow_item, s_edge_glow_strength_item, s_edge_glow_width_item,
    s_noise_item, s_luminance_noise_item, s_chroma_noise_item,
    s_noise_speed_item,
    s_output_response_item, s_response_mode_item, s_level_mapping_item,
    s_input_gamma_item,
    s_output_gamma_item, s_response_saturation_item, s_black_level_item,
    s_white_clip_item
  };
  set_shader_items_disabled(all_items,
      sizeof all_items / sizeof all_items[0], 0);

  if (!s_enable_shader_item->value ||
      !crt_shader_display_mode_supported()) {
    set_shader_items_disabled(all_items,
        sizeof all_items / sizeof all_items[0], 1);
    return;
  }

#define DISABLE_GROUP_IF_OFF(toggle, ...) do { \
  struct menu_item *group_items[] = {__VA_ARGS__}; \
  if (!(toggle)->value) { \
    set_shader_items_disabled(group_items, \
        sizeof group_items / sizeof group_items[0], 1); \
  } \
} while (0)

  DISABLE_GROUP_IF_OFF(s_curvature_item,
      s_curvature_x_item, s_curvature_y_item, s_skew_x_item, s_skew_y_item,
      s_trapezoid_item, s_rotation_item, s_overscan_item);
  DISABLE_GROUP_IF_OFF(s_convergence_item,
      s_red_offset_x_item, s_red_offset_y_item, s_blue_offset_x_item,
      s_blue_offset_y_item, s_convergence_radial_strength_item);
  DISABLE_GROUP_IF_OFF(s_horizontal_filtering_item, s_sigma_x_item);
  DISABLE_GROUP_IF_OFF(s_edge_blur_item,
      s_edge_blur_strength_item, s_edge_blur_radius_item);
  DISABLE_GROUP_IF_OFF(s_scanlines_item,
      s_multisample_item, s_scanline_weight_item,
      s_scanline_gap_brightness_item);
  DISABLE_GROUP_IF_OFF(s_mask_enable_item,
      s_mask_item, s_mask_brightness_item);
  DISABLE_GROUP_IF_OFF(s_bloom_item, s_bloom_factor_item);
  DISABLE_GROUP_IF_OFF(s_vignette_item,
      s_vignette_strength_item, s_vignette_scale_item,
      s_vignette_softness_item);
  DISABLE_GROUP_IF_OFF(s_uneven_illumination_item,
      s_uneven_illumination_strength_item, s_uneven_illumination_scale_item);
  DISABLE_GROUP_IF_OFF(s_horizontal_jitter_item,
      s_horizontal_jitter_strength_item, s_horizontal_jitter_frequency_item,
      s_horizontal_jitter_speed_item);
  DISABLE_GROUP_IF_OFF(s_composite_artifacts_item,
      s_composite_chroma_blur_item, s_composite_luma_sharpen_item,
      s_composite_color_bleed_item);
  DISABLE_GROUP_IF_OFF(s_glass_reflection_item,
      s_glass_reflection_angle_item, s_glass_reflection_width_item,
      s_glass_reflection_position_item);
  DISABLE_GROUP_IF_OFF(s_rounded_screen_mask_item,
      s_rounded_corner_radius_item, s_rounded_border_softness_item);
  DISABLE_GROUP_IF_OFF(s_edge_glow_item,
      s_edge_glow_strength_item, s_edge_glow_width_item);
  DISABLE_GROUP_IF_OFF(s_noise_item,
      s_luminance_noise_item, s_chroma_noise_item, s_noise_speed_item);
  DISABLE_GROUP_IF_OFF(s_output_response_item,
      s_response_mode_item, s_level_mapping_item, s_input_gamma_item,
      s_output_gamma_item,
      s_response_saturation_item, s_black_level_item, s_white_clip_item);

#undef DISABLE_GROUP_IF_OFF

  if (s_response_mode_item->disabled || s_response_mode_item->value == 1) {
    s_input_gamma_item->disabled = 1;
    s_output_gamma_item->disabled = 1;
  }
}







static int s_shader_fermo_nel_lettore = 0;

static void update_crt_shader_availability(void) {
  int available = allow_shader() && crt_shader_display_mode_supported() &&
                  !s_shader_fermo_nel_lettore;
  s_enable_shader_item->disabled = !available;
  strcpy(s_enable_shader_item->custom_toggle_label[0],
         available ? "No" : "Disabled");
  strcpy(s_enable_shader_item->custom_toggle_label[1],
         available ? "Yes" : "Disabled");
  s_crt_preset_item->disabled =
      !available || s_crt_preset_item->num_choices == 1;
}

static int apply_crt_shader_runtime(void) {
  int enabled = allow_shader() && crt_shader_display_mode_supported() &&
                s_enable_shader_item->value && !s_shader_fermo_nel_lettore;
  return circle_realloc_fbl(crt_shader_preview_layer(), enabled);
}

static void refresh_crt_shader_runtime(void) {
  update_crt_shader_availability();
  sanity_check_shader_params();
  int status = apply_crt_shader_runtime();
  if (status == 0) {
    return;
  }

  printf("menu: shader enable failed with %d; disabling\r\n", status);
  s_enable_shader_item->value = 0;
  emux_set_int(Setting_VideoFilter, MENU_VIDEO_FILTER_NONE);
  circle_realloc_fbl(crt_shader_preview_layer(), 0);
  update_crt_shader_availability();
  sanity_check_shader_params();
}

void menu_shader_lettore(int attivo) {
  attivo = attivo ? 1 : 0;
  if (attivo == s_shader_fermo_nel_lettore) {
    return;
  }
  s_shader_fermo_nel_lettore = attivo;


  passo_prima(attivo ? "shader fermo per il lettore SID"
                     : "shader di nuovo come nel menu");
  refresh_crt_shader_runtime();
  passo_fatto(attivo ? "shader fermo per il lettore SID"
                     : "shader di nuovo come nel menu");
}

static void handle_shader_param_change() {
  struct bmx_crt_effect_params params = {0};

  params.geometry_enabled = s_curvature_item->value;
  params.curvature_x = (float)s_curvature_x_item->value / 600.0f;
  params.curvature_y = (float)s_curvature_y_item->value / 600.0f;
  params.skew_x = (float)s_skew_x_item->value * 0.0008f;
  params.skew_y = (float)s_skew_y_item->value * 0.0008f;
  params.trapezoid = (float)s_trapezoid_item->value * 0.0015f;
  params.rotation_degrees = (float)s_rotation_item->value * 0.03f;
  params.overscan_scale = 1.0f + (float)s_overscan_item->value * 0.002f;

  params.convergence_enabled = s_convergence_item->value;
  params.red_offset_x = (float)s_red_offset_x_item->value / 100.0f;
  params.red_offset_y = (float)s_red_offset_y_item->value / 100.0f;
  params.blue_offset_x = (float)s_blue_offset_x_item->value / 100.0f;
  params.blue_offset_y = (float)s_blue_offset_y_item->value / 100.0f;
  params.convergence_radial_strength =
      (float)s_convergence_radial_strength_item->value * 0.02f;

  params.horizontal_filtering_enabled = s_horizontal_filtering_item->value;
  params.horizontal_sigma_x = (float)s_sigma_x_item->value / 100.0f;

  params.edge_blur_enabled = s_edge_blur_item->value;
  params.edge_blur_strength =
      (float)s_edge_blur_strength_item->value / 100.0f;
  params.edge_blur_radius =
      0.2f + (float)s_edge_blur_radius_item->value * 0.008f;

  params.scanlines_enabled = s_scanlines_item->value;
  params.scanline_multisample = s_multisample_item->value;
  params.scanline_weight = (float)s_scanline_weight_item->value / 10.0f;
  params.scanline_gap_brightness =
      (float)s_scanline_gap_brightness_item->value / 100.0f;

  params.phosphor_mask_enabled = s_mask_enable_item->value;
  params.phosphor_mask_type = s_mask_item->value + 1;
  params.phosphor_mask_brightness =
      (float)s_mask_brightness_item->value / 100.0f;

  params.bloom_enabled = s_bloom_item->value;
  params.bloom_factor = (float)s_bloom_factor_item->value / 100.0f;

  params.vignette_enabled = s_vignette_item->value;
  params.vignette_strength = (float)s_vignette_strength_item->value / 100.0f;
  params.vignette_scale = 0.2f + (float)s_vignette_scale_item->value * 0.008f;
  params.vignette_softness =
      0.02f + (float)s_vignette_softness_item->value * 0.0098f;

  params.uneven_illumination_enabled = s_uneven_illumination_item->value;
  params.uneven_illumination_strength =
      (float)s_uneven_illumination_strength_item->value * 0.0035f;
  params.uneven_illumination_scale =
      0.02f + (float)s_uneven_illumination_scale_item->value * 0.0023f;

  params.horizontal_jitter_enabled = s_horizontal_jitter_item->value;
  params.horizontal_jitter_strength =
      (float)s_horizontal_jitter_strength_item->value * 0.06f;
  params.horizontal_jitter_frequency =
      0.01f + (float)s_horizontal_jitter_frequency_item->value * 0.0039f;
  params.horizontal_jitter_speed =
      (float)s_horizontal_jitter_speed_item->value / 100.0f;

  params.composite_artifacts_enabled = s_composite_artifacts_item->value;
  params.composite_chroma_blur =
      (float)s_composite_chroma_blur_item->value * 0.02f;
  params.composite_luma_sharpen =
      (float)s_composite_luma_sharpen_item->value / 100.0f;
  params.composite_color_bleed =
      (float)s_composite_color_bleed_item->value * 0.006f;

  params.glass_reflection_enabled = s_glass_reflection_item->value;
  params.glass_reflection_angle =
      (float)s_glass_reflection_angle_item->value;
  params.glass_reflection_width =
      0.02f + (float)s_glass_reflection_width_item->value * 0.0058f;
  params.glass_reflection_position =
      (float)s_glass_reflection_position_item->value / 100.0f;

  params.rounded_screen_mask_enabled = s_rounded_screen_mask_item->value;
  params.rounded_corner_radius =
      (float)s_rounded_corner_radius_item->value * 0.002f;
  params.rounded_border_softness =
      (float)s_rounded_border_softness_item->value * 0.0008f;

  params.edge_glow_enabled = s_edge_glow_item->value;
  params.edge_glow_strength =
      (float)s_edge_glow_strength_item->value * 0.0035f;
  params.edge_glow_width =
      0.01f + (float)s_edge_glow_width_item->value * 0.0034f;

  params.noise_enabled = s_noise_item->value;
  params.luminance_noise = (float)s_luminance_noise_item->value * 0.001f;
  params.chroma_noise = (float)s_chroma_noise_item->value * 0.0008f;
  params.noise_speed = (float)s_noise_speed_item->value / 100.0f;

  params.output_response_enabled = s_output_response_item->value;
  params.output_response_fast =
      s_output_response_item->value && s_response_mode_item->value == 1;
  params.output_level_mapping = s_level_mapping_item->value;
  params.input_gamma = (float)s_input_gamma_item->value / 100.0f;
  params.output_gamma = (float)s_output_gamma_item->value / 100.0f;
  params.output_saturation =
      (float)s_response_saturation_item->value / 100.0f;
  params.black_level = (float)s_black_level_item->value / 100.0f;
  params.white_clip = (float)s_white_clip_item->value / 100.0f;
  params.bilinear_interpolation = scaling_interp_item->value;

  circle_set_shader_params(&params);

  // Setting shader params hides the layer.
  mark_crt_shader_preview_hidden();
}

static int crt_preset_ascii_compare(const char *left, const char *right) {
  while (*left != '\0' && *right != '\0') {
    int left_char = tolower((unsigned char)*left);
    int right_char = tolower((unsigned char)*right);
    if (left_char != right_char) {
      return left_char - right_char;
    }
    ++left;
    ++right;
  }
  return (unsigned char)*left - (unsigned char)*right;
}

static int crt_preset_name_compare(const char *left, const char *right) {
  int left_default = crt_preset_ascii_compare(left, "Default") == 0;
  int right_default = crt_preset_ascii_compare(right, "Default") == 0;
  if (left_default != right_default) {
    return left_default ? -1 : 1;
  }
  return crt_preset_ascii_compare(left, right);
}

static int crt_preset_has_extension(const char *name) {
  size_t name_length = strlen(name);
  size_t extension_length = strlen(CRT_PRESET_EXTENSION);
  return name_length > extension_length &&
         crt_preset_ascii_compare(name + name_length - extension_length,
                                  CRT_PRESET_EXTENSION) == 0;
}

static int find_crt_preset_choice(const char *name) {
  if (s_crt_preset_item == NULL) {
    return -1;
  }
  for (int i = 1; i < s_crt_preset_item->num_choices; ++i) {
    if (crt_preset_ascii_compare(s_crt_preset_item->choices[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

static void swap_crt_preset_choices(int left, int right) {
  char name[MAX_MENU_STR];
  char path[MAX_STR_VAL_LEN];

  strcpy(name, s_crt_preset_item->choices[left]);
  strcpy(s_crt_preset_item->choices[left],
         s_crt_preset_item->choices[right]);
  strcpy(s_crt_preset_item->choices[right], name);

  strcpy(path, s_crt_preset_paths[left]);
  strcpy(s_crt_preset_paths[left], s_crt_preset_paths[right]);
  strcpy(s_crt_preset_paths[right], path);
}

static void populate_crt_preset_menu(void) {
  memset(s_crt_preset_paths, 0, sizeof(s_crt_preset_paths));
  s_crt_preset_item->num_choices = 1;
  s_crt_preset_item->value = CRT_PRESET_CURRENT_CHOICE;
  strcpy(s_crt_preset_item->choices[CRT_PRESET_CURRENT_CHOICE],
         "Current Settings");
  s_crt_preset_applied_choice = CRT_PRESET_CURRENT_CHOICE;

  DIR *directory = opendir(CRT_PRESET_DIR);
  if (directory == NULL) {
    s_crt_preset_item->disabled = 1;
    printf("boot: crt preset directory missing path=%s\n", CRT_PRESET_DIR);
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(directory)) != NULL) {
    if (!crt_preset_has_extension(entry->d_name)) {
      continue;
    }
    if (s_crt_preset_item->num_choices >= MAX_CHOICES) {
      printf("boot: crt preset limit reached max=%u\n",
             (unsigned int)(MAX_CHOICES - 1));
      break;
    }

    size_t name_length = strlen(entry->d_name) - strlen(CRT_PRESET_EXTENSION);
    if (name_length == 0 || name_length >= MAX_MENU_STR) {
      printf("boot: crt preset filename skipped name=%s\n", entry->d_name);
      continue;
    }

    char display_name[MAX_MENU_STR];
    memcpy(display_name, entry->d_name, name_length);
    display_name[name_length] = '\0';
    if (find_crt_preset_choice(display_name) >= 0) {
      printf("boot: crt preset duplicate name skipped name=%s\n",
             display_name);
      continue;
    }

    char path[MAX_STR_VAL_LEN];
    int path_length = snprintf(path, sizeof(path), "%s/%s",
                               CRT_PRESET_DIR, entry->d_name);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
      printf("boot: crt preset path skipped name=%s\n", entry->d_name);
      continue;
    }
    struct stat file_info;
    if (stat(path, &file_info) != 0 || S_ISDIR(file_info.st_mode)) {
      continue;
    }

    int choice = s_crt_preset_item->num_choices++;
    strcpy(s_crt_preset_item->choices[choice], display_name);
    strcpy(s_crt_preset_paths[choice], path);
  }
  closedir(directory);

  for (int i = 2; i < s_crt_preset_item->num_choices; ++i) {
    int current = i;
    while (current > 1 &&
           crt_preset_name_compare(s_crt_preset_item->choices[current],
                                   s_crt_preset_item->choices[current - 1]) < 0) {
      swap_crt_preset_choices(current, current - 1);
      --current;
    }
  }

  s_crt_preset_item->disabled = s_crt_preset_item->num_choices == 1;
  printf("boot: crt presets found=%u default=%s\n",
         (unsigned int)(s_crt_preset_item->num_choices - 1),
         find_crt_preset_choice("Default") > 0 ? "yes" : "no");
}

static int crt_preset_item_bounds(const struct menu_item *item,
                                  int *min_value,
                                  int *max_value) {
  switch (item->type) {
    case TOGGLE:
    case CHECKBOX:
      *min_value = 0;
      *max_value = 1;
      return 1;
    case RANGE:
      *min_value = item->min;
      *max_value = item->max;
      return 1;
    case MULTIPLE_CHOICE:
      if (item->num_choices <= 0) {
        return 0;
      }
      *min_value = 0;
      *max_value = item->num_choices - 1;
      return 1;
    default:
      return 0;
  }
}

static int load_crt_preset_choice(int choice) {
  if (choice == CRT_PRESET_CURRENT_CHOICE) {
    s_crt_preset_applied_choice = CRT_PRESET_CURRENT_CHOICE;
    return 1;
  }
  if (s_crt_preset_item == NULL || choice < 1 ||
      choice >= s_crt_preset_item->num_choices) {
    return 0;
  }

  struct crt_preset_field fields[CRT_PRESET_FIELD_COUNT];
  int values[CRT_PRESET_FIELD_COUNT];
  for (size_t i = 0; i < CRT_PRESET_FIELD_COUNT; ++i) {
    struct menu_item *bound_item = *s_crt_preset_bindings[i].item;
    fields[i].key = s_crt_preset_bindings[i].key;
    if (bound_item == NULL ||
        !crt_preset_item_bounds(bound_item, &fields[i].min, &fields[i].max)) {
      printf("boot: crt preset schema error key=%s\n", fields[i].key);
      return 0;
    }
  }

  FILE *fp = fopen(s_crt_preset_paths[choice], "r");
  if (fp == NULL) {
    printf("boot: crt preset invalid name=%s status=io-error path=%s\n",
           s_crt_preset_item->choices[choice], s_crt_preset_paths[choice]);
    return 0;
  }

  struct crt_preset_result result;
  enum crt_preset_status status =
      crt_preset_parse(fp, fields, CRT_PRESET_FIELD_COUNT, values, &result);
  fclose(fp);
  if (status != CRT_PRESET_OK) {
    printf("boot: crt preset invalid name=%s status=%s line=%u key=%s\n",
           s_crt_preset_item->choices[choice],
           crt_preset_status_name(status), result.line,
           result.key[0] != '\0' ? result.key : "-");
    return 0;
  }

  for (size_t i = 0; i < CRT_PRESET_FIELD_COUNT; ++i) {
    (*s_crt_preset_bindings[i].item)->value = values[i];
  }
  sanity_check_shader_params();
  s_crt_preset_item->value = choice;
  s_crt_preset_applied_choice = choice;

  printf("boot: crt preset loaded name=%s clamped=%u unknown=%u",
         s_crt_preset_item->choices[choice], result.clamped_count,
         result.unknown_count);
  if (result.clamped_count > 0) {
    printf(" first_clamped=%s", result.first_clamped_key);
  }
  printf("\n");
  return 1;
}

static void mark_crt_preset_modified(void) {
  if (s_crt_preset_item != NULL) {
    s_crt_preset_item->value = CRT_PRESET_CURRENT_CHOICE;
  }
  s_crt_preset_applied_choice = CRT_PRESET_CURRENT_CHOICE;
}

static void disable_all_crt_effects(void) {
  static const char suffix[] = ".enabled";
  for (size_t i = 0; i < CRT_PRESET_FIELD_COUNT; ++i) {
    const char *key = s_crt_preset_bindings[i].key;
    size_t key_length = strlen(key);
    size_t suffix_length = sizeof(suffix) - 1;
    if (key_length >= suffix_length &&
        strcmp(key + key_length - suffix_length, suffix) == 0) {
      (*s_crt_preset_bindings[i].item)->value = 0;
    }
  }
  mark_crt_preset_modified();
}

static void apply_startup_crt_preset(int settings_loaded) {
  int default_choice = find_crt_preset_choice("Default");
  if (default_choice > 0 && load_crt_preset_choice(default_choice)) {
    return;
  }

  if (!settings_loaded) {
    disable_all_crt_effects();
    printf("boot: crt preset fallback=effects-off\n");
  } else {
    mark_crt_preset_modified();
    printf("boot: crt preset fallback=saved-settings\n");
  }
}









static void tasto_scanline(int passo) {
  char riga[24];
  int intensita;

  if (!allow_shader() || !crt_shader_display_mode_supported() ||
      s_shader_fermo_nel_lettore) {
    overlay_avviso("CRT SHADER NOT AVAILABLE");
    return;
  }
  if (!s_enable_shader_item->value || !s_scanlines_item->value) {
    if (passo < 0) {
      overlay_avviso("SCANLINES OFF");
      return;
    }
    if (!s_enable_shader_item->value) {
      s_enable_shader_item->value = 1;
      refresh_crt_shader_runtime();
      emux_set_int(Setting_VideoFilter, s_enable_shader_item->value ?
                   MENU_VIDEO_FILTER_CRT : MENU_VIDEO_FILTER_NONE);
      if (!s_enable_shader_item->value) {
        overlay_avviso("CRT SHADER NOT AVAILABLE");
        return;
      }
    }
    s_scanlines_item->value = 1;
  } else {
    intensita = 100 - s_scanline_gap_brightness_item->value;
    if (passo > 0) {
      intensita = (intensita / 10 + 1) * 10;
    } else {
      intensita = ((intensita + 9) / 10 - 1) * 10;
    }
    if (intensita < 0) {
      s_scanlines_item->value = 0;
      mark_crt_preset_modified();
      sanity_check_shader_params();
      handle_shader_param_change();
      overlay_avviso("SCANLINES OFF");
      return;
    }
    if (intensita > 100) {
      intensita = 100;
    }
    s_scanline_gap_brightness_item->value = 100 - intensita;
  }
  mark_crt_preset_modified();
  sanity_check_shader_params();
  handle_shader_param_change();
  snprintf(riga, sizeof(riga), "SCANLINES %d%%",
           100 - s_scanline_gap_brightness_item->value);
  overlay_avviso(riga);
}

// Interpret what menu item changed and make the change to vice
static void menu_value_changed(struct menu_item *item);


static struct menu_item *menu_trova_reu(void) {
  if (menu_radice == NULL) {
    return NULL;
  }
  return ui_find_item_by_id(menu_radice->first_child, MENU_REU);
}















static int forma_di_serie(double forma) {
  static const double serie[] = {4.0 / 3.0, 5.0 / 4.0, 16.0 / 9.0, 16.0 / 10.0};
  unsigned i;

  for (i = 0; i < sizeof(serie) / sizeof(serie[0]); i++) {
    double d = forma / serie[i] - 1.0;
    if (d < 0.01 && d > -0.01) {
      return 1;
    }
  }
  return 0;
}

static int aspetto_conto(int idx, int largo, int *hs_out, int *vs_out) {
  int layer = (idx == VDC_INDEX) ? FB_LAYER_VDC : FB_LAYER_VIC;
  int dpx, dpy, fbw, fbh, sw, sh, dw, dh;
  double schermo, voluto, hvoluto, vvoluto, pixel;
  int hs, vs;

  if (!h_stretch_item[idx] || !v_stretch_item[idx]) {
    return 0;
  }
  circle_get_fbl_dimensions(layer, &dpx, &dpy, &fbw, &fbh,
                            &sw, &sh, &dw, &dh);


























  schermo = (raspi_disp_w > 0 && raspi_disp_h > 0)
                ? (double)raspi_disp_w / (double)raspi_disp_h
                : ((dpx > 0 && dpy > 0) ? (double)dpx / (double)dpy
                                        : 4.0 / 3.0);









  pixel = 1.0;
  if (!forma_di_serie(schermo)) {
    double pannello = raspi_disp_fisico_milli > 0
                          ? (double)raspi_disp_fisico_milli / 1000.0
                          : 16.0 / 9.0;
    pixel = pannello / schermo;
    schermo = pannello;
  }
  voluto = largo ? (16.0 / 9.0) : (4.0 / 3.0);
  vvoluto = 1.0;
  hvoluto = voluto;
  if (voluto > schermo) {
    vvoluto = schermo / voluto;
    hvoluto = schermo;
  }



  if (pixel != 1.0) {
    hs = (int)(hvoluto / pixel * 1000.0 + 0.999);
  } else {
    hs = (int)(hvoluto * 1000.0 + 0.5);
  }
  vs = (int)(vvoluto * 1000.0 + 0.5);
  if (hs > h_stretch_item[idx]->max) hs = h_stretch_item[idx]->max;
  if (hs < h_stretch_item[idx]->min) hs = h_stretch_item[idx]->min;
  if (vs > v_stretch_item[idx]->max) vs = v_stretch_item[idx]->max;
  if (vs < v_stretch_item[idx]->min) vs = v_stretch_item[idx]->min;
  *hs_out = hs;
  *vs_out = vs;
  return 1;
}














static void traccia_geometria(int layer, const char *quando) {
  int idx = (layer == FB_LAYER_VDC) ? VDC_INDEX : VIC_INDEX;
  int dpx, dpy, fbw, fbh, sw, sh, dw, dh;
  char riga[160];

  circle_get_fbl_dimensions(layer, &dpx, &dpy, &fbw, &fbh,
                            &sw, &sh, &dw, &dh);
  snprintf(riga, sizeof(riga),
           "[GEO] %s strato=%d tela=%dx%d schermo=%dx%d"
           " hs=%d vs=%d rettangolo=%dx%d",
           quando, layer, fbw, fbh, dpx, dpy,
           h_stretch_item[idx] ? h_stretch_item[idx]->value : -1,
           v_stretch_item[idx] ? v_stretch_item[idx]->value : -1,
           dw, dh);
  emux_log_riga(riga);
}




static void metti_aspetto(int idx, int largo) {
  int layer = (idx == VDC_INDEX) ? FB_LAYER_VDC : FB_LAYER_VIC;
  int hs, vs;

  if (!aspetto_conto(idx, largo, &hs, &vs)) {
    return;
  }
  h_stretch_item[idx]->value = hs;
  v_stretch_item[idx]->value = vs;



  use_h_integer_stretch[idx] = 0;
  use_v_integer_stretch[idx] = 0;
  if (use_scaling_params_item[idx]) {
    use_scaling_params_item[idx]->value = 0;
  }
  ui_canvas_reveal_temp(layer);
  do_video_settings(layer);
  traccia_geometria(layer, largo ? "16:9" : "4:3");
}












static int aspetto_quale_schermo(int altro) {
  int qua;

  if (emux_machine_class != BMC64_MACHINE_CLASS_C128 ||
      h_stretch_item[VDC_INDEX] == NULL ||
      v_stretch_item[VDC_INDEX] == NULL) {
    return VIC_INDEX;
  }
  if (active_display_item != NULL &&
      active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI) {
    qua = VIC_INDEX;
  } else {
    qua = menu_active_display_is_vdc() ? VDC_INDEX : VIC_INDEX;
  }
  if (altro) {
    qua = (qua == VIC_INDEX) ? VDC_INDEX : VIC_INDEX;
  }
  return qua;
}


















static void tasto_commuta_aspetto(int altro) {
  int hs43, vs43, hs169, vs169, ch, cv, largo, idx;








  if (is_composite()) {
    overlay_avviso("NOT ON COMPOSITE");
    if (ui_lettore_sid_a_schermo()) {
      ui_lettore_sid_avviso("NOT ON COMPOSITE");
    }
    return;
  }
  idx = aspetto_quale_schermo(altro);
  if (!aspetto_conto(idx, 0, &hs43, &vs43) ||
      !aspetto_conto(idx, 1, &hs169, &vs169)) {
    return;
  }
  ch = h_stretch_item[idx]->value;
  cv = v_stretch_item[idx]->value;
  largo = (abs(ch - hs169) + abs(cv - vs169)) <
          (abs(ch - hs43) + abs(cv - vs43));

  metti_aspetto(idx, !largo);



  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
    if (idx == VDC_INDEX) {
      overlay_avviso(largo ? "VDC 4:3" : "VDC 16:9");
    } else {
      overlay_avviso(largo ? "VIC-II 4:3" : "VIC-II 16:9");
    }
  } else {
    overlay_avviso(largo ? "ASPECT 4:3" : "ASPECT 16:9");
  }
}















static void tasto_colonne(void) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_C128 ||
      c40_80_column_item == NULL) {
    overlay_avviso("C128 ONLY");
    return;
  }
  c40_80_column_item->value = c40_80_column_item->value ? 0 : 1;
  menu_value_changed(c40_80_column_item);
}










static void doppio_hdmi_riavvia(int salva) {
  int esito = cmdline_opzione("doppio_hdmi", "1");
  if (esito == 0 && salva) {
    active_display_item->value = MENU_ACTIVE_DISPLAY_DUE_HDMI;
    esito = save_settings();
  }
  if (esito != 0) {
    printf("[VID] due HDMI: la scheda non si scrive (%d)\n", esito);
    active_display_item->value = MENU_ACTIVE_DISPLAY_VICII;
    overlay_avviso("TWO HDMI: CANNOT WRITE THE CARD");
    return;
  }
  printf("[VID] due HDMI: doppio_hdmi=1 in cmdline.txt, riavvio\n");
  riavvia();
}




static void doppio_hdmi_cmdline(int voglio) {
  if (doppio_in_cmdline < 0) {
    doppio_in_cmdline = circle_doppio_hdmi_all_avvio();
  }
  if (voglio == doppio_in_cmdline) {
    return;
  }
  if (cmdline_opzione("doppio_hdmi", voglio ? "1" : NULL) == 0) {
    doppio_in_cmdline = voglio;
  }
}












static void due_hdmi_nel_giro(void) {
  if (active_display_item == NULL || secondo_hdmi_item == NULL) {
    return;
  }
  active_display_item->choice_disabled[MENU_ACTIVE_DISPLAY_DUE_HDMI] =
      secondo_hdmi_item->value ? 0 : 1;
}

static void tasto_commuta_schermo(void) {
  if (active_display_item == NULL) {
    overlay_avviso("C128 ONLY");
    return;
  }






  if (active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI &&
      circle_secondo_schermo_info(NULL, NULL, NULL) >= 0) {
    overlay_avviso("BOTH SCREENS ARE ON");
    return;
  }
  if (active_display_item->value == MENU_ACTIVE_DISPLAY_VDC) {
    active_display_item->value = MENU_ACTIVE_DISPLAY_VICII;
  } else {
    active_display_item->value = MENU_ACTIVE_DISPLAY_VDC;
  }
  menu_value_changed(active_display_item);
  overlay_avviso(active_display_item->value == MENU_ACTIVE_DISPLAY_VDC
                     ? "VDC 80 COLUMNS" : "VIC-II 40 COLUMNS");
}







static int volume_udibile = 100;






volatile unsigned menu_tasti_volume = 0;

static void menu_volume_ricorda(int v) {
  if (v > 0) {
    volume_udibile = v;
  }
}







#define VOL_TRACCIA(dove) do { } while (0)







static void tasto_muto(void) {
  static int muto = 0;
  char riga[24];

  if (volume_item == NULL) {
    return;
  }
  menu_tasti_volume++;
  if (!muto && volume_item->value > 0) {
    menu_volume_ricorda(volume_item->value);
    volume_item->value = 0;
    muto = 1;
  } else {
    volume_item->value = volume_udibile > 0 ? volume_udibile : 100;
    muto = 0;
  }
  menu_value_changed(volume_item);
  VOL_TRACCIA(muto ? "ALT+M muto" : "ALT+M riacceso");
  if (volume_item->value == 0) {
    overlay_avviso("MUTE");
  } else {
    snprintf(riga, sizeof(riga), "VOLUME %d%%", volume_item->value);
    overlay_avviso(riga);
  }
}



static void tasto_volume(int passo) {
  char riga[24];
  int v;

  if (volume_item == NULL) {
    return;
  }
  menu_tasti_volume++;
  v = volume_item->value + passo;
  if (v < volume_item->min) v = volume_item->min;
  if (v > volume_item->max) v = volume_item->max;








  if (v == volume_item->value) {
    if (v == 0) {
      overlay_avviso("MUTE");
    } else {
      snprintf(riga, sizeof(riga), "VOLUME %d%% MAX", v);
      overlay_avviso(riga);
    }
    return;
  }
  volume_item->value = v;
  menu_volume_ricorda(v);
  menu_value_changed(volume_item);
  VOL_TRACCIA("ALT+/-");
  snprintf(riga, sizeof(riga), "VOLUME %d%%", v);
  overlay_avviso(riga);
}


















static void tasto_reu_misura(void) {
  struct menu_item *item =
      ui_find_item_by_id(menu_radice->first_child, MENU_REU_SIZE);

  if (item == NULL) {
    overlay_avviso("NO REU ON THIS MACHINE");
    return;
  }
  item->value = (item->value == 2) ? 7 : 2;
  if (item->on_value_changed) {
    item->on_value_changed(item);
  }
  overlay_avviso(item->value == 7 ? "REU 16 MB - RESET TO USE"
                                  : "REU 512 KB - RESET TO USE");
}







static void tasto_reu_monta(void) {
  if (menu_trova_reu() == NULL) {
    overlay_avviso("NO REU ON THIS MACHINE");
    return;
  }
  tasto_apre_i_file(DIR_REU, FILTER_REU, MENU_REU_ATTACH_IMAGE_FILE);
}






static void tasto_reu_smonta(void) {
  struct menu_item *voce;
  if (menu_trova_reu() == NULL) {
    overlay_avviso("NO REU ON THIS MACHINE");
    return;
  }
  voce = ui_find_item_by_id(menu_radice->first_child, MENU_REU_DETACH_IMAGE);
  if (voce == NULL || voce->disabled) {
    overlay_avviso("NO REU IMAGE");
    return;
  }
  if (emux_handle_menu_change(voce)) {
    overlay_avviso("REU IMAGE REMOVED");
  }
}










int menu_nastro_pronto(void) {
  int p = emux_tape_presente();

  if (p > 0) {
    return 1;
  }
  overlay_avviso(p < 0 ? "NO TAPE ON THIS MACHINE" : "NO TAPE");
  return 0;
}

static void tasto_reu(void) {
  struct menu_item *item = menu_trova_reu();

  if (item == NULL) {
    overlay_avviso("NO REU ON THIS MACHINE");
    return;
  }
  item->value = !item->value;




  if (item->on_value_changed) {
    item->on_value_changed(item);
  }
  overlay_avviso(item->value ? "REU ON" : "REU OFF");
}












#define TASTI_RIGHE_MAX 25














#define TASTI_COLONNE_MAX 35












static const char *const tasti_disposizione[4] = {
    "Keys for: US keyboard", "Keys for: Italian keyboard",
    "Keys for: British keyboard", "Keys for: German keyboard"};
static const char *const tasti_canc[4] = {
    "Del          tape counter to 000", "Canc         tape counter to 000",
    "Del          tape counter to 000", "Entf         tape counter to 000"};
static const char *const tasti_barra[4] = {
    "Backslash    16:9 <-> 4:3", "< >          16:9 <-> 4:3",
    "Backslash    16:9 <-> 4:3", "< >          16:9 <-> 4:3"};
static const char *const tasti_barra_shift[4] = {
    "SHIFT Bksl   same, second screen", "SHIFT < >    same, second screen",
    "SHIFT Bksl   same, second screen", "SHIFT < >    same, second screen"};




static const char *const tasti_barra_lettore[4] = {
    "\\      16:9 <-> 4:3", "\\ or < >  16:9 <-> 4:3",
    "\\      16:9 <-> 4:3", "< >    16:9 <-> 4:3"};
static const char *const tasti_stamp[4] = {
    "PrtScn       VIC-II <-> VDC", "Stamp        VIC-II <-> VDC",
    "PrtScn       VIC-II <-> VDC", "Druck        VIC-II <-> VDC"};
static const char *const tasti_stamp_shift[4] = {
    "SHIFT PrtScn 40/80 column key", "SHIFT Stamp  40/80 column key",
    "SHIFT PrtScn 40/80 column key", "SHIFT Druck  40/80 column key"};
static const char *const tasti_restore[4] = {
    "Without ALT, PrtScn is RESTORE.", "Without ALT, Stamp is RESTORE.",
    "Without ALT, PrtScn is RESTORE.", "Without ALT, Druck is RESTORE."};




static const char *const tasti_spegni[2] = {
    "ALT+SHIFT+ESC twice: shut down", "Hold Power, or ALT+SHIFT+ESC x2"};

static int disposizione_della_pagina(void) {
  int d = ui_get_keyboard_layout();
  return (d >= 0 && d < 4) ? d : 0;
}








void menu_pagina_tasti_rapidi(void) {
  struct menu_item *pagina = ui_push_menu(-1, -1);
  const int d = disposizione_della_pagina();

  ui_menu_add_button(MENU_TEXT, pagina, tasti_disposizione[d]);
  ui_menu_add_button(MENU_TEXT, pagina, "Hold ALT and press:");
  ui_menu_add_button(MENU_TEXT, pagina, "");
  ui_menu_add_button(MENU_TEXT, pagina, "Arrows       tape PLAY STOP REW FF");
  ui_menu_add_button(MENU_TEXT, pagina, tasti_canc[d]);
  ui_menu_add_button(MENU_TEXT, pagina, "T  SHIFT T   tape attach / detach");
  ui_menu_add_button(MENU_TEXT, pagina, "8  SHIFT 8   drive 8 attach/detach");
  ui_menu_add_button(MENU_TEXT, pagina, "9  SHIFT 9   drive 9 attach/detach");
  ui_menu_add_button(MENU_TEXT, pagina, "C  SHIFT C   cartridge attach/det.");
  ui_menu_add_button(MENU_TEXT, pagina, "A            autostart prg or disk");
  ui_menu_add_button(MENU_TEXT, pagina, "R  SHIFT R   hard / soft reset");
  ui_menu_add_button(MENU_TEXT, pagina, "W            warp mode on / off");
  ui_menu_add_button(MENU_TEXT, pagina, "O  SHIFT O   status bar / 2nd line");



  if (menu_trova_reu() != NULL) {
    ui_menu_add_button(MENU_TEXT, pagina, "E  SHIFT E   REU on/off, 512K/16M");
    ui_menu_add_button(MENU_TEXT, pagina, "0  SHIFT 0   REU image load/clear");
  }

  if (emux_machine_class == BMC64_MACHINE_CLASS_C64) {
    ui_menu_add_button(MENU_TEXT, pagina, "S  SHIFT S   SID player / stop");

    ui_menu_add_button(MENU_TEXT, pagina, ",  .  P      prev/next tune, pause");
  }
  if (emux_machine_class != BMC64_MACHINE_CLASS_VIC20) {
    ui_menu_add_button(MENU_TEXT, pagina, "J            swap joystick ports");
  }
  ui_menu_add_button(MENU_TEXT, pagina, "+  -  M      volume up, down, mute");
  ui_menu_add_button(MENU_TEXT, pagina, "SHIFT +  -   scanlines up, down");


  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
    if (!is_composite()) {
      ui_menu_add_button(MENU_TEXT, pagina, tasti_barra[d]);
      ui_menu_add_button(MENU_TEXT, pagina, tasti_barra_shift[d]);
    }
    ui_menu_add_button(MENU_TEXT, pagina, tasti_stamp[d]);
    ui_menu_add_button(MENU_TEXT, pagina, tasti_stamp_shift[d]);
  } else if (!is_composite()) {
    ui_menu_add_button(MENU_TEXT, pagina, tasti_barra[d]);
  }









  if (!(emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
        menu_trova_reu() != NULL)) {
    ui_menu_add_button(MENU_TEXT, pagina, "");
  }
  ui_menu_add_button(MENU_TEXT, pagina, tasti_restore[d]);

  ui_menu_add_divider(pagina);
  ui_menu_add_button(MENU_TASTI_PAGINA_2, pagina, "NEXT PAGE >");
}




void menu_pagina_tasti_rapidi_2(void) {
  struct menu_item *pagina = ui_push_menu(-1, -1);

  ui_menu_add_button(MENU_TASTI_PAGINA_1, pagina, "< PREVIOUS PAGE");
  ui_menu_add_divider(pagina);

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {


    ui_menu_add_button(MENU_TEXT, pagina, "The C128 keys a C64 has not.");
    ui_menu_add_button(MENU_TEXT, pagina, "Hold ALT and press:");
    ui_menu_add_button(MENU_TEXT, pagina, "");
    ui_menu_add_button(MENU_TEXT, pagina, "F1  ESC          F5  HELP");
    ui_menu_add_button(MENU_TEXT, pagina, "F2  TAB          F6  LINE FEED");
    ui_menu_add_button(MENU_TEXT, pagina, "F3  ALT          F7  40/80 DISPLAY");
    ui_menu_add_button(MENU_TEXT, pagina, "F4  CAPS LOCK    F8  NO SCROLL");
    ui_menu_add_button(MENU_TEXT, pagina, "");
    ui_menu_add_button(MENU_TEXT, pagina, "They stay down while you hold the");
    ui_menu_add_button(MENU_TEXT, pagina, "function key.");





    ui_menu_add_button(MENU_TEXT, pagina, "The numeric keypad is the C128 one.");
    ui_menu_add_divider(pagina);
  }




  ui_menu_add_button(MENU_TEXT, pagina, "Drive model, hold ALT and press:");




  if (emux_machine_class != BMC64_MACHINE_CLASS_C64) {
    ui_menu_add_button(MENU_TEXT, pagina, "");
  }
  if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU) {
    ui_menu_add_button(MENU_TEXT, pagina, "1 none    2 1541    3 1551");
    ui_menu_add_button(MENU_TEXT, pagina, "4 1581: pick a .D81 disk");
    ui_menu_add_button(MENU_TEXT, pagina, "");
    ui_menu_add_button(MENU_TEXT, pagina, "The disk picks the drive here: a");
    ui_menu_add_button(MENU_TEXT, pagina, ".D81 is always a 1581, a .D64 is");
    ui_menu_add_button(MENU_TEXT, pagina, "never one.");
    ui_menu_add_button(MENU_TEXT, pagina, "");
    ui_menu_add_button(MENU_TEXT, pagina, "Same 1-4 with SHIFT: drive 9.");
  } else if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4) {
    ui_menu_add_button(MENU_TEXT, pagina, "1 none    2 1541    3 1541-II");
    ui_menu_add_button(MENU_TEXT, pagina, "4 1551    5 1571    6 1581");
    ui_menu_add_button(MENU_TEXT, pagina, "7 real drive on the XUM1541");
    ui_menu_add_button(MENU_TEXT, pagina, "");
    ui_menu_add_button(MENU_TEXT, pagina, "Same 1-7 with SHIFT: drive 9.");
  } else {
    ui_menu_add_button(MENU_TEXT, pagina, "1 none    2 1541    3 1541-II");
    ui_menu_add_button(MENU_TEXT, pagina, "4 1570    5 1571    6 1581");
    ui_menu_add_button(MENU_TEXT, pagina, "7 real drive on the XUM1541");
    if (emux_machine_class != BMC64_MACHINE_CLASS_C64) {
      ui_menu_add_button(MENU_TEXT, pagina, "");
    }
    ui_menu_add_button(MENU_TEXT, pagina, "Same 1-7 with SHIFT: drive 9.");
  }





  if (emux_machine_class == BMC64_MACHINE_CLASS_C64) {
    ui_menu_add_divider(pagina);
    ui_menu_add_button(MENU_TEXT, pagina, "SID player, without ALT:");
    ui_menu_add_button(MENU_TEXT, pagina, "1-9    voice on/off, 1-3 = SID 1");
    ui_menu_add_button(MENU_TEXT, pagina, "       4-6 = SID 2, 7-9 = SID 3");



    ui_menu_add_button(MENU_TEXT, pagina, "0 - +  voice on/off, SID 4");
    ui_menu_add_button(MENU_TEXT, pagina, "V T N  VU meters, time, tune info");


    ui_menu_add_button(MENU_TEXT, pagina, ", . P M  tune -/+, pause, mute");
    ui_menu_add_button(MENU_TEXT, pagina, "<- ->  10 seconds back / forward");
    ui_menu_add_button(MENU_TEXT, pagina, "F1-F4  side of SID 1-4: L C R");


    ui_menu_add_button(MENU_TEXT, pagina, "S      reSID <-> reSIDfp");
    if (!is_composite()) {
      ui_menu_add_button(MENU_TEXT, pagina,
                         tasti_barra_lettore[disposizione_della_pagina()]);
    }
    ui_menu_add_button(MENU_TEXT, pagina, "ALT 1 2 3 4  whole SID on/off");

    ui_menu_add_button(MENU_TEXT, pagina, "Up Down, ALT + -  volume");





    ui_menu_add_button(MENU_TEXT, pagina, "8-SID tunes: F1-F8 side of SID 1-8");
    ui_menu_add_button(MENU_TEXT, pagina, "ALT 1-8 whole SID, no voice keys");
  }





  if (emux_machine_class != BMC64_MACHINE_CLASS_C64 || is_composite()) {
    ui_menu_add_divider(pagina);
  }



  ui_menu_add_button(MENU_TEXT, pagina,
                     tasti_spegni[circle_ha_il_tasto_power() ? 1 : 0]);
  ui_menu_add_button(MENU_TEXT, pagina, "ALT+ESC twice: reboot. ESC: cancel");
  ui_menu_add_button(MENU_TEXT, pagina, "Menu key and F-key hotkeys: Prefs.");
}

static void menu_value_changed(struct menu_item *item) {
  struct machine_entry* head;
  int status = 0;
  int p;

  switch (item->id) {
  case MENU_ATTACH_DISK_8:
  case MENU_IECDEVICE_8:
  case MENU_IECDIR_8:
  case MENU_DRIVE_CHANGE_MODEL_8:
  case MENU_PARALLEL_8:
  case MENU_CMDHD_MODE_8:
    unit = 8;
    break;
  case MENU_ATTACH_DISK_9:
  case MENU_IECDEVICE_9:
  case MENU_IECDIR_9:
  case MENU_DRIVE_CHANGE_MODEL_9:
  case MENU_PARALLEL_9:
  case MENU_CMDHD_MODE_9:
    unit = 9;
    break;
  case MENU_ATTACH_DISK_10:
  case MENU_IECDEVICE_10:
  case MENU_IECDIR_10:
  case MENU_DRIVE_CHANGE_MODEL_10:
  case MENU_PARALLEL_10:
  case MENU_CMDHD_MODE_10:
    unit = 10;
    break;
  case MENU_ATTACH_DISK_11:
  case MENU_IECDEVICE_11:
  case MENU_IECDIR_11:
  case MENU_DRIVE_CHANGE_MODEL_11:
  case MENU_PARALLEL_11:
  case MENU_CMDHD_MODE_11:
    unit = 11;
    break;
  }

  if (emux_handle_menu_change(item)) {
    return;
  }

  switch (item->id) {
  case MENU_SAVE_SETTINGS:
    if (save_settings()) {
      ui_error("Problem saving");
    } else {
      ui_info("Settings saved");
    }
    return;
  case MENU_COLOR_PALETTE_0:
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    emux_change_palette(0, item->value);
    return;
  case MENU_COLOR_PALETTE_1:
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    emux_change_palette(1, item->value);
    return;
  case MENU_AUTOSTART_WARP:
    emux_set_int(Setting_AutostartWarp, item->value);
    return;
  case MENU_REALDRIVE_LOG:
    emux_set_real_drive_log(item->value);
    return;
  case MENU_REALDRIVE_RESPIRO:
    emux_set_real_drive_respiro(item->choice_ints[item->value]);
    return;
  case MENU_REALDRIVE_VELOCE:
    emux_set_real_drive_veloce(item->value);
    return;
  case MENU_REALDRIVE_RECOVER: {





    int v[9];
    int quanti = emux_real_drive_recover(8, v, 9);
    static const char *chi[6] = { "no answer", "1541", "1570", "1571",
                                  "1581", "something else" };
    struct menu_item *pagina = ui_push_menu(-1, -1);
    char riga[48];

    ui_menu_add_button(MENU_TEXT, pagina, "Reset drive on XUM1541");
    ui_menu_add_divider(pagina);

    if (quanti < 9 || !v[0]) {
      ui_menu_add_button(MENU_TEXT, pagina, "No adapter plugged in.");
      return;
    }

    snprintf(riga, 47, "Turbo session closed   %s", v[1] ? "yes" : "none");
    ui_menu_add_button(MENU_TEXT, pagina, riga);
    snprintf(riga, 47, "Flags cleared          %s", v[2] ? "yes" : "no");
    ui_menu_add_button(MENU_TEXT, pagina, riga);
    snprintf(riga, 47, "Adapter reopened       %s", v[3] ? "yes" : "NO");
    ui_menu_add_button(MENU_TEXT, pagina, riga);
    snprintf(riga, 47, "Bus reset              %s", v[4] ? "yes" : "NO");
    ui_menu_add_button(MENU_TEXT, pagina, riga);
    snprintf(riga, 47, "Drive answers          %d", v[5]);
    ui_menu_add_button(MENU_TEXT, pagina, riga);
    snprintf(riga, 47, "Took                   %d ms", v[6]);
    ui_menu_add_button(MENU_TEXT, pagina, riga);
    snprintf(riga, 47, "The drive looks like a %s",
             chi[(v[8] >= 0 && v[8] <= 5) ? v[8] : 5]);
    ui_menu_add_button(MENU_TEXT, pagina, riga);

    ui_menu_add_divider(pagina);
    if (v[7]) {
      ui_menu_add_button(MENU_TEXT, pagina, "It is back. 73 is what a 1541");
      ui_menu_add_button(MENU_TEXT, pagina, "says after a reset.");
    } else {
      ui_menu_add_button(MENU_TEXT, pagina, "Still not answering. Check the");
      ui_menu_add_button(MENU_TEXT, pagina, "cable and that the drive is on.");
    }
    return;
  }










  case MENU_REALDRIVE_TURBO: {











    char nome[24];
    char dove[40];



    int v[17] = {0};
    int quanti = emux_real_drive_turbo(8, nome, (int)sizeof(nome),
                                       dove, (int)sizeof(dove), v, 17);
    struct menu_item *pagina = ui_push_menu(-1, -1);
    char riga[48];

    ui_menu_add_button(MENU_TEXT, pagina, "Real drive - the fast loader");
    ui_menu_add_divider(pagina);

    if (quanti < 10) {
      ui_menu_add_button(MENU_TEXT, pagina, "No adapter answered.");
    } else if (v[0] == 5) {
      ui_menu_add_button(MENU_TEXT, pagina, "No adapter plugged in, or the");
      ui_menu_add_button(MENU_TEXT, pagina, "unit is not set to Real drive.");
    } else if (v[0] == 1) {
      ui_menu_add_button(MENU_TEXT, pagina, "No directory - is there a");
      ui_menu_add_button(MENU_TEXT, pagina, "disk in the drive?");
      snprintf(riga, 47, "Stopped at: %s", dove);
      ui_menu_add_button(MENU_TEXT, pagina, riga);
    } else {
      snprintf(riga, 47, "First file: %s", nome);
      ui_menu_add_button(MENU_TEXT, pagina, riga);
      ui_menu_add_divider(pagina);

      snprintf(riga, 47, "Fast   %6d B  %5d ms  %5d B/s", v[1], v[2], v[3]);
      ui_menu_add_button(MENU_TEXT, pagina, riga);
      snprintf(riga, 47, "Usual  %6d B  %5d ms  %5d B/s", v[4], v[5], v[6]);
      ui_menu_add_button(MENU_TEXT, pagina, riga);

      if (v[3] > 0 && v[6] > 0) {
        snprintf(riga, 47, "That is %d times faster", v[3] / v[6]);
        ui_menu_add_button(MENU_TEXT, pagina, riga);
      }
      snprintf(riga, 47, "Blocks read       %6d", v[8]);
      ui_menu_add_button(MENU_TEXT, pagina, riga);






      snprintf(riga, 47, "ms per block      %6d  (95 = at the", v[7]);
      ui_menu_add_button(MENU_TEXT, pagina, riga);
      ui_menu_add_button(MENU_TEXT, pagina, "                          ceiling)");
      snprintf(riga, 47, "Pause used        %6d us", v[10]);
      ui_menu_add_button(MENU_TEXT, pagina, riga);





      if (quanti >= 17) {
        snprintf(riga, 47, "  waiting drive   %6d ms", v[13]);
        ui_menu_add_button(MENU_TEXT, pagina, riga);
        snprintf(riga, 47, "  count byte      %6d ms", v[14]);
        ui_menu_add_button(MENU_TEXT, pagina, riga);
        snprintf(riga, 47, "  the 254 bytes   %6d ms", v[15]);
        ui_menu_add_button(MENU_TEXT, pagina, riga);



        snprintf(riga, 47, "  our own turn    %6d ms", v[16]);
        ui_menu_add_button(MENU_TEXT, pagina, riga);
      }

      ui_menu_add_divider(pagina);
      if (v[0] == 0) {
        ui_menu_add_button(MENU_TEXT, pagina,
                           v[9] ? "Same bytes (first 4K checked)"
                                : "Same bytes both ways");
      } else if (v[0] == 2) {
        ui_menu_add_button(MENU_TEXT, pagina, "The fast loader did not run.");
        ui_menu_add_button(MENU_TEXT, pagina, "Nothing is left in the drive.");
      } else if (v[0] == 3) {
        ui_menu_add_button(MENU_TEXT, pagina, "The usual way failed instead.");
      } else {
        ui_menu_add_button(MENU_TEXT, pagina, "*** THE BYTES DIFFER ***");
      }



      snprintf(riga, 47, "Got as far as: %d", v[11]);
      ui_menu_add_button(MENU_TEXT, pagina, riga);
      snprintf(riga, 47, "%s", dove);
      ui_menu_add_button(MENU_TEXT, pagina, riga);
    }

    return;
  }
  case MENU_AUTOSTART:


    show_files(DIR_PRGS, FILTER_NONE, MENU_AUTOSTART_FILE, 0);
    return;
  case MENU_LOADPRG:
    show_files(DIR_PRGS, FILTER_PRGS, MENU_LOADPRG_FILE, 0);
    return;
  case MENU_SAVE_SNAP:
    show_files(DIR_SNAPS, FILTER_SNAP, MENU_SAVE_SNAP_FILE, 0);
    return;
  case MENU_LOAD_SNAP:
    show_files(DIR_SNAPS, FILTER_SNAP, MENU_LOAD_SNAP_FILE, 0);
    return;
  case MENU_CREATE_D64:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D64_FILE, 0);
    return;
  case MENU_CREATE_D67:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D67_FILE, 0);
    return;
  case MENU_CREATE_D71:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D71_FILE, 0);
    return;
  case MENU_CREATE_D80:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D80_FILE, 0);
    return;
  case MENU_CREATE_D81:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D81_FILE, 0);
    return;
  case MENU_CREATE_D82:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D82_FILE, 0);
    return;
  case MENU_CREATE_D1M:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D1M_FILE, 0);
    return;
  case MENU_CREATE_D2M:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D2M_FILE, 0);
    return;
  case MENU_CREATE_D4M:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_D4M_FILE, 0);
    return;
  case MENU_CREATE_G64:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_G64_FILE, 0);
    return;
  case MENU_CREATE_G71:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_G71_FILE, 0);
    return;
  case MENU_CREATE_P64:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_P64_FILE, 0);
    return;
  case MENU_CREATE_X64:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_X64_FILE, 0);
    return;
  case MENU_CREATE_DHD:
    show_files(DIR_DISKS, FILTER_NONE, MENU_CREATE_DHD_FILE, 0);
    return;
  case MENU_CREATE_TAP:
    show_files(DIR_TAPES, FILTER_NONE, MENU_CREATE_TAP_FILE, 0);
    return;

  case MENU_IECDEVICE_8:
  case MENU_IECDEVICE_9:
  case MENU_IECDEVICE_10:
  case MENU_IECDEVICE_11:
    emux_set_int_1(Setting_IECDeviceN, item->value, unit);


    emux_refresh_drive_items(unit);
    return;
  case MENU_PARALLEL_8:
  case MENU_PARALLEL_9:
  case MENU_PARALLEL_10:
  case MENU_PARALLEL_11:
    emux_set_int_1(Setting_DriveNParallelCable,
       item->choice_ints[item->value], unit);
    return;
  case MENU_CMDHD_MODE_8:
  case MENU_CMDHD_MODE_9:
  case MENU_CMDHD_MODE_10:
  case MENU_CMDHD_MODE_11:
    emux_set_int_1(Setting_DriveNCMDHDMode,
       item->choice_ints[item->value], unit);
    return;
  case MENU_IECDIR_8:
  case MENU_IECDIR_9:
  case MENU_IECDIR_10:
  case MENU_IECDIR_11:
    show_files(DIR_IEC, FILTER_DIRS, MENU_IEC_DIR, 0);
    return;
  case MENU_ATTACH_DISK_8:
  case MENU_ATTACH_DISK_9:
  case MENU_ATTACH_DISK_10:
  case MENU_ATTACH_DISK_11:
    show_files(DIR_DISKS, FILTER_DISK, MENU_DISK_FILE, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_1541:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_1541, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_1541II:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_1541II, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_1551:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_1551, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_1571:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_1571, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_1581:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_1581, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_CMDHD:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_CMDHD, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_2031:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_2031, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_2040:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_2040, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_3040:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_3040, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_4040:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_4040, 0);
    return;
  case MENU_DRIVE_CHANGE_ROM_1001:
    show_files(DIR_ROMS, FILTER_NONE, MENU_DRIVE_ROM_FILE_1001, 0);
    return;
  case MENU_SID_PLAYER:
    show_files(DIR_SIDS, FILTER_SID, MENU_SID_FILE, 0);
    return;
  case MENU_SID_NEXT:
  case MENU_SID_PREV:
    if (emux_sid_quanti_brani() <= 0) {
      overlay_avviso("NO SID LOADED");
      return;
    }
    emux_sid_brano(item->id == MENU_SID_NEXT ? 1 : -1);
    sid_aggiorna_righe();
    {
      char avviso[MAX_MENU_STR];
      snprintf(avviso, sizeof(avviso), "TUNE %d/%d",
               emux_sid_quale_brano(), emux_sid_quanti_brani());
      overlay_avviso(avviso);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_SID_STOP:
    emux_sid_ferma();
    sid_aggiorna_righe();
    overlay_avviso("SID PLAYER OFF");
    ui_pop_all_and_toggle();
    return;
  case MENU_ATTACH_TAPE:
    show_files(DIR_TAPES, FILTER_TAPE, MENU_TAPE_FILE, 0);
    return;
  case MENU_C64_ATTACH_CART:
    show_files(DIR_CARTS, FILTER_CART, MENU_C64_CART_FILE, 0);
    return;
  case MENU_C64_ATTACH_CART_8K:
    show_files(DIR_CARTS, FILTER_NONE, MENU_C64_CART_8K_FILE, 0);
    return;
  case MENU_C64_ATTACH_CART_16K:
    show_files(DIR_CARTS, FILTER_NONE, MENU_C64_CART_16K_FILE, 0);
    return;
  case MENU_C64_ATTACH_CART_ULTIMAX:
    show_files(DIR_CARTS, FILTER_NONE, MENU_C64_CART_ULTIMAX_FILE, 0);
    return;
  case MENU_REU_ATTACH_IMAGE:
    show_files(DIR_REU, FILTER_REU, MENU_REU_ATTACH_IMAGE_FILE, 0);
    return;
  case MENU_REU_SAVE_IMAGE_AS:
    show_files(DIR_REU, FILTER_NONE, MENU_REU_SAVE_IMAGE_AS_FILE, 0);
    return;
  case MENU_IDE64_IMAGE_1:
    show_files(DIR_DISKS, FILTER_IDE64, MENU_IDE64_IMAGE_1_FILE, 0);
    return;
  case MENU_IDE64_IMAGE_2:
    show_files(DIR_DISKS, FILTER_IDE64, MENU_IDE64_IMAGE_2_FILE, 0);
    return;
  case MENU_IDE64_IMAGE_3:
    show_files(DIR_DISKS, FILTER_IDE64, MENU_IDE64_IMAGE_3_FILE, 0);
    return;
  case MENU_IDE64_IMAGE_4:
    show_files(DIR_DISKS, FILTER_IDE64, MENU_IDE64_IMAGE_4_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_DETECT:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_DETECT_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_GENERIC:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_GENERIC_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_16K_2000:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_16K_2000_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_16K_4000:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_16K_4000_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_16K_6000:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_16K_6000_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_8K_A000:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_8K_A000_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_4K_B000:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_4K_B000_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_BEHRBONZ:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_BEHRBONZ_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_UM:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_UM_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_FP:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_FP_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_MEGACART:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_MEGACART_FILE, 0);
    return;
  case MENU_VIC20_ATTACH_CART_FINAL_EXPANSION:
    show_files(DIR_CARTS, FILTER_NONE, MENU_VIC20_CART_FINAL_EXPANSION_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART:
    show_files(DIR_CARTS, FILTER_CART, MENU_PLUS4_CART_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART_C0_LO:
    show_files(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C0_LO_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART_C0_HI:
    show_files(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C0_HI_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART_C1_LO:
    show_files(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C1_LO_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART_C1_HI:
    show_files(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C1_HI_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART_C2_LO:
    show_files(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C2_LO_FILE, 0);
    return;
  case MENU_PLUS4_ATTACH_CART_C2_HI:
    show_files(DIR_CARTS, FILTER_NONE, MENU_PLUS4_CART_C2_HI_FILE, 0);
    return;
  case MENU_LOAD_KERNAL:
    show_files(DIR_ROMS, FILTER_NONE, MENU_KERNAL_FILE, 0);
    return;
  case MENU_LOAD_BASIC:
    show_files(DIR_ROMS, FILTER_NONE, MENU_BASIC_FILE, 0);
    return;
  case MENU_LOAD_CHARGEN:
    show_files(DIR_ROMS, FILTER_NONE, MENU_CHARGEN_FILE, 0);
    return;
  case MENU_C128_LOAD_KERNAL:
    show_files(DIR_ROMS, FILTER_NONE, MENU_C128_LOAD_KERNAL_FILE, 0);
    return;
  case MENU_C128_LOAD_BASIC_HI:
    show_files(DIR_ROMS, FILTER_NONE, MENU_C128_LOAD_BASIC_HI_FILE, 0);
    return;
  case MENU_C128_LOAD_BASIC_LO:
    show_files(DIR_ROMS, FILTER_NONE, MENU_C128_LOAD_BASIC_LO_FILE, 0);
    return;
  case MENU_C128_LOAD_CHARGEN:
    show_files(DIR_ROMS, FILTER_NONE, MENU_C128_LOAD_CHARGEN_FILE, 0);
    return;
  case MENU_C128_LOAD_64_KERNAL:
    show_files(DIR_ROMS, FILTER_NONE, MENU_C128_LOAD_64_KERNAL_FILE, 0);
    return;
  case MENU_C128_LOAD_64_BASIC:
    show_files(DIR_ROMS, FILTER_NONE, MENU_C128_LOAD_64_BASIC_FILE, 0);
    return;
  case MENU_MAKE_CART_DEFAULT:


    emux_set_cart_default();
    if (save_settings()) {
      ui_error("Problem saving");
    }
    return;
  case MENU_DETACH_DISK_8:
    ui_info("Deatching...");
    emux_detach_disk(8);
    attached_disk_name[0][0] = '\0';
    ui_pop_all_and_toggle();
    return;
  case MENU_DETACH_DISK_9:
    ui_info("Detaching...");
    emux_detach_disk(9);
    attached_disk_name[1][0] = '\0';
    ui_pop_all_and_toggle();
    return;
  case MENU_DETACH_DISK_10:
    ui_info("Detaching...");
    emux_detach_disk(10);
    attached_disk_name[2][0] = '\0';
    ui_pop_all_and_toggle();
    return;
  case MENU_DETACH_DISK_11:
    ui_info("Detaching...");
    emux_detach_disk(11);
    attached_disk_name[3][0] = '\0';
    ui_pop_all_and_toggle();
    return;
  case MENU_DETACH_TAPE:
    ui_info("Detaching...");
    emux_detach_tape();
    ui_pop_all_and_toggle();
    return;
  case MENU_DETACH_CART:
    ui_info("Detaching...");
    emux_sid_lascia_la_porta();
    emux_detach_cart(0);
    ui_pop_all_and_toggle();
    return;
  case MENU_PLUS4_DETACH_CART_C0_LO:
    ui_info("Detaching...");
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C0_LO);
    ui_pop_all_and_toggle();
    return;
  case MENU_PLUS4_DETACH_CART_C0_HI:
    ui_info("Detaching...");
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C0_HI);
    ui_pop_all_and_toggle();
    return;
  case MENU_PLUS4_DETACH_CART_C1_LO:
    ui_info("Detaching...");
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C1_LO);
    ui_pop_all_and_toggle();
    return;
  case MENU_PLUS4_DETACH_CART_C1_HI:
    ui_info("Detaching...");
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C1_HI);
    ui_pop_all_and_toggle();
    return;
  case MENU_PLUS4_DETACH_CART_C2_LO:
    ui_info("Detaching...");
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C2_LO);
    ui_pop_all_and_toggle();
    return;
  case MENU_PLUS4_DETACH_CART_C2_HI:
    ui_info("Detaching...");
    emux_detach_cart(MENU_PLUS4_DETACH_CART_C2_HI);
    ui_pop_all_and_toggle();
    return;
  case MENU_SOFT_RESET:
    menu_machine_reset(1 /* soft */, 1 /* pop */);
    return;
  case MENU_HARD_RESET:
    menu_machine_reset(0 /* hard */, 1 /* pop */);
    return;
  case MENU_SHORTCUTS: {
    menu_pagina_tasti_rapidi();
    return;
  }
  case MENU_TASTI_PAGINA_2:
    menu_pagina_tasti_rapidi_2();
    return;
  case MENU_TASTI_PAGINA_1:


    ui_pop_menu();
    return;
  case MENU_ABOUT:
    show_about();
    return;
  case MENU_LICENSE:
    show_license();
    return;
  case MENU_FONT_TEST:
    show_font_test();
    return;
  case MENU_USB_0_CONFIGURE:
  case MENU_USB_1_CONFIGURE:
  case MENU_USB_2_CONFIGURE:
  case MENU_USB_3_CONFIGURE:
    configure_usb(item->id - MENU_USB_0_CONFIGURE);
    return;
  case MENU_CONFIGURE_KEYSET1:
    configure_keyset(0);
    return;
  case MENU_CONFIGURE_KEYSET2:
    configure_keyset(1);
    return;
  case MENU_CONFIGURE_GPIO:
    configure_gpio();
    return;
  case MENU_GPIO_CONFIG:
    // Ensure GPIO pins are correct for new mode.
    circle_reset_gpio(emu_get_gpio_config());
    return;
  case MENU_NETWORKING:
    menu_update_network_status();
    return;
  case MENU_NETWORK_ENABLED:
    update_wifi_menu_enabled();
    network_reboot_prompted = 0;
    return;
  case MENU_NETWORK_MODEM_ADDRESS:
    if (!circle_set_acia_network_address(
            acia_network_addresses[item->value])) {
      item->value = acia_network_address_index(
          circle_get_acia_network_address());
      ui_error("Cannot set modem address");
    }
    return;
  case MENU_WIFI_SSID:
    if (!circle_has_onboard_wifi()) {
      ui_error("This Raspberry Pi has no WiFi");
      return;
    }
    ui_info("Scanning WiFi networks...");
    show_wifi_access_points(wifi_ssid_item, wifi_security_item);
    return;
  case MENU_WIFI_CONNECT:
    show_wifi_connect_dialog();
    return;
  case MENU_WIFI_COUNTRY:
    wifi_paese_cambiato();
    return;
  case MENU_WIFI_PSK:
    strncpy(wifi_psk, item->str_value, sizeof(wifi_psk) - 1);
    wifi_psk[sizeof(wifi_psk) - 1] = '\0';
    return;
  case MENU_WIFI_CONNECT_NOW:
    strncpy(wifi_psk, wifi_psk_item->str_value, sizeof(wifi_psk) - 1);
    wifi_psk[sizeof(wifi_psk) - 1] = '\0';
    if (save_wifi_settings() != 0) {
      ui_error("Cannot save WiFi settings");
    } else {
      if (network_device_item != NULL) {
        network_device_item->value = 2;
      }
      if (save_settings() != 0) {
        ui_error("Cannot save settings");
        return;
      }
      riavvia();
    }
    return;
  case MENU_NETWORK_ADDRESS_MODE:
    update_network_address_items();
    return;
  case MENU_SD_SHARING:
    condividi_scheda();
    return;
  case MENU_NAS_IP:
  case MENU_NAS_USER:
  case MENU_NAS_PASSWORD:
  case MENU_NAS_PROTOCOL:
    nas_applica();
    return;
  case MENU_NAS_PATH:


    nas_applica();
    if (strcmp(current_volume_name, "NAS:") == 0) {
      for (int t = 0; t < NUM_DIR_TYPES; t++) {
        strcpy(current_dir_names[t], nas_cartella_macchina());
      }
    }
    return;
  case MENU_NAS_TEST:
    nas_prova();
    return;
  case MENU_SD_SHARING_STOP:
    ui_pop_menu();
    return;
  case MENU_NETWORK_MODEM:
    if (!circle_set_acia_network_enabled(item->value)) {
      item->value = circle_get_acia_network_enabled() ? 1 : 0;
      ui_error("Cannot switch the modem");
    }
    return;
  case MENU_WARP_MODE:
    toggle_warp(item->value);
    return;
  case MENU_DEMO_MODE:
    raspi_demo_mode = item->value;
    demo_reset();
    return;
  case MENU_DRIVE_SOUND_EMULATION:
    emux_set_int(Setting_DriveSoundEmulation, item->value);
    return;
  case MENU_DRIVE_SOUND_EMULATION_VOLUME:
    emux_set_int(Setting_DriveSoundEmulationVolume, item->value);
    return;
  case MENU_TAPE_SOUND_EMULATION:
    emux_set_int(Setting_TapeSoundEmulation, item->value);
    return;
  case MENU_TAPE_SOUND_EMULATION_VOLUME:
    emux_set_int(Setting_TapeSoundEmulationVolume, item->value);
    return;
  case MENU_COLOR_BRIGHTNESS_0:
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    emux_set_color_brightness(0, item->value);
    emux_video_color_setting_changed(0);
    return;
  case MENU_COLOR_CONTRAST_0:
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    emux_set_color_contrast(0, item->value);
    emux_video_color_setting_changed(0);
    return;
  case MENU_COLOR_GAMMA_0:
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    emux_set_color_gamma(0, item->value);
    emux_video_color_setting_changed(0);
    return;
  case MENU_COLOR_TINT_0:
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    emux_set_color_tint(0, item->value);
    emux_video_color_setting_changed(0);
    return;
  case MENU_COLOR_SATURATION_0:
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    emux_set_color_saturation(0, item->value);
    emux_video_color_setting_changed(0);
    return;
  case MENU_COLOR_RESET_0:
    emux_get_default_color_setting(
      &brightness_item[0]->value,
      &contrast_item[0]->value,
      &gamma_item[0]->value,
      &tint_item[0]->value,
      &saturation_item[0]->value
    );
    emux_set_color_brightness(0, brightness_item[0]->value);
    emux_set_color_contrast(0, contrast_item[0]->value);
    emux_set_color_gamma(0, gamma_item[0]->value);
    emux_set_color_tint(0, tint_item[0]->value);
    emux_set_color_saturation(0, saturation_item[0]->value);
    emux_video_color_setting_changed(0);
    return;
  case MENU_COLOR_BRIGHTNESS_1:
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    emux_set_color_brightness(1, item->value);
    emux_video_color_setting_changed(1);
    return;
  case MENU_COLOR_CONTRAST_1:
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    emux_set_color_contrast(1, item->value);
    emux_video_color_setting_changed(1);
    return;
  case MENU_COLOR_GAMMA_1:
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    emux_set_color_gamma(1, item->value);
    emux_video_color_setting_changed(1);
    return;
  case MENU_COLOR_TINT_1:
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    emux_set_color_tint(1, item->value);
    emux_video_color_setting_changed(1);
    return;
  case MENU_COLOR_SATURATION_1:
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    emux_set_color_saturation(1, item->value);
    emux_video_color_setting_changed(1);
    return;
  case MENU_COLOR_RESET_1:
    emux_get_default_color_setting(
      &brightness_item[1]->value,
      &contrast_item[1]->value,
      &gamma_item[1]->value,
      &tint_item[1]->value,
      &saturation_item[1]->value
    );
    emux_set_color_brightness(1, brightness_item[1]->value);
    emux_set_color_contrast(1, contrast_item[1]->value);
    emux_set_color_gamma(1, gamma_item[1]->value);
    emux_set_color_tint(1, tint_item[1]->value);
    emux_set_color_saturation(1, saturation_item[1]->value);
    emux_video_color_setting_changed(1);
    return;
  case MENU_SWAP_JOYSTICKS:
    menu_swap_joysticks();
    return;
  case MENU_JOYSTICK_PORT_1:
  case MENU_JOYSTICK_PORT_2:
  case MENU_JOYSTICK_PORT_3:
  case MENU_JOYSTICK_PORT_4:
    p = item->id - MENU_JOYSTICK_PORT_1 + 1;
    set_joy_item_to_value(p, item->choice_ints[item->value]);
    set_need_mouse();
    return;
  case MENU_SPINNER_SENSITIVITY:
    emu_spinner_set_sensitivity(item->choice_ints[item->value]);
    return;
  case MENU_TAPE_START:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_PLAY);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_STOP:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_STOP);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_REWIND:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_REWIND);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_FASTFWD:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_FASTFORWARD);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_RECORD:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_RECORD);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_RESET:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_RESET);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_RESET_COUNTER:
    if (menu_nastro_pronto()) {
      emux_tape_control(EMUX_TAPE_ZERO);
    }
    ui_pop_all_and_toggle();
    return;
  case MENU_TAPE_RESET_WITH_MACHINE:
    emux_set_int(Setting_DatasetteResetWithCPU,
                      tape_reset_with_machine_item->value);
    return;
  case MENU_DRIVE_CHANGE_MODEL_8:
  case MENU_DRIVE_CHANGE_MODEL_9:
  case MENU_DRIVE_CHANGE_MODEL_10:
  case MENU_DRIVE_CHANGE_MODEL_11:
    emux_drive_change_model(unit);
    return;
  case MENU_DRIVE_CHANGE_ROM:
    drive_change_rom();
    return;
  case MENU_DRIVE_MODEL_SELECT:



    emux_leave_real_drive(unit);



    emux_use_emulated_drive(unit);
    emux_set_int_1(Setting_DriveNType, item->value, unit);
    emux_refresh_drive_items(unit);
    ui_pop_all_and_toggle();
    return;
  case MENU_DRIVE_MODEL_REAL:


    if (!emux_real_drive_available()) {
      overlay_avviso("NO XUM FOUND");
      ui_pop_all_and_toggle();
      return;
    }
    emux_want_real_drive(unit);
    emux_refresh_drive_items(unit);
    ui_pop_all_and_toggle();
    return;
  case MENU_CALC_TIMING:
    configure_timing();
    return;
  case MENU_HOTKEY_CF1:
    kbd_set_hotkey_function(
        0, KEYCODE_F1, hotkey_cf1_item->choice_ints[hotkey_cf1_item->value]);
    return;
  case MENU_HOTKEY_CF3:
    kbd_set_hotkey_function(
        1, KEYCODE_F3, hotkey_cf3_item->choice_ints[hotkey_cf3_item->value]);
    return;
  case MENU_HOTKEY_CF5:
    kbd_set_hotkey_function(
        2, KEYCODE_F5, hotkey_cf5_item->choice_ints[hotkey_cf5_item->value]);
    return;
  case MENU_HOTKEY_CF7:
    kbd_set_hotkey_function(
        3, KEYCODE_F7, hotkey_cf7_item->choice_ints[hotkey_cf7_item->value]);
    return;
  case MENU_HOTKEY_TF1:
    kbd_set_hotkey_function(
        4, KEYCODE_F1, hotkey_tf1_item->choice_ints[hotkey_tf1_item->value]);
    return;
  case MENU_HOTKEY_TF3:
    kbd_set_hotkey_function(
        5, KEYCODE_F3, hotkey_tf3_item->choice_ints[hotkey_tf3_item->value]);
    return;
  case MENU_HOTKEY_TF5:
    kbd_set_hotkey_function(
        6, KEYCODE_F5, hotkey_tf5_item->choice_ints[hotkey_tf5_item->value]);
    return;
  case MENU_HOTKEY_TF7:
    kbd_set_hotkey_function(
        7, KEYCODE_F7, hotkey_tf7_item->choice_ints[hotkey_tf7_item->value]);
    return;
  case MENU_VIC20_MEMORY_3K:
    emux_set_int(Setting_RAMBlock0, item->value);
    return;
  case MENU_VIC20_MEMORY_8K_2000:
    emux_set_int(Setting_RAMBlock1, item->value);
    return;
  case MENU_VIC20_MEMORY_8K_4000:
    emux_set_int(Setting_RAMBlock2, item->value);
    return;
  case MENU_VIC20_MEMORY_8K_6000:
    emux_set_int(Setting_RAMBlock3, item->value);
    return;
  case MENU_VIC20_MEMORY_8K_A000:
    emux_set_int(Setting_RAMBlock5, item->value);
    return;
  case MENU_SECONDO_HDMI:
    due_hdmi_nel_giro();


    if (active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI) {
       circle_secondo_schermo(-1, 0);
    }
    // fallthrough
  case MENU_ACTIVE_DISPLAY:
  case MENU_PIP_LOCATION:
  case MENU_PIP_SWAPPED:

    if (active_display_item->value != MENU_ACTIVE_DISPLAY_DUE_HDMI) {
       circle_secondo_schermo(-1, 0);
       doppio_hdmi_cmdline(0);
    }
    if (active_display_item->value == MENU_ACTIVE_DISPLAY_VICII) {
       vic_enabled = 1;
       vdc_enabled = 0;
       do_video_settings(FB_LAYER_VIC);
    } else if (active_display_item->value == MENU_ACTIVE_DISPLAY_VDC) {
       vdc_enabled = 1;
       vic_enabled = 0;
       do_video_settings(FB_LAYER_VDC);
    } else if (active_display_item->value == MENU_ACTIVE_DISPLAY_SIDE_BY_SIDE ||
               active_display_item->value == MENU_ACTIVE_DISPLAY_PIP) {
       vdc_enabled = 1;
       vic_enabled = 1;
       do_video_settings(FB_LAYER_VIC);
       do_video_settings(FB_LAYER_VDC);
    } else if (active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI &&
               secondo_hdmi_item != NULL && !secondo_hdmi_item->value) {






       printf("[VID] Two HDMI con Second Display Present spento: "
              "resto sul VIC-II\n");
       overlay_avviso("SECOND DISPLAY PRESENT: OFF");
       active_display_item->value = MENU_ACTIVE_DISPLAY_VICII;
       circle_secondo_schermo(-1, 0);
       doppio_hdmi_cmdline(0);
       vic_enabled = 1;
       vdc_enabled = 0;
       do_video_settings(FB_LAYER_VIC);
    } else if (active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI) {



       int esito_due = circle_secondo_schermo(FB_LAYER_VDC,
               secondo_hdmi_item ? secondo_hdmi_item->value : 0);







       if (esito_due == -2) {





          printf("[VID] due HDMI: lo schermo e' dello scalatore, serve il "
                 "riavvio (%s)\n", display_all_avvio ? "all'avvio" : "dal menu");
          active_display_item->value = MENU_ACTIVE_DISPLAY_VICII;
          vic_enabled = 1;
          vdc_enabled = 0;
          do_video_settings(FB_LAYER_VIC);
          if (display_all_avvio) {
             doppio_hdmi_riavvia(0);
          } else {
             ui_confirm_wrapped("Reboot?", DUE_HDMI_MSG,
                                MENU_ACTIVE_DISPLAY_DUE_HDMI,
                                MENU_ACTIVE_DISPLAY);
          }
       } else if (esito_due != 1) {



          printf("[VID] la seconda HDMI non si e' aperta: torno sul VIC-II\n");
          overlay_avviso("ONLY ONE HDMI DISPLAY");
          active_display_item->value = MENU_ACTIVE_DISPLAY_VICII;
          vic_enabled = 1;
          vdc_enabled = 0;
          do_video_settings(FB_LAYER_VIC);
       } else {
          vdc_enabled = 1;
          vic_enabled = 1;
          do_video_settings(FB_LAYER_VIC);
          do_video_settings(FB_LAYER_VDC);
          doppio_hdmi_cmdline(1);
       }
    }
    printf("[VID] Active Display: %d\n", active_display_item->value);
    refresh_crt_shader_runtime();
    break;
  case MENU_INTEGER_SCALE_W_0:
    next_integer_scaling(FB_LAYER_VIC, VIC_INDEX, 0);
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    do_video_settings(FB_LAYER_VIC);
    break;
  case MENU_INTEGER_SCALE_H_0:
    next_integer_scaling(FB_LAYER_VIC, VIC_INDEX, 1);
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    do_video_settings(FB_LAYER_VIC);
    break;
  case MENU_INTEGER_SCALE_W_1:
    next_integer_scaling(FB_LAYER_VDC, VDC_INDEX, 0);
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    do_video_settings(FB_LAYER_VDC);
    break;
  case MENU_INTEGER_SCALE_H_1:
    next_integer_scaling(FB_LAYER_VDC, VDC_INDEX, 1);
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    do_video_settings(FB_LAYER_VDC);
    break;
  case MENU_ASPECT_43_0:
  case MENU_ASPECT_169_0:
  case MENU_ASPECT_43_1:
  case MENU_ASPECT_169_1: {
    int vdc = (item->id == MENU_ASPECT_43_1 ||
               item->id == MENU_ASPECT_169_1);
    int largo = (item->id == MENU_ASPECT_169_0 ||
                 item->id == MENU_ASPECT_169_1);
    metti_aspetto(vdc ? VDC_INDEX : VIC_INDEX, largo);
    return;
  }
  case MENU_H_CENTER_0:
  case MENU_V_CENTER_0:
  case MENU_H_BORDER_0:
  case MENU_V_BORDER_0:
  case MENU_H_STRETCH_0:
  case MENU_V_STRETCH_0:
    // Any manual adjustment to stretch, go back
    // to scaled dimensions.
    if (item->id == MENU_H_STRETCH_0 || item->id == MENU_H_BORDER_0) {
       use_h_integer_stretch[0] = 0;
       use_scaling_params_item[0]->value = 0;
    } else if (item->id == MENU_V_STRETCH_0 || item->id == MENU_V_BORDER_0) {
       use_v_integer_stretch[0] = 0;
       use_scaling_params_item[0]->value = 0;
    }
    ui_canvas_reveal_temp(FB_LAYER_VIC);
    do_video_settings(FB_LAYER_VIC);
    break;
  case MENU_H_CENTER_1:
  case MENU_V_CENTER_1:
  case MENU_H_BORDER_1:
  case MENU_V_BORDER_1:
  case MENU_H_STRETCH_1:
  case MENU_V_STRETCH_1:
    // Any manual adjustment to stretch, go back
    // to scaled dimensions.
    if (item->id == MENU_H_STRETCH_1 || item->id == MENU_H_BORDER_1) {
       use_h_integer_stretch[1] = 0;
       use_scaling_params_item[1]->value = 0;
    } else if (item->id == MENU_V_STRETCH_1 || item->id == MENU_V_BORDER_1) {
       use_v_integer_stretch[1] = 0;
       use_scaling_params_item[1]->value = 0;
    }
    ui_canvas_reveal_temp(FB_LAYER_VDC);
    do_video_settings(FB_LAYER_VDC);
    break;
  case MENU_OVERLAY:
    statusbar_forced = 0;
    if (item->value == OVERLAY_ALWAYS) {
      overlay_statusbar_enable();
    } else {
      overlay_statusbar_disable();
    }
    break;
  case MENU_OVERLAY_PADDING:
    overlay_change_padding(item->value);
    break;
  case MENU_OVERLAY_INFO:
    overlay_set_info_line(item->value);
    return;
  case MENU_SWITCH_TIMER:
    if (item->value) {

      if (vidtrial_timer_set(1) != 0) {
        item->value = 0;
        ui_error("Cannot write the card");
      }
    } else {

      item->value = 1;
      ui_confirm_wrapped("WARNING!", SWITCH_TIMER_MSG, 0, MENU_SWITCH_TIMER);
    }
    return;
  case MENU_HDMI_PRINCIPALE:
  case MENU_AUDIO_OUT:




    if (!ui_value_changed_by_return()) {
      return;
    }
    item->value = ui_value_before_return();
    chiedi_riavvio_voce(item);
    return;
  case MENU_OVERCLOCK_MENU:
    show_overclock_menu();
    return;
  case MENU_OVERCLOCK:
    {
      int level = item->value;
      ui_pop_menu();


      ui_confirm_wrapped("Reboot?", OC_MSG, level, MENU_OVERCLOCK);
    }
    return;
  case MENU_GPUCLOCK_MENU:
    show_gpuclock_menu();
    return;
  case MENU_GPUCLOCK:
    {
      int level = item->value;
      ui_pop_menu();

      ui_confirm_wrapped("Reboot?", GPU_MSG, level, MENU_GPUCLOCK);
    }
    return;
  case MENU_VKBD_TRANSPARENCY:
    overlay_change_vkbd_transparency(item->value);
    break;
  case MENU_40_80_COLUMN:
    emux_set_int(Setting_C128ColumnKey, item->value);
    overlay_40_80_columns_changed(item->value);
    break;
  case MENU_VOLUME:
    menu_volume_ricorda(item->value);
    circle_set_volume(item->value);
    break;
  case MENU_SWITCH_MACHINE:
    ui_confirm_wrapped("Reboot?",SWITCH_MSG,item->value,MENU_SWITCH_MACHINE);
    break;
  case MENU_REBOOT_PI:
    ui_confirm_wrapped("Reboot?", REBOOTPI_MSG, 0, MENU_REBOOT_PI);
    break;
  case MENU_POWEROFF:
    ui_confirm_wrapped("Power Off?", POWEROFF_MSG, 0, MENU_POWEROFF);
    break;
  case MENU_CONFIRM_OK:
    int confirmation_id = item->sub_id;
    int confirmation_value = item->value;




    ui_pop_menu();
    if (confirmation_id == MENU_SWITCH_MACHINE) {
      load_machines(&head);
      struct machine_entry* ptr = head;
      status = 0;
      int trovata = 0;
      while (ptr) {
          if (ptr->id == confirmation_value) {
            status = switch_apply_files(ptr);
            trovata = 1;
            break;
          }
          ptr = ptr->next;
      }
      free_machines(head);



      if (status) {
         char failcode[32];
         sprintf (failcode, "FAILURE (CODE %d)", status);
         ui_confirm_wrapped(failcode, SWITCH_FAIL_MSG,-1,-1);
      } else {
         riavvia();
      }
    } else if (confirmation_id == MENU_HDMI_PRINCIPALE) {



      if (cmdline_opzione("fb_display",
                          confirmation_value ? "1" : NULL) != 0) {
        rimetti_voci_riavvio(-1);
        ui_error("Cannot write the card");
      } else {
        printf("[VID] Main HDMI Output: %s, riavvio\n",
               confirmation_value ? "HDMI 1" : "HDMI 0");
        riavvia();
      }
    } else if (confirmation_id == MENU_AUDIO_OUT) {
      if (audio_out_apply(confirmation_value) != 0) {
        rimetti_voci_riavvio(-1);
        ui_error("Cannot write cmdline.txt");
      } else {
        riavvia();
      }
    } else if (confirmation_id == MENU_OVERCLOCK) {
      if (overclock_apply(confirmation_value, circle_get_model()) != 0) {
        ui_error("Cannot write config.txt");
      } else {
        riavvia();
      }
    } else if (confirmation_id == MENU_GPUCLOCK) {
      if (gpuclock_apply(confirmation_value, circle_get_model()) != 0) {
        ui_error("Cannot write config.txt");
      } else {
        riavvia();
      }
    } else if (confirmation_id == MENU_REBOOT_PI) {
      riavvia();
    } else if (confirmation_id == MENU_POWEROFF) {
      spegni();
    } else if (confirmation_id == MENU_NETWORK_ENABLED) {
      if (save_settings() == 0) {
        riavvia();
      } else {
        ui_error("Cannot save settings");
      }
    } else if (confirmation_id == MENU_ACTIVE_DISPLAY) {

      doppio_hdmi_riavvia(1);
    } else if (confirmation_id == MENU_SWITCH_TIMER) {

      if (vidtrial_timer_set(0) != 0) {
        ui_error("Cannot write the card");
      } else if (switch_timer_item != NULL) {
        switch_timer_item->value = 0;
      }
    }
    break;
  case MENU_CONFIRM_CANCEL:
    ui_pop_menu();


    rimetti_voci_riavvio(-1);
    break;
  case MENU_DIR_CONVENTION:
    set_current_dir_names();
    break;
  case MENU_SHADER_ENABLE:
    reveal_crt_shader_preview();
    // Despite what the menu says, don't allow this to enable the shader
    // when conditions apply.



    passo_prima("shader: riallocazione");
    refresh_crt_shader_runtime();
    passo_fatto("shader: riallocazione");
    emux_set_int(Setting_VideoFilter, item->value ? MENU_VIDEO_FILTER_CRT : MENU_VIDEO_FILTER_NONE);
    passo_prima("shader: parametri");
    handle_shader_param_change();
    passo_fatto("shader: parametri");
    break;
  case MENU_SHADER_PRESET:
    if (item->value == CRT_PRESET_CURRENT_CHOICE) {
      s_crt_preset_applied_choice = CRT_PRESET_CURRENT_CHOICE;
      break;
    }
    reveal_crt_shader_preview();
    if (load_crt_preset_choice(item->value)) {
      handle_shader_param_change();
    } else {
      item->value = s_crt_preset_applied_choice;
      ui_error("Invalid CRT preset");
    }
    break;
  case MENU_SHADER_CURVATURE:
  case MENU_SHADER_CURVATURE_X:
  case MENU_SHADER_CURVATURE_Y:
  case MENU_SHADER_SKEW_X:
  case MENU_SHADER_SKEW_Y:
  case MENU_SHADER_TRAPEZOID:
  case MENU_SHADER_ROTATION:
  case MENU_SHADER_OVERSCAN:
  case MENU_SHADER_CONVERGENCE_ENABLE:
  case MENU_SHADER_RED_OFFSET_X:
  case MENU_SHADER_RED_OFFSET_Y:
  case MENU_SHADER_BLUE_OFFSET_X:
  case MENU_SHADER_BLUE_OFFSET_Y:
  case MENU_SHADER_CONVERGENCE_RADIAL_STRENGTH:
  case MENU_SHADER_HORIZONTAL_FILTERING:
  case MENU_SHADER_SIGMA_X:
  case MENU_SHADER_EDGE_BLUR_ENABLE:
  case MENU_SHADER_EDGE_BLUR_STRENGTH:
  case MENU_SHADER_EDGE_BLUR_RADIUS:
  case MENU_SHADER_SCANLINES:
  case MENU_SHADER_MULTISAMPLE:
  case MENU_SHADER_SCANLINE_WEIGHT:
  case MENU_SHADER_SCANLINE_GAP_BRIGHTNESS:
  case MENU_SHADER_MASK_ENABLE:
  case MENU_SHADER_MASK:
  case MENU_SHADER_MASK_BRIGHTNESS:
  case MENU_SHADER_BLOOM_ENABLE:
  case MENU_SHADER_BLOOM:
  case MENU_SHADER_VIGNETTE_ENABLE:
  case MENU_SHADER_VIGNETTE_STRENGTH:
  case MENU_SHADER_VIGNETTE_SCALE:
  case MENU_SHADER_VIGNETTE_SOFTNESS:
  case MENU_SHADER_UNEVEN_ILLUMINATION_ENABLE:
  case MENU_SHADER_UNEVEN_ILLUMINATION_STRENGTH:
  case MENU_SHADER_UNEVEN_ILLUMINATION_SCALE:
  case MENU_SHADER_HORIZONTAL_JITTER_ENABLE:
  case MENU_SHADER_HORIZONTAL_JITTER_STRENGTH:
  case MENU_SHADER_HORIZONTAL_JITTER_FREQUENCY:
  case MENU_SHADER_HORIZONTAL_JITTER_SPEED:
  case MENU_SHADER_COMPOSITE_ARTIFACTS_ENABLE:
  case MENU_SHADER_COMPOSITE_CHROMA_BLUR:
  case MENU_SHADER_COMPOSITE_LUMA_SHARPEN:
  case MENU_SHADER_COMPOSITE_COLOR_BLEED:
  case MENU_SHADER_GLASS_REFLECTION_ENABLE:
  case MENU_SHADER_GLASS_REFLECTION_ANGLE:
  case MENU_SHADER_GLASS_REFLECTION_WIDTH:
  case MENU_SHADER_GLASS_REFLECTION_POSITION:
  case MENU_SHADER_ROUNDED_SCREEN_MASK_ENABLE:
  case MENU_SHADER_ROUNDED_CORNER_RADIUS:
  case MENU_SHADER_ROUNDED_BORDER_SOFTNESS:
  case MENU_SHADER_EDGE_GLOW_ENABLE:
  case MENU_SHADER_EDGE_GLOW_STRENGTH:
  case MENU_SHADER_EDGE_GLOW_WIDTH:
  case MENU_SHADER_NOISE_ENABLE:
  case MENU_SHADER_LUMINANCE_NOISE:
  case MENU_SHADER_CHROMA_NOISE:
  case MENU_SHADER_NOISE_SPEED:
  case MENU_SHADER_OUTPUT_RESPONSE:
  case MENU_SHADER_GAMMA:
  case MENU_SHADER_LEVEL_MAPPING:
  case MENU_SHADER_INPUT_GAMMA:
  case MENU_SHADER_OUTPUT_GAMMA:
  case MENU_SHADER_SATURATION:
  case MENU_SHADER_BLACK_LEVEL:
  case MENU_SHADER_WHITE_CLIP:
    mark_crt_preset_modified();
    sanity_check_shader_params();
    reveal_crt_shader_preview();
    handle_shader_param_change();
    break;
  case MENU_SHADER_RESET_ALL:
    reveal_crt_shader_preview();
    reset_shader_params();
    mark_crt_preset_modified();
    sanity_check_shader_params();
    handle_shader_param_change();
    break;
  case MENU_USE_SCALING_PARAMS_0:
    if (item->value) {
       if (do_use_int_scaling(FB_LAYER_VIC, 0 /* not silent */)) {
          ui_canvas_reveal_temp(FB_LAYER_VIC);
          do_video_settings(FB_LAYER_VIC);
       } else {
          use_scaling_params_item[VIC_INDEX]->value = 0;
       }
    }
    break;
  case MENU_USE_SCALING_PARAMS_1:
    if (item->value) {
       if (do_use_int_scaling(FB_LAYER_VDC, 0 /* not silent */)) {
          ui_canvas_reveal_temp(FB_LAYER_VDC);
          do_video_settings(FB_LAYER_VDC);
       } else {
          use_scaling_params_item[VDC_INDEX]->value = 0;
       }
    }
    break;
  case MENU_SCALING_INTERPOLATION:
    reveal_crt_shader_preview();
    circle_set_interpolation(item->value); // dispmanx interpolation
    if (s_enable_shader_item->value) {
       sanity_check_shader_params();
       handle_shader_param_change();
    }
    break;
  }

  // Only items that were for file selection/nav should have these set...
  if (item->sub_id == MENU_SUB_PICK_FILE || item->sub_id == MENU_SUB_PICK_DIR) {
    ricerca_ricorda_scelto(item);
    select_file(item);
    return;
  } else if (item->sub_id == MENU_SUB_SEARCH) {
    ricerca_applica(item);
    return;
  } else if (item->sub_id == MENU_SUB_UP_DIR) {
    up_dir(item);
    return;
  } else if (item->sub_id == MENU_SUB_ENTER_DIR) {
    enter_dir(item);
    return;
  } else if (item->sub_id == MENU_SUB_SELECT_VOLUME) {
    filesystem_change_volume(item);
    return;
  } else if (item->sub_id == MENU_SUB_CHANGE_VOLUME) {



    if (item->value == MENU_VOLUME_NAS) {
      if (!nas_entra_nel_volume()) {
        return;
      }
    } else {
      nas_torna_alla_scheda();
    }
    switch (item->value) {
       case MENU_VOLUME_NAS:
           strcpy (current_volume_name, "NAS:");
           break;
       case MENU_VOLUME_SD:
           strcpy (current_volume_name, "SD:");
           break;
       case MENU_VOLUME_USB1:
           strcpy (current_volume_name, "USB:");
           if (!usb1_mounted) { usb1_mounted = circle_mount_usb(0); }
           break;
       case MENU_VOLUME_USB2:
           strcpy (current_volume_name, "USB2:");
           if (!usb2_mounted) { usb2_mounted = circle_mount_usb(1); }
           break;
       case MENU_VOLUME_USB3:
           strcpy (current_volume_name, "USB3:");
           if (!usb3_mounted) { usb3_mounted = circle_mount_usb(2); }
           break;
       case MENU_VOLUME_FLP1:
           strcpy (current_volume_name, "FLP1:");
           break;
       case MENU_VOLUME_FLP2:
           strcpy (current_volume_name, "FLP2:");
           break;
       default:
           break;
    }
    // Need to pop both change volume popup and old file list
    rilista_stessa_finestra(item, 2);
    return;
  }
}

// Returns what input preference user has for this usb device
void emu_get_usb_pref(int device, int *usb_pref_dst, int *x_axis, int *y_axis,
                      float *x_thresh, float *y_thresh) {
  *usb_pref_dst = usb_pref[device];
  *x_axis = usb_x_axis[device];
  *y_axis = usb_y_axis[device];
  *x_thresh = usb_x_thresh[device];
  *y_thresh = usb_y_thresh[device];
}

// KEEP in sync with kernel.cpp, kbd.c, menu_usb.c
static void set_hotkey_choices(struct menu_item *item) {
  item->num_choices = 16;
  strcpy(item->choices[HOTKEY_CHOICE_NONE], function_to_string(BTN_ASSIGN_UNDEF));
  strcpy(item->choices[HOTKEY_CHOICE_MENU], function_to_string(BTN_ASSIGN_MENU));
  strcpy(item->choices[HOTKEY_CHOICE_WARP], function_to_string(BTN_ASSIGN_WARP));
  strcpy(item->choices[HOTKEY_CHOICE_STATUS_TOGGLE], function_to_string(BTN_ASSIGN_STATUS_TOGGLE));
  strcpy(item->choices[HOTKEY_CHOICE_SWAP_PORTS], function_to_string(BTN_ASSIGN_SWAP_PORTS));
  strcpy(item->choices[HOTKEY_CHOICE_TAPE_MENU], function_to_string(BTN_ASSIGN_TAPE_MENU));
  strcpy(item->choices[HOTKEY_CHOICE_CART_MENU], function_to_string(BTN_ASSIGN_CART_MENU));
  strcpy(item->choices[HOTKEY_CHOICE_CART_FREEZE], function_to_string(BTN_ASSIGN_CART_FREEZE));
  strcpy(item->choices[HOTKEY_CHOICE_RESET_MENU], function_to_string(BTN_ASSIGN_RESET_MENU));
  strcpy(item->choices[HOTKEY_CHOICE_RESET_HARD], function_to_string(BTN_ASSIGN_RESET_HARD));
  strcpy(item->choices[HOTKEY_CHOICE_RESET_SOFT], function_to_string(BTN_ASSIGN_RESET_SOFT));
  strcpy(item->choices[HOTKEY_CHOICE_ACTIVE_DISPLAY], function_to_string(BTN_ASSIGN_ACTIVE_DISPLAY));
  strcpy(item->choices[HOTKEY_CHOICE_PIP_LOCATION], function_to_string(BTN_ASSIGN_PIP_LOCATION));
  strcpy(item->choices[HOTKEY_CHOICE_PIP_SWAP], function_to_string(BTN_ASSIGN_PIP_SWAP));
  strcpy(item->choices[HOTKEY_CHOICE_40_80_COLUMN], function_to_string(BTN_ASSIGN_40_80_COLUMN));
  strcpy(item->choices[HOTKEY_CHOICE_FLUSH_DISK], function_to_string(BTN_ASSIGN_FLUSH_DISK));
  item->choice_ints[HOTKEY_CHOICE_NONE] = BTN_ASSIGN_UNDEF;
  item->choice_ints[HOTKEY_CHOICE_MENU] = BTN_ASSIGN_MENU;
  item->choice_ints[HOTKEY_CHOICE_WARP] = BTN_ASSIGN_WARP;
  item->choice_ints[HOTKEY_CHOICE_STATUS_TOGGLE] = BTN_ASSIGN_STATUS_TOGGLE;
  item->choice_ints[HOTKEY_CHOICE_SWAP_PORTS] = BTN_ASSIGN_SWAP_PORTS;
  item->choice_ints[HOTKEY_CHOICE_TAPE_MENU] = BTN_ASSIGN_TAPE_MENU;
  item->choice_ints[HOTKEY_CHOICE_CART_MENU] = BTN_ASSIGN_CART_MENU;
  item->choice_ints[HOTKEY_CHOICE_CART_FREEZE] = BTN_ASSIGN_CART_FREEZE;
  item->choice_ints[HOTKEY_CHOICE_RESET_MENU] = BTN_ASSIGN_RESET_MENU;
  item->choice_ints[HOTKEY_CHOICE_RESET_HARD] = BTN_ASSIGN_RESET_HARD;
  item->choice_ints[HOTKEY_CHOICE_RESET_SOFT] = BTN_ASSIGN_RESET_SOFT;
  item->choice_ints[HOTKEY_CHOICE_ACTIVE_DISPLAY] = BTN_ASSIGN_ACTIVE_DISPLAY;
  item->choice_ints[HOTKEY_CHOICE_PIP_LOCATION] = BTN_ASSIGN_PIP_LOCATION;
  item->choice_ints[HOTKEY_CHOICE_PIP_SWAP] = BTN_ASSIGN_PIP_SWAP;
  item->choice_ints[HOTKEY_CHOICE_40_80_COLUMN] = BTN_ASSIGN_40_80_COLUMN;
  item->choice_ints[HOTKEY_CHOICE_FLUSH_DISK] = BTN_ASSIGN_FLUSH_DISK;

  if (emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
     item->choice_disabled[HOTKEY_CHOICE_SWAP_PORTS] = 1;
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_C64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_SCPU64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_C128) {
     item->choice_disabled[HOTKEY_CHOICE_CART_FREEZE] = 1;
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_C128) {
     item->choice_disabled[HOTKEY_CHOICE_ACTIVE_DISPLAY] = 1;
     item->choice_disabled[HOTKEY_CHOICE_PIP_LOCATION] = 1;
     item->choice_disabled[HOTKEY_CHOICE_PIP_SWAP] = 1;
     item->choice_disabled[HOTKEY_CHOICE_40_80_COLUMN] = 1;
  }
}

static void menu_build_machine_switch(struct menu_item* parent) {
  struct menu_item* holder = ui_menu_add_folder(parent, "Switch");

  struct menu_item* vic20_r = ui_menu_add_folder(holder, "VIC20");
  struct menu_item* c64_r = ui_menu_add_folder(holder, "C64");
  struct menu_item* scpu64_r = ui_menu_add_folder(holder, "SCPU64");
  struct menu_item* c128_r = ui_menu_add_folder(holder, "C128");
  struct menu_item* plus4_r = ui_menu_add_folder(holder, "Plus/4");
  struct menu_item* pet_r = ui_menu_add_folder(holder, "PET");

  struct machine_entry* head;
  load_machines(&head);

  struct machine_entry* ptr = head;
  struct menu_item* item;
  while (ptr) {
    if (!switch_voce_visibile(ptr)) {
      ptr = ptr->next;
      continue;
    }
    switch (ptr->class) {
      case BMC64_MACHINE_CLASS_VIC20:
         item = ui_menu_add_button(MENU_SWITCH_MACHINE, vic20_r, ptr->desc);
         break;
      case BMC64_MACHINE_CLASS_C64:
         item = ui_menu_add_button(MENU_SWITCH_MACHINE, c64_r, ptr->desc);
         break;
      case BMC64_MACHINE_CLASS_SCPU64:
         item = ui_menu_add_button(MENU_SWITCH_MACHINE, scpu64_r, ptr->desc);
         break;
      case BMC64_MACHINE_CLASS_C128:
         item = ui_menu_add_button(MENU_SWITCH_MACHINE, c128_r, ptr->desc);
         break;
      case BMC64_MACHINE_CLASS_PLUS4:
         item = ui_menu_add_button(MENU_SWITCH_MACHINE, plus4_r, ptr->desc);
         break;
      case BMC64_MACHINE_CLASS_PLUS4EMU:
         if (circle_get_model() >= 3) {
            item = ui_menu_add_button(MENU_SWITCH_MACHINE, plus4_r, ptr->desc);
         } else {
            item = NULL;
         }
         break;
      case BMC64_MACHINE_CLASS_PET:
         item = ui_menu_add_button(MENU_SWITCH_MACHINE, pet_r, ptr->desc);
         break;
      default:
         item = NULL;
         break;
    }

    if (item) {
       item->value = ptr->id;
    }

    ptr=ptr->next;
  }

  free_machines(head);






  ui_menu_add_divider(holder);
  switch_timer_item = ui_menu_add_toggle(MENU_SWITCH_TIMER, holder,
                                         "Safety timer (15s)",
                                         vidtrial_timer_on());
}

struct menu_item* add_joyport_options(struct menu_item* parent, int port) {
  int menu_id;
  switch (port) {
     case 1:
       menu_id = MENU_JOYSTICK_PORT_1;
       break;
     case 2:
       menu_id = MENU_JOYSTICK_PORT_2;
       break;
     case 3:
       menu_id = MENU_JOYSTICK_PORT_3;
       break;
     case 4:
       menu_id = MENU_JOYSTICK_PORT_4;
       break;
     default:
       assert(0);
  }

  struct menu_item* child = ui_menu_add_multiple_choice(
      menu_id, parent, "");
  snprintf(child->name, MAX_MENU_STR, "Port %d", port);
  child->num_choices = 16;
  child->value = 0;
  strcpy(child->choices[0], "None");
  child->choice_ints[0] = JOYDEV_NONE;
  strcpy(child->choices[1], "USB Gamepad 1");
  child->choice_ints[1] = JOYDEV_USB_0;
  strcpy(child->choices[2], "USB Gamepad 2");
  child->choice_ints[2] = JOYDEV_USB_1;
  strcpy(child->choices[3], "GPIO Bank 1");
  child->choice_ints[3] = JOYDEV_GPIO_0;
  strcpy(child->choices[4], "GPIO Bank 2");
  child->choice_ints[4] = JOYDEV_GPIO_1;
  strcpy(child->choices[5], "CURS + SPACE");
  child->choice_ints[5] = JOYDEV_CURS_SP;
  strcpy(child->choices[6], "NUMPAD 64825");
  child->choice_ints[6] = JOYDEV_NUMS_1;
  strcpy(child->choices[7], "NUMPAD 17930");
  child->choice_ints[7] = JOYDEV_NUMS_2;
  strcpy(child->choices[8], "CURS + LCTRL");
  child->choice_ints[8] = JOYDEV_CURS_LC;
  strcpy(child->choices[9], "USB Mouse (1351)");
  child->choice_ints[9] = JOYDEV_MOUSE;
  strcpy(child->choices[10], "Custom Keyset 1");
  child->choice_ints[10] = JOYDEV_KEYSET1;
  strcpy(child->choices[11], "Custom Keyset 2");
  child->choice_ints[11] = JOYDEV_KEYSET2;
  strcpy(child->choices[12], "USB Gamepad 3");
  child->choice_ints[12] = JOYDEV_USB_2;
  strcpy(child->choices[13], "USB Gamepad 4");
  child->choice_ints[13] = JOYDEV_USB_3;






  strcpy(child->choices[14], "USB Mouse (MicroMys)");
  child->choice_ints[14] = JOYDEV_MOUSE_MICROMYS;



  strcpy(child->choices[15], "USB Spinner (Paddle)");
  child->choice_ints[15] = JOYDEV_SPINNER;

  if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU || port > 2) {
     child->choice_disabled[9] = 1;
     child->choice_disabled[14] = 1;
  }
  if ((emux_machine_class != BMC64_MACHINE_CLASS_C64 &&
       emux_machine_class != BMC64_MACHINE_CLASS_C128 &&
       emux_machine_class != BMC64_MACHINE_CLASS_VIC20) || port > 2) {
     child->choice_disabled[15] = 1;
  }
  return child;
}



static void menu_update_geometry_lines(void) {
  if (meter_geo_item == NULL || meter_geo2_item == NULL ||
      meter_geo3_item == NULL) {
    return;
  }
  snprintf(meter_geo_item->name, MAX_MENU_STR, "disp %ux%u ask %ux%u",
           raspi_disp_w, raspi_disp_h, raspi_fb_ask_w, raspi_fb_ask_h);
  snprintf(meter_geo2_item->name, MAX_MENU_STR,
           "got %ux%u d%u pg%u b%u kms%d v3d%d",
           raspi_fb_got_w, raspi_fb_got_h, raspi_fb_displays,
           raspi_fb_pages, raspi_fb_bpp,
           fbl_stato_scalatore(), fbl_stato_v3d());




  int dpx, dpy, fbw, fbh, sw, sh, dw, dh;
  circle_get_fbl_dimensions(FB_LAYER_VIC, &dpx, &dpy, &fbw, &fbh,
                            &sw, &sh, &dw, &dh);
  snprintf(meter_geo3_item->name, MAX_MENU_STR,
           "src %dx%d dst %dx%d h%d v%d",
           sw, sh, dw, dh,
           h_stretch_item[VIC_INDEX] ? h_stretch_item[VIC_INDEX]->value : 0,
           v_stretch_item[VIC_INDEX] ? v_stretch_item[VIC_INDEX]->value : 0);
}

static void menu_update_sound_lines(void) {
  static const char *dev_name[] = { "none", "vchiq", "hdmi", "usb" };

  if (meter_snd_item == NULL || meter_snd2_item == NULL) {
    return;
  }
  snprintf(meter_snd_item->name, MAX_MENU_STR, "snd %s play %d re %d/%u",
           dev_name[raspi_snd_dev < 4 ? raspi_snd_dev : 0], raspi_snd_play,
           raspi_snd_restart, raspi_snd_restarts);
  snprintf(meter_snd2_item->name, MAX_MENU_STR, "snd wr %lu sp %d",
           raspi_snd_written, raspi_snd_space);
}






int menu_active_display_is_vdc(void) {
  if (active_display_item == NULL) {
    return 0;
  }



  if (active_display_item->value == MENU_ACTIVE_DISPLAY_DUE_HDMI) {
    return c40_80_column_item != NULL && c40_80_column_item->value == 0;
  }
  return active_display_item->value == MENU_ACTIVE_DISPLAY_VDC;
}

void menu_update_meter(void) {
  menu_update_sound_lines();
  menu_update_geometry_lines();
}



static const char *overclock_label(int level) {
  static char buf[MAX_MENU_STR];
  static char what[8];
  int stock = overclock_stock_mhz();
  int pct = level * 10;
  if (pct == 0) {
    strcpy(what, "Stock");
  } else {
    snprintf(what, sizeof(what), "%+d%%", pct);
  }
  snprintf(buf, sizeof(buf), "Now: %s (%d MHz)", what,
           stock + (stock * pct) / 100);
  return buf;
}




void show_overclock_menu(void) {
  struct menu_item *root = ui_push_menu(20, 10);
  ui_menu_add_read_only_heading(root, "CPU Clock");
  ui_menu_add_read_only_heading(root, overclock_label(overclock_current()));
  ui_menu_add_divider(root);
  ui_menu_add_button_with_value(MENU_OVERCLOCK, root, "-20%", -2, " ", " ");
  ui_menu_add_button_with_value(MENU_OVERCLOCK, root, "-10%", -1, " ", " ");
  ui_menu_add_button_with_value(MENU_OVERCLOCK, root, "Stock", 0, " ", " ");
  ui_menu_add_button_with_value(MENU_OVERCLOCK, root, "+10%",  1, " ", " ");
  ui_menu_add_button_with_value(MENU_OVERCLOCK, root, "+20%",  2, " ", " ");
  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_CONFIRM_CANCEL, root, "Cancel");
}






void show_gpuclock_menu(void) {
  static char riga1[MAX_MENU_STR], riga2[MAX_MENU_STR], riga3[MAX_MENU_STR];
  const int level = gpuclock_current();
  const int pct = level * 10;
  unsigned core, v3d, core_fw, v3d_fw;
  gpuclock_di_serie(circle_get_model(), &core, &v3d);
  core_fw = circle_gpu_mhz(0, 1);
  v3d_fw = circle_gpu_mhz(1, 1);
  if (core_fw == 0) core_fw = core + (core * pct) / 100;
  if (v3d_fw == 0) v3d_fw = v3d + (v3d * pct) / 100;
  if (pct == 0) {
    snprintf(riga1, sizeof(riga1), "Now: Stock");
  } else {
    snprintf(riga1, sizeof(riga1), "Now: %+d%%", pct);
  }
  snprintf(riga2, sizeof(riga2), "Core: %u MHz", core_fw);
  snprintf(riga3, sizeof(riga3), "V3D: %u MHz", v3d_fw);
  struct menu_item *root = ui_push_menu(20, 12);
  ui_menu_add_read_only_heading(root, "GPU Clock");
  ui_menu_add_read_only_heading(root, riga1);
  ui_menu_add_read_only_heading(root, riga2);
  ui_menu_add_read_only_heading(root, riga3);
  ui_menu_add_divider(root);








  ui_menu_add_button_with_value(MENU_GPUCLOCK, root, "Stock", 0, " ", " ");
  ui_menu_add_button_with_value(MENU_GPUCLOCK, root, "+10%",  1, " ", " ");
  ui_menu_add_button_with_value(MENU_GPUCLOCK, root, "+20%",  2, " ", " ");
  ui_menu_add_divider(root);
  ui_menu_add_button(MENU_CONFIRM_CANCEL, root, "Cancel");
}


//





int menu_get_drive_flush(void) {
  return drive_flush_item != NULL ? drive_flush_item->value
                                  : DRIVE_FLUSH_ON_DETACH;
}

void menu_vidtrial_start(void) {
  if (vidtrial_armed || !vidtrial_pending()) {
    return;
  }
  vidtrial_armed = 1;
  vidtrial_deadline = circle_get_ticks() + VIDTRIAL_SECONDS * 1000000UL;
  overlay_statusbar_enable();
}


void menu_vidtrial_keep(void) {
  if (!vidtrial_armed) {
    return;
  }
  vidtrial_armed = 0;
  vidtrial_accept();
}


int menu_vidtrial_left(void) {
  if (!vidtrial_armed) {
    return -1;
  }
  long us = (long)(vidtrial_deadline - circle_get_ticks());
  if (us < 0) {
    return 0;
  }
  return (int)(us / 1000000L);
}




void menu_vidtrial_tick(void) {
  if (!vidtrial_armed) {
    return;
  }
  if ((long)(circle_get_ticks() - vidtrial_deadline) < 0) {
    return;
  }
  vidtrial_armed = 0;
  vidtrial_revert();
  riavvia();
}


//




#define POWEROFF_CONFIRM_SECONDS 5

static int poweroff_armed = 0;
static int poweroff_key_pending = 0;
static int poweroff_annulla_pending = 0;
static unsigned long poweroff_deadline = 0;
static int poweroff_bar_was_showing = 0;

static void poweroff_disarm(void) {
  poweroff_armed = 0;

  if (!poweroff_bar_was_showing && !statusbar_always()) {
    overlay_statusbar_disable();
  }
}


static int poweroff_e_riavvio = 0;














void menu_poweroff_key(void) {
  poweroff_key_pending = 1;
  if (!poweroff_armed) {
    poweroff_e_riavvio = 0;
  }
}




void menu_reboot_key(void) {
  poweroff_key_pending = 1;
  if (!poweroff_armed) {
    poweroff_e_riavvio = 1;
  }
}

int menu_poweroff_is_reboot(void) {
  return poweroff_e_riavvio;
}









int menu_poweroff_annulla(void) {
  if (!poweroff_armed) {
    return 0;
  }
  poweroff_annulla_pending = 1;
  return 1;
}



void menu_poweroff_tick(void) {



  if (poweroff_annulla_pending) {
    poweroff_annulla_pending = 0;
    poweroff_key_pending = 0;
    if (poweroff_armed) {
      poweroff_disarm();
      overlay_avviso("CANCELLED");
    }
    return;
  }
  if (poweroff_key_pending) {
    poweroff_key_pending = 0;
    if (ui_enabled) {




      if (poweroff_e_riavvio) {
        ui_confirm_wrapped("Reboot?", REBOOTPI_MSG, 0, MENU_REBOOT_PI);
      } else {
        ui_confirm_wrapped("Power Off?", POWEROFF_MSG, 0, MENU_POWEROFF);
      }
    } else if (poweroff_armed) {


      poweroff_disarm();
      if (poweroff_e_riavvio) {
        riavvia();
      } else {
        spegni();
      }
      return;
    } else {
      poweroff_armed = 1;
      poweroff_deadline = circle_get_ticks() +
                          POWEROFF_CONFIRM_SECONDS * 1000000UL;
      poweroff_bar_was_showing = statusbar_showing;
      overlay_statusbar_enable();
    }
  }
  if (poweroff_armed &&
      (long)(circle_get_ticks() - poweroff_deadline) >= 0) {
    poweroff_disarm();
  }
}




void menu_banco_domanda_spegni(void) {
  printf("[PWR] domanda %s, %s\n", poweroff_armed ? "aperta" : "chiusa",
         poweroff_e_riavvio ? "riavvio" : "spegnimento");
}


















void menu_banco_rom_a_caldo(void) {
  struct menu_item finta;
  int esito;

  memset(&finta, 0, sizeof(finta));
  finta.id = MENU_DRIVE_ROM_FILE_1541;
  strncpy(finta.str_value, "dos1541ii-251968-03.bin",
          sizeof(finta.str_value) - 1);
  esito = emux_handle_rom_change(&finta, fullpath);
  printf("[ROM] 1541 -> 1541-II a caldo, esito %d\n", esito);
}























void menu_banco_elenco_dischi(void) {
  show_files(DIR_DISKS, FILTER_DISK, MENU_DISK_FILE, 1);
}





void menu_banco_volume_finto(void) {
  strcpy(current_volume_name, "USB2:");
  usb2_mounted = 1;
  printf("[VOL] volume corrente: USB2: (finto, senza chiavetta)\n");
}



void menu_banco_volume_flp1(void) {
  int floppy[2];
  circle_find_floppy(&floppy);
  strcpy(current_volume_name, "FLP1:");
  printf("[VOL] volume corrente: FLP1: (drive %s, stato %d)\n",
         floppy[0] ? "presente" : "ASSENTE", circle_floppy_stato(0));
}










void menu_banco_player2(int porta) {
  joydevs[1].device = JOYDEV_CURS_LC;
  joydevs[1].port = porta;





  emux_set_joy_port_device(porta, JOYDEV_CURS_LC);
  printf("[JOY] player 2: cursori+CTRL sulla porta %d\n", porta);
}































































































































































































































































































































































































































































































































































































































































































void menu_banco_aspetto(int largo) {
  metti_aspetto(VIC_INDEX, largo);
}

void menu_banco_videostato(void) {
  int secondo_strato, quanti_display = 0;






  int colonne_vere = -1;
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
    emux_get_int(Setting_C128ColumnKey, &colonne_vere);
  }
  secondo_strato = circle_secondo_schermo_info(NULL, NULL, &quanti_display);
  printf("[VID] secondo=%d display=%d\n", secondo_strato, quanti_display);
  printf("[VID] volume=%d hstretch=%d vstretch=%d riavvio=%d attesa=%d"
         " schermo=%d porte=%d/%d scambio=%d colonne=%d colonnevere=%d\n",
         volume_item ? volume_item->value : -1,
         h_stretch_item[VIC_INDEX] ? h_stretch_item[VIC_INDEX]->value : -1,
         v_stretch_item[VIC_INDEX] ? v_stretch_item[VIC_INDEX]->value : -1,
         poweroff_e_riavvio, poweroff_armed,
         active_display_item ? active_display_item->value : -1,
         port_1_menu_item ? port_1_menu_item->value : -1,
         port_2_menu_item ? port_2_menu_item->value : -1, joyswap,
         c40_80_column_item ? c40_80_column_item->value : -1,
         colonne_vere);
}

int menu_poweroff_left(void) {
  if (!poweroff_armed) {
    return -1;
  }
  long us = (long)(poweroff_deadline - circle_get_ticks());
  if (us < 0) {
    return 0;
  }
  return (int)(us / 1000000L) + 1;
}

static void build_overclock_menu(struct menu_item *root) {
  ui_menu_add_button(MENU_OVERCLOCK_MENU, root, "CPU Clock...");
  ui_menu_add_button(MENU_GPUCLOCK_MENU, root, "GPU Clock...");
}









static struct menu_item *cart_only_autostart;
static struct menu_item *cart_only_warp;
static struct menu_item *cart_only_loadprg;
static struct menu_item *cart_only_drives;
static struct menu_item *cart_only_tape;
static int cart_only;

static void apply_cartridge_only(void) {
  if (cart_only_autostart) cart_only_autostart->disabled = cart_only;
  if (cart_only_warp)      cart_only_warp->disabled = cart_only;
  if (cart_only_loadprg)   cart_only_loadprg->disabled = cart_only;
  if (cart_only_drives)    cart_only_drives->disabled = cart_only;
  if (cart_only_tape)      cart_only_tape->disabled = cart_only;
}

void menu_set_cartridge_only(int on) {
  cart_only = on;
  apply_cartridge_only();


  overlay_set_cartridge_only(on);
}



int menu_is_cartridge_only(void) {
  return cart_only;
}

void build_menu(struct menu_item *root) {
  struct menu_item *parent;
  struct menu_item *video_parent;
  struct menu_item *drive_parent;
  struct menu_item *machine_parent;
  struct menu_item *tape_parent;
  struct menu_item *child;
  int dev;
  int i;
  int j;

  root->cursor_listener_func = main_menu_cursor_listener;
  menu_radice = root;
  int k;
  int tmp;

  for (int k = 0; k < MAX_USB_DEVICES; k++) {
     sprintf (usb_btn_name[k], "usb_btn_%d", k);
     sprintf (usb_pref_name[k], "usb_%d", k);
     sprintf (usb_x_name[k], "usb_x_%d", k);
     sprintf (usb_y_name[k], "usb_y_%d", k);
     sprintf (usb_x_t_name[k], "usb_x_t_%d", k);
     sprintf (usb_y_t_name[k], "usb_y_t_%d", k);
  }

  attached_disk_name[0][0] = '\0';
  attached_disk_name[1][0] = '\0';
  attached_disk_name[2][0] = '\0';
  attached_disk_name[3][0] = '\0';

  emux_load_additional_settings();

  // TODO: This doesn't really belong here. Need to sort
  // out init order of structs.
  for (dev = 0; dev < MAX_JOY_PORTS; dev++) {
    memset(&joydevs[dev], 0, sizeof(struct joydev_config));
    joydevs[dev].port = dev + 1;
    joydevs[dev].device = JOYDEV_NONE;
  }

  strcpy(current_volume_name, default_volume_name);

  if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU) {
     strcpy(snap_filt_ext[0],".p4s");
  } else {
     strcpy(snap_filt_ext[0],".vsf");
  }

  char machine_info_txt[64];
  machine_info_txt[0] = '\0';

  switch (emux_machine_class) {
  case BMC64_MACHINE_CLASS_C64:
    strcat(machine_info_txt,"C64 ");
    strcpy(machine_sub_dir, "/C64");
    break;
  case BMC64_MACHINE_CLASS_SCPU64:
    strcat(machine_info_txt,"SCPU64 ");
    strcpy(machine_sub_dir, "/SCPU64");
    break;
  case BMC64_MACHINE_CLASS_C128:
    strcat(machine_info_txt,"C128 ");
    strcpy(machine_sub_dir, "/C128");
    break;
  case BMC64_MACHINE_CLASS_VIC20:
    strcat(machine_info_txt,"VIC20 ");
    strcpy(machine_sub_dir, "/VIC20");
    break;
  case BMC64_MACHINE_CLASS_PLUS4:
  case BMC64_MACHINE_CLASS_PLUS4EMU:
    strcat(machine_info_txt,"PLUS/4 ");
    strcpy(machine_sub_dir, "/PLUS4");
    break;
  case BMC64_MACHINE_CLASS_PET:
    strcat(machine_info_txt,"PET ");
    strcpy(machine_sub_dir, "/PET");
    break;
  default:
    strcat(machine_info_txt,"??? ");
    strcpy(machine_sub_dir, "/");
    break;
  }






  strcpy(files_sub_dir, emux_machine_class == BMC64_MACHINE_CLASS_SCPU64
                            ? "/C64" : machine_sub_dir);


  char scratch[16];
  switch (circle_get_machine_timing()) {
  case MACHINE_TIMING_NTSC_HDMI:
    strcat(machine_info_txt, "NTSC 60Hz HDMI");
    break;
  case MACHINE_TIMING_NTSC_DPI:
    strcat(machine_info_txt, "NTSC 60Hz DPI");
    break;
  case MACHINE_TIMING_NTSC_COMPOSITE:
  case MACHINE_TIMING_NTSC_CUSTOM_HDMI:
  case MACHINE_TIMING_NTSC_CUSTOM_DPI:
    strcat(machine_info_txt, "NTSC ");
    snprintf (scratch, sizeof(scratch), "%.3f", emux_calculate_fps());
    strcat (machine_info_txt, scratch);
    strcat(machine_info_txt, "Hz ");
    switch (circle_get_machine_timing()) {
      case MACHINE_TIMING_NTSC_COMPOSITE:
        strcat(machine_info_txt, "Composite");
        break;
      case MACHINE_TIMING_NTSC_CUSTOM_HDMI:
        strcat(machine_info_txt, "Custom HDMI");
        break;
      case MACHINE_TIMING_NTSC_CUSTOM_DPI:
        strcat(machine_info_txt, "Custom DPI");
        break;
      default:
        break;
    }
    break;
  case MACHINE_TIMING_PAL_HDMI:
    strcat(machine_info_txt, "PAL 50Hz HDMI");
    break;
  case MACHINE_TIMING_PAL_DPI:
    strcat(machine_info_txt, "PAL 50Hz DPI");
    break;
  case MACHINE_TIMING_PAL_COMPOSITE:
  case MACHINE_TIMING_PAL_CUSTOM_HDMI:
  case MACHINE_TIMING_PAL_CUSTOM_DPI:
    strcat(machine_info_txt, "PAL ");
    snprintf (scratch, sizeof(scratch), "%.3f", emux_calculate_fps());
    strcat (machine_info_txt, scratch);
    strcat(machine_info_txt, "Hz ");
    switch (circle_get_machine_timing()) {
      case MACHINE_TIMING_PAL_COMPOSITE:
        strcat(machine_info_txt, "Composite");
        break;
      case MACHINE_TIMING_PAL_CUSTOM_HDMI:
        strcat(machine_info_txt, "Custom HDMI");
        break;
      case MACHINE_TIMING_PAL_CUSTOM_DPI:
        strcat(machine_info_txt, "Custom DPI");
        break;
      default:
        break;
    }
    break;
  default:
    strcat(machine_info_txt, "Error");
    break;
  }

  ui_menu_add_button(MENU_TEXT, root, machine_info_txt);




  temperatura_item = ui_menu_add_button(MENU_TEXT, root, "CPU --");




  ora_item = ui_menu_add_button(MENU_TEXT, root, "Clock --");
  aggiorna_ora();

  ui_menu_add_button(MENU_SHORTCUTS, root, "Shortcut Keys...");
  ui_menu_add_button(MENU_ABOUT, root, "About...");
  ui_menu_add_button(MENU_LICENSE, root, "License...");

  ui_menu_add_divider(root);


  {
    network_status_item = ui_menu_add_read_only_heading(
      root, "Network Status:");
    parent = ui_menu_add_folder(root, "Network");
    parent->id = MENU_NETWORKING;
    ui_menu_add_button(MENU_SD_SHARING, root, "SD Card Sharing...");

    child = network_device_item =
      ui_menu_add_multiple_choice(MENU_NETWORK_ENABLED, parent, "Network Device");
    child->num_choices = 3;
    child->value = 0;
    strcpy(child->choices[0], "Off");
    strcpy(child->choices[1], "Ethernet");
    strcpy(child->choices[2], "WiFi");
    child->choice_disabled[1] = !circle_has_onboard_ethernet();
    child->choice_disabled[2] = !circle_has_onboard_wifi();

    child = network_address_mode_item = ui_menu_add_multiple_choice(
      MENU_NETWORK_ADDRESS_MODE, parent, "IP Address Mode (reboot)");
    child->num_choices = 2;
    child->value = 0;
    strcpy(child->choices[0], "DHCP");
    strcpy(child->choices[1], "Static");
    {
      static const char *const etichette[4] = {
          "Static IP", "Netmask", "Gateway", "DNS"};
      for (int i = 0; i < 4; i++) {
        network_static_item[i] = ui_menu_add_text_field_limit(
          MENU_NETWORK_STATIC_IP + i, parent, etichette[i], "", 15);
        network_static_item[i]->textfield_right_aligned = 1;
      }
    }


    if (emux_machine_class == BMC64_MACHINE_CLASS_C64 ||
        emux_machine_class == BMC64_MACHINE_CLASS_C128) {

    child = network_modem_item = ui_menu_add_multiple_choice(
      MENU_NETWORK_MODEM, parent, "Modem (SwiftLink)");
    child->num_choices = 2;
    child->value = circle_get_acia_network_enabled() ? 1 : 0;
    strcpy(child->choices[0], "Off");
    strcpy(child->choices[1], "On");
    child = network_modem_address_item = ui_menu_add_multiple_choice(
      MENU_NETWORK_MODEM_ADDRESS, parent, "Modem Address");
    child->num_choices = sizeof(acia_network_addresses) /
               sizeof(acia_network_addresses[0]);
    child->value = acia_network_address_index(
        circle_get_acia_network_address());
    for (int address_index = 0; address_index < child->num_choices;
         address_index++) {
      strcpy(child->choices[address_index],
             acia_network_address_labels[address_index]);
    }
    }

    timezone_offset_item = ui_menu_add_multiple_choice(
      MENU_TIMEZONE_OFFSET, parent, "Timezone (reboot)");
    configure_timezone_offsets(timezone_offset_item);
    timezone_offset_item->value = timezone_offset_index(0);

    timezone_dst_item = ui_menu_add_multiple_choice(
      MENU_TIMEZONE_DST, parent, "Daylight Saving (reboot)");
    timezone_dst_item->num_choices = 3;
    timezone_dst_item->value = 0;
    strcpy(timezone_dst_item->choices[0], "Off");
    strcpy(timezone_dst_item->choices[1], "On");
    strcpy(timezone_dst_item->choices[2], "Auto (EU)");

    network_ip_address_item = ui_menu_add_button_with_value(
      MENU_ID_DO_NOTHING, parent, "IP Address", 0,
      " ", " ");
    network_ip_address_item->disabled = 1;

    smb_password_item = ui_menu_add_text_field_limit(
      MENU_SD_SHARING_PASSWORD, parent, "Sharing Password", "bmc64", 31);
    smb_password_item->textfield_right_aligned = 1;

    parent = wifi_settings_item = ui_menu_add_folder(parent, "WiFi Settings");
    wifi_ssid_item = ui_menu_add_text_field_limit(
      MENU_WIFI_SSID, parent, "WiFi SSID", "", 32);
    wifi_ssid_item->textfield_right_aligned = 1;
    wifi_security_item = ui_menu_add_multiple_choice(
      MENU_WIFI_SECURITY, parent, "WiFi Security");
    wifi_security_item->num_choices = 2;
    strcpy(wifi_security_item->choices[0], "WPA-PSK");
    strcpy(wifi_security_item->choices[1], "None");
    wifi_country_item = ui_menu_add_text_field_limit(
      MENU_WIFI_COUNTRY, parent, "WiFi Country Code", "US", 2);
    wifi_country_item->textfield_right_aligned = 1;
    wifi_country_item->textfield_code = 1;
    wifi_connect_item = ui_menu_add_button(MENU_WIFI_CONNECT, parent,
                         "Enter Password & Reboot");
    update_wifi_menu_enabled();

    circle_set_network_status_changed_handler(network_status_changed);
    menu_update_network_status();


    parent = ui_menu_add_folder(root, "NAS");
    nas_ip_item = ui_menu_add_text_field_limit(
      MENU_NAS_IP, parent, "NAS IP Address", "", 31);
    nas_ip_item->textfield_right_aligned = 1;
    nas_user_item = ui_menu_add_text_field_limit(
      MENU_NAS_USER, parent, "NAS User Name", "", 63);
    nas_user_item->textfield_right_aligned = 1;
    nas_password_item = ui_menu_add_text_field_limit(
      MENU_NAS_PASSWORD, parent, "NAS Password", "", 63);
    nas_password_item->textfield_right_aligned = 1;
    nas_protocol_item = ui_menu_add_multiple_choice(
      MENU_NAS_PROTOCOL, parent, "SMB Protocol");
    nas_protocol_item->num_choices = 6;
    nas_protocol_item->value = 0;
    strcpy(nas_protocol_item->choices[0], "Auto");
    strcpy(nas_protocol_item->choices[1], "SMB 2.0");
    strcpy(nas_protocol_item->choices[2], "SMB 2.1");
    strcpy(nas_protocol_item->choices[3], "SMB 3.0");
    strcpy(nas_protocol_item->choices[4], "SMB 3.0.2");
    strcpy(nas_protocol_item->choices[5], "SMB 3.1.1");


    nas_path_item = ui_menu_add_text_field_limit(
      MENU_NAS_PATH, parent, "NAS Path", "", 127);
    nas_path_item->textfield_right_aligned = 1;
    ui_menu_add_button(MENU_NAS_TEST, parent, "Test NAS Connection");
    ui_menu_add_divider(root);
  }

  switch (emux_machine_class) {
    case BMC64_MACHINE_CLASS_PLUS4EMU:



     ui_menu_add_button(MENU_AUTOSTART, root, "Autostart Prg/Disk...");
     ui_menu_add_button(MENU_LOADPRG, root, "Load .PRG File...");
     break;
    case BMC64_MACHINE_CLASS_PET:
     break;
    default:
     cart_only_autostart =
         ui_menu_add_button(MENU_AUTOSTART, root, "Autostart Prg/Disk...");
     emux_get_int(Setting_AutostartWarp, &tmp);
     cart_only_warp =
         ui_menu_add_toggle(MENU_AUTOSTART_WARP, root, "Autostart Warp", tmp);
     break;
  }

  machine_parent = ui_menu_add_folder(root, "Machine");
    emux_add_machine_options(machine_parent);
    menu_build_machine_switch(machine_parent);

  drive_parent = ui_menu_add_folder(root, "Drives");
  cart_only_drives = drive_parent;
    // (-1) Options applicable to all drives
    emux_add_drive_option(drive_parent, -1);






    if (emux_machine_class != BMC64_MACHINE_CLASS_PET &&
        emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
      int b;

      realdrive_log_item = ui_menu_add_toggle(
          MENU_REALDRIVE_LOG, drive_parent, "Real Drive Log", 0);














      realdrive_veloce_item = ui_menu_add_toggle(
          MENU_REALDRIVE_VELOCE, drive_parent, "Real Drive Fast Loading", 1);




      ui_menu_add_button(MENU_REALDRIVE_RECOVER, drive_parent,
                         "Reset Drive on XUM1541");




      ui_menu_add_button(MENU_REALDRIVE_TURBO, drive_parent,
                         "Real Drive: Try the fast loader");

      realdrive_respiro_item = ui_menu_add_multiple_choice(
          MENU_REALDRIVE_RESPIRO, drive_parent, "Fast Loader Pause");
      realdrive_respiro_item->num_choices = REALDRIVE_RESPIRI_NUM;
      for (b = 0; b < REALDRIVE_RESPIRI_NUM; b++) {
        if (realdrive_respiri[b] == 0) {
          snprintf(realdrive_respiro_item->choices[b], MAX_MENU_STR,
                   "0 (1541)");
        } else if (realdrive_respiri[b] == 1000) {
          snprintf(realdrive_respiro_item->choices[b], MAX_MENU_STR,
                   "1 ms (OpenCBM)");
        } else {
          snprintf(realdrive_respiro_item->choices[b], MAX_MENU_STR,
                   "%d ms", realdrive_respiri[b] / 1000);
        }
        realdrive_respiro_item->choice_ints[b] = realdrive_respiri[b];
      }
      realdrive_respiro_item->value = 0;
    }

    parent = ui_menu_add_folder(drive_parent, "Drive 8");
    ui_menu_add_button(MENU_ATTACH_DISK_8, parent, "Attach Disk...");
    ui_menu_add_button(MENU_DETACH_DISK_8, parent, "Detach Disk");
    if (emux_machine_class != BMC64_MACHINE_CLASS_PET) {
     emux_get_int_1(Setting_IECDeviceN, &tmp, 8);
     voce_iec[0] = ui_menu_add_toggle(MENU_IECDEVICE_8, parent,
                                     "IEC FileSystem", tmp);
     ui_menu_add_button(MENU_IECDIR_8, parent, "Select IEC Dir...");
    }
    emux_add_drive_option(parent, 8);

    if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
      ui_menu_add_button(MENU_DRIVE_CHANGE_MODEL_8, parent, "Change Model...");
    }










  {
    parent = ui_menu_add_folder(drive_parent, "Drive 9");
    ui_menu_add_button(MENU_ATTACH_DISK_9, parent, "Attach Disk...");
    ui_menu_add_button(MENU_DETACH_DISK_9, parent, "Detach Disk");
    if (emux_machine_class != BMC64_MACHINE_CLASS_PET) {
     emux_get_int_1(Setting_IECDeviceN, &tmp, 9);
     voce_iec[1] = ui_menu_add_toggle(MENU_IECDEVICE_9, parent,
                                     "IEC FileSystem", tmp);
     ui_menu_add_button(MENU_IECDIR_9, parent, "Select IEC Dir...");
    }
    emux_add_drive_option(parent, 9);

    if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
      ui_menu_add_button(MENU_DRIVE_CHANGE_MODEL_9, parent, "Change Model...");
    }

    parent = ui_menu_add_folder(drive_parent, "Drive 10");
    ui_menu_add_button(MENU_ATTACH_DISK_10, parent, "Attach Disk...");
    ui_menu_add_button(MENU_DETACH_DISK_10, parent, "Detach Disk");
    if (emux_machine_class != BMC64_MACHINE_CLASS_PET) {
     emux_get_int_1(Setting_IECDeviceN, &tmp, 10);
     voce_iec[2] = ui_menu_add_toggle(MENU_IECDEVICE_10, parent,
                                     "IEC FileSystem", tmp);
     ui_menu_add_button(MENU_IECDIR_10, parent, "Select IEC Dir...");
    }
    emux_add_drive_option(parent, 10);
    if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
      ui_menu_add_button(MENU_DRIVE_CHANGE_MODEL_10, parent, "Change Model...");
    }

    parent = ui_menu_add_folder(drive_parent, "Drive 11");
    ui_menu_add_button(MENU_ATTACH_DISK_11, parent, "Attach Disk...");
    ui_menu_add_button(MENU_DETACH_DISK_11, parent, "Detach Disk");
    if (emux_machine_class != BMC64_MACHINE_CLASS_PET) {
     emux_get_int_1(Setting_IECDeviceN, &tmp, 11);
     voce_iec[3] = ui_menu_add_toggle(MENU_IECDEVICE_11, parent,
                                     "IEC FileSystem", tmp);
     ui_menu_add_button(MENU_IECDIR_11, parent, "Select IEC Dir...");
    }
    emux_add_drive_option(parent, 11);
    if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
      ui_menu_add_button(MENU_DRIVE_CHANGE_MODEL_11, parent, "Change Model...");
    }
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
    ui_menu_add_button(MENU_DRIVE_CHANGE_ROM, drive_parent, "Change ROM...");
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
    parent = ui_menu_add_folder(drive_parent, "Create empty Disk");
      ui_menu_add_button(MENU_CREATE_D64, parent, "D64...");
      ui_menu_add_button(MENU_CREATE_D67, parent, "D67...");
      ui_menu_add_button(MENU_CREATE_D71, parent, "D71...");
      ui_menu_add_button(MENU_CREATE_D80, parent, "D80...");
      ui_menu_add_button(MENU_CREATE_D81, parent, "D81...");
      ui_menu_add_button(MENU_CREATE_D82, parent, "D82...");
      ui_menu_add_button(MENU_CREATE_D1M, parent, "D1M...");
      ui_menu_add_button(MENU_CREATE_D2M, parent, "D2M...");
      ui_menu_add_button(MENU_CREATE_D4M, parent, "D4M...");
      ui_menu_add_button(MENU_CREATE_G64, parent, "G64...");
      ui_menu_add_button(MENU_CREATE_G71, parent, "G71...");
      ui_menu_add_button(MENU_CREATE_P64, parent, "P64...");
      ui_menu_add_button(MENU_CREATE_X64, parent, "X64...");
      //ui_menu_add_button(MENU_CREATE_DHD, parent, "DHD..."); // VICE doesn't do this
  }

  parent = emux_add_cartridge_options(root);

  parent = ui_menu_add_folder(root, "Tape");
  cart_only_tape = parent;

    ui_menu_add_button(MENU_ATTACH_TAPE, parent, "Attach tape image...");
    ui_menu_add_button(MENU_DETACH_TAPE, parent, "Detach tape image");

    tape_parent = ui_menu_add_folder(parent, "Datasette controls (.tap)...");
    ui_menu_add_button(MENU_TAPE_START, tape_parent, "Play");
    ui_menu_add_button(MENU_TAPE_STOP, tape_parent, "Stop");
    ui_menu_add_button(MENU_TAPE_REWIND, tape_parent, "Rewind");
    ui_menu_add_button(MENU_TAPE_FASTFWD, tape_parent, "FastFwd");
    ui_menu_add_button(MENU_TAPE_RECORD, tape_parent, "Record");
    ui_menu_add_button(MENU_TAPE_RESET, tape_parent, "Reset");
    ui_menu_add_button(MENU_TAPE_RESET_COUNTER, tape_parent, "Reset Counter");
    emux_get_int(Setting_DatasetteResetWithCPU, &tmp);
    tape_reset_with_machine_item =
      ui_menu_add_toggle(MENU_TAPE_RESET_WITH_MACHINE, tape_parent,
                         "Reset Tape with Machine Reset", tmp);
    emux_add_tape_options(tape_parent);

    ui_menu_add_button(MENU_CREATE_TAP, parent, "Create empty Tape...");





  if (emux_machine_class == BMC64_MACHINE_CLASS_C64) {
    parent = ui_menu_add_folder(root, "SID Player");
    ui_menu_add_button(MENU_SID_PLAYER, parent, "Load .SID file...");
    ui_menu_add_button(MENU_SID_NEXT, parent, "Next tune");
    ui_menu_add_button(MENU_SID_PREV, parent, "Previous tune");
    ui_menu_add_button(MENU_SID_STOP, parent, "Stop and reset");
    ui_menu_add_divider(parent);
    sid_titolo_item = ui_menu_add_button(MENU_TEXT, parent, "");
    sid_autore_item = ui_menu_add_button(MENU_TEXT, parent, "");
    sid_diritti_item = ui_menu_add_button(MENU_TEXT, parent, "");
    sid_brano_item = ui_menu_add_button(MENU_TEXT, parent, "");
    sid_aggiorna_righe();
  }

  ui_menu_add_divider(root);

  // TODO: Load/Save snapshot on PET is crashy. Figure out if upstream
  // has fixed this.
  if (emux_machine_class != BMC64_MACHINE_CLASS_PET) {
     parent = ui_menu_add_folder(root, "Snapshots");
     ui_menu_add_button(MENU_LOAD_SNAP, parent, "Load Snapshot...");
     ui_menu_add_button(MENU_SAVE_SNAP, parent, "Save Snapshot...");
  }

  video_parent = parent = ui_menu_add_folder(root, "Video");

  scaling_interp_item = ui_menu_add_toggle_labels(
     MENU_SCALING_INTERPOLATION, parent,
        "Scaling Interpolation", 1, "Off", "On");

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     // For C128, we split video options under video into VICII
     // and VDC submenus since there are two displays.  Otherwise,
     // when there is only one display, everything falls under
     // video directly.
     active_display_item = child =
        ui_menu_add_multiple_choice(MENU_ACTIVE_DISPLAY, parent,
           "Active Display");
     child->num_choices = 5;
     child->value = MENU_ACTIVE_DISPLAY_VICII;
     strcpy(child->choices[MENU_ACTIVE_DISPLAY_VICII], "VICII");
     strcpy(child->choices[MENU_ACTIVE_DISPLAY_VDC], "VDC");
     strcpy(child->choices[MENU_ACTIVE_DISPLAY_SIDE_BY_SIDE], "Side-By-Side");
     strcpy(child->choices[MENU_ACTIVE_DISPLAY_PIP], "PIP");






     strcpy(child->choices[MENU_ACTIVE_DISPLAY_DUE_HDMI], "Two HDMI");








     secondo_hdmi_item = ui_menu_add_toggle(MENU_SECONDO_HDMI, parent,
         "Second Display Present", 0);
     due_hdmi_nel_giro();

     pip_location_item = child =
        ui_menu_add_multiple_choice(MENU_PIP_LOCATION, parent,
           "PIP Location");
     child->num_choices = 4;
     child->value = MENU_PIP_TOP_RIGHT;
     strcpy(child->choices[MENU_PIP_TOP_LEFT], "Top Left");
     strcpy(child->choices[MENU_PIP_TOP_RIGHT], "Top Right");
     strcpy(child->choices[MENU_PIP_BOTTOM_RIGHT], "Bottom Right");
     strcpy(child->choices[MENU_PIP_BOTTOM_LEFT], "Bottom Left");

     pip_swapped_item =
        ui_menu_add_toggle(MENU_PIP_SWAPPED, parent, "Swap PIP", 0);
  }














  if (!is_composite()) {
    hdmi_principale_item = child =
       ui_menu_add_multiple_choice(MENU_HDMI_PRINCIPALE, parent,
          "Main HDMI Output");
    child->num_choices = 2;
    strcpy(child->choices[0], "HDMI 0");
    strcpy(child->choices[1], "HDMI 1");
    child->value = hdmi_principale_adesso();
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_C128) {
     use_scaling_params_item[0] = ui_menu_add_toggle_labels(
        MENU_USE_SCALING_PARAMS_0, parent, "Apply scaling params at boot", 1,
           "No","Yes");
  }

  struct menu_item *shader = ui_menu_add_folder(video_parent, "CRT Shader");

     int crt_filter;
     emux_get_int(Setting_VideoFilter, &crt_filter);
     s_enable_shader_item =
        ui_menu_add_toggle_labels(MENU_SHADER_ENABLE, shader,
           "Enable CRT Shader?", crt_filter != MENU_VIDEO_FILTER_NONE, "No", "Yes");

     if (!allow_shader()) {
        s_enable_shader_item->value = 0;
        s_enable_shader_item->disabled = 1;
        strcpy (s_enable_shader_item->custom_toggle_label[0], "Disabled");
        strcpy (s_enable_shader_item->custom_toggle_label[1], "Disabled");
     }

     s_crt_preset_item = ui_menu_add_multiple_choice(
        MENU_SHADER_PRESET, shader, "Preset");
     populate_crt_preset_menu();
     if (!allow_shader()) {
        s_crt_preset_item->disabled = 1;
     }

     struct menu_item *geometry = ui_menu_add_folder(shader, "Geometry");
     struct menu_item *convergence = ui_menu_add_folder(shader, "Convergence");
     struct menu_item *scanlines = ui_menu_add_folder(shader, "Scanlines");
     struct menu_item *horizontal_filtering =
        ui_menu_add_folder(shader, "Horizontal Filtering");
     struct menu_item *edge_blur = ui_menu_add_folder(shader, "Edge Blur");
     struct menu_item *phosphor_mask = ui_menu_add_folder(shader, "Phosphor Mask");
     struct menu_item *bloom = ui_menu_add_folder(shader, "Bloom");
     struct menu_item *vignette = ui_menu_add_folder(shader, "Vignette");
     struct menu_item *uneven_illumination =
        ui_menu_add_folder(shader, "Uneven Illumination");
     struct menu_item *horizontal_jitter =
        ui_menu_add_folder(shader, "Horizontal Jitter");
     struct menu_item *composite_artifacts =
        ui_menu_add_folder(shader, "Composite Artifacts");
     struct menu_item *glass_reflection =
        ui_menu_add_folder(shader, "Glass Reflection");
     struct menu_item *rounded_screen_mask =
        ui_menu_add_folder(shader, "Rounded Screen Mask");
     struct menu_item *edge_glow = ui_menu_add_folder(shader, "Edge Glow");
     struct menu_item *noise = ui_menu_add_folder(shader, "Noise");
     struct menu_item *output_response =
        ui_menu_add_folder(shader, "Output Response");

     s_curvature_item =
       ui_menu_add_toggle(MENU_SHADER_CURVATURE, geometry, "Geometry", 0);

     s_curvature_x_item =
       ui_menu_add_range(MENU_SHADER_CURVATURE_X, geometry, "Horizontal Curvature",
          0, 100, 1, 10);

     s_curvature_y_item =
       ui_menu_add_range(MENU_SHADER_CURVATURE_Y, geometry, "Vertical Curvature",
          0, 100, 1, 15);

     s_skew_x_item = ui_menu_add_range(
        MENU_SHADER_SKEW_X, geometry, "Skew X", -100, 100, 1, 0);
     s_skew_y_item = ui_menu_add_range(
        MENU_SHADER_SKEW_Y, geometry, "Skew Y", -100, 100, 1, 0);
     s_trapezoid_item = ui_menu_add_range(
        MENU_SHADER_TRAPEZOID, geometry, "Trapezoid", -100, 100, 1, 0);
     s_rotation_item = ui_menu_add_range(
        MENU_SHADER_ROTATION, geometry, "Rotation", -100, 100, 1, 0);
     s_overscan_item = ui_menu_add_range(
        MENU_SHADER_OVERSCAN, geometry, "Overscan", 0, 100, 1, 0);

     s_convergence_item = ui_menu_add_toggle(
        MENU_SHADER_CONVERGENCE_ENABLE, convergence, "Convergence", 0);
     s_red_offset_x_item = ui_menu_add_range(
        MENU_SHADER_RED_OFFSET_X, convergence, "Red Offset X",
        -100, 100, 1, 25);
     s_red_offset_y_item = ui_menu_add_range(
        MENU_SHADER_RED_OFFSET_Y, convergence, "Red Offset Y",
        -100, 100, 1, 0);
     s_blue_offset_x_item = ui_menu_add_range(
        MENU_SHADER_BLUE_OFFSET_X, convergence, "Blue Offset X",
        -100, 100, 1, -25);
     s_blue_offset_y_item = ui_menu_add_range(
        MENU_SHADER_BLUE_OFFSET_Y, convergence, "Blue Offset Y",
        -100, 100, 1, 0);
     s_convergence_radial_strength_item = ui_menu_add_range(
        MENU_SHADER_CONVERGENCE_RADIAL_STRENGTH, convergence,
        "Radial Strength", 0, 100, 1, 25);

     s_scanlines_item =
        ui_menu_add_toggle(MENU_SHADER_SCANLINES, scanlines, "Scanlines", 1);

     s_scanline_weight_item =
        ui_menu_add_range(
           MENU_SHADER_SCANLINE_WEIGHT, scanlines, "Scanline Weight",
              0, 150, 1, 60);

     s_scanline_gap_brightness_item = ui_menu_add_range(
        MENU_SHADER_SCANLINE_GAP_BRIGHTNESS, scanlines, "Scanline Gap Brightness",
           0, 100, 1, 12);

     s_multisample_item =
        ui_menu_add_toggle(MENU_SHADER_MULTISAMPLE, scanlines, "Multisample", 1);

     s_horizontal_filtering_item = ui_menu_add_toggle(
        MENU_SHADER_HORIZONTAL_FILTERING, horizontal_filtering,
        "Horizontal Filtering", 1);

     s_sigma_x_item = ui_menu_add_range(
        MENU_SHADER_SIGMA_X, horizontal_filtering, "Sigma X",
           0, 100, 1, 50);

     s_edge_blur_item = ui_menu_add_toggle(
        MENU_SHADER_EDGE_BLUR_ENABLE, edge_blur, "Edge Blur", 0);
     s_edge_blur_strength_item = ui_menu_add_range(
        MENU_SHADER_EDGE_BLUR_STRENGTH, edge_blur, "Strength",
        0, 100, 1, 30);
     s_edge_blur_radius_item = ui_menu_add_range(
        MENU_SHADER_EDGE_BLUR_RADIUS, edge_blur, "Radius",
        0, 100, 1, 70);

     s_mask_enable_item =
        ui_menu_add_toggle(MENU_SHADER_MASK_ENABLE, phosphor_mask,
           "Phosphor Mask", 0);

     s_mask_item = ui_menu_add_multiple_choice(
        MENU_SHADER_MASK, phosphor_mask, "Mask Type");
     s_mask_item->num_choices = 2;
     s_mask_item->value = 0;
     strcpy(s_mask_item->choices[0], "Green/Magenta");
     strcpy(s_mask_item->choices[1], "Trinitron");

     s_mask_brightness_item = ui_menu_add_range(
        MENU_SHADER_MASK_BRIGHTNESS, phosphor_mask, "Mask Brightness",
           0, 100, 1, 70);

     s_bloom_item =
        ui_menu_add_toggle(MENU_SHADER_BLOOM_ENABLE, bloom, "Bloom", 1);

     s_bloom_factor_item = ui_menu_add_range(
        MENU_SHADER_BLOOM, bloom, "Bloom Factor",
           0, 500, 10, 150);

     s_vignette_item = ui_menu_add_toggle(
        MENU_SHADER_VIGNETTE_ENABLE, vignette, "Vignette", 0);
     s_vignette_strength_item = ui_menu_add_range(
        MENU_SHADER_VIGNETTE_STRENGTH, vignette, "Strength",
        0, 100, 1, 25);
     s_vignette_scale_item = ui_menu_add_range(
        MENU_SHADER_VIGNETTE_SCALE, vignette, "Scale", 0, 100, 1, 75);
     s_vignette_softness_item = ui_menu_add_range(
        MENU_SHADER_VIGNETTE_SOFTNESS, vignette, "Softness",
        0, 100, 1, 45);

     s_uneven_illumination_item = ui_menu_add_toggle(
        MENU_SHADER_UNEVEN_ILLUMINATION_ENABLE, uneven_illumination,
        "Uneven Illumination", 0);
     s_uneven_illumination_strength_item = ui_menu_add_range(
        MENU_SHADER_UNEVEN_ILLUMINATION_STRENGTH, uneven_illumination,
        "Strength", 0, 100, 1, 15);
     s_uneven_illumination_scale_item = ui_menu_add_range(
        MENU_SHADER_UNEVEN_ILLUMINATION_SCALE, uneven_illumination,
        "Scale", 0, 100, 1, 25);

     s_horizontal_jitter_item = ui_menu_add_toggle(
        MENU_SHADER_HORIZONTAL_JITTER_ENABLE, horizontal_jitter,
        "Horizontal Jitter", 0);
     s_horizontal_jitter_strength_item = ui_menu_add_range(
        MENU_SHADER_HORIZONTAL_JITTER_STRENGTH, horizontal_jitter,
        "Strength", 0, 100, 1, 10);
     s_horizontal_jitter_frequency_item = ui_menu_add_range(
        MENU_SHADER_HORIZONTAL_JITTER_FREQUENCY, horizontal_jitter,
        "Frequency", 0, 100, 1, 18);
     s_horizontal_jitter_speed_item = ui_menu_add_range(
        MENU_SHADER_HORIZONTAL_JITTER_SPEED, horizontal_jitter,
        "Speed", 0, 100, 1, 0);

     s_composite_artifacts_item = ui_menu_add_toggle(
        MENU_SHADER_COMPOSITE_ARTIFACTS_ENABLE, composite_artifacts,
        "Composite Artifacts", 0);
     s_composite_chroma_blur_item = ui_menu_add_range(
        MENU_SHADER_COMPOSITE_CHROMA_BLUR, composite_artifacts,
        "Chroma Blur", 0, 100, 1, 25);
     s_composite_luma_sharpen_item = ui_menu_add_range(
        MENU_SHADER_COMPOSITE_LUMA_SHARPEN, composite_artifacts,
        "Luma Sharpen", 0, 100, 1, 10);
     s_composite_color_bleed_item = ui_menu_add_range(
        MENU_SHADER_COMPOSITE_COLOR_BLEED, composite_artifacts,
        "Color Bleed", 0, 100, 1, 15);

     s_glass_reflection_item = ui_menu_add_toggle(
        MENU_SHADER_GLASS_REFLECTION_ENABLE, glass_reflection,
        "Glass Reflection", 0);
     s_glass_reflection_angle_item = ui_menu_add_range(
        MENU_SHADER_GLASS_REFLECTION_ANGLE, glass_reflection,
        "Angle", -60, 60, 1, -20);
     s_glass_reflection_width_item = ui_menu_add_range(
        MENU_SHADER_GLASS_REFLECTION_WIDTH, glass_reflection,
        "Width", 0, 100, 1, 25);
     s_glass_reflection_position_item = ui_menu_add_range(
        MENU_SHADER_GLASS_REFLECTION_POSITION, glass_reflection,
        "Position", 0, 100, 1, 35);

     s_rounded_screen_mask_item = ui_menu_add_toggle(
        MENU_SHADER_ROUNDED_SCREEN_MASK_ENABLE, rounded_screen_mask,
        "Rounded Screen Mask", 0);
     s_rounded_corner_radius_item = ui_menu_add_range(
        MENU_SHADER_ROUNDED_CORNER_RADIUS, rounded_screen_mask,
        "Corner Radius", 0, 100, 1, 20);
     s_rounded_border_softness_item = ui_menu_add_range(
        MENU_SHADER_ROUNDED_BORDER_SOFTNESS, rounded_screen_mask,
        "Border Softness", 0, 100, 1, 15);

     s_edge_glow_item = ui_menu_add_toggle(
        MENU_SHADER_EDGE_GLOW_ENABLE, edge_glow, "Edge Glow", 0);
     s_edge_glow_strength_item = ui_menu_add_range(
        MENU_SHADER_EDGE_GLOW_STRENGTH, edge_glow, "Strength",
        0, 100, 1, 15);
     s_edge_glow_width_item = ui_menu_add_range(
        MENU_SHADER_EDGE_GLOW_WIDTH, edge_glow, "Width",
        0, 100, 1, 20);

     s_noise_item = ui_menu_add_toggle(
        MENU_SHADER_NOISE_ENABLE, noise, "Noise", 0);
     s_luminance_noise_item = ui_menu_add_range(
        MENU_SHADER_LUMINANCE_NOISE, noise, "Luminance Noise",
        0, 100, 1, 10);
     s_chroma_noise_item = ui_menu_add_range(
        MENU_SHADER_CHROMA_NOISE, noise, "Chroma Noise",
        0, 100, 1, 8);
     s_noise_speed_item = ui_menu_add_range(
        MENU_SHADER_NOISE_SPEED, noise, "Speed", 0, 100, 1, 0);

     s_output_response_item =
        ui_menu_add_toggle(MENU_SHADER_OUTPUT_RESPONSE, output_response,
           "Output Response", 1);

     s_response_mode_item =
        ui_menu_add_multiple_choice(MENU_SHADER_GAMMA, output_response,
           "Response Mode");
     s_response_mode_item->num_choices = 2;
     s_response_mode_item->value = 1;
     strcpy(s_response_mode_item->choices[0], "Accurate");
     strcpy(s_response_mode_item->choices[1], "Fast");

     s_level_mapping_item =
        ui_menu_add_multiple_choice(MENU_SHADER_LEVEL_MAPPING, output_response,
           "Level Mapping");
     s_level_mapping_item->num_choices = 3;
     s_level_mapping_item->value = BMX_OUTPUT_LEVEL_MAPPING_CUBIC;
     strcpy(s_level_mapping_item->choices[0], "Linear");
     strcpy(s_level_mapping_item->choices[1], "Cubic");
     strcpy(s_level_mapping_item->choices[2], "Toe / Shoulder");

     s_input_gamma_item = ui_menu_add_range(
        MENU_SHADER_INPUT_GAMMA, output_response, "Input Gamma",
           0, 500, 10, 240);

     s_output_gamma_item = ui_menu_add_range(
        MENU_SHADER_OUTPUT_GAMMA, output_response, "Output Gamma",
           0, 500, 10, 220);

     s_response_saturation_item = ui_menu_add_range(
        MENU_SHADER_SATURATION, output_response, "Saturation",
           0, 100, 1, 100);

     s_black_level_item = ui_menu_add_range(
        MENU_SHADER_BLACK_LEVEL, output_response, "Black Level",
           0, 100, 1, 0);

     s_white_clip_item = ui_menu_add_range(
        MENU_SHADER_WHITE_CLIP, output_response, "White Clip",
           0, 100, 1, 100);

     ui_menu_add_button(MENU_SHADER_RESET_ALL, shader, "Reset");

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     parent = ui_menu_add_folder(video_parent, "VICII");
     use_scaling_params_item[0] = ui_menu_add_toggle_labels(
        MENU_USE_SCALING_PARAMS_0, parent, "Apply scaling params at boot", 1,
           "No","Yes");
  }

  palette_item[0] = emux_add_palette_options(MENU_COLOR_PALETTE_0, parent);

  child = ui_menu_add_folder(parent, "Color Adjustments...");

  brightness_item[0] =
      ui_menu_add_range(MENU_COLOR_BRIGHTNESS_0, child, "Brightness",
         0, 2000,
            10, emux_get_color_brightness(0));
  contrast_item[0] =
      ui_menu_add_range(MENU_COLOR_CONTRAST_0, child, "Contrast",
         0, 2000,
            10, emux_get_color_contrast(0));
  gamma_item[0] =
      ui_menu_add_range(MENU_COLOR_GAMMA_0, child, "Gamma",
         0, 4000,
            10, emux_get_color_gamma(0));
  tint_item[0] =
      ui_menu_add_range(MENU_COLOR_TINT_0, child, "Tint",
         0, 2000,
            10, emux_get_color_tint(0));
  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
     saturation_item[0] =
         ui_menu_add_range(MENU_COLOR_SATURATION_0, child, "Saturation",
            0, 2000,
               10, emux_get_color_saturation(0));
  } else {
     saturation_item[0] = (struct menu_item *)malloc(sizeof(struct menu_item));
     memset(saturation_item[0], 0, sizeof(struct menu_item));
  }

  ui_menu_add_button(MENU_COLOR_RESET_0, child, "Reset");

  int defaultHStretch;
  int defaultVStretch;
  if (emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
     defaultHStretch = DEFAULT_VIC_H_STRETCH;
     defaultVStretch = DEFAULT_VIC_V_STRETCH;
  } else {
     defaultHStretch = DEFAULT_VICII_H_STRETCH;
     defaultVStretch = DEFAULT_VICII_V_STRETCH;
  }

  h_center_item[0] =
      ui_menu_add_range(MENU_H_CENTER_0, parent, "H Center",
          -48, 48, 1, 0);
  v_center_item[0] =
      ui_menu_add_range(MENU_V_CENTER_0, parent, "V Center",
          -48, 48, 1, 0);
  h_border_item[0] =
      ui_menu_add_range(MENU_H_BORDER_0, parent, "H Border (px)",
          0, canvas_state[VIC_INDEX].max_border_w,
             1, canvas_state[VIC_INDEX].max_border_w);
  v_border_item[0] =
      ui_menu_add_range(MENU_V_BORDER_0, parent, "V Border (px)",
          0, canvas_state[VIC_INDEX].max_border_h,
             1, canvas_state[VIC_INDEX].max_border_h);
  child = h_stretch_item[0] =
      ui_menu_add_range(MENU_H_STRETCH_0, parent, "H Stretch Factor",
           500, canvas_state[VIC_INDEX].max_stretch_h ?
              canvas_state[VIC_INDEX].max_stretch_h : 1800,
                 5, defaultHStretch);
  child->divisor = 1000;
  child = v_stretch_item[0] =
      ui_menu_add_range(MENU_V_STRETCH_0, parent, "V Stretch Factor",
           500, 1000, 5, defaultVStretch);
  child->divisor = 1000;

  ui_menu_add_button(MENU_INTEGER_SCALE_W_0, parent, "Next H Integer Scale");
  ui_menu_add_button(MENU_INTEGER_SCALE_H_0, parent, "Next V Integer Scale");






  if (!is_composite()) {
    ui_menu_add_button(MENU_ASPECT_43_0, parent, "Aspect 4:3");
    ui_menu_add_button(MENU_ASPECT_169_0, parent, "Aspect 16:9");
  }

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     parent = ui_menu_add_folder(video_parent, "VDC");

     use_scaling_params_item[1] = ui_menu_add_toggle_labels(
        MENU_USE_SCALING_PARAMS_1, parent, "Apply scaling params at boot", 1,
           "No","Yes");

     palette_item[1] = emux_add_palette_options(MENU_COLOR_PALETTE_1, parent);

     child = ui_menu_add_folder(parent, "Color Adjustments...");

     brightness_item[1] =
         ui_menu_add_range(MENU_COLOR_BRIGHTNESS_1, child, "Brightness",
            0, 2000,
               10, emux_get_color_brightness(1));
     contrast_item[1] =
         ui_menu_add_range(MENU_COLOR_CONTRAST_1, child, "Contrast",
            0, 2000,
               10, emux_get_color_contrast(1));
     gamma_item[1] =
         ui_menu_add_range(MENU_COLOR_GAMMA_1, child, "Gamma",
            0, 4000,
               10, emux_get_color_gamma(1));
     tint_item[1] =
         ui_menu_add_range(MENU_COLOR_TINT_1, child, "Tint",
            0, 2000,
               10, emux_get_color_tint(1));

     if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
        saturation_item[1] =
            ui_menu_add_range(MENU_COLOR_SATURATION_1, child, "Saturation",
               0, 2000,
                  10, emux_get_color_saturation(1));
     } else {
        saturation_item[1] = (struct menu_item *)malloc(sizeof(struct menu_item));
        memset(saturation_item[1], 0, sizeof(struct menu_item));
     }

     ui_menu_add_button(MENU_COLOR_RESET_1, child, "Reset");

     h_center_item[1] =
         ui_menu_add_range(MENU_H_CENTER_1, parent, "H Center",
             -48, 48, 1, 0);
     v_center_item[1] =
         ui_menu_add_range(MENU_V_CENTER_1, parent, "V Center",
             -48, 48, 1, 0);
     h_border_item[1] =
         ui_menu_add_range(MENU_H_BORDER_1, parent, "H Border (px)",
             0, canvas_state[VDC_INDEX].max_border_w,
                1, canvas_state[VDC_INDEX].max_border_w);
     v_border_item[1] =
         ui_menu_add_range(MENU_V_BORDER_1, parent, "V Border (px)",
             0, canvas_state[VDC_INDEX].max_border_h,
                1, canvas_state[VDC_INDEX].max_border_h);
     child = h_stretch_item[1] =
         ui_menu_add_range(MENU_H_STRETCH_1, parent, "H Stretch Factor",
              500, canvas_state[VDC_INDEX].max_stretch_h ?
                 canvas_state[VDC_INDEX].max_stretch_h : 1800,
                    5, DEFAULT_VDC_H_STRETCH);
     child->divisor = 1000;
     child = v_stretch_item[1] =
         ui_menu_add_range(MENU_V_STRETCH_1, parent, "V Stretch Factor",
              500, 1000, 5, DEFAULT_VDC_V_STRETCH);
     child->divisor = 1000;

     ui_menu_add_button(MENU_INTEGER_SCALE_W_1, parent, "Next H Integer Scale");
     ui_menu_add_button(MENU_INTEGER_SCALE_H_1, parent, "Next V Integer Scale");

     if (!is_composite()) {
       ui_menu_add_button(MENU_ASPECT_43_1, parent, "Aspect 4:3");
       ui_menu_add_button(MENU_ASPECT_169_1, parent, "Aspect 16:9");
     }
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
     ui_menu_add_button(MENU_CALC_TIMING, video_parent,
                     "Custom HDMI/DPI mode timing calc...");
  }

  parent = ui_menu_add_folder(root, "Sound");

  volume_item = ui_menu_add_range(MENU_VOLUME, parent,
      "Volume ", 0, 100, 1, 100);






  audio_out_item = ui_menu_add_multiple_choice(MENU_AUDIO_OUT, parent,
                                               "Audio out");
  audio_out_item->num_choices = 4;
  strcpy(audio_out_item->choices[0], "Auto");
  strcpy(audio_out_item->choices[1], "Jack");
  strcpy(audio_out_item->choices[2], "HDMI");
  strcpy(audio_out_item->choices[3], "USB DAC");
  audio_out_item->value = circle_uscita_audio();

  emux_add_sound_options(parent);

  parent = ui_menu_add_folder(root, "Keyboard");

  emux_add_keyboard_options(parent);

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     c40_80_column_item = ui_menu_add_toggle_labels(
        MENU_40_80_COLUMN, parent, "40/80 Column", 1 /* default 40 col */,
        "Down","Up");
  }

  child = hotkey_cf1_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_CF1, parent, "C= + F1 Hotkey");
  child->value = HOTKEY_CHOICE_NONE;
  set_hotkey_choices(hotkey_cf1_item);
  child = hotkey_cf3_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_CF3, parent, "C= + F3 Hotkey");
  child->value = HOTKEY_CHOICE_NONE;
  set_hotkey_choices(hotkey_cf3_item);
  child = hotkey_cf5_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_CF5, parent, "C= + F5 Hotkey");
  child->value = HOTKEY_CHOICE_NONE;
  set_hotkey_choices(hotkey_cf5_item);
  child = hotkey_cf7_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_CF7, parent, "C= + F7 Hotkey");
  child->value = HOTKEY_CHOICE_MENU;
  set_hotkey_choices(hotkey_cf7_item);
  child = hotkey_tf1_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_TF1, parent,
         "CTRL + F1 Hotkey");
  child->value = HOTKEY_CHOICE_NONE;
  set_hotkey_choices(hotkey_tf1_item);
  child = hotkey_tf3_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_TF3, parent,
         "CTRL + F3 Hotkey");
  child->value = HOTKEY_CHOICE_NONE;
  set_hotkey_choices(hotkey_tf3_item);
  child = hotkey_tf5_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_TF5, parent,
         "CTRL + F5 Hotkey");
  child->value = HOTKEY_CHOICE_NONE;
  set_hotkey_choices(hotkey_tf5_item);
  child = hotkey_tf7_item =
      ui_menu_add_multiple_choice(MENU_HOTKEY_TF7, parent,
         "CTRL + F7 Hotkey");
  child->value = HOTKEY_CHOICE_MENU;
  set_hotkey_choices(hotkey_tf7_item);

  parent = ui_menu_add_folder(root, "Joyports");

  if (emu_get_num_joysticks() > 1) {
      ui_menu_add_button(MENU_SWAP_JOYSTICKS, parent, "Swap Joystick Ports");
  }

  port_1_menu_item = NULL;
  if (emu_get_num_joysticks() > 0) {
    port_1_menu_item = add_joyport_options(parent, 1);
  }
  port_2_menu_item = NULL;
  if (emu_get_num_joysticks() > 1) {
    port_2_menu_item = add_joyport_options(parent, 2);
  }




  spinner_sens_item = NULL;
  if (emux_machine_class == BMC64_MACHINE_CLASS_C64 ||
      emux_machine_class == BMC64_MACHINE_CLASS_C128 ||
      emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
    static const int percento[8] = {25, 50, 75, 100, 150, 200, 300, 400};
    int p;
    spinner_sens_item = ui_menu_add_multiple_choice(
        MENU_SPINNER_SENSITIVITY, parent, "Spinner Sensitivity");
    spinner_sens_item->num_choices = 8;
    spinner_sens_item->value = 3;
    for (p = 0; p < 8; p++) {
      snprintf(spinner_sens_item->choices[p], MAX_MENU_STR, "%d%%", percento[p]);
      spinner_sens_item->choice_ints[p] = percento[p];
    }
  }
  port_3_menu_item = NULL;
  port_4_menu_item = NULL;

  emux_add_userport_joys(parent);

  ui_menu_add_button(MENU_USB_0_CONFIGURE, parent, "Configure USB Gamepad 1...");
  ui_menu_add_button(MENU_USB_1_CONFIGURE, parent, "Configure USB Gamepad 2...");
  ui_menu_add_button(MENU_USB_2_CONFIGURE, parent, "Configure USB Gamepad 3...");
  ui_menu_add_button(MENU_USB_3_CONFIGURE, parent, "Configure USB Gamepad 4...");

  for (j = 0; j < MAX_USB_BUTTONS; j++) {
    usb_button_bits[j] = 1 << j;
  }

  for (k = 0; k < MAX_USB_DEVICES; k++) {
    usb_gamepad_reset_to_defaults(k);
  }

  ui_menu_add_button(MENU_CONFIGURE_KEYSET1, parent, "Configure Keyset 1...");
  ui_menu_add_button(MENU_CONFIGURE_KEYSET2, parent, "Configure Keyset 2...");

  parent = ui_menu_add_folder(root, "GPIO");

  child = gpio_config_item =
      ui_menu_add_multiple_choice(MENU_GPIO_CONFIG, parent, "Config");
     child->num_choices = 6;
     child->value = 0;
     strcpy(child->choices[0], "Disabled");
     strcpy(child->choices[1], "#1 (Nav+Joy)");
     strcpy(child->choices[2], "#2 (Kyb+Joy)");
     strcpy(child->choices[3], "#3 (Waveshare Hat)");
     if (circle_gpio_outputs_enabled() &&
         emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU &&
         emux_machine_class != BMC64_MACHINE_CLASS_PLUS4) {
        strcpy(child->choices[4], "#4 (Userport+Joy)");
     } else {
        strcpy(child->choices[4], "#4 (N/A)");
     }
     strcpy(child->choices[5], "#5 (Custom)");
     child->choice_ints[0] = GPIO_CONFIG_DISABLED;
     child->choice_ints[1] = GPIO_CONFIG_NAV_JOY;
     child->choice_ints[2] = GPIO_CONFIG_KYB_JOY;
     child->choice_ints[3] = GPIO_CONFIG_WAVESHARE;
     child->choice_ints[4] = GPIO_CONFIG_USERPORT;
     child->choice_ints[5] = GPIO_CONFIG_CUSTOM;

     if (!circle_gpio_enabled()) {
        child->choice_disabled[1] = 1;
        child->choice_disabled[2] = 1;
        child->choice_disabled[3] = 1;
        child->choice_disabled[4] = 1;
        child->choice_disabled[5] = 1;
     }

     if (circle_gpio_enabled()) {
        ui_menu_add_button(MENU_CONFIGURE_GPIO,
                        parent, "Configure Custom GPIO...");
     }

  ui_menu_add_divider(root);

  parent = ui_menu_add_folder(root, "Prefs");

  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
    drive_sounds_item = ui_menu_add_toggle(MENU_DRIVE_SOUND_EMULATION, parent,
                                         "Drive sound emulation", 0);



    drive_sounds_vol_item =
        ui_menu_add_range(MENU_DRIVE_SOUND_EMULATION_VOLUME, parent,
                        "Drive sound emulation volume", 0, 4000, 250, 1000);



    tape_sounds_item = ui_menu_add_toggle(MENU_TAPE_SOUND_EMULATION, parent,
                                         "Tape sound emulation", 0);
    tape_sounds_vol_item =
        ui_menu_add_range(MENU_TAPE_SOUND_EMULATION_VOLUME, parent,
                        "Tape sound emulation volume", 0, 4096, 256, 1024);
  }

  statusbar_item =
      ui_menu_add_multiple_choice(MENU_OVERLAY, parent, "Show Status Bar");
  statusbar_item->num_choices = 3;
  statusbar_item->value = 0;
  strcpy(statusbar_item->choices[OVERLAY_NEVER], "Never");
  strcpy(statusbar_item->choices[OVERLAY_ALWAYS], "Always");
  strcpy(statusbar_item->choices[OVERLAY_ON_ACTIVITY], "On Activity");

  statusbar_padding_item =
      ui_menu_add_range(MENU_OVERLAY_PADDING, parent, "Status Bar Padding",
          0, 64, 1, 0);

  statusbar_info_item =
      ui_menu_add_toggle(MENU_OVERLAY_INFO, parent,
          "Status Bar 2nd Line (temp/MHz/speed)", 1);

  vkbd_transparency_item =
      ui_menu_add_range(MENU_VKBD_TRANSPARENCY, parent, "Keyboard Transparency %",
          0, 50, 1, 0);

  reset_confirm_item = ui_menu_add_toggle(MENU_RESET_CONFIRM, parent,
                                          "Confirm Reset from Emulator", 1);

  char emu_folder[16];
  char folder_emu[16];

  strcpy (emu_folder, files_sub_dir);
  strcat (emu_folder, "/dir");

  strcpy (folder_emu, "/dir");
  strcat (folder_emu, files_sub_dir);

  dir_convention_item = ui_menu_add_toggle_labels(
        MENU_DIR_CONVENTION, parent, "Look for files in", 0,
        folder_emu, emu_folder);





  if (emux_machine_class != BMC64_MACHINE_CLASS_PLUS4EMU) {
    drive_flush_item = ui_menu_add_multiple_choice(MENU_DRIVE_FLUSH, parent,
                                                   "Flush disk writes");
    drive_flush_item->num_choices = 3;
    strcpy(drive_flush_item->choices[DRIVE_FLUSH_ON_DETACH], "On detach");
    strcpy(drive_flush_item->choices[DRIVE_FLUSH_ON_WRITE], "On write");
    strcpy(drive_flush_item->choices[DRIVE_FLUSH_ON_WRITE_LOGGED],
           "On write (log)");
    drive_flush_item->value = DRIVE_FLUSH_ON_WRITE;
  }

  warp_item = ui_menu_add_toggle(MENU_WARP_MODE, root, "Warp Mode", 0);

  // This is an undocumented feature for now. Keep invisible unless it
  // is activated by cmdline.txt
  if (raspi_demo_mode) {
    ui_menu_add_toggle(MENU_DEMO_MODE, root, "Demo Mode", raspi_demo_mode);
  }

  parent = ui_menu_add_folder(root, "Reset");
  ui_menu_add_button(MENU_SOFT_RESET, parent, "Soft Reset");
  ui_menu_add_button(MENU_HARD_RESET, parent, "Hard Reset");
  ui_menu_add_button(MENU_REBOOT_PI, parent, "Reboot BMC64");
  ui_menu_add_button(MENU_POWEROFF, parent, "Power Off BMC64");




  if (circle_get_model() == 4 || circle_get_model() == 5) {
    build_overclock_menu(root);
  }

  ui_menu_add_button(MENU_SAVE_SETTINGS, root, "Save settings");

  ui_set_on_value_changed_callback(menu_value_changed);
  ui_tasto_nella_lista = ricerca_tasto;

  int settings_loaded = load_settings();
  nas_carica();
  apply_startup_crt_preset(settings_loaded);

  if (is_composite()) {
     geo_composito_all_avvio();
  } else if (use_scaling_params_item[0]->value) {
     if (!do_use_int_scaling(FB_LAYER_VIC, 1 /* silent */)) {
        use_scaling_params_item[VIC_INDEX]->value = 0;
     }
  }
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
         use_scaling_params_item[1]->value) {
     if (!do_use_int_scaling(FB_LAYER_VDC, 1 /* silent */)) {
        use_scaling_params_item[VDC_INDEX]->value = 0;
     }
  }

  // Apply shader params
  if (!allow_shader()) {
    s_enable_shader_item->value = 0;
    emux_set_int(Setting_VideoFilter, MENU_VIDEO_FILTER_NONE);
  }
  sanity_check_shader_params();
  handle_shader_param_change();

  set_current_dir_names();

  circle_set_volume(volume_item->value);

  if (palette_item[0] != NULL) {
    emux_change_palette(0, palette_item[0]->value);
  }
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
      palette_item[1] != NULL) {
    emux_change_palette(1, palette_item[1]->value);
  }
  ui_set_hotkeys();
  ui_set_joy_devs();
  ui_set_joy_items();

  do_video_settings(FB_LAYER_VIC);
  circle_set_interpolation(scaling_interp_item->value);

  // If we were saved with the 80 column key down, let's make the
  // active display the VDC.  If this is not wanted, we'll need
  // another flag to control this behavior.  But this is probably
  // what most people want.
  if (emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
      c40_80_column_item->value == 0 &&
      active_display_item->value != MENU_ACTIVE_DISPLAY_DUE_HDMI) {
    active_display_item->value = MENU_ACTIVE_DISPLAY_VDC;
    vdc_enabled = 1;
    vic_enabled = 0;
  }

  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {
     do_video_settings(FB_LAYER_VDC);
  }
  refresh_crt_shader_runtime();
  overlay_init(statusbar_padding_item->value,
               c40_80_column_item->value,
               vkbd_transparency_item->value);
  overlay_set_info_line(statusbar_info_item->value);







  if (statusbar_always()) {
    overlay_statusbar_enable();
  }









  if (emux_machine_class == BMC64_MACHINE_CLASS_C128) {












    due_hdmi_nel_giro();
    display_all_avvio = 1;
    menu_value_changed(active_display_item);
    display_all_avvio = 0;
  }



  apply_cartridge_only();

  emux_set_joy_pot_x(0, pot_x_high_value);
  emux_set_joy_pot_x(1, pot_x_high_value);
  emux_set_joy_pot_y(0, pot_y_high_value);
  emux_set_joy_pot_y(1, pot_y_high_value);

  emux_set_video_cache(0);
  emux_set_hw_scale(0);

  // This can somehow get turned off. Make sure its always 1.
  emux_set_int(Setting_Datasette, 1);


  emux_set_int_1(Setting_FileSystemDeviceN, 1, 8);
  emux_set_int_1(Setting_FileSystemDeviceN, 1, 9);
  emux_set_int_1(Setting_FileSystemDeviceN, 1, 10);
  emux_set_int_1(Setting_FileSystemDeviceN, 1, 11);




  if (realdrive_log_item != NULL) {
    emux_set_real_drive_log(realdrive_log_item->value);
  }


  if (realdrive_veloce_item != NULL) {
    emux_set_real_drive_veloce(realdrive_veloce_item->value);
  }




  for (int u = 8; u <= 11; u++) {
    if (real_drive_wanted[u - 8]) {




      emux_want_real_drive(u);
    }
  }

  // Restore last iec dirs for all drives
  const char *tmpf;
  emux_get_string_1(Setting_FSDeviceNDir, &tmpf, 8);
  strcpy (last_iec_dir[0], tmpf);
  emux_get_string_1(Setting_FSDeviceNDir, &tmpf, 9);
  strcpy (last_iec_dir[1], tmpf);
  emux_get_string_1(Setting_FSDeviceNDir, &tmpf, 10);
  strcpy (last_iec_dir[2], tmpf);
  emux_get_string_1(Setting_FSDeviceNDir, &tmpf, 11);
  strcpy (last_iec_dir[3], tmpf);
}

int statusbar_never(void) {
  return statusbar_item->value == OVERLAY_NEVER;
}

int statusbar_always(void) {
  return statusbar_item->value == OVERLAY_ALWAYS || statusbar_forced;
}





static int clock_scalabile(void) {
  static int chiesto = 0;
  static int si = 0;
  if (!chiesto) {
    si = (overclock_current() == 0);
    chiesto = 1;
  }
  return si;
}




static unsigned arm_hz_pieno = 0;

unsigned menu_arm_hz_pieno(void) {
  if (arm_hz_pieno == 0) {
    arm_hz_pieno = circle_get_arm_clock();
  }
  return arm_hz_pieno;
}





static unsigned long guai_audio(void) {
  return (unsigned long)raspi_snd_cambi + raspi_snd_assenze +
         raspi_snd_tagliati + raspi_snd_scartati;
}

// Stuff to do when menu is activated
void menu_about_to_activate() {



  {
    static int diag_schermo_fatta = 0;

    extern unsigned bmc_righe_boot_nuove(void);

    extern unsigned kbd_tasti_col_gs;
    static unsigned gs_scritti = 0;
    static unsigned long guai_scritti = 0;
    if (!diag_schermo_fatta) {
      diag_schermo_fatta = 1;
      gs_scritti = kbd_tasti_col_gs;
      guai_scritti = guai_audio();
      circle_diag_schermo("alla prima apertura del menu");
      circle_diag_audio("alla prima apertura del menu");
    } else if (kbd_tasti_col_gs != gs_scritti) {
      gs_scritti = kbd_tasti_col_gs;
      circle_diag_schermo("a un'altra apertura del menu, col C64 GS");
    } else if (guai_audio() != guai_scritti) {




      guai_scritti = guai_audio();
      circle_diag_audio("a un'altra apertura del menu, dopo un guaio");
    } else if (bmc_righe_boot_nuove() > 0) {





      circle_diag_schermo("a un'altra apertura del menu");
    }
  }
  emux_menu_about_to_activate();
  emux_get_int(Setting_WarpMode, &warp_item->value);
  menu_update_meter();
  if (clock_scalabile()) {
    menu_arm_hz_pieno();
    circle_cpu_slow(1);
  }
}

// Stuff to do before going back to emulator
void menu_about_to_deactivate() {


  rimetti_voci_riavvio(-1);
  if (clock_scalabile()) {
    circle_cpu_slow(0);
  }
}







static int tasto_sid_c_e(void) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_C64) {
    overlay_avviso("NO SID PLAYER ON THIS MACHINE");
    return 0;
  }
  if (emux_sid_quanti_brani() <= 0) {
    overlay_avviso("NO SID LOADED");
    return 0;
  }
  return 1;
}

static void tasto_sid_brano(int passo) {
  char avviso[24];

  if (!tasto_sid_c_e()) {
    return;
  }
  emu_sid_in_pausa = 0;
  emux_sid_brano(passo);
  sid_aggiorna_righe();
  snprintf(avviso, sizeof(avviso), "TUNE %d/%d", emux_sid_quale_brano(),
           emux_sid_quanti_brani());
  overlay_avviso(avviso);
}









static void tasto_sid_avviso(const char *testo);

static void tasto_sid_salto(int secondi) {
  if (!tasto_sid_c_e()) {
    return;
  }
  if (emu_sid_in_pausa) {
    tasto_sid_avviso("TUNE PAUSED");
    return;
  }
  if (ui_lettore_sid_salta(secondi) < 0) {
    return;
  }
  tasto_sid_avviso(secondi > 0 ? "+10 SEC" : "-10 SEC");
}

static void tasto_sid_pausa(void) {
  if (!tasto_sid_c_e()) {
    return;
  }
  if (emu_sid_in_pausa) {

    emu_sid_in_pausa = 0;
    overlay_avviso("TUNE PLAYING");
    return;
  }
  emu_sid_in_pausa = 1;
  overlay_avviso("TUNE PAUSED");
  emux_trap_main_loop(emu_sid_pausa_trap, NULL);
}








static void tasto_sid_avviso(const char *testo) {
  overlay_avviso(testo);
  ui_lettore_sid_avviso(testo);
}

static void tasto_sid_voce(int i) {
  char avviso[24];
  int mute;

  if (!tasto_sid_c_e()) {
    return;
  }



  if (ui_lettore_sid_quanti() > 4) {
    tasto_sid_avviso("ALT+1-8: SID ON/OFF");
    return;
  }
  if (i / 3 >= ui_lettore_sid_quanti()) {
    snprintf(avviso, sizeof(avviso), "NO SID %d IN THIS TUNE", i / 3 + 1);
    tasto_sid_avviso(avviso);
    return;
  }
  mute = emux_sid_voci_mute() ^ (1 << i);
  emux_sid_voci_imposta(mute);
  snprintf(avviso, sizeof(avviso), "VOICE %d %s", i + 1,
           (mute & (1 << i)) ? "OFF" : "ON");
  tasto_sid_avviso(avviso);
}

static void tasto_sid_muta(int s) {
  char avviso[24];
  const int tre = 7 << (s * 3);
  int mute;

  if (!tasto_sid_c_e()) {
    return;
  }
  if (s >= ui_lettore_sid_quanti()) {
    snprintf(avviso, sizeof(avviso), "NO SID %d IN THIS TUNE", s + 1);
    tasto_sid_avviso(avviso);
    return;
  }

  mute = emux_sid_voci_mute();
  mute = (mute & tre) == tre ? (mute & ~tre) : (mute | tre);
  emux_sid_voci_imposta(mute);
  snprintf(avviso, sizeof(avviso), "SID %d %s", s + 1,
           (mute & tre) ? "OFF" : "ON");
  tasto_sid_avviso(avviso);
}






static void tasto_sid_lato(int s) {
  static const char *const nomi[3] = { "LEFT", "CENTER", "RIGHT" };
  char avviso[24];
  int lato;

  if (!tasto_sid_c_e()) {
    return;
  }
  if (s >= ui_lettore_sid_quanti()) {
    snprintf(avviso, sizeof(avviso), "NO SID %d IN THIS TUNE", s + 1);
    tasto_sid_avviso(avviso);
    return;
  }
  lato = emux_sid_lato_cambia(s);
  if (lato < 0 || lato > 2) {
    tasto_sid_avviso("CHANNELS: MONO");
    return;
  }
  snprintf(avviso, sizeof(avviso), "SID %d %s", s + 1, nomi[lato]);
  tasto_sid_avviso(avviso);
}










extern const char *emux_sid_motore_cambia(void) __attribute__((weak));

static void tasto_sid_motore(void) {
  const char *nome = NULL;

  if (!tasto_sid_c_e()) {
    return;
  }
  if (emux_sid_motore_cambia) {
    nome = emux_sid_motore_cambia();
  }
  if (nome == NULL) {
    nome = "NO reSIDfp HERE";
  }



  overlay_avviso(nome);
  ui_lettore_sid_motore(nome);
}

static void tasto_sid_mostra(int cosa) {
  static const char *nomi[3] = { "VU METERS", "TIME", "TUNE INFO" };
  char avviso[24];

  if (!tasto_sid_c_e()) {
    return;
  }
  snprintf(avviso, sizeof(avviso), "%s %s", nomi[cosa],
           ui_lettore_sid_mostra(cosa) ? "ON" : "OFF");
  tasto_sid_avviso(avviso);
}




static const char *nome_del_tasto_rapido(int b) {



  if ((b >= BTN_ASSIGN_MODELLO8_NONE && b <= BTN_ASSIGN_MODELLO8_VERO) ||
      (b >= BTN_ASSIGN_MODELLO9_NONE && b <= BTN_ASSIGN_MODELLO9_VERO)) {
    return "modello di drive";
  }
  switch (b) {
    case BTN_ASSIGN_OVERLAY_2:        return "barra di stato";
    case BTN_ASSIGN_OVERLAY_INFO_2:   return "seconda riga della barra";
    case BTN_ASSIGN_ASPETTO_2:        return "proporzioni dello schermo";
    case BTN_ASSIGN_ATTACH_DISK_8_2:
    case BTN_ASSIGN_ATTACH_DISK_9_2:  return "monta un dischetto";
    case BTN_ASSIGN_DETACH_DISK_8_2:
    case BTN_ASSIGN_DETACH_DISK_9_2:  return "smonta il dischetto";
    case BTN_ASSIGN_ATTACH_CART_2:
    case BTN_ASSIGN_DETACH_CART_2:    return "la cartuccia";
    case BTN_ASSIGN_ATTACH_TAPE_2:
    case BTN_ASSIGN_DETACH_TAPE_2:    return "il nastro";
    case BTN_ASSIGN_AUTOSTART_2:      return "autostart";
    case BTN_ASSIGN_RESET_HARD:
    case BTN_ASSIGN_RESET_HARD2:      return "reset duro";
    case BTN_ASSIGN_RESET_SOFT:
    case BTN_ASSIGN_RESET_SOFT2:      return "reset morbido";
    case BTN_ASSIGN_WARP:             return "warp";
    case BTN_ASSIGN_REU_2:            return "REU accesa/spenta";
    case BTN_ASSIGN_SID_2:            return "lettore di SID";
    case BTN_ASSIGN_SID_FERMA:        return "SID fermo e reset";
    case BTN_ASSIGN_REU_MISURA:       return "misura della REU";
    case BTN_ASSIGN_REU_MONTA:        return "immagine della REU";
    case BTN_ASSIGN_REU_SMONTA:       return "immagine della REU via";
    case BTN_ASSIGN_SID_PREV:         return "brano del SID di prima";
    case BTN_ASSIGN_SID_NEXT:         return "brano del SID dopo";
    case BTN_ASSIGN_SID_PAUSA:        return "brano del SID in pausa";
    case BTN_ASSIGN_SID_INDIETRO:     return "brano del SID 10 secondi indietro";
    case BTN_ASSIGN_SID_AVANTI:       return "brano del SID 10 secondi avanti";
    case BTN_ASSIGN_SOLCO_SU:         return "scanline piu' forti";
    case BTN_ASSIGN_SOLCO_GIU:        return "scanline piu' deboli";
    case BTN_ASSIGN_MUTO:             return "silenziatore";
    case BTN_ASSIGN_FLUSH_DISK:       return "riversa il dischetto";
    default:                          return "";
  }
}

// These are called on the main loop
static void menu_quick_func_dentro(int button_assignment);







void menu_quick_func(int button_assignment) {
  char nome[40];









  if (button_assignment == BTN_ASSIGN_VOLUME_SU ||
      button_assignment == BTN_ASSIGN_VOLUME_GIU) {
    menu_quick_func_dentro(button_assignment);
    return;
  }
  snprintf(nome, sizeof(nome), "il tasto rapido %d (%s)", button_assignment,
           nome_del_tasto_rapido(button_assignment));
  passo_prima(nome);
  menu_quick_func_dentro(button_assignment);
  passo_fatto(nome);
}

static void menu_quick_func_dentro(int button_assignment) {
  int value;

  if (emux_handle_quick_func(button_assignment, fullpath)) {
    return;
  }


  if ((button_assignment >= BTN_ASSIGN_TAPE_PLAY2 &&
       button_assignment <= BTN_ASSIGN_TAPE_ZERO2) ||
      button_assignment == BTN_ASSIGN_TAPE_FF_2X ||
      button_assignment == BTN_ASSIGN_TAPE_REW_2X) {
    if (!menu_nastro_pronto()) {
      return;
    }
  }

  switch (button_assignment) {
  case BTN_ASSIGN_TAPE_PLAY2:
    emux_tape_control(EMUX_TAPE_PLAY);
    break;
  case BTN_ASSIGN_TAPE_STOP2:
    emux_tape_control(EMUX_TAPE_STOP);
    break;
  case BTN_ASSIGN_TAPE_FF2:
    emux_tape_control(EMUX_TAPE_FASTFORWARD);
    break;
  case BTN_ASSIGN_TAPE_REW2:
    emux_tape_control(EMUX_TAPE_REWIND);
    break;
  case BTN_ASSIGN_TAPE_ZERO2:
    emux_tape_control(EMUX_TAPE_ZERO);
    break;
  case BTN_ASSIGN_TAPE_FF_2X:
    emux_tape_control_veloce(EMUX_TAPE_FASTFORWARD, 2);
    overlay_avviso("FF 2X");
    break;
  case BTN_ASSIGN_TAPE_REW_2X:
    emux_tape_control_veloce(EMUX_TAPE_REWIND, 2);
    overlay_avviso("REW 2X");
    break;
  case BTN_ASSIGN_WARP:
    emux_get_int(Setting_WarpMode, &value);
    toggle_warp(1 - value);
    break;
  case BTN_ASSIGN_SWAP_PORTS:



    if (emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
      overlay_avviso("ONE JOYSTICK PORT ONLY");
      break;
    }
    menu_swap_joysticks();
    break;
  case BTN_ASSIGN_VKBD_TOGGLE:
    if (vkbd_showing) {
       vkbd_disable();
    } else {
       vkbd_enable();
    }
    break;
  case BTN_ASSIGN_STATUS_TOGGLE:
    // Ignore this if it's already showing.
    if (statusbar_item->value == OVERLAY_ALWAYS)
      return;

    if (statusbar_showing || statusbar_forced) {
      // Dismiss
      statusbar_forced = 0;
      overlay_statusbar_dismiss();
    } else {
      statusbar_forced = 1;
      overlay_statusbar_enable();
    }
    break;
  case BTN_ASSIGN_TAPE_MENU:

    if (emux_tape_presente() < 0) {
      overlay_avviso("NO TAPE ON THIS MACHINE");
      break;
    }
    show_tape_osd_menu();
    break;

  case BTN_ASSIGN_ATTACH_DISK_8_2:
    unit = 8;
    tasto_apre_i_file(DIR_DISKS, FILTER_DISK, MENU_DISK_FILE);
    break;
  case BTN_ASSIGN_ATTACH_DISK_9_2:
    unit = 9;
    tasto_apre_i_file(DIR_DISKS, FILTER_DISK, MENU_DISK_FILE);
    break;
  case BTN_ASSIGN_DETACH_DISK_8_2:
    emux_detach_disk(8);
    attached_disk_name[0][0] = '\0';
    break;
  case BTN_ASSIGN_DETACH_DISK_9_2:
    emux_detach_disk(9);
    attached_disk_name[1][0] = '\0';
    break;
  case BTN_ASSIGN_ATTACH_CART_2:
    tasto_apre_la_cartuccia();
    break;
  case BTN_ASSIGN_DETACH_CART_2:
    tasto_stacca_la_cartuccia();
    break;
  case BTN_ASSIGN_ATTACH_TAPE_2:
    tasto_apre_i_file(DIR_TAPES, FILTER_TAPE, MENU_TAPE_FILE);
    break;
  case BTN_ASSIGN_DETACH_TAPE_2:
    emux_detach_tape();
    break;
  case BTN_ASSIGN_AUTOSTART_2:
    tasto_apre_i_file(DIR_PRGS, FILTER_NONE, MENU_AUTOSTART_FILE);
    break;
  case BTN_ASSIGN_MODELLO8_NONE:
    tasto_mette_il_modello(8, 1, 0);
    break;
  case BTN_ASSIGN_MODELLO8_1541:
    tasto_mette_il_modello(8, 2, 1541);
    break;
  case BTN_ASSIGN_MODELLO8_1541II:
    tasto_mette_il_modello(8, 3, DRIVE_MOD_1541II);
    break;
  case BTN_ASSIGN_MODELLO8_1570:
    tasto_mette_il_modello(8, 4, 1570);
    break;
  case BTN_ASSIGN_MODELLO8_1571:
    tasto_mette_il_modello(8, 5, DRIVE_MOD_1571);
    break;
  case BTN_ASSIGN_MODELLO8_1581:
    tasto_mette_il_modello(8, 6, DRIVE_MOD_1581);
    break;
  case BTN_ASSIGN_MODELLO8_VERO:



    if (plus4emu_due_modelli(8, 7)) {
      break;
    }






    if (!emux_real_drive_available()) {
      overlay_avviso("NO XUM FOUND");
      break;
    }

    passo_prima("prendere il drive vero sull'8");
    emux_want_real_drive(8);
    passo_fatto("prendere il drive vero sull'8");
    emux_refresh_drive_items(8);
    avviso_del_modello(8, "REAL DRIVE");
    break;
  case BTN_ASSIGN_MODELLO9_NONE:
    tasto_mette_il_modello(9, 1, 0);
    break;
  case BTN_ASSIGN_MODELLO9_1541:
    tasto_mette_il_modello(9, 2, 1541);
    break;
  case BTN_ASSIGN_MODELLO9_1541II:
    tasto_mette_il_modello(9, 3, DRIVE_MOD_1541II);
    break;
  case BTN_ASSIGN_MODELLO9_1570:
    tasto_mette_il_modello(9, 4, 1570);
    break;
  case BTN_ASSIGN_MODELLO9_1571:
    tasto_mette_il_modello(9, 5, DRIVE_MOD_1571);
    break;
  case BTN_ASSIGN_MODELLO9_1581:
    tasto_mette_il_modello(9, 6, DRIVE_MOD_1581);
    break;
  case BTN_ASSIGN_MODELLO9_VERO:



    if (plus4emu_due_modelli(9, 7)) {
      break;
    }






    if (!emux_real_drive_available()) {
      overlay_avviso("NO XUM FOUND");
      break;
    }
    emux_want_real_drive(9);
    emux_refresh_drive_items(9);
    avviso_del_modello(9, "REAL DRIVE");
    break;
  case BTN_ASSIGN_SCHERMO_2:
    tasto_commuta_schermo();
    break;
  case BTN_ASSIGN_COLONNE_2:
    tasto_colonne();
    break;
  case BTN_ASSIGN_OVERLAY_2: {



    static int overlay_prima = OVERLAY_ON_ACTIVITY;

    if (statusbar_item == NULL) {
      break;
    }
    if (statusbar_item->value != OVERLAY_NEVER) {
      overlay_prima = statusbar_item->value;
      statusbar_item->value = OVERLAY_NEVER;
    } else {
      statusbar_item->value = overlay_prima;
    }
    menu_value_changed(statusbar_item);
    break;
  }
  case BTN_ASSIGN_OVERLAY_INFO_2: {



    if (statusbar_info_item == NULL) {
      break;
    }
    statusbar_info_item->value = !statusbar_info_item->value;
    menu_value_changed(statusbar_info_item);
    overlay_avviso(statusbar_info_item->value ? "STATUS BAR 2ND LINE ON"
                                              : "STATUS BAR 2ND LINE OFF");
    break;
  }
  case BTN_ASSIGN_MUTO:
    tasto_muto();
    break;
  case BTN_ASSIGN_SID_2:
    tasto_sid();
    break;
  case BTN_ASSIGN_REU_2:
    tasto_reu();
    break;
  case BTN_ASSIGN_REU_MISURA:
    tasto_reu_misura();
    break;
  case BTN_ASSIGN_REU_MONTA:
    tasto_reu_monta();
    break;
  case BTN_ASSIGN_REU_SMONTA:
    tasto_reu_smonta();
    break;
  case BTN_ASSIGN_SID_PREV:
  case BTN_ASSIGN_SID_NEXT:
    tasto_sid_brano(button_assignment == BTN_ASSIGN_SID_NEXT ? 1 : -1);
    break;
  case BTN_ASSIGN_SID_PAUSA:
    tasto_sid_pausa();
    break;
  case BTN_ASSIGN_SID_VOCE_1:
  case BTN_ASSIGN_SID_VOCE_2:
  case BTN_ASSIGN_SID_VOCE_3:
  case BTN_ASSIGN_SID_VOCE_4:
  case BTN_ASSIGN_SID_VOCE_5:
  case BTN_ASSIGN_SID_VOCE_6:
  case BTN_ASSIGN_SID_VOCE_7:
  case BTN_ASSIGN_SID_VOCE_8:
  case BTN_ASSIGN_SID_VOCE_9:
    tasto_sid_voce(button_assignment - BTN_ASSIGN_SID_VOCE_1);
    break;
  case BTN_ASSIGN_SID_MUTA_1:
  case BTN_ASSIGN_SID_MUTA_2:
  case BTN_ASSIGN_SID_MUTA_3:
    tasto_sid_muta(button_assignment - BTN_ASSIGN_SID_MUTA_1);
    break;


  case BTN_ASSIGN_SID_VOCE_10:
  case BTN_ASSIGN_SID_VOCE_11:
  case BTN_ASSIGN_SID_VOCE_12:
    tasto_sid_voce(9 + button_assignment - BTN_ASSIGN_SID_VOCE_10);
    break;
  case BTN_ASSIGN_SID_MUTA_4:
    tasto_sid_muta(3);
    break;

  case BTN_ASSIGN_SID_LATO_1:
  case BTN_ASSIGN_SID_LATO_2:
  case BTN_ASSIGN_SID_LATO_3:
  case BTN_ASSIGN_SID_LATO_4:
    tasto_sid_lato(button_assignment - BTN_ASSIGN_SID_LATO_1);
    break;


  case BTN_ASSIGN_SID_LATO_5:
  case BTN_ASSIGN_SID_LATO_6:
  case BTN_ASSIGN_SID_LATO_7:
  case BTN_ASSIGN_SID_LATO_8:
    tasto_sid_lato(4 + button_assignment - BTN_ASSIGN_SID_LATO_5);
    break;
  case BTN_ASSIGN_SID_MUTA_5:
  case BTN_ASSIGN_SID_MUTA_6:
  case BTN_ASSIGN_SID_MUTA_7:
  case BTN_ASSIGN_SID_MUTA_8:
    tasto_sid_muta(4 + button_assignment - BTN_ASSIGN_SID_MUTA_5);
    break;
  case BTN_ASSIGN_SID_INDIETRO:
  case BTN_ASSIGN_SID_AVANTI:
    tasto_sid_salto(button_assignment == BTN_ASSIGN_SID_AVANTI ? 10 : -10);
    break;
  case BTN_ASSIGN_SID_VU:
    tasto_sid_mostra(0);
    break;
  case BTN_ASSIGN_SID_TEMPO:
    tasto_sid_mostra(1);
    break;
  case BTN_ASSIGN_SID_INFO:
    tasto_sid_mostra(2);
    break;
  case BTN_ASSIGN_SID_MOTORE:
    tasto_sid_motore();
    break;
  case BTN_ASSIGN_SID_FERMA:

    if (emux_machine_class != BMC64_MACHINE_CLASS_C64) {
      overlay_avviso("NO SID PLAYER ON THIS MACHINE");
      break;
    }
    emu_sid_in_pausa = 0;
    emux_sid_ferma();
    sid_aggiorna_righe();
    overlay_avviso("SID PLAYER OFF");
    break;
  case BTN_ASSIGN_VOLUME_SU:
    tasto_volume(5);
    break;
  case BTN_ASSIGN_VOLUME_GIU:
    tasto_volume(-5);
    break;
  case BTN_ASSIGN_SOLCO_SU:
    tasto_scanline(10);
    break;
  case BTN_ASSIGN_SOLCO_GIU:
    tasto_scanline(-10);
    break;
  case BTN_ASSIGN_ASPETTO_2:
    tasto_commuta_aspetto(0  );
    break;
  case BTN_ASSIGN_ASPETTO_ALTRO:
    tasto_commuta_aspetto(1  );
    break;
  case BTN_ASSIGN_CART_MENU:







    if (emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU) {
      tasto_apre_la_cartuccia();
    } else {
      emux_show_cart_osd_menu();
    }
    break;
  case BTN_ASSIGN_RESET_MENU:
    show_reset_osd_menu();
    return;
  case BTN_ASSIGN_RESET_HARD:
    if (reset_confirm_item->value) {
      // Will come back here with HARD2 if confirmed.
      show_confirm_osd_menu(BTN_ASSIGN_RESET_HARD2);
      return;
    }
  // fallthrough
  case BTN_ASSIGN_RESET_HARD2:
    menu_machine_reset(0 /* hard */, 0 /* no pop */);
    break;
  case BTN_ASSIGN_RESET_SOFT:
    if (reset_confirm_item->value) {
      // Will come back here with SOFT2 if confirmed.
      show_confirm_osd_menu(BTN_ASSIGN_RESET_SOFT2);
      return;
    }
  // fallthrough
  case BTN_ASSIGN_RESET_SOFT2:
    menu_machine_reset(1 /* soft */, 0 /* no pop */);
    break;
  case BTN_ASSIGN_ACTIVE_DISPLAY:


    do {
       active_display_item->value++;
       if (active_display_item->value > MENU_ACTIVE_DISPLAY_DUE_HDMI) {
          active_display_item->value = 0;
       }
    } while (active_display_item->choice_disabled[active_display_item->value]);
    menu_value_changed(active_display_item);
    break;
  case BTN_ASSIGN_PIP_LOCATION:
    pip_location_item->value++;
    if (pip_location_item->value > 3) {
       pip_location_item->value = 0;
    }
    menu_value_changed(pip_location_item);
    break;
  case BTN_ASSIGN_PIP_SWAP:
    pip_swapped_item->value = 1 - pip_swapped_item->value;
    menu_value_changed(pip_swapped_item);
    break;
  case BTN_ASSIGN_40_80_COLUMN:
    c40_80_column_item->value = 1 - c40_80_column_item->value;
    menu_value_changed(c40_80_column_item);
    break;
  default:
    break;
  }
}

int emu_get_gpio_config() {
  return gpio_config_item->choice_ints[gpio_config_item->value];
}

int emu_get_num_joysticks(void) {
  if (emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
    return 1;
  } else if (emux_machine_class == BMC64_MACHINE_CLASS_PET) {
    return 0;
  }
  return 2;
}

const char* function_to_string(int button_func) {
  switch (button_func) {
    case BTN_ASSIGN_UNDEF:
       return "None";
    case BTN_ASSIGN_FIRE:
       return "Fire";
    case BTN_ASSIGN_MENU:
       return "Menu";
    case BTN_ASSIGN_WARP:
       return "Warp";
    case BTN_ASSIGN_STATUS_TOGGLE:
       return "Status Toggle";
    case BTN_ASSIGN_SWAP_PORTS:
       return "Swap Ports";
    case BTN_ASSIGN_UP:
       return "Up";
    case BTN_ASSIGN_DOWN:
       return "Down";
    case BTN_ASSIGN_LEFT:
       return "Left";
    case BTN_ASSIGN_RIGHT:
       return "Right";




    case BTN_ASSIGN_POTX:
       return "Fire 2 (POT X)";
    case BTN_ASSIGN_POTY:
       return "Fire 3 (POT Y)";
    case BTN_ASSIGN_TAPE_MENU:
       return "Tape OSD";
    case BTN_ASSIGN_CART_MENU:
       return "Cart OSD";
    case BTN_ASSIGN_CART_FREEZE:
       return "Cart Freeze";
    case BTN_ASSIGN_RESET_MENU:
       return "Reset OSD";
    case BTN_ASSIGN_RESET_HARD:
       return "Hard Reset";
    case BTN_ASSIGN_RESET_SOFT:
       return "Soft Reset";
    case BTN_ASSIGN_RUN_STOP_BACK:
       return "Menu Back";
    case BTN_ASSIGN_CUSTOM_KEY_1:
       return "Custom Key 1";
    case BTN_ASSIGN_CUSTOM_KEY_2:
       return "Custom Key 2";
    case BTN_ASSIGN_CUSTOM_KEY_3:
       return "Custom Key 3";
    case BTN_ASSIGN_CUSTOM_KEY_4:
       return "Custom Key 4";
    case BTN_ASSIGN_CUSTOM_KEY_5:
       return "Custom Key 5";
    case BTN_ASSIGN_CUSTOM_KEY_6:
       return "Custom Key 6";
    case BTN_ASSIGN_ACTIVE_DISPLAY:
       return "Cycle Display";
    case BTN_ASSIGN_PIP_LOCATION:
       return "PIP Location";
    case BTN_ASSIGN_PIP_SWAP:
       return "PIP Swap";
    case BTN_ASSIGN_40_80_COLUMN:
       return "40/80 Column Key";
    case BTN_ASSIGN_VKBD_TOGGLE:
       return "Virtual Keyboard";
    case BTN_ASSIGN_FLUSH_DISK:
       return "Flush Disks";
    default:
       return "Unknown";
  }
}

void emux_geometry_changed(int layer) {

  // Update the allowed min for border trim items. This lets the user
  // start padding the edges with negative trim values.
  // These are expressed in terms of percentage of the max because they
  // are going into the range item.
  int canvas_index = -1;
  if (layer == FB_LAYER_VIC) {
     canvas_index = VIC_INDEX;
  } else if (layer == FB_LAYER_VDC) {
     canvas_index = VDC_INDEX;
  }

  int dpx, dpy, fbw, fbh, sw, sh, dw, dh;
  circle_get_fbl_dimensions(layer,
                            &dpx, &dpy,
                            &fbw, &fbh,
                            &sw, &sh,
                            &dw, &dh);

  if (canvas_index >= 0) {
    int max_padding_w = MIN(
        canvas_state[canvas_index].extra_offscreen_border_left,
        canvas_state[canvas_index].extra_offscreen_border_right);
    int max_padding_h = canvas_state[canvas_index].first_displayed_line;

    // Update the allowed max h stretch based on the display width and height
    double max_scale = ceil((double)dpx / (double)dpy) * 1000;

    if (h_border_item[canvas_index]) h_border_item[canvas_index]->min = 0;
    if (v_border_item[canvas_index]) v_border_item[canvas_index]->min = 0;
    if (h_border_item[canvas_index]) h_border_item[canvas_index]->max = canvas_state[canvas_index].max_border_w + max_padding_w;
    if (v_border_item[canvas_index]) v_border_item[canvas_index]->max = canvas_state[canvas_index].max_border_h + max_padding_h;
    if (h_stretch_item[canvas_index]) h_stretch_item[canvas_index]->max = max_scale;

    // Stuff these into the canvas state
    canvas_state[canvas_index].max_padding_w = max_padding_w;
    canvas_state[canvas_index].max_padding_h = max_padding_h;
    canvas_state[canvas_index].max_stretch_h = max_scale;
  }



  traccia_geometria(layer, "avvio");

  if (layer == FB_LAYER_VIC) {
     // When the first display changes, we need to update the UI since
     // it's frame buffer dimensions must match.
     ui_geometry_changed(dpx, dpy, fbw, fbh, sw, sh, dw, dh);
  }
}

void emux_frame_buffer_changed(int layer) {
  int canvas_index = layer == FB_LAYER_VIC ? VIC_INDEX : VDC_INDEX;














  if (canvas_index == VIC_INDEX && is_composite()) {
     if (geo_comp[geo_std()].presente != 0x3F &&
         h_border_item[VIC_INDEX] != NULL) {
        do_use_int_scaling(layer, 1 /* silent */);
     }
  } else if (use_scaling_params_item[canvas_index] != NULL &&
      use_scaling_params_item[canvas_index]->value) {
     if (!do_use_int_scaling(layer, 1 /* silent */)) {
        use_scaling_params_item[canvas_index]->value = 0;
     }
  }
  do_video_settings(layer);
}
