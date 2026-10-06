/*
 * vice_api.c - VICE specific impl of emux_api.h
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

#include "emux_api.h"
#include "bmc_nib.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// VICE includes
#include "raspi_machine.h"
#include "autostart.h"
#include "autostart-prg.h"
#include "diskimage.h"
#include "attach.h"
#include "iecbus.h"
#ifdef HAVE_REALDEVICE
#include "opencbm_xum1541.h"
#endif
#include "cartridge.h"
#include "archdep.h"
#include "interrupt.h"
#include "machine.h"
#include "mem.h"
#include "videoarch.h"
#include "menu.h"
#include "menu_text_layout.h"
#include "menu_timing.h"
#include "bmc64_ui.h"
#include "keyboard.h"
#include "kbd.h"
#include "drive-sound.h"
#include "keymap.h"
#include "sysfile.h"
#include "userport.h"
#include "demo.h"
#include "datasette.h"
#include "tapeport.h"
#include "log.h"
#include "resources.h"
#include "maincpu.h"
#include "vsync.h"
#include "snapshot.h"
#include "drive.h"
#include "drive-check.h"
#include "machine-drive.h"
#include "iecdrive.h"
#include "joyport.h"
#include "joyport/joystick.h"
#include "vdrive.h"
#include "vdrive-internal.h"
#include "tape.h"
#include "sid.h"
#include "sid-resources.h"
#include "userport/userport_joystick.h"
#include "cbmimage.h"

// RASPI includes
#include "circle.h"
#include "keycodes.h"

struct menu_item *sid_dual_item;

struct menu_item *sid_mono_item;
static int uscita_mono = 0;



int sid_lettore_stereo = 0;
struct menu_item *sid_base_address_item;
struct menu_item *sid_engine_item;
struct menu_item *sid_model_item[2];
struct menu_item *sid_filter_item;

struct menu_item *bmc_sidcard_item;
struct menu_item *bmc_sidcard_address_item;
struct menu_item *sid_resampling_item;

static struct menu_item *resid_filtri_item[6];
#ifdef HAVE_RESIDFP
static struct menu_item *residfp_item[6];
#endif

struct menu_item *keyboard_mapping_item;
struct menu_item *keyboard_layout_item;

// TODO: Fix these
extern struct menu_item *port_3_menu_item;
extern struct menu_item *port_4_menu_item;
extern struct menu_item* add_joyport_options(struct menu_item* parent, int port);
extern void ui_set_joy_items();

struct menu_item *enable_item;
struct menu_item *swap_item;
struct menu_item *adapter_type_item;




static struct menu_item *riga_restore_item;
static char riga_restore[40] = "Restore key: ?";

static void aggiorna_riga_restore(void) {
  if (key_ctrl_restore1 == -1) {
    strcpy(riga_restore, "Restore key: nessuno");
  } else if (key_ctrl_restore2 == -1 ||
             key_ctrl_restore2 == key_ctrl_restore1) {
    snprintf(riga_restore, sizeof(riga_restore), "Restore key: %s",
             keycode_to_string((long)key_ctrl_restore1));
  } else {


    snprintf(riga_restore, sizeof(riga_restore), "Restore key: %s, %s",
             keycode_to_string((long)key_ctrl_restore1),
             keycode_to_string((long)key_ctrl_restore2));
  }
  if (riga_restore_item != NULL) {
    strncpy(riga_restore_item->name, riga_restore,
            sizeof(riga_restore_item->name) - 1);
  }
}

void raspi_keymap_changed(int, int, signed long);

// SID resource setters mark the engine dirty even when the value is unchanged.
// Avoid reinitializing state that was just restored from a snapshot.
static void set_int_if_changed(const char *resource, int value) {
  int current;
  if (resources_get_int(resource, &current) < 0 || current != value) {
    resources_set_int(resource, value);
  }
}

// Make sure SID options are sane for this model




static int userport_e_joystick(int dev) {
  return dev >= USERPORT_DEVICE_JOYSTICK_CGA &&
         dev <= USERPORT_DEVICE_JOYSTICK_WOJ;
}

static int userport_tipo_scelto(void) {
  if (adapter_type_item != NULL) {
    return adapter_type_item->choice_ints[adapter_type_item->value];
  }
  return USERPORT_DEVICE_JOYSTICK_CGA;
}

static void check_sid_options() {
  int value;
  resources_get_int("SidResidSampling", &value);
  // For less capable Pi's, we force fast sampling.
  if (circle_get_model() < 3) {
     set_int_if_changed("SidResidSampling", SID_RESID_SAMPLING_FAST);
  } else if (circle_get_model() < 4) {
     if (value == SID_RESID_SAMPLING_RESAMPLING) {
       set_int_if_changed("SidResidSampling",
                          SID_RESID_SAMPLING_FAST_RESAMPLING);
     }
  }








  // When dual sid is enabled, SidStereo=1, SoundOutput=2 and
  // the audio driver must be configured for 2 channel output.
  // Otherwise, SidStereo=0, SoundOutput=1 and the driver is
  // configured for 1 channel.
  //
  // Never allow SidStereo=0 and SoundOutput=2 because is results
  // in duplicating the mono channel to 2 channels which costs
  // enough CPU on the Pi2 to blow the vsync budget. When dual sid
  // is enabled, we have some VICE changes to produce the 2nd SID
  // stream on another core so there is no performance penalty.
  if (circle_get_model() >= 2) {




















     set_int_if_changed("SoundOutput",
                        uscita_mono ? 1 : (sid_lettore_stereo ? 2 : 0));
  } else {
     // Always mono for < Pi2
      set_int_if_changed("SidStereo", 0);
      set_int_if_changed("SoundOutput", 1);
  }
}

void emu_machine_init(int raster_skip_enabled, int raster_skip2_enabled) {
  switch (machine_class) {
    case VICE_MACHINE_C64:
    case VICE_MACHINE_C64SC:
       emux_machine_class = BMC64_MACHINE_CLASS_C64;
       break;
    case VICE_MACHINE_SCPU64:
       emux_machine_class = BMC64_MACHINE_CLASS_SCPU64;
       break;
    case VICE_MACHINE_C128:
       emux_machine_class = BMC64_MACHINE_CLASS_C128;
       break;
    case VICE_MACHINE_VIC20:
       emux_machine_class = BMC64_MACHINE_CLASS_VIC20;
       break;
    case VICE_MACHINE_PLUS4:
       emux_machine_class = BMC64_MACHINE_CLASS_PLUS4;
       break;
    case VICE_MACHINE_PET:
       emux_machine_class = BMC64_MACHINE_CLASS_PET;
       break;
    default:
       assert(0);
       break;
  }

  canvas_state[VIC_INDEX].raster_skip = raster_skip_enabled ? 2 : 1;
  canvas_state[VDC_INDEX].raster_skip = raster_skip2_enabled ? 2 : 1;

  // If raster skip enabled via kernel params, enable lines.
  set_raster_lines(raster_skip_enabled, raster_skip2_enabled);
}

void emux_trap_main_loop_ui(void) {
  interrupt_maincpu_trigger_trap(emu_pause_trap, 0);
}

void emux_trap_main_loop(void (*trap_func)(uint16_t, void *data), void* data) {
  interrupt_maincpu_trigger_trap(trap_func, data);
}

void emux_kbd_set_latch_keyarr(int row, int col, int value) {
  demo_reset_timeout();
  keyboard_set_keyarr(row, col, value);
}






int emux_disk_is_read_only(int unit) {
  struct vdrive_s *vd;
  disk_image_t *img;

  if (unit < 8 || unit >= 8 + NUM_DISK_UNITS) {
    return 0;
  }
  vd = file_system_get_vdrive((unsigned int)unit);
  if (vd == NULL) {
    return 0;
  }
  img = vdrive_get_image(vd, 0);
  return (img != NULL && img->read_only) ? 1 : 0;
}




static char *nib_in_g64(char *filename, char *g64, unsigned int len) {
  if (!bmc_nib_e_nib(filename)) {
    return filename;
  }
  if (bmc_nib_prepara(filename, g64, len) < 0) {
    log_error(LOG_DEFAULT, "%s: the .NIB image could not be converted", filename);
    return NULL;
  }
  return g64;
}

int emux_attach_disk_image(int unit, char* filename) {
  static char g64[256];
  filename = nib_in_g64(filename, g64, sizeof g64);
  if (filename == NULL) {
    return -1;
  }


  int esito = file_system_attach_disk(unit, 0, filename);
  if (esito >= 0 && unit >= 8 && unit < 8 + NUM_DISK_UNITS) {
    drive_sound_disk_change(1, unit - 8);
    if (emux_disk_is_read_only(unit)) {
      log_message(LOG_DEFAULT,
                  "unit %d: image attached READ ONLY, writes will fail",
                  unit);
    }
  }
  return esito;
}

void emux_detach_disk(int unit) {
  file_system_detach_disk(unit, 0);
  if (unit >= 8 && unit < 8 + NUM_DISK_UNITS) {
    drive_sound_disk_change(0, unit - 8);
  }
}









void emux_detach_cart(int bank) {
  int reu = 0;
  // Ignore bank for vice
  if (resources_get_int("REU", &reu) < 0) {
    reu = 0;
  }
  cartridge_detach_image(CARTRIDGE_NONE);
  if (reu) {
    resources_set_int("REU", 1);
  }
}

void emux_set_cart_default(void) {
   cartridge_set_default();
}

void emux_real_drive_molla(void) {
#ifdef HAVE_REALDEVICE
  raspi_opencbm_molla();
#endif
}

void emux_reset(int soft) {












  keyboard_clear_keymatrix();









  kbd_aspetta_il_rilascio();
  raspi_alza_i_tasti(3);

  machine_trigger_reset(soft ?
      MACHINE_RESET_MODE_RESET_CPU : MACHINE_RESET_MODE_POWER_CYCLE);
}










void emux_sid_uscita_come_i_chip(void) {
  check_sid_options();
}




const char *emux_sid_modello(int chip) {




  static const char *const risorsa[8] = { "SidModel", "Sid2Model", "Sid3Model",
                                          "Sid3Model", "Sid3Model", "Sid3Model",
                                          "Sid3Model", "Sid3Model" };
  int m = -1;

  if (chip < 0 || chip > 7 || resources_get_int(risorsa[chip], &m) < 0) {
    return NULL;
  }
  switch (m) {
    case SID_MODEL_6581:
      return "6581";
    case SID_MODEL_8580:
      return "8580";
    case SID_MODEL_8580D:
      return "8580D";
    default:
      return NULL;
  }
}

unsigned long emux_orologio_cpu(void) {
  return (unsigned long)maincpu_clk;
}




unsigned long emux_cicli_al_secondo(void) {
  return (unsigned long)machine_get_cycles_per_second();
}

void emux_alza_la_tastiera(void) {
  keyboard_key_clear();
}

int emux_save_state(char *filename) {
  int status = machine_write_snapshot(filename, 1, 1, 0);
  if (status < 0) {
    const char *module = snapshot_get_current_module();
    log_error(LOG_DEFAULT,
              "Snapshot save failed: file=%s status=%d error=%d module=%s",
              filename, status, snapshot_get_error(),
              module != NULL ? module : "none");
  }
  return status;
}

int emux_load_state(char *filename) {
  int status = machine_read_snapshot(filename, 0);
  // Somehow, this gets turned off. Vice bug?


  resources_set_int("TapePort1Device", TAPEPORT_DEVICE_DATASETTE);

  if (machine_class == VICE_MACHINE_PET) {
     // This is a hack to get sound working after a snapshot load.
     // For some reason, the sound engine is closed after a load
     // Snapshots are disabled for PET but keeping this here in case
     // it's needed again.
     int sid_engine;
     resources_get_int("SidEngine", &sid_engine);
     resources_set_int("SidEngine", 1-sid_engine);
     resources_set_int("SidEngine", sid_engine);
  }

  // This makes sure sid options are sane.
  check_sid_options();

  int tmp;
  tmp = 0;
  resources_get_int("UserportDevice", &tmp);
  enable_item->value = userport_e_joystick(tmp);

  resources_get_int("UserportDevice", &tmp);
  for (int i=0;i<adapter_type_item->num_choices;i++) {
     if (adapter_type_item->choice_ints[0] == tmp) {
         adapter_type_item->value = i;
         break;
     }
  }



  if (sid_dual_item != NULL) {
     resources_get_int("SidStereo", &tmp);
     sid_dual_item->value = tmp;
  }

  if (sid_base_address_item != NULL) {
     resources_get_int("Sid2AddressStart", &tmp);
     for (int i=0;i<sid_base_address_item->num_choices;i=i+1) {
        if (sid_base_address_item->choice_ints[i] == tmp) {
           sid_base_address_item->value = i;
           break;
        }
     }
  }

  // Do other menu items too.
  //   Drive%iType, Drive%iParallelCable
  //   Drive%iRAM2000-A000
  //   KeymapIndex, SidEngine, SidModel, SidFilters, DriveSoundEmulation
  //   DriveSoundEmulationVolume, C128ColumnKey, DatasetteResetWithCPU
  //   IECDevice%i, FSDevice%iDir
  

  return status;
}

static int emux_tape_control_interno(int cmd);







int emux_tape_presente(void) {
  const char *nome;

  if (machine_class == VICE_MACHINE_SCPU64) {
    return -1;
  }
  nome = tape_get_file_name(0);
  return (nome != NULL && nome[0] != '\0') ? 1 : 0;
}




int emux_tape_control_veloce(int cmd, int volte) {
  datasette_svelto = (volte >= 2) ? 2.0 : 1.0;
  return emux_tape_control_interno(cmd);
}

int emux_tape_control(int cmd) {
  datasette_svelto = 1.0;
  return emux_tape_control_interno(cmd);
}

static int emux_tape_control_interno(int cmd) {






  if (emux_tape_presente() <= 0) {
    return -1;
  }
  switch (cmd) {
    case EMUX_TAPE_PLAY:
      datasette_control(0, DATASETTE_CONTROL_START);
      break;
    case EMUX_TAPE_STOP:
      datasette_control(0, DATASETTE_CONTROL_STOP);
      break;
    case EMUX_TAPE_REWIND:
      datasette_control(0, DATASETTE_CONTROL_REWIND);
      break;
    case EMUX_TAPE_FASTFORWARD:
      datasette_control(0, DATASETTE_CONTROL_FORWARD);
      break;
    case EMUX_TAPE_RECORD:
      datasette_control(0, DATASETTE_CONTROL_RECORD);
      break;
    case EMUX_TAPE_RESET:
      datasette_control(0, DATASETTE_CONTROL_RESET);
      break;
    case EMUX_TAPE_ZERO:
      datasette_control(0, DATASETTE_CONTROL_RESET_COUNTER);
      break;
    default:
      assert(0);
      break;
  }



  return 0;
}
























static void autostart_scegli_la_strada(void)
{
   int tipo = DRIVE_TYPE_NONE;
   const char *dischetto = NULL;
   int modo;

   resources_get_int_sprintf("Drive%iType", &tipo, 8);







   resources_get_string("AutostartPrgDiskImage", &dischetto);

   modo = (tipo == DRIVE_TYPE_NONE ||
           dischetto == NULL || dischetto[0] == '\0')
              ? AUTOSTART_PRG_MODE_INJECT
              : AUTOSTART_PRG_MODE_DISK;
   resources_set_int("AutostartPrgMode", modo);




   log_message(LOG_DEFAULT,
               "autostart: unita 8 tipo %d, dischetto temporaneo '%s', "
               "strada %s",
               tipo, dischetto != NULL ? dischetto : "(nessuno)",
               modo == AUTOSTART_PRG_MODE_INJECT ? "RAM" : "dischetto");
}
















static int autostart_riprova_iniettando(char *filename, unsigned int modo)
{
   int mode_prima = AUTOSTART_PRG_MODE_DISK;

   resources_get_int("AutostartPrgMode", &mode_prima);
   if (mode_prima == AUTOSTART_PRG_MODE_INJECT) {
      return -1;
   }
   log_message(LOG_DEFAULT, "autostart: il dischetto temporaneo non e'"
                            " riuscito, riprovo mettendo il programma"
                            " dritto in RAM");
   resources_set_int("AutostartPrgMode", AUTOSTART_PRG_MODE_INJECT);
   return autostart_autodetect(filename, NULL, 0, modo);
}

int emux_autostart_file(char* filename) {
   static char g64[256];
   int esito;

   filename = nib_in_g64(filename, g64, sizeof g64);
   if (filename == NULL) {
      return -1;
   }
   autostart_scegli_la_strada();
   esito = autostart_autodetect(filename, NULL, 0, AUTOSTART_MODE_RUN);
   if (esito < 0) {
      esito = autostart_riprova_iniettando(filename, AUTOSTART_MODE_RUN);
   }
   return esito;
}

int emux_load_prg_file(char* filename) {
   static char g64[256];
   int esito;

   filename = nib_in_g64(filename, g64, sizeof g64);
   if (filename == NULL) {
      return -1;
   }
   autostart_scegli_la_strada();
   esito = autostart_autodetect(filename, NULL, 0, AUTOSTART_MODE_LOAD);
   if (esito < 0) {
      esito = autostart_riprova_iniettando(filename, AUTOSTART_MODE_LOAD);
   }
   return esito;
}

void emux_drive_change_model(int unit) {
  struct menu_item *item;



  const int larghezza_max = 38;
  const int altezza_max = 22;

  int current_drive_type;
  int drive_reale_acceso = 0;
  resources_get_int_sprintf("Drive%iType", &current_drive_type, unit);
#ifdef HAVE_REALDEVICE
  drive_reale_acceso = emux_is_real_drive(unit);
#endif

  static int num_supported_drives = 19;
  static int supported_drives[] = {
     DRIVE_TYPE_1540,
     DRIVE_TYPE_1541,
     DRIVE_TYPE_1541II,
     DRIVE_TYPE_1551,
     DRIVE_TYPE_1570,
     DRIVE_TYPE_1571,
     DRIVE_TYPE_1571CR,
     DRIVE_TYPE_1581,
     DRIVE_TYPE_2000,
     DRIVE_TYPE_4000,
     DRIVE_TYPE_CMDHD,
     DRIVE_TYPE_2031,
     DRIVE_TYPE_2040,
     DRIVE_TYPE_3040,
     DRIVE_TYPE_4040,
     DRIVE_TYPE_1001,
     DRIVE_TYPE_8050,
     DRIVE_TYPE_8250,
     DRIVE_TYPE_9000,
  };

  static const char* drive_labels[] = {
     "1540",
     "1541",
     "1541II",
     "1551",
     "1570",
     "1571",
     "1571CR",
     "1581",
     "FD-2000",
     "FD-4000",
     "CMD HD",
     "2031",
     "2040",
     "3040",
     "4040",
     "SFD-1001",
     "8050",
     "8250",
     "D9090/60",
  };










  int usabili[32];
  int mancanti[8];
  int quanti = 0;
  int quanti_mancanti = 0;
  unsigned int bus = iec_available_busses();
  for (int i = 0 ; i < num_supported_drives; i++) {
    int tipo = supported_drives[i];



    if (tipo == DRIVE_TYPE_1540 && machine_class != VICE_MACHINE_VIC20) {
      continue;
    }












    if (drive_check_type(tipo, unit - 8) > 0) {
      usabili[quanti++] = i;
      continue;
    }
    if (   (tipo == DRIVE_TYPE_2000
         || tipo == DRIVE_TYPE_4000
         || tipo == DRIVE_TYPE_CMDHD)
        && drive_check_bus(tipo, bus)) {
      mancanti[quanti_mancanti++] = i;
    }
  }

#ifdef HAVE_REALDEVICE



  int mostra_reale = (bus & IEC_BUS_IEC) != 0;
  int reale_disponibile = emux_real_drive_available();



  int dove_reale = quanti > 0 ? quanti - 1 : 0;
#endif





  int larghezza = (int)strlen("None");
  for (int k = 0; k < quanti; k++) {
    int n = (int)strlen(drive_labels[usabili[k]]);
    if (n > larghezza) {
      larghezza = n;
    }
  }
  for (int k = 0; k < quanti_mancanti; k++) {
    int n = (int)strlen(drive_labels[mancanti[k]]) + (int)strlen(" (no ROM)");
    if (n > larghezza) {
      larghezza = n;
    }
  }
  int righe = quanti + quanti_mancanti + 1;
#ifdef HAVE_REALDEVICE
  if (mostra_reale) {
    if ((int)strlen("Real drive") > larghezza) {
      larghezza = (int)strlen("Real drive");
    }
    righe++;
  }
#endif
  larghezza += (int)strlen(" (*)") + 2;
  if (larghezza > larghezza_max) {
    larghezza = larghezza_max;
  }
  if (righe > altezza_max) {
    righe = altezza_max;
  }

  struct menu_item *model_root = ui_push_menu(larghezza, righe);

  item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "None");
  item->value = DRIVE_TYPE_NONE;
  if (current_drive_type == DRIVE_TYPE_NONE && !drive_reale_acceso) {
    strcat(item->displayed_value, " (*)");
  }

  for (int k = 0; k < quanti; k++) {
#ifdef HAVE_REALDEVICE
    if (mostra_reale && k == dove_reale) {



      item = ui_menu_add_button(MENU_DRIVE_MODEL_REAL, model_root,
                                "Real drive");
      if (!reale_disponibile) {
        item->disabled = 1;
      } else if (drive_reale_acceso) {
        strcat(item->displayed_value, " (*)");
      }
    }
#endif
    int i = usabili[k];
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, drive_labels[i]);
    item->value = supported_drives[i];



    if (current_drive_type == supported_drives[i] && !drive_reale_acceso) {
      strcat(item->displayed_value, " (*)");
    }
  }



  for (int k = 0; k < quanti_mancanti; k++) {
    char scritta[48];
    int i = mancanti[k];
    snprintf(scritta, sizeof(scritta), "%s (no ROM)", drive_labels[i]);
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, scritta);
    item->value = supported_drives[i];
    item->disabled = 1;
  }
}







int emux_get_shiftlock(void) {
  return keyboard_get_shiftlock();
}






















































































































































































































































































































































































































































void overlay_avviso(const char *testo);

int emux_real_drive_available(void) {








#ifdef HAVE_REALDEVICE
  return raspi_opencbm_available();
#else
  return 0;
#endif
}






#ifdef HAVE_REALDEVICE











static int reale_passa_dal_bus(void) {
  return machine_class != VICE_MACHINE_VIC20;
}







#define RIPROVE_REALE 30
static int reale_voluto[4];
static int reale_tentativi[4];
static int reale_giri;



static int tde_prima_del_reale[4] = { -1, -1, -1, -1 };





static int bus_prima_del_reale[4] = { -1, -1, -1, -1 };
#endif

void emux_set_real_drive(int unit) {
#ifdef HAVE_REALDEVICE
  int tmp = 1;
  int fsdev = ATTACH_DEVICE_NONE;

  if (unit < 8 || unit > 11) {
    return;
  }

  if (unit >= 8 && unit <= 11 && tde_prima_del_reale[unit - 8] < 0) {
    resources_get_int_sprintf("Drive%iTrueEmulation", &tmp, unit);
    tde_prima_del_reale[unit - 8] = tmp;
  }

  if (unit >= 8 && unit <= 11 && bus_prima_del_reale[unit - 8] < 0) {
    tmp = 0;
    if (resources_get_int_sprintf("BusDevice%i", &tmp, unit) >= 0) {
      bus_prima_del_reale[unit - 8] = tmp;
    }
  }







  resources_set_int_sprintf("TrapDevice%i", reale_passa_dal_bus() ? 0 : 1,
                            unit);
  resources_set_int_sprintf("Drive%iTrueEmulation", 0, unit);
  if (reale_passa_dal_bus()) {
    resources_set_int_sprintf("BusDevice%i", 1, unit);
  }
  resources_set_int_sprintf("FileSystemDevice%i", ATTACH_DEVICE_REAL, unit);





  reale_voluto[unit - 8] = 1;
  resources_get_int_sprintf("FileSystemDevice%i", &fsdev, unit);
  if (fsdev == ATTACH_DEVICE_REAL) {
    reale_tentativi[unit - 8] = 0;
  } else {
    reale_tentativi[unit - 8]++;
  }
#else
  (void)unit;
#endif
}






void emux_set_real_drive_log(int acceso) {
#ifdef HAVE_REALDEVICE
  raspi_opencbm_set_log(acceso);
#else
  (void)acceso;
#endif
}

int emux_real_drive_counters(int *v, int n) {
#ifdef HAVE_REALDEVICE
  return raspi_opencbm_conti(v, n);
#else
  (void)v; (void)n;
  return 0;
#endif
}

void emux_real_drive_reset_counters(void) {
#ifdef HAVE_REALDEVICE
  raspi_opencbm_azzera_conti();
#endif
}

int emux_real_drive_measure(int *v, int n) {
#ifdef HAVE_REALDEVICE
  return raspi_opencbm_misura(v, n);
#else
  (void)v; (void)n;
  return 0;
#endif
}

void emux_set_real_drive_veloce(int acceso) {
#ifdef HAVE_REALDEVICE
  raspi_opencbm_set_veloce(acceso);
#else
  (void)acceso;
#endif
}

int emux_real_drive_recover(int unit, int *v, int n) {
#ifdef HAVE_REALDEVICE
  return raspi_opencbm_rimetti_in_piedi(unit, v, n);
#else
  (void)unit; (void)v; (void)n;
  return 0;
#endif
}

void emux_set_real_drive_respiro(int us) {
#ifdef HAVE_REALDEVICE
  raspi_opencbm_set_respiro(us);
#else
  (void)us;
#endif
}

int emux_real_drive_turbo(int unit, char *nome, int nome_max,
                          char *dove, int dove_max, int *v, int n) {
#ifdef HAVE_REALDEVICE
  return raspi_opencbm_turbo_prova(unit, nome, nome_max, dove, dove_max, v, n);
#else
  (void)unit; (void)nome; (void)nome_max; (void)dove; (void)dove_max;
  (void)v; (void)n;
  return 0;
#endif
}

void emux_want_real_drive(int unit) {
#ifdef HAVE_REALDEVICE
  if (unit < 8 || unit > 11) {
    return;
  }
  reale_voluto[unit - 8] = 1;
  reale_tentativi[unit - 8] = 0;
  if (emux_real_drive_available()) {
    emux_set_real_drive(unit);
  }
#else
  (void)unit;
#endif
}



void emux_real_drive_tick(void) {
#ifdef HAVE_REALDEVICE
  int u;
  int lavoro = 0;

  for (u = 0; u < 4; u++) {
    if (reale_voluto[u] && reale_tentativi[u] < RIPROVE_REALE) {
      lavoro = 1;
    }
  }
  if (!lavoro) {
    return;
  }
  if (++reale_giri < 50) {
    return;
  }
  reale_giri = 0;




  if (!emux_real_drive_available()) {







    static unsigned long usb_pronta_da = 0;
    if (bmc_usb_pronta && usb_pronta_da == 0) {
      usb_pronta_da = circle_get_ticks();
      if (usb_pronta_da == 0) {
        usb_pronta_da = 1;
      }
    } else if (usb_pronta_da != 0 &&
               circle_get_ticks() - usb_pronta_da >= 3000000UL) {
      int tornate = 0;
      for (u = 0; u < 4; u++) {
        int fsdev = ATTACH_DEVICE_NONE;
        if (!reale_voluto[u]) {
          continue;
        }
        resources_get_int_sprintf("FileSystemDevice%i", &fsdev, u + 8);
        if (fsdev == ATTACH_DEVICE_REAL) {
          continue;
        }
        emux_leave_real_drive(u + 8);
        emux_refresh_drive_items(u + 8);
        tornate++;




      }
      if (tornate) {
        overlay_avviso("NO XUM FOUND");
      }
    }
    return;
  }

  for (u = 0; u < 4; u++) {
    int fsdev = ATTACH_DEVICE_NONE;
    if (!reale_voluto[u] || reale_tentativi[u] >= RIPROVE_REALE) {
      continue;
    }
    resources_get_int_sprintf("FileSystemDevice%i", &fsdev, u + 8);
    if (fsdev == ATTACH_DEVICE_REAL) {
      reale_tentativi[u] = 0;
      continue;
    }
    emux_set_real_drive(u + 8);
    emux_refresh_drive_items(u + 8);
  }
#endif
}





void emux_leave_real_drive(int unit) {
#ifdef HAVE_REALDEVICE
  int fsdev = ATTACH_DEVICE_NONE;
  int bus = 0;

  if (unit < 8 || unit > 11) {
    return;
  }

  resources_get_int_sprintf("FileSystemDevice%i", &fsdev, unit);
  resources_get_int_sprintf("BusDevice%i", &bus, unit);








  if (fsdev != ATTACH_DEVICE_REAL && bus == 0 && !reale_voluto[unit - 8]) {
    return;
  }



  reale_voluto[unit - 8] = 0;
  reale_tentativi[unit - 8] = 0;

  resources_set_int_sprintf("FileSystemDevice%i", ATTACH_DEVICE_FS, unit);





  resources_set_int_sprintf("Drive%iTrueEmulation",
                            tde_prima_del_reale[unit - 8] >= 0
                                ? tde_prima_del_reale[unit - 8] : 1, unit);
  tde_prima_del_reale[unit - 8] = -1;

  resources_set_int_sprintf("BusDevice%i",
                            bus_prima_del_reale[unit - 8] >= 0
                                ? bus_prima_del_reale[unit - 8] : 0, unit);
  bus_prima_del_reale[unit - 8] = -1;
#else
  (void)unit;
#endif
}








void emux_use_emulated_drive(int unit) {
  if (unit < 8 || unit > 11) {
    return;
  }
  resources_set_int_sprintf("TrapDevice%i", 0, unit);
  resources_set_int_sprintf("Drive%iTrueEmulation", 1, unit);
}






int emux_is_real_drive(int unit) {
  int fsdev = ATTACH_DEVICE_NONE;
  int bus = 0;

  if (unit < 8 || unit > 11) {
    return 0;
  }
  resources_get_int_sprintf("FileSystemDevice%i", &fsdev, unit);
  resources_get_int_sprintf("BusDevice%i", &bus, unit);
#ifdef HAVE_REALDEVICE




  if (reale_voluto[unit - 8]) {
    return 1;
  }
#endif
  return (fsdev == ATTACH_DEVICE_REAL || bus != 0) ? 1 : 0;
}






void emux_refresh_drive_items(int unit) {
  int tmp = 0;

  if (unit < 8 || unit > 11) {
    return;
  }
  if (voce_tde[unit - 8] != NULL) {
    resources_get_int_sprintf("Drive%iTrueEmulation", &tmp, unit);
    voce_tde[unit - 8]->value = tmp ? 1 : 0;
  }
  if (voce_iec[unit - 8] != NULL) {
    emux_get_int_1(Setting_IECDeviceN, &tmp, unit);
    voce_iec[unit - 8]->value = tmp ? 1 : 0;
  }
}

void emux_add_drive_option(struct menu_item* root, int drive) {
  int tmp;





  if (emux_machine_class == BMC64_MACHINE_CLASS_PET ||
      emux_machine_class == BMC64_MACHINE_CLASS_PLUS4EMU) {
    return;
  }

  if (drive < 0) {
     // Options applicable to all drives


     tmp = 0;
     emux_get_int_1(Setting_IECDeviceN, &tmp, 8);
     ui_menu_add_toggle(MENU_VIRTUAL_DEVICES, root, "Virtual Devices", tmp);
     return;
  }

  assert (drive >=8 && drive <=11);

  struct menu_item* parent = ui_menu_add_folder(root, "Options");

  resources_get_int_sprintf("Drive%iTrueEmulation", &tmp, drive);
  voce_tde[drive - 8] =
      ui_menu_add_toggle(MENU_DRIVE_TRUE_EMULATION, parent,
                         "True Drive Emulation", tmp);
  voce_tde[drive - 8]->sub_id = drive;



  if (emux_machine_class != BMC64_MACHINE_CLASS_C64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_SCPU64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_C128) {
    return;
  }

  resources_get_int_sprintf("Drive%iParallelCable", &tmp, drive);

  int index = 0;
  switch (tmp) {
    case DRIVE_PC_NONE:
       index = 0; break;
    case DRIVE_PC_STANDARD:
       index = 1; break;
    case DRIVE_PC_DD3:
       index = 2; break;
    case DRIVE_PC_FORMEL64:
       index = 3; break;
    default:
       return;
  }

  int id;
  switch (drive) {
     case 8:
        id = MENU_PARALLEL_8;
        break;
     case 9:
        id = MENU_PARALLEL_9;
        break;
     case 10:
        id = MENU_PARALLEL_10;
        break;
     case 11:
        id = MENU_PARALLEL_11;
        break;
     default:
        id = MENU_PARALLEL_8;
  }

  struct menu_item* child =
      ui_menu_add_multiple_choice(id, parent, "Parallel Cable");
  child->num_choices = 4;
  child->value = index;
  strcpy(child->choices[0], "None");
  strcpy(child->choices[1], "Standard");
  strcpy(child->choices[2], "Dolphin DOS");
  strcpy(child->choices[3], "Formel 64");
  child->choice_ints[0] = DRIVE_PC_NONE;
  child->choice_ints[1] = DRIVE_PC_STANDARD;
  child->choice_ints[2] = DRIVE_PC_DD3;
  child->choice_ints[3] = DRIVE_PC_FORMEL64;

  resources_get_int_sprintf("Drive%iRAM2000", &tmp, drive);
  ui_menu_add_toggle(MENU_DRIVE_RAM_2000, parent, "RAM 2000", tmp)
     ->sub_id = drive;

  resources_get_int_sprintf("Drive%iRAM4000", &tmp, drive);
  ui_menu_add_toggle(MENU_DRIVE_RAM_4000, parent, "RAM 4000", tmp)
     ->sub_id = drive;

  resources_get_int_sprintf("Drive%iRAM6000", &tmp, drive);
  ui_menu_add_toggle(MENU_DRIVE_RAM_6000, parent, "RAM 6000", tmp)
     ->sub_id = drive;

  resources_get_int_sprintf("Drive%iRAM8000", &tmp, drive);
  ui_menu_add_toggle(MENU_DRIVE_RAM_8000, parent, "RAM 8000", tmp)
     ->sub_id = drive;

  resources_get_int_sprintf("Drive%iRAMA000", &tmp, drive);
  ui_menu_add_toggle(MENU_DRIVE_RAM_A000, parent, "RAM A000", tmp)
     ->sub_id = drive;

  int button;
  switch (drive) {
     case 8:
        id = MENU_CMDHD_MODE_8;
        button = drive_get_button(0);
        break;
     case 9:
        id = MENU_CMDHD_MODE_9;
        button = drive_get_button(1);
        break;
     case 10:
        id = MENU_CMDHD_MODE_10;
        button = drive_get_button(2);
        break;
     case 11:
        id = MENU_CMDHD_MODE_11;
        button = drive_get_button(3);
        break;
     default:
        id = MENU_CMDHD_MODE_8;
        button = drive_get_button(0);
        break;
  }

  if (button == 1) {
     index = 2; // Configuration
  } else if (button == 6) {
     index = 1; // Initialization
  } else {
     index = 0; // Normal
  }

  child = ui_menu_add_multiple_choice(id, parent, "CMDHD Mode");
  child->num_choices = 3;
  child->value = index;
  strcpy(child->choices[0], "Normal");
  strcpy(child->choices[1], "Initialization");
  strcpy(child->choices[2], "Configuration");
  child->choice_ints[0] = 0; // all switches off
  child->choice_ints[1] = 6; // swap8 and swap9 on
  child->choice_ints[2] = 1; // write protect on
}

void emux_create_disk(struct menu_item* item, fullpath_func f_fullpath) {
     char ext[5];
     int image_type;
     switch (item->id) {
       case MENU_CREATE_D64_FILE:
         image_type = DISK_IMAGE_TYPE_D64;
         strcpy(ext, ".d64");
         break;
       case MENU_CREATE_D67_FILE:
         image_type = DISK_IMAGE_TYPE_D67;
         strcpy(ext, ".d67");
         break;
       case MENU_CREATE_D71_FILE:
         image_type = DISK_IMAGE_TYPE_D71;
         strcpy(ext, ".d71");
         break;
       case MENU_CREATE_D80_FILE:
         image_type = DISK_IMAGE_TYPE_D80;
         strcpy(ext, ".d80");
         break;
       case MENU_CREATE_D81_FILE:
         image_type = DISK_IMAGE_TYPE_D81;
         strcpy(ext, ".d81");
         break;
       case MENU_CREATE_D82_FILE:
         image_type = DISK_IMAGE_TYPE_D82;
         strcpy(ext, ".d82");
         break;
       case MENU_CREATE_D1M_FILE:
         image_type = DISK_IMAGE_TYPE_D1M;
         strcpy(ext, ".d1m");
         break;
       case MENU_CREATE_D2M_FILE:
         image_type = DISK_IMAGE_TYPE_D2M;
         strcpy(ext, ".d2m");
         break;
       case MENU_CREATE_D4M_FILE:
         image_type = DISK_IMAGE_TYPE_D4M;
         strcpy(ext, ".d4m");
         break;
       case MENU_CREATE_G64_FILE:
         image_type = DISK_IMAGE_TYPE_G64;
         strcpy(ext, ".g64");
         break;
       case MENU_CREATE_G71_FILE:
         image_type = DISK_IMAGE_TYPE_G71;
         strcpy(ext, ".g71");
         break;
       case MENU_CREATE_P64_FILE:
         image_type = DISK_IMAGE_TYPE_P64;
         strcpy(ext, ".p64");
         break;
       case MENU_CREATE_X64_FILE:
         image_type = DISK_IMAGE_TYPE_D64;
         strcpy(ext, ".x64");
         break;
       case MENU_CREATE_DHD_FILE:
         image_type = DISK_IMAGE_TYPE_DHD;
         strcpy(ext, ".dhd");
         break;
       default:
         return;
     }

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
          strcat(fname, ext);
        } else {
          ui_error("Too long");
          return;
        }
      } else {
        if (strncasecmp(dot, ext, 4) != 0) {
          ui_error("Wrong extension");
          return;
        }
      }
    } else {
      // Don't allow overwriting an existing file. Just ignore it.
      return;
    }

    ui_info("Creating...");
    if (vdrive_internal_create_format_disk_image(
         f_fullpath(DIR_DISKS, fname), "DISK", image_type) < 0) {
      ui_pop_menu();
      ui_error("Create disk image failed");
    } else {
      ui_pop_menu();
      ui_pop_menu();
      ui_info("Disk Created");
    }
}

void emux_create_tape(struct menu_item* item, fullpath_func f_fullpath) {
    char ext[5];
    int image_type;

    image_type = DISK_IMAGE_TYPE_TAP;
    strcpy(ext, ".tap");

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
          strcat(fname, ext);
        } else {
          ui_error("Too long");
          return;
        }
      } else {
        if (strncasecmp(dot, ext, 4) != 0) {
          ui_error("Wrong extension");
          return;
        }
      }
    } else {
      // Don't allow overwriting an existing file. Just ignore it.
      return;
    }

    ui_info("Creating...");
    if (cbmimage_create_image(
         f_fullpath(DIR_TAPES, fname), image_type) < 0) {
      ui_pop_menu();
      ui_error("Create tape image failed");
    } else {
      ui_pop_menu();
      ui_pop_menu();
      ui_info("Tape Created");
    }
}

void emux_set_joy_port_device(int port_num, int dev_id) {
  int vice_id = JOYPORT_ID_NONE;
  switch (dev_id) {
     case JOYDEV_NONE:
        vice_id = JOYPORT_ID_NONE;
        break;
     case JOYDEV_MOUSE:
        vice_id = JOYPORT_ID_MOUSE_1351;
        break;
     case JOYDEV_MOUSE_MICROMYS:
        vice_id = JOYPORT_ID_MOUSE_MICROMYS;
        break;
     case JOYDEV_SPINNER:





        vice_id = JOYPORT_ID_PADDLES;
        if (port_num == 1 || port_num == 2) {
           resources_set_int(port_num == 1 ? "PaddlesInput1" : "PaddlesInput2",
                             1);
           joystick_set_paddle_value(port_num - 1, 0);
        }
        break;
     default:
        vice_id = JOYPORT_ID_JOYSTICK;
        break;
  }
  switch (port_num) {
  case 1:
     resources_set_int("JoyPort1Device", vice_id);
     break;
  case 2:
     resources_set_int("JoyPort2Device", vice_id);
     break;
  case 3:
     resources_set_int("JoyPort3Device", vice_id);
     break;
  case 4:
     resources_set_int("JoyPort4Device", vice_id);
     break;
  }
}

void emux_set_joy_pot_x(int port, int value) {
   joystick_set_potx(port, value);
}

void emux_set_joy_pot_y(int port, int value) {
   joystick_set_poty(port, value);
}

int emux_attach_tape_image(char* filename) {
   return tape_image_attach(1, filename);
}

void emux_detach_tape(void) {
   tape_image_detach(1);
}

static int viceSidEngineToBmcChoice(int viceEngine) {
  switch (viceEngine) {
  case SID_ENGINE_FASTSID:
    return MENU_SID_ENGINE_FAST;
  case SID_ENGINE_RESID:
    return MENU_SID_ENGINE_RESID;
#ifdef HAVE_RESIDFP
  case SID_ENGINE_RESIDFP:
    return MENU_SID_ENGINE_RESIDFP;
#endif
  default:
    return MENU_SID_ENGINE_RESID;
  }
}

static int viceSidModelToBmcChoice(int viceModel) {
  switch (viceModel) {
  case SID_MODEL_6581:
    return MENU_SID_MODEL_6581;
  case SID_MODEL_8580:
    return MENU_SID_MODEL_8580;
  default:
    return MENU_SID_MODEL_6581;
  }
}

static int viceSidResamplingToBmcChoice(int method) {
  switch (method) {
  case SID_RESID_SAMPLING_FAST:
    return MENU_SID_SAMPLING_FAST;
  case SID_RESID_SAMPLING_INTERPOLATION:
    return MENU_SID_SAMPLING_INTERPOLATION;
  case SID_RESID_SAMPLING_RESAMPLING:
    return MENU_SID_SAMPLING_RESAMPLING;
  case SID_RESID_SAMPLING_FAST_RESAMPLING:
    return MENU_SID_SAMPLING_FAST_RESAMPLING;
  default:
    return MENU_SID_SAMPLING_FAST;
  }
}

void emux_add_tape_options(struct menu_item* parent) {
}




static int layout_conta_per_indice(int idx) {
  return idx == KBD_INDEX_SYM || idx == KBD_INDEX_POS;
}


static int keymap_esiste(const char *nome) {
  char *dove = NULL;
  int res = sysfile_locate(nome, machine_name, &dove);
  lib_free(dove);
  return res == 0;
}



static int tipo_tastiera(void) {
  int tipo;
  if (resources_get_int("KeyboardType", &tipo) < 0) {
    return -1;
  }
  return tipo;
}

static const struct {
  const char *nome;
  int mappa;
} le_disposizioni[] = {
  { "US", KBD_MAPPING_US },
  { "Italiana", KBD_MAPPING_IT },
  { "British", KBD_MAPPING_UK },
  { "Deutsch", KBD_MAPPING_DE },
};
#define N_DISPOSIZIONI ((int)(sizeof(le_disposizioni) / sizeof(le_disposizioni[0])))











static int mappatura_per_il_menu(int idx) {
  switch (idx) {
  case KBD_INDEX_POS:     return KEYBOARD_MAPPING_POS;
  case KBD_INDEX_USERPOS: return KEYBOARD_MAPPING_MAXI;
  default:                return KEYBOARD_MAPPING_SYM;
  }
}

static int disposizione_per_il_menu(int mappa) {
  switch (mappa) {
  case KBD_MAPPING_IT: return MENU_TEXT_LAYOUT_IT;
  case KBD_MAPPING_UK: return MENU_TEXT_LAYOUT_UK;
  case KBD_MAPPING_DE: return MENU_TEXT_LAYOUT_DE;
  default:             return MENU_TEXT_LAYOUT_US;
  }
}




static void aggiorna_tastiera_del_menu(void) {
  if (keyboard_mapping_item != NULL) {
    ui_set_keyboard_mapping(mappatura_per_il_menu(
        keyboard_mapping_item->choice_ints[keyboard_mapping_item->value]));
  }
  if (keyboard_layout_item != NULL) {
    ui_set_keyboard_layout(disposizione_per_il_menu(
        keyboard_layout_item->choice_ints[keyboard_layout_item->value]));
  }
}

static void rifai_elenco_disposizioni(int idx) {
  int tipo = tipo_tastiera();
  int mappa_ora = KBD_MAPPING_US;
  int conta = layout_conta_per_indice(idx);
  int n = 0;
  int i;

  if (keyboard_layout_item == NULL) {
    return;
  }
  resources_get_int("KeyboardMapping", &mappa_ora);

  for (i = 0; i < N_DISPOSIZIONI; i++) {
    if (!conta && le_disposizioni[i].mappa != KBD_MAPPING_US) {
      continue;
    }
    if (conta &&
        keyboard_is_keymap_valid(idx, le_disposizioni[i].mappa, tipo) != 0) {
      continue;
    }
    strcpy(keyboard_layout_item->choices[n], le_disposizioni[i].nome);
    keyboard_layout_item->choice_ints[n] = le_disposizioni[i].mappa;
    n++;
  }
  if (n == 0) {
    strcpy(keyboard_layout_item->choices[0], "US");
    keyboard_layout_item->choice_ints[0] = KBD_MAPPING_US;
    n = 1;
  }
  keyboard_layout_item->num_choices = n;
  keyboard_layout_item->disabled = (n <= 1);

  keyboard_layout_item->value = 0;
  for (i = 0; i < n; i++) {
    if (keyboard_layout_item->choice_ints[i] == mappa_ora) {
      keyboard_layout_item->value = i;
      break;
    }
  }



  if (keyboard_layout_item->choice_ints[keyboard_layout_item->value] !=
      mappa_ora) {
    resources_set_int("KeyboardMapping",
        keyboard_layout_item->choice_ints[keyboard_layout_item->value]);
  }
}

void emux_add_keyboard_options(struct menu_item* parent) {
  int tipo = tipo_tastiera();
  int n_map = 0;
  int i;

  keyboard_mapping_item = ui_menu_add_multiple_choice(
      MENU_KEYBOARD_MAPPING, parent, "Mapping");




  if (keyboard_is_keymap_valid(KBD_INDEX_SYM, KBD_MAPPING_US, tipo) == 0) {
    strcpy(keyboard_mapping_item->choices[n_map], "Symbolic");
    keyboard_mapping_item->choice_ints[n_map++] = KBD_INDEX_SYM;
  }
  if (keyboard_is_keymap_valid(KBD_INDEX_POS, KBD_MAPPING_US, tipo) == 0) {
    strcpy(keyboard_mapping_item->choices[n_map], "Positional");
    keyboard_mapping_item->choice_ints[n_map++] = KBD_INDEX_POS;
  }
  if (keymap_esiste("rpi_maxi_pos.vkm")) {
    strcpy(keyboard_mapping_item->choices[n_map], "Maxi Positional");
    keyboard_mapping_item->choice_ints[n_map++] = KBD_INDEX_USERPOS;
  }
  if (keymap_esiste("rpi_petsciiboard_sym.vkm")) {
    strcpy(keyboard_mapping_item->choices[n_map], "PETSCIIBOARD");
    keyboard_mapping_item->choice_ints[n_map++] = KBD_INDEX_USERSYM;
  }
  if (n_map == 0) {
    strcpy(keyboard_mapping_item->choices[n_map], "Symbolic");
    keyboard_mapping_item->choice_ints[n_map++] = KBD_INDEX_SYM;
  }
  keyboard_mapping_item->num_choices = n_map;

  int tmp_value;
  resources_get_int("KeymapIndex", &tmp_value);
  keyboard_mapping_item->value = 0;
  for (i = 0; i < n_map; i++) {
    if (keyboard_mapping_item->choice_ints[i] == tmp_value) {
      keyboard_mapping_item->value = i;
      break;
    }
  }


  if (keyboard_mapping_item->choice_ints[keyboard_mapping_item->value] !=
      tmp_value) {
    resources_set_int("KeymapIndex",
        keyboard_mapping_item->choice_ints[keyboard_mapping_item->value]);
  }






  keyboard_layout_item = ui_menu_add_multiple_choice(
      MENU_KEYBOARD_LAYOUT, parent, "Layout");
  rifai_elenco_disposizioni(
      keyboard_mapping_item->choice_ints[keyboard_mapping_item->value]);




  riga_restore_item = ui_menu_add_read_only_heading(parent, riga_restore);
  aggiorna_riga_restore();
  aggiorna_tastiera_del_menu();

}

#ifdef HAVE_RESIDFP









static void residfp_regolazioni(struct menu_item *parent) {
  struct menu_item *fp = ui_menu_add_folder(parent, "reSIDfp Settings");
  struct menu_item *fp6581 = ui_menu_add_folder(fp, "6581");
  struct menu_item *fp8580 = ui_menu_add_folder(fp, "8580");
  struct menu_item *c;
  int v, k;

  v = RESIDFP_COMBINED_WAVEFORM_STRENGTH_DEFAULT;
  resources_get_int("SidResidCombinedWaveformStrength", &v);
  c = residfp_item[0] = ui_menu_add_multiple_choice(
     MENU_SID_RESIDFP_COMBINED, fp, "Combined Waveforms");
  c->num_choices = 3;
  strcpy(c->choices[0], "Weak");
  strcpy(c->choices[1], "Average");
  strcpy(c->choices[2], "Strong");
  c->choice_ints[0] = 0;
  c->choice_ints[1] = 1;
  c->choice_ints[2] = 2;
  c->value = (v >= 0 && v <= 2) ? v : RESIDFP_COMBINED_WAVEFORM_STRENGTH_DEFAULT;





  v = 0;
  resources_get_int("SidResidBackgroundNoise", &v);
  c = residfp_item[5] = ui_menu_add_multiple_choice(
     MENU_SID_RESIDFP_NOISE, fp, "Background Noise");
  c->num_choices = 11;
  strcpy(c->choices[0], "OFF");
  c->choice_ints[0] = 0;
  for (k = 1; k <= 10; k++) {
    sprintf(c->choices[k], "%d%%", k * 10);
    c->choice_ints[k] = k * 10;
  }
  c->value = (v >= 0 && v <= 100) ? (v + 5) / 10 : 0;

  v = RESIDFP_6581_FILTER_CURVE_DEFAULT;
  resources_get_int("SidResid6581FilterCurve", &v);
  c = residfp_item[1] = ui_menu_add_range(MENU_SID_RESIDFP_6581_CURVE,
     fp6581, "Filter Curve", RESIDFP_6581_FILTER_CURVE_MIN,
     RESIDFP_6581_FILTER_CURVE_MAX, 50, v);
  c->ministep = 10;
  c->divisor = RESIDFP_6581_FILTER_CURVE_ONE;
  v = RESIDFP_6581_FILTER_RANGE_DEFAULT;
  resources_get_int("SidResid6581FilterRange", &v);
  c = residfp_item[2] = ui_menu_add_range(MENU_SID_RESIDFP_6581_RANGE,
     fp6581, "Filter Range", RESIDFP_6581_FILTER_RANGE_MIN,
     RESIDFP_6581_FILTER_RANGE_MAX, 50, v);
  c->ministep = 10;
  c->divisor = RESIDFP_6581_FILTER_RANGE_ONE;
  v = 0;
  resources_get_int("SidResid6581OldCaps", &v);
  residfp_item[3] = ui_menu_add_toggle(MENU_SID_RESIDFP_6581_OLDCAPS,
     fp6581, "Old Caps (2200pF)", v);

  v = RESIDFP_8580_FILTER_CURVE_DEFAULT;
  resources_get_int("SidResid8580FilterCurve", &v);
  c = residfp_item[4] = ui_menu_add_range(MENU_SID_RESIDFP_8580_CURVE,
     fp8580, "Filter Curve", RESIDFP_8580_FILTER_CURVE_MIN,
     RESIDFP_8580_FILTER_CURVE_MAX, 50, v);
  c->ministep = 10;
  c->divisor = RESIDFP_8580_FILTER_CURVE_ONE;
}
#endif








static void sid_card_motore(struct menu_item *parent) {
  struct menu_item *child;
  int v = SID_ENGINE_RESID;

  resources_get_int("SidEngine", &v);
#ifdef HAVE_RESIDFP
  if (v != SID_ENGINE_RESIDFP) {
    v = SID_ENGINE_RESID;
  }
#else
  v = SID_ENGINE_RESID;
#endif
  set_int_if_changed("SidEngine", v);
  child = sid_engine_item =
      ui_menu_add_multiple_choice(MENU_SID_ENGINE, parent, "SID Card Engine");
  child->num_choices = 1;
  strcpy(child->choices[0], "ReSid");
  child->choice_ints[0] = SID_ENGINE_RESID;
  child->value = 0;
#ifdef HAVE_RESIDFP
  child->num_choices = 2;
  strcpy(child->choices[1], "ReSIDfp");
  child->choice_ints[1] = SID_ENGINE_RESIDFP;
  child->value = v == SID_ENGINE_RESIDFP ? 1 : 0;
  residfp_regolazioni(parent);
#endif
}








static void plus4_la_cartuccia_sid(struct menu_item* parent) {
  struct menu_item* child;
  int v;

  v = 0;
  resources_get_int("SidCart", &v);
  bmc_sidcard_item =
      ui_menu_add_toggle(MENU_SIDCART_ENABLE, parent, "SID Card", v);

  v = SID_MODEL_6581;
  resources_get_int("SidModel", &v);
  child = sid_model_item[0] =
      ui_menu_add_multiple_choice(MENU_SID_MODEL, parent, "SID Card Model");
  child->num_choices = 2;
  strcpy(child->choices[MENU_SID_MODEL_6581], "6581");
  strcpy(child->choices[MENU_SID_MODEL_8580], "8580");
  child->choice_ints[MENU_SID_MODEL_6581] = SID_MODEL_6581;
  child->choice_ints[MENU_SID_MODEL_8580] = SID_MODEL_8580;
  child->value = viceSidModelToBmcChoice(v);

  v = 0xfd40;
  resources_get_int("SidAddress", &v);
  child = bmc_sidcard_address_item =
      ui_menu_add_multiple_choice(MENU_SIDCART_ADDRESS, parent,
                                  "SID Card Address");
  child->num_choices = 2;
  strcpy(child->choices[0], "$FD40");
  strcpy(child->choices[1], "$FE80");
  child->choice_ints[0] = 0xfd40;
  child->choice_ints[1] = 0xfe80;
  child->value = v == 0xfe80 ? 1 : 0;

  v = 1;
  resources_get_int("SidFilters", &v);
  sid_filter_item = ui_menu_add_toggle(MENU_SID_FILTER, parent,
                                       "SID Card Filter", v);




  sid_card_motore(parent);
}






static void pet_la_cartuccia_sid(struct menu_item* parent) {
  struct menu_item* child;
  int v;

  v = 0;
  resources_get_int("SidCart", &v);
  bmc_sidcard_item =
      ui_menu_add_toggle(MENU_SIDCART_ENABLE, parent, "SID Card", v);

  v = SID_MODEL_6581;
  resources_get_int("SidModel", &v);
  child = sid_model_item[0] =
      ui_menu_add_multiple_choice(MENU_SID_MODEL, parent, "SID Card Model");
  child->num_choices = 2;
  strcpy(child->choices[MENU_SID_MODEL_6581], "6581");
  strcpy(child->choices[MENU_SID_MODEL_8580], "8580");
  child->choice_ints[MENU_SID_MODEL_6581] = SID_MODEL_6581;
  child->choice_ints[MENU_SID_MODEL_8580] = SID_MODEL_8580;
  child->value = viceSidModelToBmcChoice(v);

  v = 0x8f00;
  resources_get_int("SidAddress", &v);
  child = bmc_sidcard_address_item =
      ui_menu_add_multiple_choice(MENU_SIDCART_ADDRESS, parent,
                                  "SID Card Address");
  child->num_choices = 2;
  strcpy(child->choices[0], "$8F00");
  strcpy(child->choices[1], "$E900");
  child->choice_ints[0] = 0x8f00;
  child->choice_ints[1] = 0xe900;
  child->value = v == 0xe900 ? 1 : 0;

  v = 1;
  resources_get_int("SidFilters", &v);
  sid_filter_item = ui_menu_add_toggle(MENU_SID_FILTER, parent,
                                       "SID Card Filter", v);




  sid_card_motore(parent);
}

// NOTES: 0xd400 is normally not an option in VICE for the 2nd SID but
// we added mirroring support for BMC64. Also, each sid can be configured
// to have different models unlike upstream VICE.
void emux_add_sound_options(struct menu_item* parent) {

  static int addresses[] = {
      0xd400, 0xd420, 0xd440, 0xd460, 0xd480, 0xd4a0, 0xd4c0, 0xd4d0,
      0xd500, 0xd520, 0xd540, 0xd560, 0xd580, 0xd5a0, 0xd5c0, 0xd5d0,
      0xd600, 0xd620, 0xd640, 0xd660, 0xd680, 0xd6a0, 0xd6c0, 0xd6d0,
      0xd700, 0xd720, 0xd740, 0xd760, 0xd780, 0xd7a0, 0xd7c0, 0xd7d0,
      0xde00, 0xde20, 0xde40, 0xde60, 0xde80, 0xdea0, 0xdec0, 0xded0,
      0xdf00, 0xdf20, 0xdf40, 0xdf60, 0xdf80, 0xdfa0, 0xdfc0, 0xdfd0,
  };

  check_sid_options();

  // The pet has terrible lag when using ReSid, use FAST since it only
  // ever makes simple beeps anyway.
  if (machine_class == VICE_MACHINE_PET) {










     pet_la_cartuccia_sid(parent);
     return;
  }







  if (machine_class == VICE_MACHINE_VIC20) {
     set_int_if_changed("SidCart", 0);
     return;
  }

  if (machine_class == VICE_MACHINE_PLUS4) {
     plus4_la_cartuccia_sid(parent);
     return;
  }

  int supports_dual_sid = (machine_class == VICE_MACHINE_C64 || machine_class == VICE_MACHINE_C64SC ||
                           machine_class == VICE_MACHINE_SCPU64 || machine_class == VICE_MACHINE_C128) &&
                           circle_get_model() >= 2;





  if (supports_dual_sid) {
     sid_mono_item = ui_menu_add_toggle_labels(MENU_SOUND_MONO, parent,
        "Channels", uscita_mono, "Stereo", "Mono");
  }

  // Resid by default
  struct menu_item* child = sid_engine_item =
      ui_menu_add_multiple_choice(MENU_SID_ENGINE, parent, "SID Engine");
  child->num_choices = 2;
  child->value = MENU_SID_ENGINE_RESID;
  strcpy(child->choices[MENU_SID_ENGINE_FAST], "Fast");
  strcpy(child->choices[MENU_SID_ENGINE_RESID], "ReSid");
  child->choice_ints[MENU_SID_ENGINE_FAST] = SID_ENGINE_FASTSID;
  child->choice_ints[MENU_SID_ENGINE_RESID] = SID_ENGINE_RESID;
#ifdef HAVE_RESIDFP



  if (machine_class != VICE_MACHINE_C64DTV) {
    child->num_choices = 3;
    strcpy(child->choices[MENU_SID_ENGINE_RESIDFP], "ReSIDfp");
    child->choice_ints[MENU_SID_ENGINE_RESIDFP] = SID_ENGINE_RESIDFP;
  }
#endif

  // 6581 by default
  child = sid_model_item[0] =
    ui_menu_add_multiple_choice(MENU_SID_MODEL, parent, "SID Model");
  child->num_choices = 2;
  child->value = MENU_SID_MODEL_6581;
  strcpy(child->choices[MENU_SID_MODEL_6581], "6581");
  strcpy(child->choices[MENU_SID_MODEL_8580], "8580");
  child->choice_ints[MENU_SID_MODEL_6581] = SID_MODEL_6581;
  child->choice_ints[MENU_SID_MODEL_8580] = SID_MODEL_8580;

  // Filter on by default
  sid_filter_item =
      ui_menu_add_toggle(MENU_SID_FILTER, parent, "SID Filter", 0);

  if (circle_get_model() >= 3) {
     child = sid_resampling_item =
         ui_menu_add_multiple_choice(MENU_SID_SAMPLING,
             parent, "SID Resampling");
     child->num_choices = 4;
     strcpy(child->choices[MENU_SID_SAMPLING_FAST], "Fast");
     strcpy(child->choices[MENU_SID_SAMPLING_INTERPOLATION], "Interpolation");
     strcpy(child->choices[MENU_SID_SAMPLING_RESAMPLING], "Resampling");
     strcpy(child->choices[MENU_SID_SAMPLING_FAST_RESAMPLING], "Fast Resampling");
     child->choice_ints[MENU_SID_SAMPLING_FAST] =
         SID_RESID_SAMPLING_FAST;
     child->choice_ints[MENU_SID_SAMPLING_INTERPOLATION] =
         SID_RESID_SAMPLING_INTERPOLATION;
     child->choice_ints[MENU_SID_SAMPLING_RESAMPLING] =
         SID_RESID_SAMPLING_RESAMPLING;
     child->choice_ints[MENU_SID_SAMPLING_FAST_RESAMPLING] =
         SID_RESID_SAMPLING_FAST_RESAMPLING;

     if (circle_get_model() < 4) {
        child->choice_disabled[MENU_SID_SAMPLING_RESAMPLING] = 1;
     }
  }







  {
     struct menu_item *filtri =
        ui_menu_add_folder(parent, "reSID Filter Settings");
     struct menu_item *m6581 = ui_menu_add_folder(filtri, "6581");
     struct menu_item *m8580 = ui_menu_add_folder(filtri, "8580");
     int v;

     v = RESID_6581_PASSBAND_DEFAULT;
     resources_get_int("SidResidPassband", &v);
     resid_filtri_item[0] = ui_menu_add_range(MENU_SID_RESID_6581_PASSBAND,
        m6581, "Passband %", RESID_6581_PASSBAND_MIN, RESID_6581_PASSBAND_MAX,
        1, v);
     v = RESID_6581_FILTER_GAIN_DEFAULT;
     resources_get_int("SidResidGain", &v);
     resid_filtri_item[1] = ui_menu_add_range(MENU_SID_RESID_6581_GAIN,
        m6581, "Gain %", RESID_6581_FILTER_GAIN_MIN,
        RESID_6581_FILTER_GAIN_MAX, 1, v);
     v = RESID_6581_FILTER_BIAS_DEFAULT;
     resources_get_int("SidResidFilterBias", &v);
     resid_filtri_item[2] = ui_menu_add_range(MENU_SID_RESID_6581_FILTER_BIAS,
        m6581, "Filter Bias (mV)", RESID_6581_FILTER_BIAS_MIN,
        RESID_6581_FILTER_BIAS_MAX, 100, v);
     resid_filtri_item[2]->ministep = 10;

     v = RESID_8580_PASSBAND_DEFAULT;
     resources_get_int("SidResid8580Passband", &v);
     resid_filtri_item[3] = ui_menu_add_range(MENU_SID_RESID_8580_PASSBAND,
        m8580, "Passband %", RESID_8580_PASSBAND_MIN, RESID_8580_PASSBAND_MAX,
        1, v);
     v = RESID_8580_FILTER_GAIN_DEFAULT;
     resources_get_int("SidResid8580Gain", &v);
     resid_filtri_item[4] = ui_menu_add_range(MENU_SID_RESID_8580_GAIN,
        m8580, "Gain %", RESID_8580_FILTER_GAIN_MIN,
        RESID_8580_FILTER_GAIN_MAX, 1, v);
     v = RESID_8580_FILTER_BIAS_DEFAULT;
     resources_get_int("SidResid8580FilterBias", &v);
     resid_filtri_item[5] = ui_menu_add_range(MENU_SID_RESID_8580_FILTER_BIAS,
        m8580, "Filter Bias (mV)", RESID_8580_FILTER_BIAS_MIN,
        RESID_8580_FILTER_BIAS_MAX, 100, v);
     resid_filtri_item[5]->ministep = 10;
  }

#ifdef HAVE_RESIDFP


  if (machine_class != VICE_MACHINE_C64DTV) {
     residfp_regolazioni(parent);
  }
#endif

  if (supports_dual_sid) {
     ui_menu_add_divider(parent);

     int value;
     resources_get_int("SidStereo", &value);
     if (value > 1) {
        resources_set_int("SidStereo", 1);
        value = 1;
     }
     sid_dual_item =
        ui_menu_add_toggle(MENU_SID2_ENABLE, parent, "Dual SID", value);

     child = sid_base_address_item =
        ui_menu_add_multiple_choice(MENU_SID2_ADDRESS, parent, "SID2 Address");
     child->num_choices = 48;

     int cur_addr;
     resources_get_int("Sid2AddressStart", &cur_addr);
     for (int i=0;i<48;i=i+1) {
        char label[32];
        sprintf (label, "0x%04x", addresses[i]);
        strcpy(child->choices[i], label);
        child->choice_ints[i] = addresses[i];
        if (addresses[i] == cur_addr) {
           child->value = i;
        }
     }

     // 6581 by default
     child = sid_model_item[1] =
       ui_menu_add_multiple_choice(MENU_SID2_MODEL, parent, "SID2 Model");
     child->num_choices = 2;
     child->value = MENU_SID_MODEL_6581;
     strcpy(child->choices[MENU_SID_MODEL_6581], "6581");
     strcpy(child->choices[MENU_SID_MODEL_8580], "8580");
     child->choice_ints[MENU_SID_MODEL_6581] = SID_MODEL_6581;
     child->choice_ints[MENU_SID_MODEL_8580] = SID_MODEL_8580;
  }

  int tmp_value;

  resources_get_int("SidEngine", &tmp_value);
  sid_engine_item->value = viceSidEngineToBmcChoice(tmp_value);

  resources_get_int("SidModel", &tmp_value);
  sid_model_item[0]->value = viceSidModelToBmcChoice(tmp_value);



  if (sid_model_item[1] != NULL) {
     resources_get_int("Sid2Model", &tmp_value);
     sid_model_item[1]->value = viceSidModelToBmcChoice(tmp_value);
  }

  resources_get_int("SidFilters", &tmp_value);
  sid_filter_item->value = tmp_value;

  if (circle_get_model() >= 3 ) {
    resources_get_int("SidResidSampling", &tmp_value);
    sid_resampling_item->value =
        viceSidResamplingToBmcChoice(tmp_value);
  }
}

void emux_set_warp(int warp) {









  resources_set_int("SoundEmulateOnWarp", 0);
  vsync_set_warp_mode(warp);
}










void emux_sid_corsa(int on) {
  resources_set_int("SoundEmulateOnWarp", 0);
  vsync_set_warp_mode(on ? 1 : 0);
}











static void tipi_leggi(int t[4]) {
  int u;
  for (u = 0; u < 4; u++) {
    t[u] = 0;
    resources_get_int_sprintf("Drive%iType", &t[u], u + 8);
  }
}





static void tipi_rimetti(const int t[4]) {
  int u, ora;
  for (u = 0; u < 4; u++) {
    ora = 0;
    resources_get_int_sprintf("Drive%iType", &ora, u + 8);
    if (ora != t[u]) {
      printf("[ROM] unita %d spenta dal VICE (era %d), la rimetto a %d\n",
             u + 8, ora, t[u]);
      resources_set_int_sprintf("Drive%iType", t[u], u + 8);
    }
  }
}

static int metti_rom(const char *risorsa, struct menu_item *item,
                     fullpath_func f_fullpath) {
  const char *prima = NULL;
  char vecchio[512];
  char *pieno;
  char *barra;
  int tipi[4];



  tipi_leggi(tipi);

  vecchio[0] = '\0';
  if (resources_get_string(risorsa, &prima) >= 0 && prima != NULL) {
    strncpy(vecchio, prima, sizeof(vecchio) - 1);
    vecchio[sizeof(vecchio) - 1] = '\0';
  }

  if (resources_set_string(risorsa, item->str_value) >= 0) {
    tipi_rimetti(tipi);
    return 0;
  }

  pieno = f_fullpath(DIR_ROMS, item->str_value);
  barra = pieno != NULL ? strchr(pieno, '/') : NULL;
  if (barra != NULL && resources_set_string(risorsa, barra) >= 0) {
    tipi_rimetti(tipi);
    return 0;
  }

  resources_set_string(risorsa, vecchio);
  tipi_rimetti(tipi);
  return -1;
}

int emux_handle_rom_change(struct menu_item* item, fullpath_func f_fullpath) {
  int esito = 0;

  switch (item->id) {
     case MENU_DRIVE_ROM_FILE_1541:
       esito = metti_rom("DosName1541", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_1541II:
       esito = metti_rom("DosName1541ii", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_1551:
       esito = metti_rom("DosName1551", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_1571:
       esito = metti_rom("DosName1571", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_1581:
       esito = metti_rom("DosName1581", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_CMDHD:
       esito = metti_rom("DosNameCMDHD", item, f_fullpath);
       break;



     case MENU_DRIVE_ROM_FILE_2031:
       esito = metti_rom("DosName2031", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_2040:
       esito = metti_rom("DosName2040", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_3040:
       esito = metti_rom("DosName3040", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_4040:
       esito = metti_rom("DosName4040", item, f_fullpath);
       break;
     case MENU_DRIVE_ROM_FILE_1001:
       esito = metti_rom("DosName1001", item, f_fullpath);
       break;
     case MENU_KERNAL_FILE:
       esito = metti_rom("KernalName", item, f_fullpath);
       break;
     case MENU_BASIC_FILE:
       esito = metti_rom("BasicName", item, f_fullpath);
       break;
     case MENU_CHARGEN_FILE:
       esito = metti_rom("ChargenName", item, f_fullpath);
       break;
     case MENU_C128_LOAD_KERNAL_FILE:
       esito = metti_rom("KernalIntName", item, f_fullpath);
       break;
     case MENU_C128_LOAD_BASIC_HI_FILE:
       esito = metti_rom("BasicHiName", item, f_fullpath);
       break;
     case MENU_C128_LOAD_BASIC_LO_FILE:
       esito = metti_rom("BasicLoName", item, f_fullpath);
       break;
     case MENU_C128_LOAD_CHARGEN_FILE:
       esito = metti_rom("ChargenIntName", item, f_fullpath);
       break;
     case MENU_C128_LOAD_64_KERNAL_FILE:
       esito = metti_rom("Kernal64Name", item, f_fullpath);
       break;
     case MENU_C128_LOAD_64_BASIC_FILE:
       esito = metti_rom("Basic64Name", item, f_fullpath);
       break;
     default:
       assert(0);
  }
  return esito;
}







int emux_handle_reu_image_change(const char *path) {
  FILE *file;
  int size;

  file = fopen(path, MODE_READ);
  if (file == NULL) {
    return -1;
  }


  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return -1;
  }
  size = (int)ftell(file);
  fclose(file);

  if (size <= 0 || (size % 1024) != 0) {
    return -1;
  }

  if (resources_set_int("REU", 0) < 0 ||
      resources_set_int("REUsize", size / 1024) < 0 ||
      resources_set_string("REUfilename", path) < 0) {
    return -1;
  }






  {
    unsigned long t0 = circle_get_ticks();

    if (resources_set_int("REU", 1) < 0) {
      return -1;
    }
    printf("[REU] immagine di %d KB montata in %lu ms\n", size / 1024,
           (circle_get_ticks() - t0) / 1000);
  }

  emux_reu_image_loaded(size / 1024);
  return 0;
}

int emux_handle_ide64_image_change(int device, const char *path) {
  if (device >= 1 && device <= 4) {
    int result = resources_set_string_sprintf("IDE64Image%i", path, device);
    if (result == 0) {
      machine_trigger_reset(MACHINE_RESET_MODE_POWER_CYCLE);
    }
    return result;
  }
  return -1;
}

void emux_set_iec_dir(int unit, char* dir) {
  resources_set_string_sprintf("FSDevice%iDir", dir, unit);
}

void emux_set_int(IntSetting setting, int value) {
 switch (setting) {
   case Setting_C128ColumnKey:
     resources_set_int("C128ColumnKey", value);
     break;
   case Setting_Datasette:
     resources_set_int("TapePort1Device",
                       value ? TAPEPORT_DEVICE_DATASETTE
                             : TAPEPORT_DEVICE_NONE);
     break;
   case Setting_DatasetteResetWithCPU:
     resources_set_int("DatasetteResetWithCPU", value);
     break;
   case Setting_DriveSoundEmulation:
     resources_set_int("DriveSoundEmulation", value);
     break;
   case Setting_DriveSoundEmulationVolume:
     resources_set_int("DriveSoundEmulationVolume", value);
     break;
   case Setting_Mouse:
     resources_set_int("Mouse", value);
     break;
   case Setting_RAMBlock0:
     resources_set_int("RAMBlock0", value);
     break;
   case Setting_RAMBlock1:
     resources_set_int("RAMBlock1", value);
     break;
   case Setting_RAMBlock2:
     resources_set_int("RAMBlock2", value);
     break;
   case Setting_RAMBlock3:
     resources_set_int("RAMBlock3", value);
     break;
   case Setting_RAMBlock5:
     resources_set_int("RAMBlock5", value);
     break;
   case Setting_VideoFilter:
     set_filter(0, value);
     break;
   case Setting_AutostartWarp:
     resources_set_int("AutostartWarp", value);
     break;
   case Setting_TapeSoundEmulation:
     resources_set_int("DatasetteSound", value);
     break;
   case Setting_TapeSoundEmulationVolume:
     resources_set_int("DatasetteSoundVolume", value);
     break;
   default:
     assert(0);
 }
}

void emux_set_int_1(IntSetting setting, int value, int param) {
 switch (setting) {
   case Setting_FileSystemDeviceN:
     resources_set_int_sprintf("FileSystemDevice%i", value, param);
     break;
   case Setting_DriveNParallelCable:
     resources_set_int_sprintf("Drive%iParallelCable", value, param);
     break;
   case Setting_DriveNType:
     resources_set_int_sprintf("Drive%iType", value, param);
     break;
   case Setting_IECDeviceN:




     if (value) {
       resources_set_int_sprintf("TrapDevice%i", 1, param);
       resources_set_int_sprintf("Drive%iTrueEmulation", 0, param);
       resources_set_int_sprintf("FileSystemDevice%i", ATTACH_DEVICE_FS,
                                 param);
     } else {
       resources_set_int_sprintf("TrapDevice%i", 0, param);
       resources_set_int_sprintf("Drive%iTrueEmulation", 1, param);
     }
     break;
   case Setting_DriveNCMDHDMode:
     drive_cpu_trigger_reset_button(param-8, value);
     break;
   default:
     assert(0);
 }
}

void emux_get_int(IntSetting setting, int* dest) {
  switch (setting) {
    case Setting_WarpMode:
      *dest = vsync_get_warp_mode();
      break;
    case Setting_DriveSoundEmulation:
      resources_get_int("DriveSoundEmulation", dest);
      break;
    case Setting_DriveSoundEmulationVolume:
      resources_get_int("DriveSoundEmulationVolume", dest);
      break;
    case Setting_C128ColumnKey:
      resources_get_int("C128ColumnKey", dest);
      break;
    case Setting_DatasetteResetWithCPU:
      resources_get_int("DatasetteResetWithCPU", dest);
      break;
    case Setting_VideoSize:
      resources_get_int("VideoSize", dest);
      break;
    case Setting_VideoFilter:
      *dest = get_filter(0);
      break;
    case Setting_AutostartWarp:
      resources_get_int("AutostartWarp", dest);
      break;
    case Setting_TapeSoundEmulation:
      resources_get_int("DatasetteSound", dest);
      break;
    case Setting_TapeSoundEmulationVolume:
      resources_get_int("DatasetteSoundVolume", dest);
      break;
    default:
      assert(0);
  }
}

void emux_get_int_1(IntSetting setting, int* dest, int param) {
  switch (setting) {
    case Setting_DriveNType:
      resources_get_int_sprintf("Drive%iType", dest, param);
      break;
    case Setting_IECDeviceN:
      {



        int tmp_trap = 0;
        int tmp_fsd = ATTACH_DEVICE_NONE;
        resources_get_int_sprintf("TrapDevice%i", &tmp_trap, param);
        resources_get_int_sprintf("FileSystemDevice%i", &tmp_fsd, param);
        *dest = (tmp_trap && tmp_fsd == ATTACH_DEVICE_FS) ? 1 : 0;
      }
      break;
    default:
      assert(0);
  }
}

void emux_get_string_1(StringSetting setting, const char** dest, int param) {
  switch (setting) {
    case Setting_FSDeviceNDir:
      resources_get_string_sprintf("FSDevice%iDir", dest, param);
      break;
    default:
      assert(0);
  }
}

int emux_save_settings(void) {
   return resources_save(NULL);
}

void emux_log_settings_file(const char *filename) {
  log_message(LOG_DEFAULT, "Writing settings file `%s'.", filename);
}

void emux_log_riga(const char *riga) {
  log_message(LOG_DEFAULT, "%s", riga);
}














extern int easyflash_flash_scritta(void) __attribute__((weak));

int emux_prepare_shutdown(void) {
  int scrivi = 0;
  int esito = 0;

  if (resources_get_int("REUImageWrite", &scrivi) == 0 && scrivi &&
      cartridge_can_flush_image(CARTRIDGE_REU)) {
    if (cartridge_flush_image(CARTRIDGE_REU) < 0) {
      log_error(LOG_DEFAULT, "REU: il file non si scrive, allo spegnimento");
      esito = -1;
    } else {
      log_message(LOG_DEFAULT, "REU: file scritto allo spegnimento");
    }
  }
  scrivi = 0;
  if (easyflash_flash_scritta != NULL && easyflash_flash_scritta() &&
      resources_get_int("EasyFlashWriteCRT", &scrivi) == 0 && scrivi) {
    if (cartridge_flush_image(CARTRIDGE_EASYFLASH) < 0) {
      log_error(LOG_DEFAULT, "EasyFlash: il .crt non si scrive, allo spegnimento");
      esito = -1;
    } else {
      log_message(LOG_DEFAULT, "EasyFlash: .crt scritto allo spegnimento");
    }
  }
  return esito;
}

int emux_handle_menu_change(struct menu_item* item) {
  switch (item->id) {
    case MENU_SID2_ADDRESS:
      resources_set_int("Sid2AddressStart", item->choice_ints[item->value]);
      return 1;
    case MENU_SID2_ENABLE:
      resources_set_int("SidStereo", item->value);
      check_sid_options();
      return 1;
    case MENU_SOUND_MONO:
      uscita_mono = item->value ? 1 : 0;
      check_sid_options();
      return 1;
    case MENU_SID_ENGINE:
      resources_set_int("SidEngine", item->choice_ints[item->value]);


      {
        int motore = SID_ENGINE_RESID;
        int i;
        resources_get_int("SidEngine", &motore);
        item->value = viceSidEngineToBmcChoice(motore);


        for (i = 0; i < item->num_choices; i++) {
          if (item->choice_ints[i] == motore) {
            item->value = i;
            break;
          }
        }
      }
      check_sid_options();
      return 1;
    case MENU_SID_MODEL:
      resources_set_int("SidModel", item->choice_ints[item->value]);
      check_sid_options();
      return 1;
    case MENU_SID2_MODEL:
      resources_set_int("Sid2Model", item->choice_ints[item->value]);
      check_sid_options();
      return 1;
    case MENU_SID_FILTER:
      resources_set_int("SidFilters", item->value);
      check_sid_options();
      return 1;
    case MENU_SID_SAMPLING:
      resources_set_int("SidResidSampling", item->value);
      return 1;


    case MENU_SID_RESID_6581_PASSBAND:
      resources_set_int("SidResidPassband", item->value);
      resources_get_int("SidResidPassband", &item->value);
      return 1;
    case MENU_SID_RESID_6581_GAIN:
      resources_set_int("SidResidGain", item->value);
      resources_get_int("SidResidGain", &item->value);
      return 1;
    case MENU_SID_RESID_6581_FILTER_BIAS:
      resources_set_int("SidResidFilterBias", item->value);
      resources_get_int("SidResidFilterBias", &item->value);
      return 1;
    case MENU_SID_RESID_8580_PASSBAND:
      resources_set_int("SidResid8580Passband", item->value);
      resources_get_int("SidResid8580Passband", &item->value);
      return 1;
    case MENU_SID_RESID_8580_GAIN:
      resources_set_int("SidResid8580Gain", item->value);
      resources_get_int("SidResid8580Gain", &item->value);
      return 1;
    case MENU_SID_RESID_8580_FILTER_BIAS:
      resources_set_int("SidResid8580FilterBias", item->value);
      resources_get_int("SidResid8580FilterBias", &item->value);
      return 1;
#ifdef HAVE_RESIDFP


    case MENU_SID_RESIDFP_COMBINED:
      resources_set_int("SidResidCombinedWaveformStrength",
                        item->choice_ints[item->value]);
      return 1;
    case MENU_SID_RESIDFP_6581_CURVE:
      resources_set_int("SidResid6581FilterCurve", item->value);
      resources_get_int("SidResid6581FilterCurve", &item->value);
      return 1;
    case MENU_SID_RESIDFP_6581_RANGE:
      resources_set_int("SidResid6581FilterRange", item->value);
      resources_get_int("SidResid6581FilterRange", &item->value);
      return 1;
    case MENU_SID_RESIDFP_6581_OLDCAPS:
      resources_set_int("SidResid6581OldCaps", item->value);
      resources_get_int("SidResid6581OldCaps", &item->value);
      return 1;
    case MENU_SID_RESIDFP_8580_CURVE:
      resources_set_int("SidResid8580FilterCurve", item->value);
      resources_get_int("SidResid8580FilterCurve", &item->value);
      return 1;
    case MENU_SID_RESIDFP_NOISE:

      resources_set_int("SidResidBackgroundNoise",
                        item->choice_ints[item->value]);
      return 1;
#endif
    case MENU_SIDCART_ENABLE:
      resources_set_int("SidCart", item->value);


      resources_get_int("SidCart", &item->value);
      check_sid_options();
      return 1;
    case MENU_SIDCART_ADDRESS:
      resources_set_int("SidAddress", item->choice_ints[item->value]);
      return 1;
    case MENU_SAVE_EASYFLASH:
      if (cartridge_flush_image(CARTRIDGE_EASYFLASH) < 0) {
        ui_error("Problem saving");
      } else {
        ui_pop_all_and_toggle();
      }
      return 1;
    case MENU_REU_SAVE_IMAGE:
      if (cartridge_flush_image(CARTRIDGE_REU) < 0) {
        ui_error("Problem saving");
      } else {
        ui_pop_all_and_toggle();
      }
      return 1;
    case MENU_REU_IMAGE_WRITE:

      resources_set_int("REUImageWrite", item->value);
      resources_get_int("REUImageWrite", &item->value);
      return 1;
    case MENU_REU_DETACH_IMAGE:


      if (resources_set_string("REUfilename", "") < 0) {
        ui_error("Problem detaching REU image");
      } else {
        emux_reu_image_loaded(0);
      }
      return 1;
    case MENU_CART_FREEZE:
      cartridge_freeze();
      ui_pop_all_and_toggle();
      return 1;
    case MENU_DRIVE_RAM_2000:
      resources_set_int_sprintf("Drive%iRAM2000", item->value, item->sub_id);
      return 1;
    case MENU_DRIVE_RAM_4000:
      resources_set_int_sprintf("Drive%iRAM4000", item->value, item->sub_id);
      return 1;
    case MENU_DRIVE_RAM_6000:
      resources_set_int_sprintf("Drive%iRAM6000", item->value, item->sub_id);
      return 1;
    case MENU_DRIVE_RAM_8000:
      resources_set_int_sprintf("Drive%iRAM8000", item->value, item->sub_id);
      return 1;
    case MENU_DRIVE_RAM_A000:
      resources_set_int_sprintf("Drive%iRAMA000", item->value, item->sub_id);
      return 1;
    case MENU_KEYBOARD_LAYOUT:

      resources_set_int("KeyboardMapping", item->choice_ints[item->value]);





      aggiorna_riga_restore();
      aggiorna_tastiera_del_menu();
      if (ui_value_changed_by_return()) {
        ui_info("Remember to save..");
      }
      return 1;
    case MENU_KEYBOARD_MAPPING: {


      int idx_nuovo = item->choice_ints[item->value];
      if (idx_nuovo == KBD_INDEX_USERPOS) {
         resources_set_string("KeymapUserPosFile", "rpi_maxi_pos.vkm");
      }
      else if (idx_nuovo == KBD_INDEX_USERSYM) {
         resources_set_string("KeymapUserSymFile", "rpi_petsciiboard_sym.vkm");
      }
      resources_set_int("KeymapIndex", idx_nuovo);

      rifai_elenco_disposizioni(idx_nuovo);
      aggiorna_riga_restore();
      aggiorna_tastiera_del_menu();
      return 1;
    }
    case MENU_DRIVE_TRUE_EMULATION:



      if (item->value) {
        emux_use_emulated_drive(item->sub_id);
      } else {
        resources_set_int_sprintf("Drive%iTrueEmulation", 0, item->sub_id);
      }
      emux_refresh_drive_items(item->sub_id);
      return 1;
    case MENU_VIRTUAL_DEVICES:
      for (int u = 8; u <= 11; u++) {
        emux_set_int_1(Setting_IECDeviceN, item->value, u);
        emux_refresh_drive_items(u);
      }
      return 1;
    default:
      break;
  }

  return 0;
}

int emux_handle_quick_func(int button_func, fullpath_func f_fullpath) {
  int drive;
  struct menu_item *root;
  struct menu_item *child;
  switch (button_func) {
    case BTN_ASSIGN_CART_FREEZE:
       cartridge_freeze();
       return 1;
    case BTN_ASSIGN_FLUSH_DISK:
       if (ui_enabled) {
         ui_dismiss_osd_if_active();
         return 1;
       }

       for (drive=0;drive<4;drive++) {


          file_system_detach_disk(drive+8, 0);
          if (strlen(attached_disk_name[drive]) > 0) {
             file_system_attach_disk(drive+8, 0,
                f_fullpath(DIR_DISKS, attached_disk_name[drive]));
          }
       }

       root = ui_push_menu(18, 3);
       root->on_popped_off = glob_osd_popped;
       child = ui_menu_add_button(MENU_ID_DO_NOTHING, root, "Disks flushed...");
       ui_enable_osd();
       return 1;
    default:
       break;
  }
  return 0;
}

void emux_load_additional_settings() {
  // Vice settings are automatically loaded by the emulator. Nothing
  // to do here.

  // CHEAT: Temporarily using this hook to get the max border settings
  // into the canvas structure early.  These are now reqiured by
  // the menu before the border items are created. TODO: FIX THIS!!
  set_canvas_borders(VIC_INDEX,
                     &canvas_state[VIC_INDEX].max_border_w,
                     &canvas_state[VIC_INDEX].max_border_h);
  canvas_state[VIC_INDEX].max_border_h *=
     canvas_state[VIC_INDEX].raster_skip;

  if (machine_class == VICE_MACHINE_C128) {
     set_canvas_borders(VDC_INDEX,
                        &canvas_state[VDC_INDEX].max_border_w,
                        &canvas_state[VDC_INDEX].max_border_h);
     canvas_state[VDC_INDEX].max_border_h *=
        canvas_state[VDC_INDEX].raster_skip;
  }
}

void emux_save_additional_settings(FILE *fp) {

  if (sid_mono_item != NULL) {
    fprintf(fp, "sound_mono=%d\n", uscita_mono);
  }
}

void emux_get_default_color_setting(int *brightness, int *contrast,
                                    int *gamma, int *tint, int *saturation) {
    *brightness = 1000;
    *contrast = 1250;
    *gamma = 2200;
    *tint = 1000;
    *saturation = 1000;
}

int emux_handle_loaded_setting(char *name, char* value_str, int value) {

  if (strcmp(name, "sound_mono") == 0) {
    if (sid_mono_item != NULL) {
      uscita_mono = value ? 1 : 0;
      sid_mono_item->value = uscita_mono;
      check_sid_options();
    }
    return 1;
  }
  return 0;
}



void emux_menu_about_to_activate(void) {
  aggiorna_riga_restore();
}

void emux_load_settings_done(void) {
  aggiorna_riga_restore();


  aggiorna_tastiera_del_menu();
  emux_machine_load_settings_done();
}

static void swap_userport_joysticks() {
  int tmp = joydevs[2].device;
  joydevs[2].device = joydevs[3].device;
  joydevs[3].device = tmp;
  ui_set_joy_items();
}

static void menu_value_changed(struct menu_item *item) {
   switch (item->id) {
      case MENU_USERPORT_JOYSTICKS:
         resources_set_int("UserportDevice",
             item->value ? userport_tipo_scelto()
                         : USERPORT_DEVICE_NONE);
         break;
      case MENU_SWAP_USERPORT_JOYSTICKS:
         swap_userport_joysticks();
         break;
      case MENU_USERPORT_TYPE:
         if (enable_item == NULL || enable_item->value) {
            resources_set_int("UserportDevice",
                item->choice_ints[item->value]);
         }
         break;
      default:
         break;
   }
}

void emux_add_userport_joys(struct menu_item* parent) {
  struct menu_item* parent2 =
     ui_menu_add_folder(parent,
        "Userport Joystick Adapter");
  int value;
  value = 0;
  resources_get_int("UserportDevice", &value);
  value = userport_e_joystick(value);
  enable_item =
     ui_menu_add_toggle(MENU_USERPORT_JOYSTICKS, parent2, "Enable", value);
  swap_item =
     ui_menu_add_button(MENU_SWAP_USERPORT_JOYSTICKS, parent2,
        "Swap Joystick Ports");
  port_3_menu_item = add_joyport_options(parent2, 3);
  port_4_menu_item = add_joyport_options(parent2, 4);

  enable_item->on_value_changed = menu_value_changed;
  swap_item->on_value_changed = menu_value_changed;

  adapter_type_item =
      ui_menu_add_multiple_choice(MENU_USERPORT_TYPE, parent2, "Adapter Type");
  adapter_type_item->num_choices = 6;
  resources_get_int("UserportDevice", &value);
  adapter_type_item->value = 0;
  for (int i = 0; i < adapter_type_item->num_choices; i++) {
     if (adapter_type_item->choice_ints[i] == value) {
        adapter_type_item->value = i;
        break;
     }
  }
  strcpy(adapter_type_item->choices[0], "CGA");
  strcpy(adapter_type_item->choices[1], "PET");
  strcpy(adapter_type_item->choices[2], "Hummer");
  strcpy(adapter_type_item->choices[3], "OEM");
  strcpy(adapter_type_item->choices[4], "HIT");
  strcpy(adapter_type_item->choices[5], "Kingsoft");
  strcpy(adapter_type_item->choices[6], "Starbyte");
  adapter_type_item->choice_ints[0] = USERPORT_DEVICE_JOYSTICK_CGA;
  adapter_type_item->choice_ints[1] = USERPORT_DEVICE_JOYSTICK_PET;
  adapter_type_item->choice_ints[2] = USERPORT_DEVICE_JOYSTICK_HUMMER;
  adapter_type_item->choice_ints[3] = USERPORT_DEVICE_JOYSTICK_OEM;
  adapter_type_item->choice_ints[4] = USERPORT_DEVICE_JOYSTICK_HIT;
  adapter_type_item->choice_ints[5] = USERPORT_DEVICE_JOYSTICK_KINGSOFT;
  adapter_type_item->choice_ints[6] = USERPORT_DEVICE_JOYSTICK_STARBYTE;
  adapter_type_item->on_value_changed = menu_value_changed;

  switch (machine_class) {
    case VICE_MACHINE_VIC20:
    case VICE_MACHINE_PET:
       adapter_type_item->choice_disabled[4] = 1;
       adapter_type_item->choice_disabled[5] = 1;
       adapter_type_item->choice_disabled[6] = 1;
       break;
    case VICE_MACHINE_PLUS4:
       adapter_type_item->choice_disabled[0] = 1;
       adapter_type_item->choice_disabled[4] = 1;
       adapter_type_item->choice_disabled[5] = 1;
       adapter_type_item->choice_disabled[6] = 1;
       break;
    default:
       break;
  }
}

uint8_t circle_get_userport_ddr(void) {
  switch (machine_class) {
    case VICE_MACHINE_C64:
    case VICE_MACHINE_C64SC:
    case VICE_MACHINE_SCPU64:
    case VICE_MACHINE_C128:
    case VICE_MACHINE_VIC20:
    case VICE_MACHINE_PET:
      return userport_get_ddr();
      break;
    default:
      break;
  }
  return 0;
}

uint8_t circle_get_userport(void) {
  switch (machine_class) {
    case VICE_MACHINE_C64:
    case VICE_MACHINE_C64SC:
    case VICE_MACHINE_SCPU64:
    case VICE_MACHINE_C128:
    case VICE_MACHINE_VIC20:
    case VICE_MACHINE_PET:
      return userport_get();
      break;
    default:
      break;
  }
  return 0xff;
}

void circle_set_userport(uint8_t value) {
  switch (machine_class) {
    case VICE_MACHINE_C64:
    case VICE_MACHINE_C64SC:
    case VICE_MACHINE_SCPU64:
    case VICE_MACHINE_C128:
    case VICE_MACHINE_VIC20:
    case VICE_MACHINE_PET:
      userport_set(value);
      break;
    default:
      break;
  }
}




#include "sound.h"
extern int emu_is_ui_activated(void);
int emux_set_sound_sample_rate(int sample_rate) {
  int result = resources_set_int("SoundSampleRate", sample_rate);

  /* The bare-metal menu pauses the normal VSync loop, which otherwise
     consumes sound_state_changed in sound_flush().  Flush synchronously
     while the menu is open so hotplug can reopen and publish the selected
     output before the next menu frame. */
  if (result == 0 && emu_is_ui_activated()) {
    sound_flush();
  }
  return result;
}

const char *emux_get_engine_version(void) {
#ifdef VERSION



  switch (machine_class) {
    case VICE_MACHINE_C64:    return "VICE " VERSION " x64";
    case VICE_MACHINE_C64SC:  return "VICE " VERSION " x64sc";
    case VICE_MACHINE_SCPU64: return "VICE " VERSION " xscpu64";
    case VICE_MACHINE_C128:   return "VICE " VERSION " x128";
    case VICE_MACHINE_VIC20:  return "VICE " VERSION " xvic";
    case VICE_MACHINE_PLUS4:  return "VICE " VERSION " xplus4";
    case VICE_MACHINE_PET:    return "VICE " VERSION " xpet";
    default:                  break;
  }
  return "VICE " VERSION;
#else
  return "VICE";
#endif
}

void raspi_keymap_changed(int row, int col, signed long sym) {
  if (row == -1 && col == -1) {
     // Reset. Mark as not set and default to sane values.
     commodore_key_sym_set = 0;
     ctrl_key_sym_set = 0;
     restore_key_sym_set = 0;
     commodore_key_sym = KEYCODE_LeftControl;
     ctrl_key_sym = KEYCODE_Tab;




     restore_key_sym = KEYCODE_PrintScreen;
  }

  machine_keymap_changed(row, col, sym);
}




void emux_flush_disks_now(fullpath_func f_fullpath) {
  int drive;
  for (drive = 0; drive < 4; drive++) {
    file_system_detach_disk(drive + 8, 0);
    if (strlen(attached_disk_name[drive]) > 0) {
      file_system_attach_disk(drive + 8, 0,
         f_fullpath(DIR_DISKS, attached_disk_name[drive]));
    }
  }
}












































