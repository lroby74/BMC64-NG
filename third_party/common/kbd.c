/*
 * kbd.c
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

#include "kbd.h"

#include <stdio.h>
#include <string.h>

// RASPI includes
#include "emux_api.h"
#include "circle.h"
#include "demo.h"
#include "joy.h"
#include "menu.h"
#include "overlay.h"
#include "menu_keyset.h"
#include "menu_switch.h"
#include "ui.h"

#define NUM_KEY_COMBOS 8
#define TICKS_PER_SECOND 1000000L

static int commodore_down = 0;
static int control_down = 0;


































static int alt_down = 0;
static int shift_down = 0;









static int solo_gs(void) {
  return menu_is_cartridge_only();
}


unsigned kbd_tasti_arrivati = 0;
unsigned kbd_tasti_col_gs = 0;
long kbd_ultimo_tasto = -1;

static int e_modificatore(long key) {
  return key >= KEYCODE_LeftShift && key <= KEYCODE_RightSuper;
}


static int combo_ammessa(int funzione) {
  if (!solo_gs()) {
    return 1;
  }
  return funzione == BTN_ASSIGN_MENU || funzione == BTN_ASSIGN_RESET_MENU ||
         funzione == BTN_ASSIGN_RESET_HARD ||
         funzione == BTN_ASSIGN_RESET_SOFT;
}








static int tasto_alt_gs(long key) {
  return key == KEYCODE_r || key == KEYCODE_Escape ||
         key == KEYCODE_c || key == KEYCODE_o;
}















static int tasto_alt_lettore(long key) {

  if (!shift_down && key >= KEYCODE_1 && key <= KEYCODE_4) {
    return 1;
  }
  switch (key) {
  case KEYCODE_Comma:
  case KEYCODE_Period:
  case KEYCODE_p:
  case KEYCODE_m:
  case KEYCODE_s:
  case KEYCODE_Escape:
  case KEYCODE_w:
  case KEYCODE_BackSlash:
  case KEYCODE_KP_BackSlash:
    return 1;
  case KEYCODE_Equals:
  case KEYCODE_RightBracket:
  case KEYCODE_KP_Add:
  case KEYCODE_Dash:
  case KEYCODE_Slash:
  case KEYCODE_KP_Subtract:
    return !shift_down;
  default:
    return 0;
  }
}



















#define TASTI_QUANTI 0x108
static unsigned char pressione_mangiata[TASTI_QUANTI];

static void segna_mangiata(long key, int si) {
  if (key >= 0 && key < TASTI_QUANTI) {
    pressione_mangiata[key] = (unsigned char)si;
  }
}














static long c128_finto(long key) {
  switch (key) {
    case KEYCODE_F1: return KEYCODE_C128_ESC;
    case KEYCODE_F2: return KEYCODE_C128_TAB;
    case KEYCODE_F3: return KEYCODE_C128_ALT;
    case KEYCODE_F4: return KEYCODE_C128_CAPS;
    case KEYCODE_F5: return KEYCODE_C128_HELP;
    case KEYCODE_F6: return KEYCODE_C128_LINEFEED;



    case KEYCODE_F8: return KEYCODE_C128_NOSCROLL;
    default: return 0;
  }
}






static long c128_finto_dato[TASTI_QUANTI];

static int era_mangiata(long key) {
  return key >= 0 && key < TASTI_QUANTI && pressione_mangiata[key];
}












static int modificatori_da_lasciare = 0;


static int modificatori_adesso(void) {
  return (shift_down     ? 1 : 0) |
         (alt_down       ? 2 : 0) |
         (control_down   ? 4 : 0) |
         (commodore_down ? 8 : 0);
}

void kbd_aspetta_il_rilascio(void) {
  modificatori_da_lasciare = modificatori_adesso();
}

int kbd_aspetta_ancora(void) {


  modificatori_da_lasciare &= modificatori_adesso();
  return modificatori_da_lasciare != 0;
}

















#define MODIFICATORE_APPENA_US (1 * TICKS_PER_SECOND)

static unsigned long alt_lasciato = 0;
static unsigned long shift_lasciato = 0;

















#define MOD_UTILI 0x66

static unsigned char mod_ora = 0;
static unsigned char mod_prima = 0;
static unsigned long mod_cambiato = 0;

void emu_modificatori_grezzi(unsigned char modificatori) {
  if (modificatori == mod_ora) {
    return;
  }
  mod_prima = mod_ora;
  mod_ora = modificatori;
  mod_cambiato = circle_get_ticks();
}



static unsigned char modificatore_grezzo(void) {
  unsigned long ora = circle_get_ticks();

  if (mod_ora & MOD_UTILI) {
    return mod_ora;
  }



  if ((mod_prima & MOD_UTILI) && mod_cambiato != 0 &&
      ora - mod_cambiato < MODIFICATORE_APPENA_US) {
    return mod_prima;
  }
  return 0;
}

static int modificatore_appena(void) {
  unsigned long ora = circle_get_ticks();

  if (modificatore_grezzo()) {
    return 1;
  }
  if (alt_down || shift_down) {
    return 1;
  }
  if (alt_lasciato != 0 && ora - alt_lasciato < MODIFICATORE_APPENA_US) {
    return 1;
  }
  if (shift_lasciato != 0 && ora - shift_lasciato < MODIFICATORE_APPENA_US) {
    return 1;
  }
  return 0;
}

static int comando_nastro(long key) {






  if (!shift_down && ui_lettore_sid_a_schermo() &&
      key >= KEYCODE_1 && key <= KEYCODE_3) {
    emu_quick_func_interrupt(BTN_ASSIGN_SID_MUTA_1 + (int)(key - KEYCODE_1));
    return 1;
  }



  if (!shift_down && ui_lettore_sid_a_schermo() && key == KEYCODE_4) {
    emu_quick_func_interrupt(BTN_ASSIGN_SID_MUTA_4);
    return 1;
  }
  switch (key) {
  case KEYCODE_Up:
    emu_quick_func_interrupt(BTN_ASSIGN_TAPE_PLAY2);
    return 1;
  case KEYCODE_Down:
    emu_quick_func_interrupt(BTN_ASSIGN_TAPE_STOP2);
    return 1;
  case KEYCODE_Right:

    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_TAPE_FF_2X
                                        : BTN_ASSIGN_TAPE_FF2);
    return 1;
  case KEYCODE_Left:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_TAPE_REW_2X
                                        : BTN_ASSIGN_TAPE_REW2);
    return 1;
  case KEYCODE_Delete:
    emu_quick_func_interrupt(BTN_ASSIGN_TAPE_ZERO2);
    return 1;
  case KEYCODE_w:






    if (ui_lettore_sid_a_schermo()) {
      overlay_avviso("NOT IN SID PLAYER");
      return 1;
    }
    emu_quick_func_interrupt(BTN_ASSIGN_WARP);
    return 1;
  case KEYCODE_Escape:

















    if (shift_down) {
      menu_poweroff_key();
    } else {
      menu_reboot_key();
    }
    return 1;
  case KEYCODE_8:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_DETACH_DISK_8_2
                                        : BTN_ASSIGN_ATTACH_DISK_8_2);
    return 1;
  case KEYCODE_9:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_DETACH_DISK_9_2
                                        : BTN_ASSIGN_ATTACH_DISK_9_2);
    return 1;
  case KEYCODE_c:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_DETACH_CART_2
                                        : BTN_ASSIGN_ATTACH_CART_2);
    return 1;
  case KEYCODE_t:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_DETACH_TAPE_2
                                        : BTN_ASSIGN_ATTACH_TAPE_2);
    return 1;
  case KEYCODE_j:
    emu_quick_func_interrupt(BTN_ASSIGN_SWAP_PORTS);
    return 1;
  case KEYCODE_r:






    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_RESET_SOFT
                                        : BTN_ASSIGN_RESET_HARD);
    return 1;
  case KEYCODE_a:
    emu_quick_func_interrupt(BTN_ASSIGN_AUTOSTART_2);
    return 1;
  case KEYCODE_o:


    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_OVERLAY_INFO_2
                                        : BTN_ASSIGN_OVERLAY_2);
    return 1;
  case KEYCODE_e:



    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_REU_MISURA
                                        : BTN_ASSIGN_REU_2);
    return 1;
  case KEYCODE_s:



    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_SID_FERMA
                                        : BTN_ASSIGN_SID_2);
    return 1;
  case KEYCODE_m:
    emu_quick_func_interrupt(BTN_ASSIGN_MUTO);
    return 1;






  case KEYCODE_Equals:
  case KEYCODE_RightBracket:
  case KEYCODE_KP_Add:

    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_SOLCO_SU
                                        : BTN_ASSIGN_VOLUME_SU);
    return 1;
  case KEYCODE_Dash:
  case KEYCODE_Slash:
  case KEYCODE_KP_Subtract:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_SOLCO_GIU
                                        : BTN_ASSIGN_VOLUME_GIU);
    return 1;








  case KEYCODE_BackSlash:
  case KEYCODE_KP_BackSlash:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_ASPETTO_ALTRO
                                        : BTN_ASSIGN_ASPETTO_2);
    return 1;













  case KEYCODE_PrintScreen:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_COLONNE_2
                                        : BTN_ASSIGN_SCHERMO_2);
    return 1;




  case KEYCODE_1:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_NONE
                                        : BTN_ASSIGN_MODELLO8_NONE);
    return 1;
  case KEYCODE_2:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_1541
                                        : BTN_ASSIGN_MODELLO8_1541);
    return 1;
  case KEYCODE_3:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_1541II
                                        : BTN_ASSIGN_MODELLO8_1541II);
    return 1;
  case KEYCODE_4:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_1570
                                        : BTN_ASSIGN_MODELLO8_1570);
    return 1;
  case KEYCODE_5:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_1571
                                        : BTN_ASSIGN_MODELLO8_1571);
    return 1;
  case KEYCODE_6:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_1581
                                        : BTN_ASSIGN_MODELLO8_1581);
    return 1;
  case KEYCODE_7:
    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_MODELLO9_VERO
                                        : BTN_ASSIGN_MODELLO8_VERO);
    return 1;
  case KEYCODE_Comma:
  case KEYCODE_Period:





    emu_quick_func_interrupt(key == KEYCODE_Period ? BTN_ASSIGN_SID_NEXT
                                                   : BTN_ASSIGN_SID_PREV);
    return 1;
  case KEYCODE_p:
    emu_quick_func_interrupt(BTN_ASSIGN_SID_PAUSA);
    return 1;
  case KEYCODE_0:



    emu_quick_func_interrupt(shift_down ? BTN_ASSIGN_REU_SMONTA
                                        : BTN_ASSIGN_REU_MONTA);
    return 1;
  default:
    return 0;
  }
}
static int f7_down = 0;
static unsigned long video_reset_time_down;
static unsigned long video_reset_time_delay = TICKS_PER_SECOND * 5;

key_combo_state_t key_combo_states[NUM_KEY_COMBOS];

extern void reboot(void);

void kbd_arch_init(void) {}

void kbd_set_hotkey_function(unsigned int slot, long key, int function) {
  if (slot >= NUM_KEY_COMBOS)
    return;
  key_combo_states[slot].second_key = key;
  key_combo_states[slot].invoked = 0;
  key_combo_states[slot].function = function;
}

// Tests keyname var against given string
#define KCMP(x) (strcmp(keyname, x) == 0)

// v2.5 made keycodes consistently BMC64 usb keycode
// names rather than a mix of codes and c64 labels. But
// to keep compatibility with existing files out there,
// we still match the old names. The most unfortunate
// one was "Delete" which has to stay. The new code is
// "Del".
#define LEGACY_KCMP(x) (strcmp(keyname, x) == 0)

signed long kbd_arch_keyname_to_keynum(char *keyname) {
  if (strlen(keyname) == 1) {
    switch (keyname[0]) {
    case 'a':
      return KEYCODE_a;
    case 'b':
      return KEYCODE_b;
    case 'c':
      return KEYCODE_c;
    case 'd':
      return KEYCODE_d;
    case 'e':
      return KEYCODE_e;
    case 'f':
      return KEYCODE_f;
    case 'g':
      return KEYCODE_g;
    case 'h':
      return KEYCODE_h;
    case 'i':
      return KEYCODE_i;
    case 'j':
      return KEYCODE_j;
    case 'k':
      return KEYCODE_k;
    case 'l':
      return KEYCODE_l;
    case 'm':
      return KEYCODE_m;
    case 'n':
      return KEYCODE_n;
    case 'o':
      return KEYCODE_o;
    case 'p':
      return KEYCODE_p;
    case 'q':
      return KEYCODE_q;
    case 'r':
      return KEYCODE_r;
    case 's':
      return KEYCODE_s;
    case 't':
      return KEYCODE_t;
    case 'u':
      return KEYCODE_u;
    case 'v':
      return KEYCODE_v;
    case 'w':
      return KEYCODE_w;
    case 'x':
      return KEYCODE_x;
    case 'y':
      return KEYCODE_y;
    case 'z':
      return KEYCODE_z;
    case '1':
      return KEYCODE_1;
    case '2':
      return KEYCODE_2;
    case '3':
      return KEYCODE_3;
    case '4':
      return KEYCODE_4;
    case '5':
      return KEYCODE_5;
    case '6':
      return KEYCODE_6;
    case '7':
      return KEYCODE_7;
    case '8':
      return KEYCODE_8;
    case '9':
      return KEYCODE_9;
    case '0':
      return KEYCODE_0;
    }
    return 0;
  } else if (KCMP("Return")) {
    return (long)KEYCODE_Return;
  } else if (KCMP("BackSpace")) {
    return (long)KEYCODE_Backspace;
  } else if (KCMP("PageUp") || LEGACY_KCMP("Delete")) {
    return (long)KEYCODE_PageUp;
  } else if (KCMP("PrintScreen")) {

    return (long)KEYCODE_PrintScreen;
  } else if (KCMP("PageDown")) {
    return (long)KEYCODE_PageDown;
  } else if (KCMP("CapsLock")) {
    return (long)KEYCODE_CapsLock;
  } else if (KCMP("Up")) {
    return (long)KEYCODE_Up;
  } else if (KCMP("Down")) {
    return (long)KEYCODE_Down;
  } else if (KCMP("Left")) {
    return (long)KEYCODE_Left;
  } else if (KCMP("Right")) {
    return (long)KEYCODE_Right;
  } else if (KCMP("Up")) {
    return (long)KEYCODE_Up;
  } else if (KCMP("Comma") || LEGACY_KCMP("comma")) {
    return (long)KEYCODE_Comma;
  } else if (KCMP("Period") || LEGACY_KCMP("period")) {
    return (long)KEYCODE_Period;
  } else if (KCMP("Space") || LEGACY_KCMP("space")) {
    return (long)KEYCODE_Space;
  } else if (KCMP("RightBracket") || LEGACY_KCMP("asterisk")) {
    return (long)KEYCODE_RightBracket;
  } else if (KCMP("Del") || LEGACY_KCMP("arrowup")) {
    return (long)KEYCODE_Delete;
  } else if (KCMP("Shift_L")) {
    return (long)KEYCODE_LeftShift;
  } else if (KCMP("Shift_R")) {
    return (long)KEYCODE_RightShift;
  } else if (KCMP("Dash") || LEGACY_KCMP("plus")) {
    return (long)KEYCODE_Dash;
  } else if (KCMP("BackQuote") || LEGACY_KCMP("arrowleft")) {
    return (long)KEYCODE_BackQuote;
  } else if (KCMP("Equals") || LEGACY_KCMP("minus")) {
    return (long)KEYCODE_Equals;
  } else if (KCMP("SemiColon") || LEGACY_KCMP("colon")) {
    return (long)KEYCODE_SemiColon;
  } else if (KCMP("Home")) {
    return (long)KEYCODE_Home;
  } else if (KCMP("End")) {
    return (long)KEYCODE_End;
  } else if (KCMP("Slash") || LEGACY_KCMP("slash")) {
    return (long)KEYCODE_Slash;
  } else if (KCMP("BackSlash")) {
    return (long)KEYCODE_BackSlash;
  } else if (KCMP("C128_ESC")) {



    return (long)KEYCODE_C128_ESC;
  } else if (KCMP("C128_TAB")) {
    return (long)KEYCODE_C128_TAB;
  } else if (KCMP("C128_ALT")) {
    return (long)KEYCODE_C128_ALT;
  } else if (KCMP("C128_CAPS")) {
    return (long)KEYCODE_C128_CAPS;
  } else if (KCMP("C128_HELP")) {
    return (long)KEYCODE_C128_HELP;
  } else if (KCMP("C128_LINEFEED")) {
    return (long)KEYCODE_C128_LINEFEED;
  } else if (KCMP("C128_4080")) {
    return (long)KEYCODE_C128_4080;
  } else if (KCMP("C128_NOSCROLL")) {
    return (long)KEYCODE_C128_NOSCROLL;
  } else if (KCMP("NonUSBackSlash") || KCMP("KP_BackSlash")) {








    return (long)KEYCODE_KP_BackSlash;
  } else if (KCMP("Pound")) {
    return (long)KEYCODE_Pound;
  } else if (KCMP("Insert") || LEGACY_KCMP("sterling")) {
    return (long)KEYCODE_Insert;
  } else if (KCMP("SingleQuote") || LEGACY_KCMP("semicolon")) {
    return (long)KEYCODE_SingleQuote;
  } else if (KCMP("Tab")) {
    return (long)KEYCODE_Tab;
  } else if (KCMP("Control_L")) {
    return (long)KEYCODE_LeftControl;
  } else if (KCMP("Control_R")) {
    return (long)KEYCODE_RightControl;
  } else if (KCMP("Alt_L")) {
    return (long)KEYCODE_LeftAlt;
  } else if (KCMP("Alt_R")) {
    return (long)KEYCODE_RightAlt;
  } else if (KCMP("Super_L")) {
    return (long)KEYCODE_LeftSuper;
  } else if (KCMP("Super_R")) {
    return (long)KEYCODE_RightSuper;
  } else if (KCMP("Escape")) {
    return (long)KEYCODE_Escape;
  } else if (KCMP("LeftBracket") || LEGACY_KCMP("at")) {
    return (long)KEYCODE_LeftBracket;
  } else if (KCMP("F1")) {
    return (long)KEYCODE_F1;
  } else if (KCMP("F2")) {
    return (long)KEYCODE_F2;
  } else if (KCMP("F3")) {
    return (long)KEYCODE_F3;
  } else if (KCMP("F4")) {
    return (long)KEYCODE_F4;
  } else if (KCMP("F5")) {
    return (long)KEYCODE_F5;
  } else if (KCMP("F6")) {
    return (long)KEYCODE_F6;
  } else if (KCMP("F7")) {
    return (long)KEYCODE_F7;
  } else if (KCMP("F8")) {
    return (long)KEYCODE_F8;
  } else if (KCMP("F9")) {
    return (long)KEYCODE_F9;
  } else if (KCMP("F10")) {
    return (long)KEYCODE_F10;
  } else if (KCMP("F11")) {
    return (long)KEYCODE_F11;
  } else if (KCMP("ScrollLock")) {
    return (long)KEYCODE_ScrollLock;
  } else if (KCMP("KP_Divide")) {
    return (long)KEYCODE_KP_Divide;
  } else if (KCMP("KP_Decimal")) {
    return (long)KEYCODE_KP_Decimal;
  } else if (KCMP("KP_Multiply")) {
    return (long)KEYCODE_KP_Multiply;
  } else if (KCMP("KP_Subtract")) {
    return (long)KEYCODE_KP_Subtract;
  } else if (KCMP("KP_Add")) {
    return (long)KEYCODE_KP_Add;
  } else if (KCMP("KP_Enter")) {
    return (long)KEYCODE_KP_Enter;
  } else if (KCMP("KP_1")) {
    return (long)KEYCODE_KP1;
  } else if (KCMP("KP_2")) {
    return (long)KEYCODE_KP2;
  } else if (KCMP("KP_3")) {
    return (long)KEYCODE_KP3;
  } else if (KCMP("KP_4")) {
    return (long)KEYCODE_KP4;
  } else if (KCMP("KP_5")) {
    return (long)KEYCODE_KP5;
  } else if (KCMP("KP_6")) {
    return (long)KEYCODE_KP6;
  } else if (KCMP("KP_7")) {
    return (long)KEYCODE_KP7;
  } else if (KCMP("KP_8")) {
    return (long)KEYCODE_KP8;
  } else if (KCMP("KP_9")) {
    return (long)KEYCODE_KP9;
  } else if (KCMP("KP_0")) {
    return (long)KEYCODE_KP0;
  }

  return 0;
}

const char *kbd_arch_keynum_to_keyname(signed long keynum) { return 0; }

void kbd_initialize_numpad_joykeys(int *joykeys) {}

// Return 1 if press is consumed
static int handle_key_combo_press(long key) {
  int i;
  // CBM Commodore key checks
  if (commodore_down) {
    for (i = 0; i < 4; i++) {
      if (key_combo_states[i].second_key == key &&
          combo_ammessa(key_combo_states[i].function)) {
        key_combo_states[i].invoked = 1;
        return 1;
      }
    }
  }
  // CBM Control key checks
  if (control_down) {
    for (i = 4; i < 8; i++) {
      if (key_combo_states[i].second_key == key &&
          combo_ammessa(key_combo_states[i].function)) {
        key_combo_states[i].invoked = 1;
        return 1;
      }
    }
  }
  return 0;
}

// Return 1 if release is consumed
// Some things we can do on the up event of the 2nd key
static int handle_key_combo_release(long key) {
  int i;
  for (i = 0; i < NUM_KEY_COMBOS; i++) {
    if (key_combo_states[i].second_key == key && key_combo_states[i].invoked) {
      // Can we do this now?
      // KEEP THIS IN SYNC WITH kernel.cpp
      switch (key_combo_states[i].function) {
      case BTN_ASSIGN_WARP:
      case BTN_ASSIGN_SWAP_PORTS:
      case BTN_ASSIGN_STATUS_TOGGLE:
      case BTN_ASSIGN_CART_FREEZE:
      case BTN_ASSIGN_ACTIVE_DISPLAY:
      case BTN_ASSIGN_PIP_LOCATION:
      case BTN_ASSIGN_PIP_SWAP:
      case BTN_ASSIGN_40_80_COLUMN:
      case BTN_ASSIGN_FLUSH_DISK:
        emu_quick_func_interrupt(key_combo_states[i].function);
        key_combo_states[i].invoked = 0;
        return 1;
      default:
        break;
      }
      return 0;
    }
  }
  return 0;
}

// Some things we can only do on the up event of the cntrl key
static void handle_key_combo_function() {
  int i;
  for (i = 0; i < NUM_KEY_COMBOS; i++) {
    if (key_combo_states[i].invoked) {
      key_combo_states[i].invoked = 0;

      switch (key_combo_states[i].function) {
      case BTN_ASSIGN_MENU:
        // When transitioning to the menu, make sure to give emulator
        // at least one pass through main loop after sending the up evt.
        circle_lock_acquire();
        ui_toggle_pending = 2;
        circle_lock_release();
        break;
      case BTN_ASSIGN_RESET_MENU:
      case BTN_ASSIGN_RESET_HARD:
      case BTN_ASSIGN_RESET_SOFT:
      case BTN_ASSIGN_TAPE_MENU:
      case BTN_ASSIGN_CART_MENU:
        emu_quick_func_interrupt(key_combo_states[i].function);
        break;
      default:
        break;
      }
    }
  }
}





static int porta_di_tasti(int devd) {
  return devd == JOYDEV_NUMS_1 || devd == JOYDEV_NUMS_2 ||
         devd == JOYDEV_CURS_SP || devd == JOYDEV_CURS_LC ||
         devd == JOYDEV_KEYSET1 || devd == JOYDEV_KEYSET2;
}

void emu_key_pressed(long key) {
  kbd_tasti_arrivati++;
  kbd_ultimo_tasto = key;
  if (solo_gs()) {
    kbd_tasti_col_gs++;
  }



  menu_vidtrial_keep();





  segna_mangiata(key, 0);



  if (key == KEYCODE_Power) {
    return;
  }





  if (key == KEYCODE_Escape && !alt_down && menu_poweroff_annulla()) {
    return;
  }

  if (raw_keycode_func) {
    // Just consume this.
    return;
  }

  if (key == KEYCODE_LeftShift || key == KEYCODE_RightShift) {

    shift_down = 1;
  }

  if (key == KEYCODE_LeftAlt || key == KEYCODE_RightAlt) {
    alt_down = 1;
  } else if (alt_down && !ui_enabled &&
             emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
             key == KEYCODE_F7) {







    emu_quick_func_interrupt(BTN_ASSIGN_COLONNE_2);
    segna_mangiata(key, 1);
    return;
  } else if (alt_down && !ui_enabled &&
             emux_machine_class == BMC64_MACHINE_CLASS_C128 &&
             c128_finto(key) != 0) {



    long finto = c128_finto(key);
    if (key >= 0 && key < TASTI_QUANTI) {


      if (c128_finto_dato[key] != 0 && c128_finto_dato[key] != finto) {
        emux_key_interrupt(c128_finto_dato[key], 0);
      }
      c128_finto_dato[key] = finto;
    }
    emux_key_interrupt(finto, 1);
    return;
  } else if (alt_down && !ui_enabled && solo_gs() && !e_modificatore(key) &&
             !tasto_alt_gs(key)) {

    overlay_avviso("NOT ON C64 GS");
    segna_mangiata(key, 1);
    return;
  } else if (alt_down && !ui_enabled && ui_lettore_sid_a_schermo() &&
             !e_modificatore(key) && !tasto_alt_lettore(key)) {


    overlay_avviso("NOT IN SID PLAYER");
    segna_mangiata(key, 1);
    return;
  } else if (alt_down && !ui_enabled && comando_nastro(key)) {
    segna_mangiata(key, 1);
    return;
  }




















  if (!alt_down && !control_down && !commodore_down && !ui_enabled &&
      ui_lettore_sid_a_schermo()) {
    int b = 0;
    if (key >= KEYCODE_1 && key <= KEYCODE_9) {
      b = BTN_ASSIGN_SID_VOCE_1 + (int)(key - KEYCODE_1);
    } else if (key == KEYCODE_v) {
      b = BTN_ASSIGN_SID_VU;
    } else if (key == KEYCODE_t) {
      b = BTN_ASSIGN_SID_TEMPO;
    } else if (key == KEYCODE_n) {
      b = BTN_ASSIGN_SID_INFO;
    } else if (key == KEYCODE_Comma) {
      b = BTN_ASSIGN_SID_PREV;
    } else if (key == KEYCODE_Period) {
      b = BTN_ASSIGN_SID_NEXT;
    } else if (key == KEYCODE_p) {
      b = BTN_ASSIGN_SID_PAUSA;
    } else if (key == KEYCODE_m) {
      b = BTN_ASSIGN_MUTO;
    } else if (key == KEYCODE_0) {






      b = BTN_ASSIGN_SID_VOCE_10;
    } else if (key == KEYCODE_Dash || key == KEYCODE_Slash ||
               key == KEYCODE_KP_Subtract) {
      b = BTN_ASSIGN_SID_VOCE_11;
    } else if (key == KEYCODE_Equals || key == KEYCODE_RightBracket ||
               key == KEYCODE_KP_Add) {
      b = BTN_ASSIGN_SID_VOCE_12;
    } else if (key == KEYCODE_BackSlash || key == KEYCODE_KP_BackSlash ||
               key == KEYCODE_BackQuote) {
      b = BTN_ASSIGN_ASPETTO_2;
    } else if (key == KEYCODE_F1 || key == KEYCODE_F2 || key == KEYCODE_F3 ||
               key == KEYCODE_F4) {





      b = BTN_ASSIGN_SID_LATO_1 + (key == KEYCODE_F1 ? 0 :
                                   key == KEYCODE_F2 ? 1 :
                                   key == KEYCODE_F3 ? 2 : 3);
    } else if (key == KEYCODE_Left || key == KEYCODE_Right) {



      b = key == KEYCODE_Left ? BTN_ASSIGN_SID_INDIETRO : BTN_ASSIGN_SID_AVANTI;
    } else if (key == KEYCODE_Up || key == KEYCODE_Down) {



      b = key == KEYCODE_Up ? BTN_ASSIGN_VOLUME_SU : BTN_ASSIGN_VOLUME_GIU;
    }
    if (b != 0) {
      emu_quick_func_interrupt(b);
      segna_mangiata(key, 1);
      return;
    }
  }

  if (key == commodore_key_sym) {
    commodore_down = 1;
  } else if (key == ctrl_key_sym) {
    control_down = 1;
  } else if (key == KEYCODE_F7) {
    f7_down = 1;
    if (commodore_down) {
       video_reset_time_down = circle_get_ticks();
    }
  }

  // Intercept keys meant to become joystick values
  {





    int porta;
    for (porta = 0; porta < MAX_JOY_PORTS; porta++) {
      if (!porta_di_tasti(joydevs[porta].device)) {
        continue;
      }
      if (joy_key_down(porta, key)) {
        return;
      }
    }
  }

  if (handle_key_combo_press(key)) {
    return;
  }

  if (ui_enabled) {
    emu_ui_key_interrupt(key, 1 /* down */);
  } else if (solo_gs()) {



  } else {
    emux_key_interrupt(key, 1 /* down */);
  }
}

void emu_key_released(long key) {
  if (raw_keycode_func) {
    raw_keycode_func(key);
    return;
  }




  if (key == KEYCODE_LeftShift || key == KEYCODE_RightShift) {
    shift_down = 0;
    shift_lasciato = circle_get_ticks();
  }




  if (key >= 0 && key < TASTI_QUANTI && c128_finto_dato[key] != 0) {
    emux_key_interrupt(c128_finto_dato[key], 0);
    c128_finto_dato[key] = 0;
    return;
  }

  if (key == KEYCODE_LeftAlt || key == KEYCODE_RightAlt) {
    alt_down = 0;
    alt_lasciato = circle_get_ticks();
  } else if (era_mangiata(key)) {



    segna_mangiata(key, 0);
    return;
  }

  if (key == KEYCODE_Power) {








    if (modificatore_appena()) {
      menu_reboot_key();
    } else {





      char traccia[24];
      snprintf(traccia, sizeof(traccia), "PWR NO MOD %02x/%02x",
               mod_ora, mod_prima);
      overlay_avviso(traccia);
      menu_poweroff_key();
    }
    return;
  }

  if (key == commodore_key_sym) {
    commodore_down = 0;
  } else if (key == ctrl_key_sym) {
    control_down = 0;
  } else if (key == KEYCODE_F7) {
    f7_down = 0;
    if (commodore_down &&
       (circle_get_ticks() - video_reset_time_down >= video_reset_time_delay)) {
       // Reset to 'safe' video mode.
       switch_safe();
       reboot();
    }
  }

  if (key == KEYCODE_F12) {
    if (ui_enabled) {
      // Let the ui handle the menu action as it sees fit.
      emu_ui_key_interrupt(key, 0 /* up */);
    } else {
      // When transitioning to the menu, make sure to give emulator
      // at least one pass through main loop after sending the up event.
      emux_key_interrupt(key, 0 /* up */);
      circle_lock_acquire();
      ui_toggle_pending = 2;
      circle_lock_release();
    }
    return;
  }

  // Intercept keys meant to become joystick values
  {





    int porta;
    for (porta = 0; porta < MAX_JOY_PORTS; porta++) {
      if (!porta_di_tasti(joydevs[porta].device)) {
        continue;
      }
      if (joy_key_up(porta, key)) {
        return;
      }
    }
  }

  if (handle_key_combo_release(key)) {
    return;
  }

  if (ui_enabled) {
    emu_ui_key_interrupt(key, 0 /* up */);
  } else if (!solo_gs()) {
    emux_key_interrupt(key, 0 /* up */);
  }

  // Check hotkey combo here
  if (key == commodore_key_sym || key == ctrl_key_sym) {
    // We invoke the hot key func here after the modifier key is released
    // so emulator is not left in a weird state.
    handle_key_combo_function();
  }
}
