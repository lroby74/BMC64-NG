/*
 * menu_c128.c
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

#include "../raspi_machine.h"

#include <memory.h>

// VICE includes
#include "c128/c128.h"
#include "c128/c128model.h"
#include "cia.h"
#include "drive.h"
#include "sid.h"
#include "vdc.h"
#include "resources.h"
#include "keyboard.h"
#include "cartridge.h"
#include "c64/cart/reu.h"

// RASPI includes
#include "emux_api.h"
#include "menu.h"
#include "ui.h"
#include "overlay.h"
#include "keycodes.h"






static int reu_size_to_index[8] =
    { 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

static struct menu_item *reu_image_menu;
static struct menu_item *reu_attach_image_item;
static struct menu_item *reu_detach_image_item;
static struct menu_item *reu_save_image_item;
static struct menu_item *reu_save_image_as_item;
static struct menu_item *reu_auto_save_item;
static struct menu_item *reu_size_item;



static void update_reu_image_enabled(int enabled) {
  const char *filename;
  int disabled = enabled == 0;

  resources_get_string("REUfilename", &filename);
  reu_image_menu->disabled = disabled;
  reu_attach_image_item->disabled = disabled;
  reu_detach_image_item->disabled =
      disabled || filename == NULL || *filename == '\0';
  reu_save_image_item->disabled =
      disabled || filename == NULL || *filename == '\0';
  reu_save_image_as_item->disabled = disabled;
  reu_auto_save_item->disabled = disabled;
}





static int cartuccia_attaccata(void) {
  static const int altre[] = {
    CARTRIDGE_IEEE488, CARTRIDGE_RAMLINK, CARTRIDGE_IEEEFLASH64,
    CARTRIDGE_MAGIC_VOICE, CARTRIDGE_MMC64, CARTRIDGE_DQBB,
    CARTRIDGE_EXPERT, CARTRIDGE_ISEPIC, CARTRIDGE_RAMCART };
  unsigned int i;

  if (cartridge_get_id(0) != CARTRIDGE_NONE) {
    return 1;
  }
  for (i = 0; i < sizeof(altre) / sizeof(altre[0]); i++) {
    if (cartridge_type_enabled(altre[i])) {
      return 1;
    }
  }
  return 0;
}

static void menu_value_changed(struct menu_item *item) {
  switch (item->id) {
    case MENU_REU:
      if (item->value && cartuccia_attaccata()) {
        emux_detach_cart(0);
      }
      if (resources_set_int("REU", item->value) < 0) {
        item->value = 0;
        ui_error("Unable to enable RAM Expansion");
      }
      update_reu_image_enabled(item->value);
      break;
    case MENU_REU_SIZE:
      if (item->value >= 0 && item->value < 8)
        resources_set_int("REUsize", reu_size_to_index[item->value]);
      break;
    default:
      break;
  }
}

// TODO: Should really move 40/80 stuff into here...
extern struct menu_item *c40_80_column_item;

unsigned long emux_calculate_timing(double fps) {
  if (fps >= 49 && fps <= 51) {
    return C128_PAL_CYCLES_PER_LINE * C128_PAL_SCREEN_LINES * fps;
  } else if (fps >= 59 && fps <= 61) {
    return C128_NTSC_CYCLES_PER_LINE * C128_NTSC_SCREEN_LINES * fps;
  } else {
    return 0;
  }
}

double emux_calculate_fps() {
  if (is_ntsc()) {
     return (double)circle_cycles_per_sec() / (C128_NTSC_CYCLES_PER_LINE * C128_NTSC_SCREEN_LINES);
  }
  return (double)circle_cycles_per_sec() / (C128_PAL_CYCLES_PER_LINE * C128_PAL_SCREEN_LINES);
}

void emux_set_color_brightness(int display_num, int value) {
  if (display_num == 0) {
    resources_set_int("VICIIColorBrightness", value);
  } else {
    resources_set_int("VDCColorBrightness", value);
  }
}

void emux_set_color_contrast(int display_num, int value) {
  if (display_num == 0) {
    resources_set_int("VICIIColorContrast", value);
  } else {
    resources_set_int("VDCColorContrast", value);
  }
}

void emux_set_color_gamma(int display_num, int value) {
  if (display_num == 0) {
    resources_set_int("VICIIColorGamma", value);
  } else {
    resources_set_int("VDCColorGamma", value);
  }
}

void emux_set_color_tint(int display_num, int value) {
  if (display_num == 0) {
    resources_set_int("VICIIColorTint", value);
  } else {
    resources_set_int("VDCColorTint", value);
  }
}

void emux_set_color_saturation(int display_num, int value) {
  if (display_num == 0) {
    resources_set_int("VICIIColorSaturation", value);
  } else {
    resources_set_int("VDCColorSaturation", value);
  }
}

void emux_set_video_cache(int value) {
  resources_set_int("VICIIVideoCache", value);
  resources_set_int("VDCVideoCache", value);
}

void emux_set_hw_scale(int value) {




}

int emux_get_color_brightness(int display_num) {
  int value;
  if (display_num == 0) {
    resources_get_int("VICIIColorBrightness", &value);
  } else {
    resources_get_int("VDCColorBrightness", &value);
  }
  return value;
}

int emux_get_color_contrast(int display_num) {
  int value;
  if (display_num == 0) {
    resources_get_int("VICIIColorContrast", &value);
  } else {
    resources_get_int("VDCColorContrast", &value);
  }
  return value;
}

int emux_get_color_gamma(int display_num) {
  int value;
  if (display_num == 0) {
    resources_get_int("VICIIColorGamma", &value);
  } else {
    resources_get_int("VDCColorGamma", &value);
  }
  return value;
}

int emux_get_color_tint(int display_num) {
  int value;
  if (display_num == 0) {
    resources_get_int("VICIIColorTint", &value);
  } else {
    resources_get_int("VDCColorTint", &value);
  }
  return value;
}

int emux_get_color_saturation(int display_num) {
  int value;
  if (display_num == 0) {
    resources_get_int("VICIIColorSaturation", &value);
  } else {
    resources_get_int("VDCColorSaturation", &value);
  }
  return value;
}

void cartridge_freeze(void) {
  keyboard_clear_keymatrix();
  cartridge_trigger_freeze();
}

struct menu_item* emux_add_palette_options(int menu_id, struct menu_item* parent) {
  struct menu_item* palette_item =
      ui_menu_add_multiple_choice(menu_id, parent, "Color Palette");
  if (menu_id == MENU_COLOR_PALETTE_1) {



    palette_item->num_choices = 5;
    palette_item->value = 0;
    strcpy(palette_item->choices[0], "RGBI (16 colors)");
    strcpy(palette_item->choices[1], "Composite");
    strcpy(palette_item->choices[2], "White Phosphor");
    strcpy(palette_item->choices[3], "Green Phosphor");
    strcpy(palette_item->choices[4], "Amber Phosphor");
  } else {
    palette_item->num_choices = 17;
    palette_item->value = 0;
    strcpy(palette_item->choices[0], "VICE");
    strcpy(palette_item->choices[1], "Pepto (PAL)");
    strcpy(palette_item->choices[2], "Pepto (old PAL)");
    strcpy(palette_item->choices[3], "Pepto (NTSC, Sony)");
    strcpy(palette_item->choices[4], "Pepto (NTSC)");
    strcpy(palette_item->choices[5], "Colodore (PAL)");
    strcpy(palette_item->choices[6], "ChristopherJam");
    strcpy(palette_item->choices[7], "C64HQ");
    strcpy(palette_item->choices[8], "C64S");
    strcpy(palette_item->choices[9], "CCS64");
    strcpy(palette_item->choices[10], "Frodo");
    strcpy(palette_item->choices[11], "Godot");
    strcpy(palette_item->choices[12], "PC64");
    strcpy(palette_item->choices[13], "RGB");
    strcpy(palette_item->choices[14], "Deekay");
    strcpy(palette_item->choices[15], "Ptoing");
    strcpy(palette_item->choices[16], "Community Colors");
  }
  return palette_item;
}

// We added a hook for this 'cause there appeared to be no way
// to get this notification from VICE
void column4080_key_toggled(void) {
  int v;
  resources_get_int("C128ColumnKey", &v);
  c40_80_column_item->value = v;
  overlay_40_80_columns_changed(v);
}











static void c128_apply_model(int modello) {
  int board, vdc_rev, vdc64k, cia, sid, drive;

  switch (modello) {
    case 1:
      board = BOARD_C128D; vdc_rev = VDC_REVISION_1; vdc64k = 0;
      cia = CIA_MODEL_6526; sid = SID_MODEL_6581; drive = DRIVE_TYPE_1571;
      break;
    case 2:
      board = BOARD_C128D; vdc_rev = VDC_REVISION_2; vdc64k = 1;
      cia = CIA_MODEL_6526A; sid = SID_MODEL_8580; drive = DRIVE_TYPE_1571CR;
      break;
    default:
      board = BOARD_C128; vdc_rev = VDC_REVISION_1; vdc64k = 0;
      cia = CIA_MODEL_6526; sid = SID_MODEL_6581; drive = DRIVE_TYPE_NONE;
      break;
  }

  resources_set_int("BoardType", board);
  resources_set_int("VDCRevision", vdc_rev);
  resources_set_int("VDC64KB", vdc64k);
  resources_set_int("CIA1Model", cia);
  resources_set_int("CIA2Model", cia);
  resources_set_int("Drive8Type", drive);



  {
    int old_engine, old_sid;
    resources_get_int("SidEngine", &old_engine);
    resources_get_int("SidModel", &old_sid);
    if ((old_sid == SID_MODEL_8580 || old_sid == SID_MODEL_8580D)
        != (sid == SID_MODEL_8580)) {
      sid_set_engine_model(old_engine, sid);
    }
  }





  emux_reset(0);
}

static const char *c128_model_names[] = { "C128", "C128D", "C128DCR" };
#define C128_NUM_MODELS 3

static int c128_current_model(void) {
  int board = BOARD_C128, drive = DRIVE_TYPE_NONE;
  resources_get_int("BoardType", &board);
  resources_get_int("Drive8Type", &drive);
  if (board == BOARD_C128) {
    return 0;
  }
  return (drive == DRIVE_TYPE_1571CR) ? 2 : 1;
}

static void c128_model_picked(struct menu_item *item) {
  c128_apply_model(item->value);
  ui_pop_all_and_toggle();
}

static void c128_open_model_list(struct menu_item *item) {
  struct menu_item *lista = ui_push_menu(12, 8);
  int in_vigore = c128_current_model();
  int i;
  for (i = 0; i < C128_NUM_MODELS; i++) {
    struct menu_item *v =
        ui_menu_add_button(MENU_C128_MODEL_SELECT, lista, c128_model_names[i]);
    v->value = i;
    if (i == in_vigore) {
      strcat(v->displayed_value, " (*)");
    }
    v->on_value_changed = c128_model_picked;
  }
}

static void c128_add_model_option(struct menu_item *parent) {
  struct menu_item *item =
      ui_menu_add_button(MENU_C128_MODEL, parent, "Model...");
  item->on_value_changed = c128_open_model_list;
}

void emux_add_machine_options(struct menu_item* parent) {
  c128_add_model_option(parent);
  struct menu_item* roms_parent = ui_menu_add_folder(parent, "ROMs...");
  ui_menu_add_button(MENU_C128_LOAD_KERNAL, roms_parent, "Load C128 Kernal ROM...");
  ui_menu_add_button(MENU_C128_LOAD_BASIC_HI, roms_parent, "Load C128 Basic HI ROM...");
  ui_menu_add_button(MENU_C128_LOAD_BASIC_LO, roms_parent, "Load C128 Basic LO ROM...");
  ui_menu_add_button(MENU_C128_LOAD_CHARGEN, roms_parent, "Load C128 Chargen ROM...");
  ui_menu_add_button(MENU_C128_LOAD_64_KERNAL, roms_parent, "Load C64 Kernal ROM...");
  ui_menu_add_button(MENU_C128_LOAD_64_BASIC, roms_parent, "Load C64 Basic ROM...");
}

struct menu_item* emux_add_cartridge_options(struct menu_item* root) {
  struct menu_item* parent = ui_menu_add_folder(root, "Cartridge");
  ui_menu_add_button(MENU_C64_ATTACH_CART, parent, "Attach cart...");
  ui_menu_add_button(MENU_C64_ATTACH_CART_8K, parent, "Attach 8k raw...");
  ui_menu_add_button(MENU_C64_ATTACH_CART_16K, parent, "Attach 16 raw...");
  ui_menu_add_button(MENU_C64_ATTACH_CART_ULTIMAX, parent, "Attach Ultimax raw...");
  ui_menu_add_button(MENU_DETACH_CART, parent, "Detach cartridge");

  ui_menu_add_button(MENU_TEXT, parent, "");
  ui_menu_add_button(MENU_MAKE_CART_DEFAULT, parent,
                     "Set current cart default");

  ui_menu_add_button(MENU_SAVE_EASYFLASH, parent, "Save EasyFlash Now");
  ui_menu_add_button(MENU_CART_FREEZE, parent, "Cartridge Freeze");







  struct menu_item* child =
      ui_menu_add_folder(root, "Ram Expansion Unit (REU)");

  int tmp;
  resources_get_int("REU", &tmp);
  struct menu_item* reu_item =
     ui_menu_add_toggle(MENU_REU, child, "Ram Expansion", tmp);
  reu_item->on_value_changed = menu_value_changed;

  reu_size_item =
      ui_menu_add_multiple_choice(MENU_REU_SIZE, child, "Memory Size");
  reu_size_item->on_value_changed = menu_value_changed;
  reu_size_item->num_choices = 8;

  resources_get_int("REUsize", &tmp);
  reu_size_item->value = 2;
  for (int t = 0; t < 8; t++) {
    if (tmp == reu_size_to_index[t])
       reu_size_item->value = t;
  }

  strcpy(reu_size_item->choices[0], "128k");
  strcpy(reu_size_item->choices[1], "256k");
  strcpy(reu_size_item->choices[2], "512k");
  strcpy(reu_size_item->choices[3], "1024k");
  strcpy(reu_size_item->choices[4], "2048k");
  strcpy(reu_size_item->choices[5], "4096k");
  strcpy(reu_size_item->choices[6], "8192k");
  strcpy(reu_size_item->choices[7], "16384k");





  reu_image_menu = ui_menu_add_folder(child, "Ram Image (optional)");
  reu_attach_image_item = ui_menu_add_button(
    MENU_REU_ATTACH_IMAGE, reu_image_menu, "Load image...");
  reu_detach_image_item = ui_menu_add_button(
    MENU_REU_DETACH_IMAGE, reu_image_menu, "Clear image");
  reu_save_image_item = ui_menu_add_button(
    MENU_REU_SAVE_IMAGE, reu_image_menu, "Save image now");
  reu_save_image_as_item = ui_menu_add_button(
    MENU_REU_SAVE_IMAGE_AS, reu_image_menu, "Save image as...");






  {
    int scrivi = 0;
    resources_get_int("REUImageWrite", &scrivi);
    reu_auto_save_item = ui_menu_add_toggle(
      MENU_REU_IMAGE_WRITE, reu_image_menu, "Auto-save image", scrivi);
  }
  update_reu_image_enabled(reu_item->value);

  return parent;
}

int emux_save_reu_image(const char *path) {
  if (!reu_cart_enabled()) {
    return -2;
  }
  return reu_bin_save(path);
}





int emux_sid_carica(const char *percorso) { (void)percorso; return -1; }
int emux_sid_brano(int passo) { (void)passo; return -1; }
int emux_sid_quanti_brani(void) { return 0; }
int emux_sid_quale_brano(void) { return 0; }

int emux_sid_foto_chiedi(int secondo, unsigned long clk0) { (void)secondo; (void)clk0; return 0; }
int emux_sid_foto_cerca(int dove) { (void)dove; return -1; }
int emux_sid_foto_torna(int secondo, unsigned long *clk0) { (void)secondo; (void)clk0; return -1; }
int emux_sid_foto_in_volo(void) { return -1; }
int emux_sid_foto_andata_male(void) { return 0; }
void emux_sid_foto_via(void) {}
void emux_sid_foto_conta(int *quante, int *dal, int *al, unsigned long *byte,
                         unsigned long *pagine) {
  *quante = 0; *dal = -1; *al = -1; *byte = 0; *pagine = 0;
}
int emux_sid_livelli(int livello[3]) { (void)livello; return 0; }
int emux_sid_voci_mute(void) { return 0; }
void emux_sid_voci_imposta(int mute) { (void)mute; }

int emux_sid_lato(int s) { (void)s; return -1; }
int emux_sid_lato_cambia(int s) { (void)s; return -1; }
void emux_sid_drive_zitti(int zitti) { (void)zitti; }
int emux_sid_drive_zitti_ora(void) { return 0; }
const char *emux_sid_chi_suona(int quale) { (void)quale; return ""; }
const char *emux_sid_velocita(void) { return ""; }
void emux_sid_ferma(void) {}
void emux_sid_lascia_la_porta(void) {}

void emux_reu_image_loaded(int size_kb) {
  if (size_kb > 0) {
    int index;
    for (index = 0; index < 8; index++) {
      if (reu_size_to_index[index] == size_kb) {
        reu_size_item->value = index;
        break;
      }
    }
  }
  update_reu_image_enabled(reu_cart_enabled());
}

void emux_machine_load_settings_done(void) {
}

void machine_keymap_changed(int row, int col, signed long sym) {
  if (row == 7 && col == 5 && !commodore_key_sym_set) {
     commodore_key_sym = sym;
     commodore_key_sym_set = 1;
  } else if (row == 7 && col == 2 && !ctrl_key_sym_set) {
     ctrl_key_sym = sym;
     ctrl_key_sym_set = 1;
  } else if (row == -3 && col == 0 && !restore_key_sym_set) {
     restore_key_sym = sym;
     restore_key_sym_set = 1;
  }
}
