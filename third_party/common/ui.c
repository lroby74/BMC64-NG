/*
 * ui.c
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

#include "ui.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

// RASPI includes
#include "emux_api.h"
#include "circle.h"
#include "joy.h"
#include "kbd.h"
#include "menu.h"
#include "menu_text_layout.h"
#include "font.h"
#include "menu_timing.h"
#include "ui_geometry.h"
#include "overlay.h"
#include "menu_usb.h"



































































































extern int circle_file_aperti(int stampa);

#define COLOR16(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)

#ifdef RASPI_LITE

#define BG_COLOR 6
#define FG_COLOR 3
#define DISABLED_COLOR 11
#define READ_ONLY_DESCRIPTION_COLOR 15
#define HILITE_COLOR 2
#define BORDER_COLOR 14
#define TRANSPARENT_COLOR 16

#else

#define BG_COLOR 0
#define FG_COLOR 1
#define DISABLED_COLOR 11
#define READ_ONLY_DESCRIPTION_COLOR 15
#define HILITE_COLOR 2
#define BORDER_COLOR 3
#define TRANSPARENT_COLOR 16

#endif

uint8_t *video_font;
uint16_t video_font_translate[256];
uint8_t *raw_video_font;










#define UI_USE_COMMON_MENU_FONT 1






#define FONT_NOTDEF_INDEX 0x7F





static const char *ui_utf8_next(const char *s, uint32_t *cp) {
  const uint8_t *p = (const uint8_t *)s;
  uint8_t c = p[0];
  int n;
  int i;
  uint32_t v;
  if (c < 0x80) { *cp = c; return s + 1; }
  else if ((c & 0xE0) == 0xC0) { n = 1; v = c & 0x1F; }
  else if ((c & 0xF0) == 0xE0) { n = 2; v = c & 0x0F; }
  else if ((c & 0xF8) == 0xF0) { n = 3; v = c & 0x07; }
  else { *cp = 0xFFFD; return s + 1; }
  for (i = 1; i <= n; i++) {
    if ((p[i] & 0xC0) != 0x80) { *cp = 0xFFFD; return s + 1; }
    v = (v << 6) | (p[i] & 0x3F);
  }
  *cp = v;
  return s + n + 1;
}

// Is the UI layer enabled? (either OSD or MENU)
volatile int ui_enabled;
int ui_showing;
// Countdown to toggle menu on/off
int ui_toggle_pending;
// One of the quick functions that can be invoked by button assignments
int pending_emu_quick_func;

static int osd_active;
static int ui_commodore_down;
static int ui_transparent;
static int ui_transparent_layer; // which layer we are revealing for adjustment
static int ui_render_current_item_only;

// Stubs for vice callbacks. Unimplemented for now.
void ui_pause_emulation(int flag) {}
int ui_emulation_is_paused(void) { return 0; }

// Width and height of our text menu in characters
const int menu_width_chars = 40;
const int menu_height_chars = 25;











static int ui_menu_scale = UI_MENU_SCALE_DEFAULT;
static int ui_menu_row_gap = UI_MENU_ROW_GAP_DEFAULT;
static void ui_apply_menu_row_gap_layout(void);

int ui_get_menu_scale_percent(void) {
  return ui_menu_scale;
}

int ui_set_menu_scale_percent(int percent) {
  if (!ui_menu_scale_is_valid(percent)) {
    return 0;
  }
  ui_menu_scale = percent;
  circle_set_scale_fbl(FB_LAYER_UI, percent);
  if (ui_showing) {
    circle_hide_fbl(FB_LAYER_UI);
    ui_showing = 0;
  }
  return 1;
}

int ui_get_menu_row_gap(void) {
  return ui_menu_row_gap;
}

int ui_set_menu_row_gap(int gap) {
  if (!ui_menu_row_gap_is_valid(gap)) {
    return 0;
  }
  if (gap != ui_menu_row_gap) {
    ui_menu_row_gap = gap;
    ui_apply_menu_row_gap_layout();
  }
  return 1;
}


static int ui_menu_altezza(int righe) {
  int h = ui_menu_height_for_rows(righe, ui_menu_row_gap);
  if (h <= 0 || h > menu_height_chars * UI_MENU_GLYPH_HEIGHT) {
    h = menu_height_chars * UI_MENU_GLYPH_HEIGHT;
  }
  return h;
}

// Stack of menu screens
static int current_menu = -1;
struct menu_item menu_roots[NUM_MENU_ROOTS];

// Where is our cursor in the menu?
static int menu_cursor[NUM_MENU_ROOTS];
struct menu_item *menu_cursor_item[NUM_MENU_ROOTS];

// Sliding window marking start and stop of what we're showing.
static int menu_window_top[NUM_MENU_ROOTS];
static int menu_window_bottom[NUM_MENU_ROOTS];

// The index of the last item + 1. Can't set cursor to this or higher.
static int max_index[NUM_MENU_ROOTS];

#define PENDING_UI_KEY_SIZE 64
#define PENDING_UI_KEY_MASK (PENDING_UI_KEY_SIZE - 1)
static int pending_ui_key_head = 0;
static int pending_ui_key_tail = 0;
static long pending_ui_key[PENDING_UI_KEY_SIZE];
static int pending_ui_key_pressed[PENDING_UI_KEY_SIZE];

// Global callback for events that happen on menu items
void (*on_value_changed)(struct menu_item *) = NULL;




int (*ui_tasto_nella_lista)(struct menu_item *cur, char ch) = NULL;







void (*ui_campo_scritto)(struct menu_item *cur) = NULL;
void (*ui_dopo_i_tasti)(void) = NULL;











#define UI_MOUSE_CODA 32U
#define UI_MOUSE_SINISTRO 1U
#define UI_MOUSE_DESTRO 2U
#define UI_MOUSE_CENTRALE 4U
#define UI_MOUSE_ZONA_MORTA 12
#define UI_MOUSE_PASSO_TRASCINA 16
#define UI_MOUSE_MAX_PASSI 4
#define UI_MOUSE_SOMMA_MAX 10000
struct ui_mouse_evento {
  unsigned tasti;
  int dx;
  int dy;
  int rotella;
};
static unsigned ui_mouse_testa;
static unsigned ui_mouse_coda_fine;
static struct ui_mouse_evento ui_mouse_eventi[UI_MOUSE_CODA];
static volatile int ui_mouse_acceso = 1;
static int ui_mouse_sessione;
static int ui_mouse_visto;
static int ui_mouse_x;
static int ui_mouse_y;
static unsigned ui_mouse_tasti;
static struct menu_item *ui_mouse_trascina;
static int ui_mouse_trascina_asse;
static int ui_mouse_trascina_dx;
static int ui_mouse_trascina_dy;
static int ui_mouse_trascina_resto;

static int ui_mouse_al_menu;
static unsigned ui_mouse_ignorati;
static void ui_mouse_sessione_apri(void);
static void ui_mouse_sessione_chiudi(void);
static void ui_mouse_freccia(void);





// Key presses turn into these. Some actions are repeatable and
// the frequency at which they are executed can accelerate the
// longer they are enabled. Key releases will cancel the repeat.
#define ACTION_None 0
#define ACTION_Up 1
#define ACTION_Down 2
#define ACTION_Left 3
#define ACTION_Right 4
#define ACTION_Return 5
#define ACTION_Escape 6
#define ACTION_Exit 7
#define ACTION_MiniLeft 8
#define ACTION_MiniRight 9

#define ACTION_BloccoSu 10
#define ACTION_BloccoGiu 11

#define INITIAL_ACTION_DELAY 24
#define INITIAL_ACTION_REPEAT_DELAY 8

// State variables managing hold and repeat behavior of menu
// actions.  Frequency of repeat will increase as time goes
// on.
static int ui_key_action;
static long ui_key_ticks;
static long ui_key_ticks_next;
static int ui_key_ticks_repeats;
static int ui_key_ticks_repeats_next;

static void ui_action(long action);





static int ui_keyboard_mapping = KEYBOARD_MAPPING_SYM;
static int ui_keyboard_layout = MENU_TEXT_LAYOUT_US;

void ui_set_keyboard_mapping(int mapping) {
  ui_keyboard_mapping = mapping;
}



int ui_get_keyboard_layout(void) {
  return ui_keyboard_layout;
}

void ui_set_keyboard_layout(int layout) {
  ui_keyboard_layout = layout;
}
static void ui_action_corpo(long action);




static long ui_azione_in_corso = ACTION_None;

int ui_value_changed_by_return(void) {
  return ui_azione_in_corso == ACTION_Return;
}





static int ui_valore_prima_invio = 0;

int ui_value_before_return(void) {
  return ui_valore_prima_invio;
}

static int keyboard_shift = 0;















static int ui_alt = 0;

static uint8_t* ui_fb;
static int ui_fb_pitch;
static int ui_fb_w;
static int ui_fb_h;

void ui_init_menu(void) {
  int i;

  assert(emux_machine_class != BMC64_MACHINE_CLASS_UNKNOWN);

  ui_enabled = 0;
  ui_showing = 0;
  current_menu = -1;

  // Init menu roots
  for (i = 0; i < NUM_MENU_ROOTS; i++) {
    memset(&menu_roots[i], 0, sizeof(struct menu_item));
    menu_roots[i].type = FOLDER;
    menu_roots[i].is_expanded = 1;
    menu_roots[i].name[0] = '\0';
  }

  // Root menu is never popped
  struct menu_item *root = ui_push_menu(-2, -2);

  // This also loads our custom settings file. It's safe to have settings
  // here that our videoarch code needs since this is called before any
  // canvases are created.
  build_menu(root);
  passo_nota("menu costruito");

  ui_key_action = ACTION_None;
  ui_key_ticks = 0;
  ui_key_ticks_next = 0;
  ui_key_ticks_repeats = 0;
  ui_key_ticks_repeats_next = 0;
}




static void ui_draw_char(uint32_t c, int pos_x, int pos_y, int color,
                         uint8_t *dst, int dst_pitch, int stretch,
                         int translate) {
  int x, y, s;
  uint8_t fontchar;
  uint8_t *font_pos;
  uint8_t *draw_pos;

  // Destination is our ui frame buffer if not specified.
  if (dst == NULL) {
    dst_pitch = ui_fb_pitch;
    dst = ui_fb;

    // Don't draw out of bounds
    if (pos_y < 0 || pos_y > ui_fb_h - 8*stretch) {
      return;
    }
    if (pos_x < 0 || pos_x > ui_fb_w - 8*stretch) {
      return;
    }
  }

  if (translate) {
#if UI_USE_COMMON_MENU_FONT


     uint32_t gi = (c <= 0xFF) ? c : (uint32_t)FONT_NOTDEF_INDEX;
     font_pos = &(((uint8_t *)font8x8_basic)[8 * gi]);
#else


     font_pos = &(video_font[video_font_translate[c & 0xff]]);
#endif
  } else {
     font_pos = &(raw_video_font[(c & 0xff) * 8]);
  }
  draw_pos = &(dst[pos_x + pos_y * dst_pitch]);

  for (y = 0; y < 8*stretch; ++y) {
    fontchar = *font_pos;
    for (x = 0; x < 8; ++x) {
      if (fontchar & (0x80 >> x)) {
        for (s = 0; s < stretch; s++) {
           draw_pos[x*stretch+s] = color;
        }
      }
    }
    if (y % stretch == stretch-1) ++font_pos;
    draw_pos += dst_pitch;
  }
}

// Draw a string of text at location x,y. Does not word wrap.

void ui_draw_text_buf(const char *text, int x, int y, int color, uint8_t *dst,
                      int dst_pitch, int stretch) {
  int x2 = x;
  const char *p = text;
  uint32_t cp;
  while (*p) {
    if (*p == '\n') {
      y = y + 8*stretch;
      x2 = x;
      p++;
      continue;
    }
    p = ui_utf8_next(p, &cp);
    ui_draw_char(cp, x2, y, color, dst, dst_pitch, stretch, 1);
    x2 = x2 + 8*stretch;
  }
}

// No font translation from ascii to petscii
void ui_draw_char_raw(const char singlechar, int x, int y, int color,
                      uint8_t *dst, int dst_pitch, int stretch) {
   ui_draw_char((uint8_t)singlechar, x, y, color, dst, dst_pitch, stretch, 0);
}








static uint8_t ui_petscii_codice_schermo(uint8_t code, int reverse_mode) {
  uint8_t rev = (uint8_t)(reverse_mode ? 0x80 : 0x00);

  if (code >= 0x40 && code <= 0x5f) {
    return (uint8_t)((code - 0x40) | rev);
  } else if (code >= 0x60 && code <= 0x7f) {
    return (uint8_t)((code - 0x20) | rev);
  } else if (code >= 0xa0 && code <= 0xbf) {
    return (uint8_t)((code - 0x40) | rev);
  } else if (code >= 0xc0 && code <= 0xfe) {
    return (uint8_t)((code - 0x80) | rev);
  } else if (code == 0xff) {
    return (uint8_t)(0x5e | rev);
  }
  return (uint8_t)(code | rev);
}

static uint8_t ui_petscii_a_schermo(uint8_t c) {
  if (c < 0x20U) {
    return (uint8_t)(c | 0x80U);
  }
  if (c >= 0x80U && c < 0xa0U) {
    return ui_petscii_codice_schermo((uint8_t)(c - 0x20U), 1);
  }
  return ui_petscii_codice_schermo(c, 0);
}

static void ui_draw_text_petscii(const char *text, int x, int y, int color) {
  const unsigned char *p = (const unsigned char *)text;
  int x2 = x;
  while (*p) {
    ui_draw_char(ui_petscii_a_schermo(*p), x2, y, color, NULL, 0, 1, 0);
    x2 += 8;
    p++;
  }
}

void ui_draw_text(const char *text, int x, int y, int color) {
  ui_draw_text_buf(text, x, y, color, NULL, 0, 1);
}

// Draw a rectangle at x/y of given w/h into the offscreen area
void ui_draw_rect_buf(int x, int y, int w, int h, int color, int fill,
                      uint8_t *dst, int dst_pitch) {
  int xx, yy, x2, y2;

  // Destination is ui frame buffer if not specified.
  if (dst == NULL) {
    dst_pitch = ui_fb_pitch;
    dst = ui_fb;
  }

  if (dst == NULL) {
    return;
  }
  x2 = x + w;
  y2 = y + h;
  for (xx = x, yy = y; yy < y2; xx++) {
    if (xx >= x2) {
      xx = x - 1;
      yy++;
    } else {
      int p1 = xx + yy * dst_pitch;
      if (fill | (yy == y || yy == (y2 - 1) || (xx == x) || xx == (x2 - 1))) {
        dst[p1] = color;
      }
    }
  }
}

void ui_draw_rect(int x, int y, int w, int h, int color, int fill) {
  ui_draw_rect_buf(x, y, w, h, color, fill, NULL, 0);
}






int ui_text_width(const char *text) {
  int n = 0;
  const char *p = text;
  uint32_t cp;
  while (*p) { p = ui_utf8_next(p, &cp); n++; }
  return 8 * n;
}

static void do_on_value_changed(struct menu_item *item) {








  int lascia_traccia = (item != NULL) && (item->type != RANGE)
                       && (item->type != TEXTFIELD);
  char nome[48];

  if (lascia_traccia) {
    snprintf(nome, sizeof(nome), "la voce di menu '%.30s'",
             item->name ? item->name : "?");
    passo_prima(nome);
  }
  if (item->on_value_changed) {
    item->on_value_changed(menu_cursor_item[current_menu]);
  } else if (on_value_changed) {
    on_value_changed(menu_cursor_item[current_menu]);
  }
  if (lascia_traccia) {
    passo_fatto(nome);
  }
}



static struct menu_item *ui_campo_toccato;

static void ui_type_char(char ch) {
  struct menu_item *cur = menu_cursor_item[current_menu];
  if (cur->type == TEXTFIELD) {
    ui_campo_toccato = cur;







    if (cur->textfield_masked) {
      cur->textfield_masked = 0;
      cur->str_value[0] = '\0';
      cur->value = 0;
      if (ch == '\b') {
        return;
      }
    }









    if (cur->textfield_code) {
      if (ch == '\b') {
        cur->str_value[0] = '\0';
        cur->value = 0;
        return;
      }
      if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
      }
      if (strlen(cur->str_value) >= cur->max_length) {
        cur->str_value[0] = '\0';
        cur->value = 0;
      }
    }
    if (ch == '\b') {
      if (cur->value <= 0) {

        if (ui_tasto_nella_lista != NULL) {
          (void)ui_tasto_nella_lista(cur, ch);
        }
        return;
      }
      char *str = cur->str_value;
      memmove(str + cur->value - 1, str + cur->value,
              (strlen(str) - cur->value + 1) * sizeof(char));
      cur->value--;
      if (ui_campo_scritto != NULL) {
        ui_campo_scritto(cur);
      }
    } else {
      if (strlen(cur->str_value) >= cur->max_length)
        return;

      char *str = cur->str_value;
      memmove(str + cur->value + 1, str + cur->value,
              (strlen(str) - cur->value + 1) * sizeof(char));
      str[cur->value] = ch;
      cur->value++;
      if (cur->textfield_code && strlen(str) == cur->max_length) {
        do_on_value_changed(cur);
      }
      if (ui_campo_scritto != NULL) {
        ui_campo_scritto(cur);
      }
    }
  } else {




    if (ui_tasto_nella_lista != NULL && ui_tasto_nella_lista(cur, ch)) {
      return;
    }
    ui_find_first(ch);
  }
}









static void ui_stato_normale(void) {
  ui_transparent = 0;
  ui_transparent_layer = -1;
  ui_render_current_item_only = 0;
}




static void ui_action_tasto(long action) {
  if (action == ACTION_BloccoSu) {
    ui_page_up();
  } else if (action == ACTION_BloccoGiu) {
    ui_page_down();
  } else {
    ui_action(action);
  }
}







static int ui_browser_tail_delay = 250;
static int invio_giu;
static int invio_consumato;
static int invio_riparte;
static struct menu_item *invio_voce;
static unsigned long invio_da;
static int lettere_utf8(const char *s);

static int invio_voce_lunga(struct menu_item *v) {
  return v != NULL && !v->disabled && v->nome_lungo != NULL &&
         lettere_utf8(v->nome_lungo) > MAX_MENU_STR - 1;
}

static void ui_invio_giu(long key) {
  if (key != KEYCODE_Return || ui_browser_tail_delay <= 0 || invio_giu) {
    return;
  }
  if (!invio_voce_lunga(menu_cursor_item[current_menu])) {
    return;
  }
  invio_giu = 1;
  invio_consumato = 0;
  invio_voce = menu_cursor_item[current_menu];
  invio_da = circle_get_ticks();
}



static int ui_invio_su(long key) {
  int mangiato;
  if (key != KEYCODE_Return || !invio_giu) {
    return 0;
  }
  mangiato = invio_consumato || invio_voce != menu_cursor_item[current_menu] ||
             (circle_get_ticks() - invio_da) >=
                 (unsigned long)ui_browser_tail_delay * 1000UL;
  invio_giu = 0;
  invio_voce = NULL;
  if (mangiato) {
    invio_riparte = 1;
  }
  return mangiato;
}


static void ui_invio_reset(void) {
  invio_voce = NULL;
  if (invio_giu) {
    invio_consumato = 1;
  }
}

int ui_get_browser_tail_delay(void) {
  return ui_browser_tail_delay;
}

int ui_set_browser_tail_delay(int ms) {
  if (ms != 0 && ms != 100 && ms != 250 && ms != 500 && ms != 750 &&
      ms != 1000) {
    return 0;
  }
  ui_browser_tail_delay = ms;
  ui_invio_reset();
  return 1;
}

// Happens on main loop.
static void ui_key_pressed(long key) {
  ui_invio_giu(key);
  // Anything other than left/right will reset transparency
  // and render current item only flags. They are applicable
  // only while the user is on the item they were triggered
  // for.
  if (key != KEYCODE_Left && key != KEYCODE_Right) {
    ui_stato_normale();
  }

  if (key == commodore_key_sym) {
     ui_commodore_down = 1;
     return;
  }



  if (menu_roots[current_menu].key_listener_func != NULL &&
      menu_roots[current_menu].key_listener_func(
          &menu_roots[current_menu], menu_cursor_item[current_menu], key)) {
    return;
  }









  int shift_eff = keyboard_shift || emu_get_keyboard_shiftlock();

  if (menu_cursor_item[current_menu] != NULL &&
      menu_cursor_item[current_menu]->type == TEXTFIELD) {



    if (ui_alt & 2) {
      char gr = menu_text_layout_altgr_to_char(key, shift_eff,
                                               ui_keyboard_mapping,
                                               ui_keyboard_layout);
      if (gr != '\0') {
        ui_type_char(gr);
        return;
      }
      if (key >= KEYCODE_a && key <= KEYCODE_0 &&
          (ui_keyboard_layout == MENU_TEXT_LAYOUT_IT ||
           ui_keyboard_layout == MENU_TEXT_LAYOUT_DE)) {
        return;
      }
    }
    char ch = menu_text_layout_key_to_char(key, shift_eff,
                                           ui_keyboard_mapping,
                                           ui_keyboard_layout);
    if (ch != '\0') {
      ui_type_char(ch);
      return;
    }
  }

  switch (key) {
  case KEYCODE_Up:
  case KEYCODE_Down:
  case KEYCODE_Left:
  case KEYCODE_Right:
  case KEYCODE_Comma:
  case KEYCODE_Period:
    switch (key) {
      case KEYCODE_Up:
        ui_key_action = ui_alt ? ACTION_BloccoSu : ACTION_Up; break;
      case KEYCODE_Down:
        ui_key_action = ui_alt ? ACTION_BloccoGiu : ACTION_Down; break;
      case KEYCODE_Left:
        ui_key_action = ACTION_Left; break;
      case KEYCODE_Right:
        ui_key_action = ACTION_Right; break;
      case KEYCODE_Comma:
        ui_key_action = ACTION_MiniLeft; break;
      case KEYCODE_Period:
        ui_key_action = ACTION_MiniRight; break;
      default:
        return;
    }
    ui_key_ticks = INITIAL_ACTION_DELAY;
    ui_key_ticks_next = INITIAL_ACTION_REPEAT_DELAY;
    ui_key_ticks_repeats = 0;
    ui_key_ticks_repeats_next = 8;
    ui_action_tasto(ui_key_action);
    return;
  case KEYCODE_Escape:
    return;
  case KEYCODE_LeftShift:
    keyboard_shift |= 1;
    return;
  case KEYCODE_RightShift:
    keyboard_shift |= 2;
    return;
  case KEYCODE_LeftAlt:
    ui_alt |= 1;
    return;
  case KEYCODE_RightAlt:
    ui_alt |= 2;
    return;
  }

  if (key >= KEYCODE_a && key <= KEYCODE_z) {
    char ch;
    if (shift_eff)
      ch = 'A' + key - KEYCODE_a;
    else
      ch = 'a' + key - KEYCODE_a;
    ui_type_char(ch);
  } else if (key == KEYCODE_Backspace) {
    ui_type_char('\b');
  } else {



    char ch = menu_text_layout_key_to_char(key, shift_eff,
                                           ui_keyboard_mapping,
                                           ui_keyboard_layout);
    if (ch != '\0') {
      ui_type_char(ch);
    }
  }
}

// Happens on main loop. Process a key release for the ui.
static void ui_key_released(long key) {
  if (ui_invio_su(key)) {
    return;
  }
  if (key == commodore_key_sym) {
    ui_commodore_down = 0;
    return;
  }

  switch (key) {
  case KEYCODE_Up:
  case KEYCODE_Down:
  case KEYCODE_Left:
  case KEYCODE_Right:
  case KEYCODE_Comma:
  case KEYCODE_Period:
    ui_key_action = ACTION_None;
    return;
  case KEYCODE_Return:
    ui_action(ACTION_Return);
    return;
  case KEYCODE_Space:


    menu_quick_access_try_assign(menu_cursor_item[current_menu]);
    return;
  case KEYCODE_Escape:
  case KEYCODE_BackQuote:
    ui_action(ACTION_Escape);
    return;
  case KEYCODE_F12:
    ui_action(ACTION_Exit);
    return;
  // Since FX keys are also used for hotkeys,
  // best not to perform these ui functions if
  // the cntrl key is down. It may trigger a
  // hotkey function and we don't want these
  // to happen as well.
  case KEYCODE_Home:
  case KEYCODE_F1:
    if (!ui_commodore_down) ui_to_top();
    return;
  case KEYCODE_End:
  case KEYCODE_F7:
    if (!ui_commodore_down) ui_to_bottom();
    return;
  case KEYCODE_PageUp:
  case KEYCODE_F3:
    if (!ui_commodore_down) ui_page_up();
    return;
  case KEYCODE_PageDown:
  case KEYCODE_F5:
    if (!ui_commodore_down) ui_page_down();
    return;
  case KEYCODE_LeftShift:
    keyboard_shift &= ~1;
    return;
  case KEYCODE_RightShift:
    keyboard_shift &= ~2;
    return;
  case KEYCODE_LeftAlt:
    ui_alt &= ~1;
    return;
  case KEYCODE_RightAlt:
    ui_alt &= ~2;
    return;
  }
}

// Do the next ui action based on key pressed and timeout
static void ui_action_frame() {
  if (ui_key_action != ACTION_None) {
    ui_key_ticks--;
    // When key ticks hits zero, repeat the action.
    if (ui_key_ticks == 0) {
      ui_action_tasto(ui_key_action);
      // Set new ticks
      ui_key_ticks = ui_key_ticks_next;
      // Keep track of how many repeats
      ui_key_ticks_repeats++;
      if (ui_key_ticks_repeats >= ui_key_ticks_repeats_next) {
        ui_key_ticks_repeats_next *= 4;
        ui_key_ticks_next /= 2;
        if (ui_key_ticks_next < 2)
          ui_key_ticks_next = 2;
      }
    }
  }
}

void ui_render_single_frame() {
  menu_update_network_status();
  menu_update_temperatura();
  menu_usb_capture_refresh();




  if (ui_fb == NULL) {
    return;
  }

  // Start with transparent
  memset(ui_fb, TRANSPARENT_COLOR, ui_fb_h * ui_fb_pitch);

  for (int msi=0;msi<=current_menu;msi++) {
     ui_render_now(msi);
  }
  ui_mouse_freccia();



  {
    uint32_t ready_mask = FB_LAYER_MASK(FB_LAYER_UI);
    if (overlay_dirty) {
      ready_mask |= FB_LAYER_MASK(FB_LAYER_STATUS);
      overlay_dirty = 0;
    }
    circle_present_fbl(ready_mask, 1 /* sync */);
  }
  circle_yield();
}










#define LET_SEG_MAX 20
#define LET_NERO 0
#define LET_BIANCO 1
#define LET_GRIGIO 15
#define LET_GRIGIO_SCURO 11
#define LET_VERDE 40
#define LET_VERDE_SPENTO 41
#define LET_GIALLO 42
#define LET_GIALLO_SPENTO 43
#define LET_ROSSO 44
#define LET_ROSSO_SPENTO 45
#define LET_PICCO 46

static int lettore_mostrato = 0;


extern struct menu_item *volume_item;
static int let_volume_scritto = -1;
#define LET_VOL_LARGO 6






extern volatile unsigned menu_tasti_volume;
static unsigned let_volume_tasti = 0;
static int let_volume_visibile = 0;
static unsigned long let_volume_da = 0;
#define LET_VOL_US 1000000UL





static char let_motore[24];
static int let_motore_visibile = 0;




#define LET_MOTORE_QUADRI 50
static int let_motore_quadri = 0;




static int lettore_vic_spento = 0;

static void lettore_spegne_vic(void) {
  if (!lettore_vic_spento) {
    lettore_vic_spento = 1;
    vic_enabled = 0;
  }
}

static void lettore_rimette_vic(void) {
  if (lettore_vic_spento) {
    lettore_vic_spento = 0;
    vic_enabled = 1;
  }
}
static int lettore_sid = 0;
static int lettore_brano = -1;



static int lettore_acceso[24];
static int lettore_picco[24];
static int lettore_picco_tempo[24];


static int lettore_livelli[24];
static int lettore_livelli_c = 0;



static char lettore_etichetta[8][16];
static const char *lettore_modelli[8];






static unsigned long let_clk0;
static int let_secondi_scritti = -1;
static int let_seg, let_seg_passo, let_seg_h, let_y_basso;
static int let_x0, let_col, let_gap, let_gap_sid;


static int let_due_righe;

static int let_vol_y;


static int let_ax, let_ay, let_aw, let_ah;





static int let_mostra_vu = 1;
static int let_mostra_tempo = 1;
static int let_mostra_info = 1;
static int let_ridisegna = 0;


static char let_avviso[32];
static int let_avviso_visibile = 0;
static unsigned long let_avviso_da = 0;
#define LET_AVVISO_US 1000000UL



static int let_salto_fino = -1;
static int let_salto_brano = 0;


static int let_foto_ultima = -1;

static void lettore_area(void) {
  int rs = canvas_state[VIC_INDEX].raster_skip;

  if (rs < 1) {
    rs = 1;
  }
  let_ax = canvas_state[VIC_INDEX].left;
  let_aw = canvas_state[VIC_INDEX].vis_w;
  let_ay = canvas_state[VIC_INDEX].first_displayed_line +
           (canvas_state[VIC_INDEX].max_border_h -
            canvas_state[VIC_INDEX].border_h) / rs;
  let_ah = canvas_state[VIC_INDEX].vis_h / rs;
  if (let_ax < 0) {
    let_ax = 0;
  }
  if (let_ay < 0) {
    let_ay = 0;
  }
  if (let_ax + let_aw > ui_fb_w) {
    let_aw = ui_fb_w - let_ax;
  }
  if (let_ay + let_ah > ui_fb_h) {
    let_ah = ui_fb_h - let_ay;
  }
  if (let_aw < 64 || let_ah < 64) {
    let_ax = 0;
    let_ay = 0;
    let_aw = ui_fb_w;
    let_ah = ui_fb_h;
  }
}

static void lettore_tavolozza(void) {
  circle_set_palette32_fbl(FB_LAYER_UI, LET_VERDE, 0xFF30E838);
  circle_set_palette32_fbl(FB_LAYER_UI, LET_VERDE_SPENTO, 0xFF0C2E10);
  circle_set_palette32_fbl(FB_LAYER_UI, LET_GIALLO, 0xFFFFD820);
  circle_set_palette32_fbl(FB_LAYER_UI, LET_GIALLO_SPENTO, 0xFF3C3208);
  circle_set_palette32_fbl(FB_LAYER_UI, LET_ROSSO, 0xFFFF3020);
  circle_set_palette32_fbl(FB_LAYER_UI, LET_ROSSO_SPENTO, 0xFF3C0C08);
  circle_set_palette32_fbl(FB_LAYER_UI, LET_PICCO, 0xFFFFFFFF);
  circle_update_palette_fbl(FB_LAYER_UI);
}

static void lettore_geometria(void) {
  int alto;
  int largo;
  int unita = 16 * lettore_sid + 6 * (lettore_sid - 1);
  int u;
  int righe;
  const int fondo = let_ay + let_ah - 12;

  lettore_area();
  alto = let_ah - 40 - 36;
  largo = let_aw * 4 / 5;
  u = largo / unita;
  let_seg = LET_SEG_MAX;
  if (alto / let_seg < 3) {
    let_seg = alto / 3;
  }
  if (let_seg < 4) {
    let_seg = 4;
  }
  let_seg_passo = alto / let_seg;
  if (let_seg_passo < 2) {
    let_seg_passo = 2;
  }
  if (u > 15) {
    u = 15;
  }
  if (u < 1) {
    u = 1;
  }
  let_col = 4 * u;
  let_gap = 2 * u;
  let_gap_sid = 6 * u;
  let_x0 = let_ax + (let_aw - (unita * u)) / 2;













  if (lettore_sid >= 4) {
    const int n = lettore_sid;
    const int spazio = let_aw - 8;
    int c = 60;

    while (c > 4 && 3 * n * c + 2 * n * (c / 4) + (n - 1) * c > spazio) {
      c--;
    }
    if (c > let_col) {
      const int resto = spazio - (3 * n * c + 2 * n * (c / 4) + (n - 1) * c);

      let_col = c;
      let_gap = c / 4;
      let_gap_sid = c + resto / (n - 1);
      let_x0 = let_ax + 4 + (resto % (n - 1)) / 2;
    }
  }






  righe = (9 * 8 > 3 * let_col + 2 * let_gap + let_gap_sid) ? 2 : 1;
  let_due_righe = (righe == 2);
  for (;;) {
    let_seg_h = let_seg_passo - (let_seg_passo >= 5 ? 2 : 1);
    let_y_basso = let_ay + 40 + let_seg * let_seg_passo;
    if (let_y_basso + 14 + 10 * righe + 8 <= fondo || let_seg_passo <= 2) {
      break;
    }
    let_seg_passo--;
  }
  let_vol_y = let_y_basso + 14 + 10 * righe;
}

static int lettore_x(int s, int v) {
  return let_x0 + s * (3 * let_col + 2 * let_gap + let_gap_sid)
         + v * (let_col + let_gap);
}

static int lettore_colore(int seg, int acceso, int picco) {
  if (picco) {
    return LET_PICCO;
  }
  if (seg >= let_seg - let_seg * 3 / 20) {
    return acceso ? LET_ROSSO : LET_ROSSO_SPENTO;
  }
  if (seg >= let_seg - let_seg * 8 / 20) {
    return acceso ? LET_GIALLO : LET_GIALLO_SPENTO;
  }
  return acceso ? LET_VERDE : LET_VERDE_SPENTO;
}

static void lettore_testo(const char *s, int x, int y, int colore) {
  char buf[64];
  int n = 0;
  int i;

  for (i = 0; s != NULL && s[i] && n < (int)sizeof(buf) - 1; i++) {
    unsigned char c = (unsigned char)s[i];
    buf[n++] = (c >= 32 && c < 127) ? (char)c : '?';
  }
  buf[n] = 0;
  if (n > let_aw / 8) {
    n = let_aw / 8;
    buf[n] = 0;
  }
  if (x < 0) {
    x = let_ax + (let_aw - n * 8) / 2;
  }
  ui_draw_text_buf(buf, x, y, colore, ui_fb, ui_fb_pitch, 1);
}


static void lettore_testo_in_mezzo(const char *s, int y, int colore, int margine) {
  char buf[48];
  int max = (let_aw - 2 * margine) / 8;
  int n = 0;
  int i;

  if (max > (int)sizeof(buf) - 1) {
    max = (int)sizeof(buf) - 1;
  }
  for (i = 0; s != NULL && s[i] && n < max; i++) {
    unsigned char c = (unsigned char)s[i];
    buf[n++] = (c >= 32 && c < 127) ? (char)c : '?';
  }
  buf[n] = 0;
  lettore_testo(buf, let_ax + margine + ((let_aw - 2 * margine) - n * 8) / 2,
                y, colore);
}

static void lettore_tempo_riparte(void) {
  let_clk0 = emux_orologio_cpu();
  let_secondi_scritti = -1;
  let_foto_ultima = -1;
}






static int lettore_secondi(void) {
  unsigned long hz = emux_cicli_al_secondo();
  unsigned long ora = emux_orologio_cpu();

  if (ora < let_clk0) {
    let_clk0 = ora;
  }
  if (hz == 0) {
    return 0;
  }
  return (int)((ora - let_clk0) / hz);
}



static void lettore_disegna_tempo(void) {
  char t[12];
  const char *v = emux_sid_velocita();
  int s = lettore_secondi();
  int n = (int)strlen(v);

  snprintf(t, sizeof(t), "%02d:%02d", (s / 60) % 100, s % 60);
  ui_draw_rect_buf(let_ax + 4, let_ay + 28, 6 * 8, 10, LET_NERO, 1,
                   ui_fb, ui_fb_pitch);
  if (let_mostra_tempo) {
    lettore_testo(t, let_ax + 4, let_ay + 28, LET_GRIGIO);
  }



  ui_draw_rect_buf(let_ax + let_aw - 4 - 10 * 8, let_ay + 28, 10 * 8, 10,
                   LET_NERO, 1, ui_fb, ui_fb_pitch);
  if (let_mostra_info && n > 0 && n <= 10) {
    lettore_testo(v, let_ax + let_aw - 4 - n * 8, let_ay + 28, LET_GRIGIO);
  }
  let_secondi_scritti = s;
}









static int lettore_volume(void) {
  return volume_item != NULL ? volume_item->value : 100;
}



static void lettore_volume_geometria(int *bx, int *sw, int *tx, int *x1) {
  const int fine = lettore_x(lettore_sid - 1, 2) + let_col;
  const int spazio = fine - let_x0 - 5 * 8;

  *bx = let_x0;
  *x1 = fine;
  *sw = spazio >= 3 * let_seg ? (spazio + 2) / let_seg - 2 : 0;
  *tx = let_x0 + let_seg * (*sw + 2) - 2 + 8;
}

static void lettore_disegna_volume(void) {
  const int v = lettore_volume();
  int bx, sw, tx, x1, accesi, k;
  char t[8];

  let_volume_scritto = v;
  lettore_volume_geometria(&bx, &sw, &tx, &x1);
  if (x1 <= bx) {
    return;
  }
  ui_draw_rect_buf(bx, let_vol_y, x1 - bx, 9, LET_NERO, 1, ui_fb, ui_fb_pitch);
  if (sw <= 0) {
    return;
  }
  accesi = (v * let_seg + 50) / 100;
  if (v > 0 && accesi == 0) {
    accesi = 1;
  }
  for (k = 0; k < let_seg; k++) {
    ui_draw_rect_buf(bx + k * (sw + 2), let_vol_y + 1, sw, 6,
                     k < accesi ? LET_GRIGIO : LET_GRIGIO_SCURO, 1,
                     ui_fb, ui_fb_pitch);
  }
  if (v == 0) {
    snprintf(t, sizeof(t), "MUTE");
  } else {
    snprintf(t, sizeof(t), "%d%%", v);
  }
  lettore_testo(t, tx, let_vol_y, v == 0 ? LET_ROSSO : LET_GRIGIO);
}



static void lettore_cancella_volume(void) {
  int bx, sw, tx, x1;

  let_volume_scritto = -1;
  lettore_volume_geometria(&bx, &sw, &tx, &x1);
  if (x1 <= bx) {
    return;
  }
  ui_draw_rect_buf(bx, let_vol_y, x1 - bx, 9, LET_NERO, 1, ui_fb, ui_fb_pitch);
}


static void lettore_disegna_motore(void) {
  int bx, sw, tx, x1, x;

  lettore_volume_geometria(&bx, &sw, &tx, &x1);
  if (x1 <= bx) {
    return;
  }
  ui_draw_rect_buf(bx, let_vol_y, x1 - bx, 9, LET_NERO, 1, ui_fb, ui_fb_pitch);
  x = bx + ((x1 - bx) - (int)strlen(let_motore) * 8) / 2;
  lettore_testo(let_motore, x > bx ? x : bx, let_vol_y, LET_GRIGIO);
}

static void lettore_disegna_colonne(void) {
  int s, v, seg;
  const int spente = emux_sid_voci_mute();

  if (!let_mostra_vu) {
    return;
  }
  for (s = 0; s < lettore_sid; s++) {
    for (v = 0; v < 3; v++) {
      int i = s * 3 + v;
      int x = lettore_x(s, v);
      for (seg = 0; seg < let_seg; seg++) {
        int y = let_y_basso - (seg + 1) * let_seg_passo;

        ui_draw_rect_buf(x, y, let_col, let_seg_h,
                         (spente & (1 << i)) ? LET_GRIGIO_SCURO :
                         lettore_colore(seg, seg < lettore_acceso[i],
                                        lettore_picco[i] > 0 &&
                                        seg == lettore_picco[i] - 1),
                         1, ui_fb, ui_fb_pitch);
      }
    }
  }
}

static void lettore_disegna_tutto(void) {
  char riga[48];
  int s, v;
  int brano = emux_sid_quale_brano();
  int quanti = emux_sid_quanti_brani();

  memset(ui_fb, LET_NERO, ui_fb_h * ui_fb_pitch);
  lettore_geometria();
  memset(lettore_etichetta, 0, sizeof(lettore_etichetta));
  memset(lettore_modelli, 0, sizeof(lettore_modelli));
  if (let_mostra_info) {
    lettore_testo(emux_sid_chi_suona(0), -1, let_ay + 6, LET_BIANCO);
    lettore_testo(emux_sid_chi_suona(1), -1, let_ay + 17, LET_GRIGIO);

    lettore_testo_in_mezzo(emux_sid_chi_suona(2), let_ay + 28,
                           LET_GRIGIO_SCURO, 11 * 8);
  }
  for (s = 0; s < lettore_sid; s++) {
    for (v = 0; v < 3 && let_mostra_vu; v++) {
      snprintf(riga, sizeof(riga), "%d", v + 1);

      lettore_testo(riga, lettore_x(s, v) + let_col / 2 - 4,
                    let_y_basso + 4,
                    (emux_sid_voci_mute() & (1 << (s * 3 + v)))
                        ? LET_ROSSO : LET_GRIGIO_SCURO);
    }





    {
      const char *modello = emux_sid_modello(s);
      int passo = 3 * let_col + 2 * let_gap + let_gap_sid;
      char sotto[16];

      lettore_modelli[s] = modello;
      riga[0] = 0;
      sotto[0] = 0;








      {
        const int lato = emux_sid_lato(s);
        char l[3] = { 0, 0, 0 };

        if (lato >= 0 && lato <= 2) {
          l[0] = ' ';
          l[1] = "LCR"[lato];
        }
        if (modello != NULL) {
          snprintf(riga, sizeof(riga), "%d %s%s", s + 1, modello, l);
          if (((int)strlen(riga) + 1) * 8 > passo) {
            if (let_due_righe) {
              snprintf(riga, sizeof(riga), "%d%s", s + 1, l);
              snprintf(sotto, sizeof(sotto), "%s", modello);
            } else {
              snprintf(riga, sizeof(riga), "%s", modello);
            }
          }
        } else {
          snprintf(riga, sizeof(riga), "%d%s", s + 1, l);
        }
      }
      if (sotto[0]) {
        snprintf(lettore_etichetta[s], sizeof(lettore_etichetta[s]), "%s %s",
                 riga, sotto);
      } else {
        snprintf(lettore_etichetta[s], sizeof(lettore_etichetta[s]), "%s", riga);
      }
      if (riga[0] && let_mostra_vu) {
        lettore_testo(riga, lettore_x(s, 1) + let_col / 2 - (int)strlen(riga) * 4,
                      let_y_basso + 14, LET_GRIGIO);
      }
      if (sotto[0] && let_mostra_vu) {
        lettore_testo(sotto, lettore_x(s, 1) + let_col / 2 - (int)strlen(sotto) * 4,
                      let_y_basso + 24, LET_GRIGIO);
      }
    }
  }
  if (brano > 0) {
    snprintf(riga, sizeof(riga), "TUNE %d OF %d", brano, quanti);
  } else {
    snprintf(riga, sizeof(riga), "DEFAULT TUNE (%d IN FILE)", quanti);
  }
  if (let_avviso_visibile) {
    lettore_testo(let_avviso, -1, let_ay + let_ah - 10, LET_GIALLO);
  } else {
    lettore_testo(riga, -1, let_ay + let_ah - 10, LET_GRIGIO);
  }
  lettore_disegna_tempo();
  if (let_volume_visibile) {
    lettore_disegna_volume();
  } else if (let_motore_visibile) {
    lettore_disegna_motore();
  }
  lettore_disegna_colonne();
}

int ui_lettore_sid_a_schermo(void) {
  return lettore_mostrato;
}



int ui_lettore_sid_mostra(int cosa) {
  int *f = cosa == 0 ? &let_mostra_vu
                     : (cosa == 1 ? &let_mostra_tempo : &let_mostra_info);

  *f = !*f;
  let_ridisegna = 1;
  return *f;
}


void ui_lettore_sid_avviso(const char *testo) {
  snprintf(let_avviso, sizeof(let_avviso), "%s", testo);
  let_avviso_visibile = 1;
  let_avviso_da = circle_get_ticks();
  let_ridisegna = 1;
}



void ui_lettore_sid_motore(const char *testo) {
  snprintf(let_motore, sizeof(let_motore), "%s", testo);
  let_motore_visibile = 1;
  let_motore_quadri = LET_MOTORE_QUADRI;
  let_volume_visibile = 0;
  let_ridisegna = 1;
}


int ui_lettore_sid_quanti(void) {
  return lettore_mostrato ? lettore_sid : 0;
}








static void lettore_salto_ferma(void) {
  if (let_salto_fino >= 0) {
    emux_sid_corsa(0);
    let_salto_fino = -1;
  }
}















int ui_lettore_sid_salta(int secondi) {
  int ora, dove, da, volo;
  unsigned long clk0 = 0;

  if (!lettore_mostrato) {
    return -1;
  }
  volo = emux_sid_foto_in_volo();
  ora = volo >= 0 ? volo : lettore_secondi();
  dove = (let_salto_fino >= 0 ? let_salto_fino : ora) + secondi;
  if (dove < 0) {
    dove = 0;
  }
  da = dove > 0 ? emux_sid_foto_cerca(dove) : -1;
  if (da >= 0 && (dove < ora || da > ora) &&
      emux_sid_foto_torna(da, &clk0) == 0) {
    let_clk0 = clk0;
    let_secondi_scritti = -1;
    ora = da;
  } else if (dove < ora || (dove == 0 && secondi < 0)) {
    emux_sid_brano(0);
    lettore_tempo_riparte();
    ora = 0;
  }
  if (dove <= ora) {
    lettore_salto_ferma();
    return dove;
  }
  let_salto_brano = emux_sid_quale_brano();
  let_salto_fino = dove;
  emux_sid_corsa(1);
  return dove;
}





















int ui_lettore_sid_tick(void) {
  int livello[24];
  int quanti;
  int i;
  int cambiato = 0;






  if (emux_sid_foto_andata_male()) {
    lettore_salto_ferma();
    lettore_tempo_riparte();
  }



  if (let_salto_fino >= 0 && emux_sid_foto_in_volo() < 0 &&
      lettore_secondi() >= let_salto_fino) {
    lettore_salto_ferma();
  }
  if (ui_fb == NULL || ui_enabled) {
    return 0;
  }
  for (i = 0; i < 24; i++) {
    livello[i] = -1;
  }
  quanti = emux_sid_livelli(livello);
  memcpy(lettore_livelli, livello, sizeof(lettore_livelli));
  lettore_livelli_c = quanti;
  if (quanti <= 0) {
    lettore_salto_ferma();
    if (lettore_mostrato) {
      memset(ui_fb, TRANSPARENT_COLOR, ui_fb_h * ui_fb_pitch);
      circle_hide_fbl(FB_LAYER_UI);
      lettore_rimette_vic();
      ui_showing = 0;
      lettore_mostrato = 0;
    }


    overlay_lettore_sid(0);
    lettore_tempo_riparte();
    menu_shader_lettore(0);


    if (emux_sid_voci_mute() != 0) {
      emux_sid_voci_imposta(0);
    }
    emux_sid_drive_zitti(0);
    return 0;
  }




  if (lettore_mostrato && lettore_brano == emux_sid_quale_brano() &&
      emux_sid_foto_in_volo() < 0) {
    const int s = lettore_secondi();

    if (s != let_foto_ultima) {
      let_foto_ultima = s;
      emux_sid_foto_chiedi(s, let_clk0);
    }
  }


  menu_shader_lettore(1);

  emux_sid_drive_zitti(1);





  if (lettore_mostrato && !ui_showing) {
    lettore_mostrato = 0;
  }
  if (quanti != lettore_sid) {
    lettore_sid = quanti;
    memset(lettore_acceso, 0, sizeof(lettore_acceso));
    memset(lettore_picco, 0, sizeof(lettore_picco));
    lettore_mostrato = 0;
  }
  if (!lettore_mostrato) {
    lettore_geometria();
  }
  for (i = 0; i < 3 * lettore_sid; i++) {
    int n = (livello[i] * let_seg + 128) / 256;
    if (n > let_seg) {
      n = let_seg;
    }






    if (n >= lettore_acceso[i]) {
      if (n != lettore_acceso[i]) {
        cambiato = 1;
      }
      lettore_acceso[i] = n;
    } else {
      lettore_acceso[i] -= (lettore_acceso[i] - n + 1) / 2;
      cambiato = 1;
    }

    if (lettore_acceso[i] >= lettore_picco[i]) {
      if (lettore_picco[i] != lettore_acceso[i]) {
        cambiato = 1;
      }
      lettore_picco[i] = lettore_acceso[i];
      lettore_picco_tempo[i] = 15;
    } else if (--lettore_picco_tempo[i] <= 0) {
      lettore_picco[i]--;
      lettore_picco_tempo[i] = 1;
      cambiato = 1;
    }
  }
  {
    int s;

    for (s = 0; s < lettore_sid && s < 8; s++) {
      if (emux_sid_modello(s) != lettore_modelli[s]) {
        lettore_mostrato = 0;
      }
    }
  }


  if (!lettore_mostrato || let_ridisegna ||
      lettore_brano != emux_sid_quale_brano()) {
    let_ridisegna = 0;
    if (lettore_brano != emux_sid_quale_brano()) {
      lettore_tempo_riparte();
      if (emux_sid_quale_brano() != let_salto_brano) {
        lettore_salto_ferma();
      }
    }
    if (!lettore_mostrato) {

      let_volume_tasti = menu_tasti_volume;
      let_volume_visibile = 0;
      let_motore_visibile = 0;
    }
    lettore_tavolozza();
    lettore_disegna_tutto();






    circle_show_fbl(FB_LAYER_UI);
    lettore_spegne_vic();
    overlay_lettore_sid(1);
    ui_showing = 1;
    lettore_mostrato = 1;
    lettore_brano = emux_sid_quale_brano();
    return 1;
  }
  if (let_salto_fino >= 0) {


    if (lettore_secondi() != let_secondi_scritti) {
      lettore_disegna_tempo();
      return 1;
    }
    return 0;
  }
  if (lettore_secondi() != let_secondi_scritti) {
    lettore_disegna_tempo();
    cambiato = 1;
  }
  if (menu_tasti_volume != let_volume_tasti) {

    let_volume_tasti = menu_tasti_volume;
    let_volume_da = circle_get_ticks();
    let_volume_visibile = 1;
    let_motore_visibile = 0;
    lettore_disegna_volume();
    cambiato = 1;
  } else if (let_volume_visibile) {
    if (lettore_volume() != let_volume_scritto) {
      lettore_disegna_volume();
      cambiato = 1;
    }
    if (circle_get_ticks() - let_volume_da >= LET_VOL_US) {
      let_volume_visibile = 0;
      lettore_cancella_volume();
      cambiato = 1;
    }
  }


  if (let_motore_visibile && --let_motore_quadri <= 0) {
    let_motore_visibile = 0;
    if (!let_volume_visibile) {
      lettore_cancella_volume();
    }
    cambiato = 1;
  }

  if (let_avviso_visibile &&
      circle_get_ticks() - let_avviso_da >= LET_AVVISO_US) {
    let_avviso_visibile = 0;
    let_ridisegna = 1;
  }
  if (cambiato) {
    lettore_disegna_colonne();
    return 1;
  }
  return 0;
}


static void ui_lettore_sid_sonda(void) {
  int l[24];
  int c;
  int i;
  int x, y;
  long opachi = 0;
  long accesi = 0;
  long fuori = 0;
  long bianchi = 0;

  for (i = 0; i < 24; i++) {
    l[i] = -1;
  }



  if (lettore_mostrato) {
    memcpy(l, lettore_livelli, sizeof(l));
    c = lettore_livelli_c;
  } else {
    c = emux_sid_livelli(l);
  }
  if (ui_fb != NULL) {
    for (y = 0; y < ui_fb_h; y++) {
      for (x = 0; x < ui_fb_w; x++) {
        uint8_t p = ui_fb[y * ui_fb_pitch + x];
        if (p != TRANSPARENT_COLOR) {
          opachi++;
        }
        if (p == LET_VERDE || p == LET_GIALLO || p == LET_ROSSO) {
          accesi++;
        }
        if (lettore_mostrato && p != LET_NERO &&
            p != TRANSPARENT_COLOR &&
            (x < let_ax || x >= let_ax + let_aw ||
             y < let_ay || y >= let_ay + let_ah)) {
          fuori++;
        }
        if (lettore_mostrato && p == LET_BIANCO) {
          bianchi++;
        }
      }
    }
  }
  printf("[VU] c=%d mostrato=%d livelli %d %d %d | %d %d %d | %d %d %d"
         " segmenti %d %d %d | %d %d %d opachi=%ld%% accesi=%ld %dx%d"
         " menu=%d osd=%d area %d,%d %dx%d fuori=%ld bianchi=%ld"
         " audio %d/%u vis=%d vic=%d comp=%u clk=%lu\n",
         c, lettore_mostrato, l[0], l[1], l[2], l[3], l[4], l[5],
         l[6], l[7], l[8],
         lettore_acceso[0], lettore_acceso[1], lettore_acceso[2],
         lettore_acceso[3], lettore_acceso[4], lettore_acceso[5],
         ui_fb ? opachi * 100 / ((long)ui_fb_w * ui_fb_h) : -1L, accesi,
         ui_fb_w, ui_fb_h, (int)ui_enabled, ui_osd_attivo(),
         let_ax, let_ay, let_aw, let_ah, fuori, bianchi,
         raspi_snd_restart, raspi_snd_restarts, ui_showing,
         vic_showing, fbl_composizioni,
         emux_orologio_cpu());
  printf("[VU] etichette: '%s' '%s' '%s'\n", lettore_etichetta[0],
         lettore_etichetta[1], lettore_etichetta[2]);

  printf("[VU] lati: %d %d %d %d\n", emux_sid_lato(0), emux_sid_lato(1),
         emux_sid_lato(2), emux_sid_lato(3));
  if (c >= 4) {
    printf("[VU] sid4: livelli %d %d %d etichetta '%s'\n", l[9], l[10],
           l[11], lettore_etichetta[3]);
  }


  if (c > 4) {
    int s;

    for (s = 4; s < c && s < 8; s++) {
      printf("[VU] sid%d: livelli %d %d %d etichetta '%s' lato %d\n", s + 1,
             l[s * 3], l[s * 3 + 1], l[s * 3 + 2], lettore_etichetta[s],
             emux_sid_lato(s));
    }
  }



  printf("[VU] geometria col=%d gap=%d gap_sid=%d margine=%d due_righe=%d"
         " fine=%d area=%d volume_y=%d etichette_y=%d fondo_y=%d\n",
         let_col, let_gap, let_gap_sid, let_x0 - let_ax, let_due_righe,
         lettore_x(lettore_sid - 1, 2) + let_col - let_ax, let_aw,
         let_vol_y - let_ay, let_y_basso + 14 - let_ay, let_ah - 10);
  printf("[VU] tasti: spente=%03x vu=%d tempo=%d info=%d drive_zitti=%d avviso=%d '%s'"
         " secondi=%d salto=%d\n",
         emux_sid_voci_mute(), let_mostra_vu, let_mostra_tempo,
         let_mostra_info, emux_sid_drive_zitti_ora(), let_avviso_visibile, let_avviso,
         lettore_secondi(), let_salto_fino);
}












static struct menu_item *ui_voce_prima_di_esc = NULL;
static void ui_riporta_voce_prima_di_esc(void);
static void ui_traverse(void);

static void ui_toggle(void) {
  if (ui_enabled) {
    ui_riporta_voce_prima_di_esc();
  }




  joy_alza_tutto();
  emux_alza_la_tastiera();
  ui_enabled = 1 - ui_enabled;
  if (!ui_enabled) {
    ui_esc_chiude_tutto = 0;
  }

  if (ui_enabled) {
    ui_mouse_sessione_apri();
  } else {
    ui_mouse_sessione_chiudi();
  }

  lettore_mostrato = 0;
  lettore_rimette_vic();
  if (ui_enabled) {


    ui_stato_normale();
    ui_voce_prima_di_esc = NULL;
    ui_alt = 0;
    emux_trap_main_loop_ui();
  }
}






int ui_esc_chiude_tutto = 0;

void ui_pop_all_and_toggle() {
  while (current_menu > 0) {
    ui_pop_menu();
  }
  ui_toggle();
}

static void cursor_pos_updated() {

  ui_voce_prima_di_esc = NULL;
  ui_campo_toccato = NULL;
  ui_invio_reset();
  // Tell listener
  if (menu_roots[current_menu].cursor_listener_func) {
     menu_roots[current_menu].cursor_listener_func(&menu_roots[current_menu],
                                                   menu_cursor[current_menu]);
  }
}

static struct menu_item *ui_item_at_index(struct menu_item *node,
                                          int target_index, int *index) {
  while (node != NULL) {
    if (*index == target_index) {
      return node;
    }

    *index = *index + 1;
    if (node->type == FOLDER && node->is_expanded &&
        node->first_child != NULL) {
      struct menu_item *item = ui_item_at_index(node->first_child,
                                                target_index, index);
      if (item != NULL) {
        return item;
      }
    }
    node = node->next;
  }

  return NULL;
}

static int ui_cursor_is_selectable(int index) {
  int current_index = 0;
  struct menu_item *item = ui_item_at_index(
      menu_roots[current_menu].first_child, index, &current_index);
  if (item == NULL || item->disabled) {
    return 0;
  }



  return item->type != DIVIDER &&
         item->type != READ_ONLY_HEADING &&
         item->type != READ_ONLY_DESCRIPTION;
}

static void ui_traverse_children(struct menu_item *node, int *index);





void ui_menu_fit_height(int max_rows) {
  struct menu_item *node = menu_roots[current_menu].first_child;
  int n = 0;

  while (node != NULL) {
    n++;
    node = node->next;
  }
  if (n > max_rows) {
    n = max_rows;
  }
  if (ui_menu_altezza(n) > menu_roots[current_menu].menu_height) {

    menu_roots[current_menu].menu_height = ui_menu_altezza(n);
    menu_roots[current_menu].menu_rows = n;
    menu_window_bottom[current_menu] = menu_window_top[current_menu] +
        ui_menu_visible_rows(menu_roots[current_menu].menu_height,
                             ui_menu_row_gap);





    node = menu_roots[current_menu].first_child;
    while (node != NULL) {
      node->menu_height = menu_roots[current_menu].menu_height;
      node = node->next;
    }
  }
}





static void ui_sync_cursor_item(void) {
  int h = menu_window_bottom[current_menu] - menu_window_top[current_menu];
  int idx = 0;

  if (h <= 0) {
    h = 1;
  }
  if (menu_cursor[current_menu] < menu_window_top[current_menu]) {
    menu_window_top[current_menu] = menu_cursor[current_menu];
  } else if (menu_cursor[current_menu] >= menu_window_bottom[current_menu]) {
    menu_window_top[current_menu] = menu_cursor[current_menu] - h + 1;
  }
  if (menu_window_top[current_menu] < 0) {


  menu_cursor_item[current_menu] = NULL;
  menu_window_top[current_menu] = 0;
  }
  menu_window_bottom[current_menu] = menu_window_top[current_menu] + h;

  menu_cursor_item[current_menu] = NULL;
  ui_traverse_children(menu_roots[current_menu].first_child, &idx);
}




static int ui_riga_della_voce(struct menu_item *node,
                              const struct menu_item *v, int *index) {
  while (node != NULL) {
    if (node == v) {
      return *index;
    }
    *index = *index + 1;
    if (node->type == FOLDER && node->is_expanded &&
        node->first_child != NULL) {
      int riga = ui_riga_della_voce(node->first_child, v, index);
      if (riga >= 0) {
        return riga;
      }
    }
    node = node->next;
  }
  return -1;
}




static void ui_riporta_voce_prima_di_esc(void) {
  struct menu_item *v = ui_voce_prima_di_esc;
  int index = 0;
  int riga;

  ui_voce_prima_di_esc = NULL;
  if (v == NULL || current_menu != 0) {
    return;
  }
  riga = ui_riga_della_voce(menu_roots[0].first_child, v, &index);
  if (riga < 0 || !ui_cursor_is_selectable(riga)) {
    return;
  }
  menu_cursor[0] = riga;
  ui_traverse();
  ui_sync_cursor_item();
  cursor_pos_updated();
}

void ui_select_first_interactive_item(void) {
  int index = 0;
  int current_index;

  while (1) {
    current_index = 0;
    if (ui_item_at_index(menu_roots[current_menu].first_child, index,
                         &current_index) == NULL) {
      break;
    }
    if (ui_cursor_is_selectable(index)) {
      menu_cursor[current_menu] = index;
      ui_sync_cursor_item();
      return;
    }
    index++;
  }
}

static void ui_move_cursor(int direction) {
  int old_cursor = menu_cursor[current_menu];

  do {
    menu_cursor[current_menu] += direction;
    if (menu_cursor[current_menu] < 0) {
      menu_cursor[current_menu] = 0;
      break;
    }
    if (menu_cursor[current_menu] >= max_index[current_menu]) {
      menu_cursor[current_menu] = max_index[current_menu] - 1;
      break;
    }
  } while (!ui_cursor_is_selectable(menu_cursor[current_menu]));

  if (!ui_cursor_is_selectable(menu_cursor[current_menu])) {
    menu_cursor[current_menu] = old_cursor;
  }

  cursor_pos_updated();
}

static void ui_action(long action) {


  if (action != ACTION_Escape && action != ACTION_Exit) {
    ui_voce_prima_di_esc = NULL;
  }

  ui_azione_in_corso = action;
  ui_action_corpo(action);
  ui_azione_in_corso = ACTION_None;
}

static void ui_action_corpo(long action) {
  struct menu_item *cur = menu_cursor_item[current_menu];




  if (cur == NULL) {
    ui_sync_cursor_item();
    cur = menu_cursor_item[current_menu];
    if (cur == NULL && action != ACTION_Escape && action != ACTION_Exit) {
      return;
    }
  }
  switch (action) {
  case ACTION_Up:
  case ACTION_Down:





    if (cur != NULL && cur == ui_campo_toccato && cur->type == TEXTFIELD &&
        !cur->disabled && cur->sub_id != MENU_SUB_SEARCH &&
        cur->sub_id != MENU_SUB_PICK_FILE && cur->id != MENU_WIFI_SSID &&
        !cur->textfield_code) {
      int menu_prima = current_menu;
      ui_campo_toccato = NULL;
      do_on_value_changed(cur);
      if (current_menu != menu_prima) {
        return;
      }
    }
    ui_move_cursor(action == ACTION_Up ? -1 : 1);







    if (menu_cursor[current_menu] < menu_window_top[current_menu] ||
        menu_cursor[current_menu] >= menu_window_bottom[current_menu]) {
      ui_sync_cursor_item();
    }
    break;
  case ACTION_Left:
  case ACTION_MiniLeft:
    if (action == ACTION_Left &&
        menu_roots[current_menu].left_right_listener_func &&
        menu_roots[current_menu].left_right_listener_func(
            &menu_roots[current_menu], cur, 0)) {
      break;
    }
    if (cur->disabled) break;
    if (action == ACTION_Left && cur->type == FOLDER) {

      if (cur->is_expanded) {
        cur->is_expanded = 0;
        do_on_value_changed(menu_cursor_item[current_menu]);
      }
    } else if (cur->type == RANGE) {
      int prima = cur->value;
      if (action == ACTION_MiniLeft)
         cur->value -= cur->ministep;
      else
         cur->value -= cur->step;




      if (cur->value < cur->min) {
        cur->value = cur->min;
      }
      if (cur->value != prima) {
        do_on_value_changed(menu_cursor_item[current_menu]);
      }
    } else if (cur->type == MULTIPLE_CHOICE) {
      int orig = cur->value;
      cur->value -= 1;
      if (cur->value < 0) {
        cur->value = cur->num_choices - 1;
      }



      while (cur->choice_disabled[cur->value] && cur->value != orig) {
        cur->value = (cur->value + cur->num_choices - 1) % cur->num_choices;
      }
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == TOGGLE) {
      cur->value = 1 - cur->value;
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == TEXTFIELD) {
      // Move cursor left
      cur->value--;
      if (cur->value < 0) {
        cur->value = 0;
      }
    }
    break;
  case ACTION_Right:
  case ACTION_MiniRight:
    if (action == ACTION_Right &&
        menu_roots[current_menu].left_right_listener_func &&
        menu_roots[current_menu].left_right_listener_func(
            &menu_roots[current_menu], cur, 1)) {
      break;
    }
    if (cur->disabled) break;
    if (action == ACTION_Right && cur->type == FOLDER) {

      if (!cur->is_expanded) {
        cur->is_expanded = 1;
        do_on_value_changed(menu_cursor_item[current_menu]);
      }
    } else if (cur->type == RANGE) {
      int prima = cur->value;
      if (action == ACTION_MiniRight)
         cur->value += cur->ministep;
      else
         cur->value += cur->step;

      if (cur->value > cur->max) {
        cur->value = cur->max;
      }
      if (cur->value != prima) {
        do_on_value_changed(menu_cursor_item[current_menu]);
      }
    } else if (cur->type == MULTIPLE_CHOICE) {
      int orig = cur->value;
      cur->value += 1;
      if (cur->value >= cur->num_choices) {
        cur->value = 0;
      }
      while (cur->choice_disabled[cur->value] && cur->value != orig) {
        cur->value = (cur->value + 1) % cur->num_choices;
      }
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == TOGGLE) {
      cur->value = 1 - cur->value;
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == TEXTFIELD) {
      // Move cursor right
      cur->value++;
      if (cur->value >= strlen(cur->str_value)) {
        cur->value = strlen(cur->str_value);
      }
    }
    break;
  case ACTION_Return:
    if (cur->disabled) break;
    if (cur->type == FOLDER) {
      cur->is_expanded = 1 - cur->is_expanded;
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == CHECKBOX) {
      cur->value = 1 - cur->value;
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == TOGGLE) {
      cur->value = 1 - cur->value;
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == BUTTON) {
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == MULTIPLE_CHOICE) {
      int orig = cur->value;
      ui_valore_prima_invio = orig;
      cur->value += 1;
      if (cur->value >= cur->num_choices) {
        cur->value = 0;
      }



      while (cur->choice_disabled[cur->value] && cur->value != orig) {
        cur->value = (cur->value + 1) % cur->num_choices;
      }
      do_on_value_changed(menu_cursor_item[current_menu]);
    } else if (cur->type == TEXTFIELD) {
      ui_campo_toccato = NULL;
      do_on_value_changed(menu_cursor_item[current_menu]);
    }
    break;
  case ACTION_Escape:
    if (current_menu > 0) {
      if (osd_active || ui_esc_chiude_tutto) {
        ui_pop_all_and_toggle();
        return;
      }
      ui_pop_menu();
    } else {


      struct menu_item *quick_access = ui_find_item_by_id(
          menu_roots[0].first_child, MENU_QUICK_ACCESS_FOLDER);
      if (quick_access != NULL && cur != quick_access) {


        struct menu_item *toccata = cur;
        ui_focus_item(quick_access);
        cursor_pos_updated();
        ui_voce_prima_di_esc = toccata;
      } else {
        ui_toggle();
      }
    }
    break;
  case ACTION_Exit:
    ui_pop_all_and_toggle();
    break;
  }
}

// queue a key for press/release on the UI loop
void emu_ui_key_interrupt(long key, int pressed) {
  circle_lock_acquire();



  if (pending_ui_key_tail - pending_ui_key_head < PENDING_UI_KEY_SIZE) {
    int i = pending_ui_key_tail & PENDING_UI_KEY_MASK;
    pending_ui_key[i] = key;
    pending_ui_key_pressed[i] = pressed;
    pending_ui_key_tail++;
  }
  circle_lock_release();
}







#define SCORRI_FERMO_US 800000UL
#define SCORRI_PASSO_US 100000UL
#define SCORRI_FINE_US 1200000UL

static int lettere_utf8(const char *s) {
  int n = 0;
  for (; *s != '\0'; s++) {
    if (((unsigned char)*s & 0xC0) != 0x80) {
      n++;
    }
  }
  return n;
}

static const char *salta_lettere_utf8(const char *s, int quante) {
  while (*s != '\0' && quante > 0) {
    s++;
    while (((unsigned char)*s & 0xC0) == 0x80) {
      s++;
    }
    quante--;
  }
  return s;
}




static int lettere_del_nome(const struct menu_item *node, int indent) {
  int colonne = MAX_MENU_STR - 1;
  if (strcmp(node->displayed_value, "(dir)") == 0) {
    int libere = node->menu_width / 8 - indent - 1 - 6;
    if (libere > 0 && libere < colonne) {
      colonne = libere;
    }
  }
  return colonne;
}



static void nome_che_scorre(const char *nome, unsigned long trascorso,
                            int finestra, char *buf, size_t n) {
  int oltre = lettere_utf8(nome) - finestra;
  int passo = 0;
  if (oltre > 0) {
    unsigned long giro = SCORRI_FERMO_US +
                         (unsigned long)oltre * SCORRI_PASSO_US + SCORRI_FINE_US;
    unsigned long t = trascorso % giro;
    if (t >= SCORRI_FERMO_US) {
      passo = (int)((t - SCORRI_FERMO_US) / SCORRI_PASSO_US);
      if (passo > oltre) {
        passo = oltre;
      }
    }
  }
  const char *da = salta_lettere_utf8(nome, passo);
  const char *a = salta_lettere_utf8(da, finestra);
  size_t quanti = (size_t)(a - da);
  if (quanti > n - 1) {
    quanti = n - 1;
  }
  memcpy(buf, da, quanti);
  buf[quanti] = '\0';
}

static struct menu_item *scorre_voce;
static int scorre_cursore = -1;
static unsigned long scorre_da;



static int ui_invio_fine_del_nome(struct menu_item *node, unsigned long ora,
                                  int finestra, char *buf, size_t n) {
  int lettere;
  const char *da;
  size_t quanti;
  if (!invio_giu || invio_voce != node || node->nome_lungo == NULL) {
    return 0;
  }
  if (!invio_consumato &&
      ora - invio_da < (unsigned long)ui_browser_tail_delay * 1000UL) {
    return 0;
  }
  invio_consumato = 1;
  lettere = lettere_utf8(node->nome_lungo);
  da = salta_lettere_utf8(node->nome_lungo,
                          lettere > finestra ? lettere - finestra : 0);
  quanti = strlen(da);
  if (quanti > n - 1) {
    quanti = n - 1;
  }
  memcpy(buf, da, quanti);
  buf[quanti] = '\0';
  return 1;
}



static const char *testo_della_voce(struct menu_item *node,
                                    int sotto_il_cursore, int indent, char *buf,
                                    size_t n) {
  int finestra = lettere_del_nome(node, indent);
  const char *intero = node->name;
  if (node->nome_lungo != NULL &&
      strncmp(node->nome_lungo, node->name, strlen(node->name)) == 0) {
    intero = node->nome_lungo;
  }
  if (lettere_utf8(intero) <= finestra) {
    return node->name;
  }
  if (!sotto_il_cursore) {
    nome_che_scorre(node->name, 0, finestra, buf, n);
    return buf;
  }
  unsigned long ora = circle_get_ticks();
  if (intero == node->nome_lungo &&
      ui_invio_fine_del_nome(node, ora, finestra, buf, n)) {
    return buf;
  }
  if (invio_riparte) {
    scorre_voce = NULL;
    invio_riparte = 0;
  }
  if (node != scorre_voce || menu_cursor[current_menu] != scorre_cursore) {
    scorre_voce = node;
    scorre_cursore = menu_cursor[current_menu];
    scorre_da = ora;
  }
  nome_che_scorre(intero, ora - scorre_da, finestra, buf, n);
  return buf;
}










































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































int ui_get_menu_mouse_enabled(void) {
  return ui_mouse_acceso;
}

void ui_set_menu_mouse_enabled(int acceso) {
  ui_mouse_acceso = acceso ? 1 : 0;
  if (!ui_mouse_acceso) {
    ui_mouse_sessione_chiudi();
  } else if (ui_enabled && !ui_mouse_sessione) {
    ui_mouse_sessione_apri();
  }
}

static void ui_mouse_svuota(void) {
  circle_lock_acquire();
  ui_mouse_testa = ui_mouse_coda_fine;
  circle_lock_release();
}

static void ui_mouse_trascina_finita(void) {
  ui_mouse_trascina = NULL;
  ui_mouse_trascina_asse = 0;
  ui_mouse_trascina_dx = 0;
  ui_mouse_trascina_dy = 0;
  ui_mouse_trascina_resto = 0;
}

static void ui_mouse_sessione_apri(void) {
  struct menu_item *p = NULL;

  ui_mouse_svuota();
  ui_mouse_tasti = 0;
  ui_mouse_trascina_finita();
  ui_mouse_visto = 0;
  ui_mouse_sessione = ui_mouse_acceso;

  if (current_menu >= 0) {
    p = menu_roots[current_menu].first_child;
  }
  if (p != NULL && p->menu_width > 0 && p->menu_height > 0) {
    ui_mouse_x = p->menu_left + p->menu_width / 2;
    ui_mouse_y = p->menu_top + p->menu_height / 2;
  } else {
    ui_mouse_x = ui_fb_w / 2;
    ui_mouse_y = ui_fb_h / 2;
  }
}

static void ui_mouse_sessione_chiudi(void) {
  ui_mouse_svuota();
  ui_mouse_sessione = 0;
  ui_mouse_visto = 0;
  ui_mouse_tasti = 0;
  ui_mouse_trascina_finita();
}

static int ui_mouse_somma(int a, int b) {
  long s = (long)a + (long)b;
  if (s > UI_MOUSE_SOMMA_MAX) return UI_MOUSE_SOMMA_MAX;
  if (s < -UI_MOUSE_SOMMA_MAX) return -UI_MOUSE_SOMMA_MAX;
  return (int)s;
}


static void ui_mouse_metti(unsigned tasti, int dx, int dy, int rotella) {
  struct ui_mouse_evento *e;

  circle_lock_acquire();
  if (ui_mouse_testa != ui_mouse_coda_fine) {
    e = &ui_mouse_eventi[(ui_mouse_coda_fine - 1U) & (UI_MOUSE_CODA - 1U)];
    if (e->tasti == tasti) {
      e->dx = ui_mouse_somma(e->dx, dx);
      e->dy = ui_mouse_somma(e->dy, dy);
      e->rotella = ui_mouse_somma(e->rotella, rotella);
      circle_lock_release();
      return;
    }
  }
  if (ui_mouse_coda_fine - ui_mouse_testa >= UI_MOUSE_CODA) {
    ui_mouse_testa++;
  }
  e = &ui_mouse_eventi[ui_mouse_coda_fine & (UI_MOUSE_CODA - 1U)];
  e->tasti = tasti;
  e->dx = dx;
  e->dy = dy;
  e->rotella = rotella;
  ui_mouse_coda_fine++;
  circle_lock_release();
}





int ui_mouse_rapporto(unsigned *tasti, unsigned *tasti_prima, int dx, int dy, int rotella) {
  if (ui_enabled && ui_mouse_acceso) {
    if (!ui_mouse_al_menu) {
      if (*tasti_prima & UI_MOUSE_SINISTRO) emu_mouse_button_left(0);
      if (*tasti_prima & UI_MOUSE_DESTRO) emu_mouse_button_right(0);
      if (*tasti_prima & UI_MOUSE_CENTRALE) emu_mouse_button_middle(0);
      ui_mouse_ignorati = *tasti_prima & *tasti;
      ui_mouse_al_menu = 1;
    }
    ui_mouse_ignorati &= *tasti;
    ui_mouse_metti(*tasti & ~ui_mouse_ignorati & 7U, dx, dy, rotella);
    return 1;
  }
  if (ui_mouse_al_menu) {
    ui_mouse_al_menu = 0;
    ui_mouse_ignorati = *tasti;
  }
  ui_mouse_ignorati &= *tasti;
  if (ui_mouse_ignorati) {
    *tasti &= ~ui_mouse_ignorati;
    *tasti_prima &= ~ui_mouse_ignorati;
  }
  return 0;
}

static void ui_mouse_muovi(int dx, int dy) {
  ui_mouse_x = ui_mouse_somma(ui_mouse_x, dx);
  ui_mouse_y = ui_mouse_somma(ui_mouse_y, dy);
  if (ui_mouse_x < 0) ui_mouse_x = 0;
  if (ui_mouse_y < 0) ui_mouse_y = 0;
  if (ui_fb_w > 0 && ui_mouse_x > ui_fb_w - 1) ui_mouse_x = ui_fb_w - 1;
  if (ui_fb_h > 0 && ui_mouse_y > ui_fb_h - 1) ui_mouse_y = ui_fb_h - 1;
}



static int ui_mouse_riga(void) {
  struct menu_item *p;
  int indice;

  if (current_menu < 0) return -1;
  p = menu_roots[current_menu].first_child;
  if (p == NULL) return -1;
  if (ui_mouse_x < p->menu_left || ui_mouse_x >= p->menu_left + p->menu_width ||
      ui_mouse_y < p->menu_top || ui_mouse_y >= p->menu_top + p->menu_height) {
    return -1;
  }
  indice = menu_window_top[current_menu] +
           (ui_mouse_y - p->menu_top) / ui_menu_row_pitch(ui_menu_row_gap);
  if (indice >= menu_window_bottom[current_menu] || indice >= max_index[current_menu]) {
    return -1;
  }
  return indice;
}



static struct menu_item *ui_mouse_scegli(void) {
  int quale = current_menu;
  int i = ui_mouse_riga();

  if (i < 0 || !ui_cursor_is_selectable(i)) return NULL;
  if (i != menu_cursor[current_menu]) {
    menu_cursor[current_menu] = i;
    cursor_pos_updated();
    if (current_menu != quale) return NULL;
    ui_sync_cursor_item();
  }
  return menu_cursor_item[current_menu];
}



static void ui_mouse_bordo(int dy) {
  struct menu_item *p;

  if (current_menu < 0 || dy == 0) return;
  p = menu_roots[current_menu].first_child;
  if (p == NULL) return;
  if (ui_mouse_x < p->menu_left || ui_mouse_x >= p->menu_left + p->menu_width) return;
  if (dy < 0 && ui_mouse_y <= p->menu_top && menu_window_top[current_menu] > 0) {
    if (ui_cursor_is_selectable(menu_window_top[current_menu])) {
      menu_cursor[current_menu] = menu_window_top[current_menu];
    }
    ui_action(ACTION_Up);
  } else if (dy > 0 && ui_mouse_y >= p->menu_top + p->menu_height - 1 &&
             menu_window_bottom[current_menu] < max_index[current_menu]) {
    if (ui_cursor_is_selectable(menu_window_bottom[current_menu] - 1)) {
      menu_cursor[current_menu] = menu_window_bottom[current_menu] - 1;
    }
    ui_action(ACTION_Down);
  }
}


static void ui_mouse_cambia(struct menu_item *v, int passi, int fini) {
  int incremento;
  long nuovo;

  if (v == NULL || v->type != RANGE || v->disabled || passi == 0) return;
  incremento = fini ? v->ministep : v->step;
  if (incremento <= 0) incremento = 1;
  nuovo = (long)v->value + (long)passi * incremento;
  if (nuovo < v->min) nuovo = v->min;
  if (nuovo > v->max) nuovo = v->max;
  if (nuovo == v->value) return;
  v->value = (int)nuovo;
  do_on_value_changed(v);
}



static void ui_mouse_trascina_avanti(const struct ui_mouse_evento *e) {
  int appena = 0;
  int passi;
  int ax, ay;

  if (!(e->tasti & UI_MOUSE_SINISTRO) || current_menu < 0 ||
      ui_mouse_trascina != menu_cursor_item[current_menu]) {
    ui_mouse_trascina_finita();
    return;
  }
  if (ui_mouse_trascina_asse == 0) {
    ui_mouse_trascina_dx = ui_mouse_somma(ui_mouse_trascina_dx, e->dx);
    ui_mouse_trascina_dy = ui_mouse_somma(ui_mouse_trascina_dy, e->dy);
    ax = ui_mouse_trascina_dx < 0 ? -ui_mouse_trascina_dx : ui_mouse_trascina_dx;
    ay = ui_mouse_trascina_dy < 0 ? -ui_mouse_trascina_dy : ui_mouse_trascina_dy;
    if (ax < UI_MOUSE_ZONA_MORTA && ay < UI_MOUSE_ZONA_MORTA) return;
    ui_mouse_trascina_asse = ax >= ay ? 1 : 2;
    ui_mouse_trascina_resto = ui_mouse_trascina_asse == 1 ? ui_mouse_trascina_dx : -ui_mouse_trascina_dy;
    if (ui_mouse_trascina_resto > 0) {
      ui_mouse_trascina_resto -= UI_MOUSE_ZONA_MORTA;
    } else if (ui_mouse_trascina_resto < 0) {
      ui_mouse_trascina_resto += UI_MOUSE_ZONA_MORTA;
    }
    appena = 1;
  }
  if (!appena) {
    ui_mouse_trascina_resto = ui_mouse_somma(ui_mouse_trascina_resto,
                                             ui_mouse_trascina_asse == 1 ? e->dx : -e->dy);
  }
  passi = ui_mouse_trascina_resto / UI_MOUSE_PASSO_TRASCINA;
  if (passi != 0) {
    ui_mouse_trascina_resto %= UI_MOUSE_PASSO_TRASCINA;
    if (passi > UI_MOUSE_MAX_PASSI) passi = UI_MOUSE_MAX_PASSI;
    if (passi < -UI_MOUSE_MAX_PASSI) passi = -UI_MOUSE_MAX_PASSI;
    ui_mouse_cambia(ui_mouse_trascina, passi, 1);
  }
}

static void ui_mouse_servi(const struct ui_mouse_evento *e) {
  unsigned premuti = e->tasti & ~ui_mouse_tasti;
  int era_visto = ui_mouse_visto;
  struct menu_item *v;
  int r;

  ui_mouse_tasti = e->tasti;
  if (!ui_mouse_sessione || !ui_mouse_acceso || !ui_enabled || current_menu < 0) {
    ui_mouse_trascina_finita();
    return;
  }
  ui_mouse_visto = 1;
  if (ui_mouse_trascina != NULL) {
    ui_mouse_trascina_avanti(e);
    return;
  }
  if (e->dx != 0 || e->dy != 0) {
    ui_mouse_muovi(e->dx, e->dy);
    ui_mouse_bordo(e->dy);
    (void)ui_mouse_scegli();
  }
  if (premuti & UI_MOUSE_DESTRO) {
    ui_action(ACTION_Escape);
    return;
  }
  r = e->rotella;
  if (r > 16) r = 16;
  if (r < -16) r = -16;
  if (r != 0) {

    int i = era_visto ? ui_mouse_riga() : -1;
    int contati = 0;
    struct menu_item *sotto =
        i >= 0 ? ui_item_at_index(menu_roots[current_menu].first_child, i, &contati) : NULL;
    if (sotto != NULL && sotto->type == RANGE && !sotto->disabled) {
      v = ui_mouse_scegli();
      if (v == sotto) {
        ui_mouse_cambia(v, r, 0);
      }
      return;
    }
    for (; r > 0; r--) ui_action(ACTION_Up);
    for (; r < 0; r++) ui_action(ACTION_Down);
    return;
  }
  if ((premuti & UI_MOUSE_SINISTRO) && era_visto) {
    v = ui_mouse_scegli();
    if (v == NULL) return;
    if (v->type == RANGE && !v->disabled) {
      ui_mouse_trascina_finita();
      ui_mouse_trascina = v;
    } else {
      ui_action(ACTION_Return);
    }
  }
}


static void ui_check_mouse(void) {
  struct ui_mouse_evento ev[UI_MOUSE_CODA];
  unsigned n = 0;
  unsigned i;

  if (ui_enabled && ui_mouse_acceso && !ui_mouse_sessione) {
    ui_mouse_sessione_apri();
  }
  circle_lock_acquire();
  while (ui_mouse_testa != ui_mouse_coda_fine && n < UI_MOUSE_CODA) {
    ev[n++] = ui_mouse_eventi[ui_mouse_testa & (UI_MOUSE_CODA - 1U)];
    ui_mouse_testa++;
  }
  circle_lock_release();
  for (i = 0; i < n; i++) {
    ui_mouse_servi(&ev[i]);
  }
}


static void ui_mouse_freccia(void) {
  static const char *const disegno[] = {
      "#",          "##",         "#=#",        "#==#",       "#===#",      "#====#",
      "#=====#",    "#======#",   "#=======#",  "#====#####", "#==#=#",     "#=#.#=#",
      "##..#=#",    "#....#=#",   ".....#=#",   "......#",
  };
  unsigned y;
  unsigned x;

  if (!ui_mouse_sessione || !ui_mouse_visto || !ui_mouse_acceso || ui_transparent || ui_fb == NULL) {
    return;
  }
  for (y = 0; y < sizeof(disegno) / sizeof(disegno[0]); y++) {
    for (x = 0; disegno[y][x] != '\0'; x++) {
      int px = ui_mouse_x + (int)x;
      int py = ui_mouse_y + (int)y;
      if (disegno[y][x] == '.' || px < 0 || py < 0 || px >= ui_fb_w || py >= ui_fb_h) continue;
      ui_draw_rect(px, py, 1, 1, disegno[y][x] == '#' ? BG_COLOR : FG_COLOR, 1);
    }
  }
}

























// Do key press/releases on the main loop
void ui_check_key(void) {
  static long process_ui_key[PENDING_UI_KEY_SIZE];
  static int process_ui_key_pressed[PENDING_UI_KEY_SIZE];

  if (!ui_enabled) {
    return;
  }

  // Process ui key event queue
  // Don't hold on to the lock while we call ui handlers.  It causes
  // locking problems with dispmanx calls. Take a copy, then process
  // outside the queue lock.
  circle_lock_acquire();
  int process_index = 0;
  while (pending_ui_key_head != pending_ui_key_tail &&
         process_index < PENDING_UI_KEY_SIZE) {
    int i = pending_ui_key_head & PENDING_UI_KEY_MASK;
    process_ui_key[process_index] = pending_ui_key[i];
    process_ui_key_pressed[process_index] = pending_ui_key_pressed[i];
    process_index++;
    pending_ui_key_head++;
  }
  circle_lock_release();

  // Now process the ui keys
  for (int i=0;i<process_index;i++) {
    if (process_ui_key_pressed[i]) {
      ui_key_pressed(process_ui_key[i]);
    } else {
      ui_key_released(process_ui_key[i]);
    }
  }

  if (process_index > 0 && ui_dopo_i_tasti != NULL) {
    ui_dopo_i_tasti();
  }
  ui_check_mouse();
  {

    extern void menu_before_render(void);
    menu_before_render();
  }

  // Ui action frame tick
  ui_action_frame();
}






static volatile int modo_sicuro_chiesto = 0;

void emu_modo_sicuro_interrupt(void) {
  modo_sicuro_chiesto = 1;
}

void ui_handle_toggle_or_quick_func() {
  if (modo_sicuro_chiesto) {
    modo_sicuro_chiesto = 0;
    menu_modo_sicuro();
    return;
  }
  // This ensures we transition from emulator to ui only after we've
  // submitted key events and let the emulator process them. Otherwise,
  // we can leave keys in a down state unintentionally. Needs to be set
  // to 2 to ensure we dequeue, then let the emulator process those events.
  if (ui_toggle_pending) {
    ui_toggle_pending--;
    if (ui_toggle_pending == 0) {
      // Even when we are entering the menu, we can't assume there aren't
      // already menus stacked on the root. This will ensure we always enter
      // and leave the menu in a known state (only root menu is on stack).
      ui_pop_all_and_toggle();
    }
  } else if (pending_emu_quick_func) {
    menu_quick_func(pending_emu_quick_func);
    pending_emu_quick_func = 0;
  }
}

void ui_add_all(struct menu_item *src, struct menu_item *dest) {
  assert(src != NULL);
  assert(src->type == FOLDER);
  assert(dest != NULL);
  assert(dest->type == FOLDER);
  struct menu_item *dest_prev = NULL;
  struct menu_item *dest_ptr = dest->first_child;
  struct menu_item *src_ptr = src->first_child;

  // Move to end of dest list
  while (dest_ptr != 0) {
    dest_prev = dest_ptr;
    dest_ptr = dest_ptr->next;
  }

  while (src_ptr != 0) {
    // Children must inheret these properties from new parent.
    src_ptr->menu_width = dest->menu_width;
    src_ptr->menu_height = dest->menu_height;
    src_ptr->menu_top = dest->menu_top;
    src_ptr->menu_left = dest->menu_left;
    src_ptr = src_ptr->next;
  }

  // Put src's children onto dest and cut link from src
  if (dest_prev == NULL) {
    dest->first_child = src->first_child;
  } else {
    dest_prev->next = src->first_child;
  }
  src->first_child = NULL;
}

static char *get_button_display_str(struct menu_item *node) {
  if (node->prefer_str || strlen(node->displayed_value) > 0) {
    return node->displayed_value;
  } else {
    // Turn value into string as fallback
    if (node->map_value_func) {
      sprintf(node->scratch, "%d", node->map_value_func(node->value));
    } else {
      sprintf(node->scratch, "%d", node->value);
    }
    return node->scratch;
  }
}

static void append(struct menu_item *folder, struct menu_item *new_item) {
  assert(folder != NULL);
  assert(folder->type == FOLDER);
  struct menu_item *prev = NULL;
  struct menu_item *ptr = folder->first_child;
  while (ptr != 0) {
    prev = ptr;
    ptr = ptr->next;
  }
  if (prev == NULL) {
    folder->first_child = new_item;
  } else {
    prev->next = new_item;
  }
}














int ui_memoria_finita = 0;

static struct menu_item *ui_new_item(struct menu_item *parent, const char *name,
                                     int id) {
  struct menu_item *new_item =
      (struct menu_item *)malloc(sizeof(struct menu_item));
  if (new_item == NULL) {
    ui_memoria_finita = 1;
    return NULL;
  }
  memset(new_item, 0, sizeof(struct menu_item));



  strncpy(new_item->name, name, MAX_MENU_STR - 1);
  new_item->name[MAX_MENU_STR - 1] = '\0';



  if (strlen(name) > MAX_MENU_STR - 1) {


    size_t lungo = strlen(name) + 1;
    new_item->nome_lungo = (char *)malloc(lungo);
    if (new_item->nome_lungo != NULL) {
      memcpy(new_item->nome_lungo, name, lungo);
    }
    int k = MAX_MENU_STR - 1;
    int inizio = k;
    while (inizio > 0 &&
           ((unsigned char)new_item->name[inizio - 1] & 0xC0) == 0x80) {
      inizio--;
    }
    if (inizio > 0) {
      unsigned char c = (unsigned char)new_item->name[inizio - 1];
      int lungo = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 :
                  (c & 0xF0) == 0xE0 ? 3 : 4;
      if (k - (inizio - 1) < lungo) {
        new_item->name[inizio - 1] = '\0';
      }
    }
  }
  new_item->id = id;

  // Inherit parent dimensions
  new_item->menu_width = parent->menu_width;
  new_item->menu_height = parent->menu_height;
  new_item->menu_top = parent->menu_top;
  new_item->menu_left = parent->menu_left;
  return new_item;
}

struct menu_item *ui_menu_add_toggle(int id, struct menu_item *folder,
                                     char *name, int initial_state) {
  struct menu_item *new_item = ui_new_item(folder, name, id);
  if (new_item == NULL) return NULL;
  new_item->type = TOGGLE;
  new_item->value = initial_state;
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_toggle_labels(int id, struct menu_item *folder,
                                     char *name, int initial_state,
                                     char *custom_0, char *custom_1) {
  struct menu_item *new_item =
     ui_menu_add_toggle(id, folder, name, initial_state);
  if (new_item == NULL) return NULL;
  strcpy(new_item->custom_toggle_label[0], custom_0);
  strcpy(new_item->custom_toggle_label[1], custom_1);
  return new_item;
}

struct menu_item *ui_menu_add_checkbox(int id, struct menu_item *folder,
                                       char *name, int initial_state) {
  struct menu_item *new_item = ui_new_item(folder, name, id);
  if (new_item == NULL) return NULL;
  new_item->type = CHECKBOX;
  new_item->value = initial_state;
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_multiple_choice(int id, struct menu_item *folder,
                                              char *name) {
  struct menu_item *new_item = ui_new_item(folder, name, id);
  if (new_item == NULL) return NULL;
  new_item->type = MULTIPLE_CHOICE;
  new_item->num_choices = 0;
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_button(int id, struct menu_item *folder,
                                     const char *name) {
  return ui_menu_add_button_with_value(id, folder, name, 0, " ", " ");
}

struct menu_item *ui_menu_add_button_with_value(int id,
                                                struct menu_item *folder,
                                                const char *name, int value,
                                                const char *str_value,
                                                const char *displayed_value) {
  struct menu_item *new_item = ui_new_item(folder, name, id);
  if (new_item == NULL) return NULL;
  new_item->type = BUTTON;
  new_item->value = value;
  strncpy(new_item->str_value, str_value, MAX_STR_VAL_LEN - 1);
  new_item->str_value[MAX_STR_VAL_LEN - 1] = '\0';
  strncpy(new_item->displayed_value, displayed_value, MAX_DSP_VAL_LEN - 1);
  new_item->displayed_value[MAX_DSP_VAL_LEN - 1] = '\0';
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_range(int id, struct menu_item *folder,
                                    char *name, int min, int max, int step,
                                    int initial_value) {
  struct menu_item *new_item = ui_new_item(folder, name, id);
  if (new_item == NULL) return NULL;
  new_item->type = RANGE;
  new_item->min = min;
  new_item->max = max;
  new_item->step = step;
  new_item->ministep = 1;
  new_item->divisor = 1;
  new_item->value = initial_value;
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_folder(struct menu_item *folder, char *name) {
  struct menu_item *new_item = ui_new_item(folder, name, MENU_ID_DO_NOTHING);
  if (new_item == NULL) return NULL;
  new_item->type = FOLDER;
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_divider(struct menu_item *folder) {
  struct menu_item *new_item = ui_new_item(folder, "", MENU_ID_DO_NOTHING);
  if (new_item == NULL) return NULL;
  new_item->type = DIVIDER;
  append(folder, new_item);
  return new_item;
}

static struct menu_item *ui_menu_add_read_only(struct menu_item *folder,
                                               const char *name,
                                               menu_item_type type) {
  struct menu_item *new_item = ui_new_item(folder, name, MENU_ID_DO_NOTHING);
  if (new_item == NULL) return NULL;
  new_item->type = type;
  new_item->disabled = 1;
  append(folder, new_item);
  return new_item;
}

struct menu_item *ui_menu_add_read_only_heading(struct menu_item *folder,
                                                const char *name) {
  return ui_menu_add_read_only(folder, name, READ_ONLY_HEADING);
}

struct menu_item *ui_menu_add_text_field(int id, struct menu_item *folder,
                                         char *name, char *value_str) {
  return ui_menu_add_text_field_limit(id, folder, name, value_str,
                                      MAX_FN_NAME);
}

struct menu_item *ui_menu_add_text_field_limit(int id, struct menu_item *folder,
                                               char *name, char *value_str,
                                               int max_length) {
  struct menu_item *new_item = ui_new_item(folder, name, id);
  if (new_item == NULL) return NULL;
  new_item->type = TEXTFIELD;
  new_item->max_length = max_length < MAX_STR_VAL_LEN - 1
                             ? max_length : MAX_STR_VAL_LEN - 1;
  strncpy(new_item->str_value, value_str, new_item->max_length);
  new_item->str_value[new_item->max_length] = '\0';
  new_item->value = strlen(new_item->str_value);
  append(folder, new_item);
  return new_item;
}










static void ui_campo_finestra(struct menu_item *node, int fuoco, int *primo,
                              int *quanti, int *x) {
  int len = strlen(node->str_value);
  int sx = node->menu_left + ui_text_width(node->name) + 8 +
           node->textfield_gap * 8;
  int dx = node->menu_left + node->menu_width;
  int caselle = (dx - sx) / 8;
  int c = node->value;
  int i = 0;
  int n, usate;

  if (caselle < 2) {
    caselle = 2;
  }
  if (c < 0) {
    c = 0;
  }
  if (c > len) {
    c = len;
  }






  if (!fuoco) {
    n = len < caselle ? len : caselle;
    if (n > (int)sizeof(node->scratch) - 1) {
      n = (int)sizeof(node->scratch) - 1;
    }
    *primo = 0;
    *quanti = n;
    *x = node->textfield_right_aligned ? dx - n * 8 : sx;
    return;
  }
  if (len >= caselle) {
    i = c - (caselle - 1);
    if (i < 0) {
      i = 0;
    }
  }
  n = len - i;
  if (n > caselle) {
    n = caselle;
  }
  if (n > (int)sizeof(node->scratch) - 1) {
    n = (int)sizeof(node->scratch) - 1;
  }
  usate = n;
  if (c - i >= n) {
    usate = c - i + 1;
  }
  *primo = i;
  *quanti = n;
  *x = node->textfield_right_aligned ? dx - usate * 8 : sx;
}


































static void ui_render_children(struct menu_item *node,
                               int stack_index, int *index, int indent,
                               int row_pitch) {
  while (node != NULL) {
    node->render_index = *index;

    int colour = node->disabled ? DISABLED_COLOR : FG_COLOR;
    if (node->type == READ_ONLY_HEADING) {
      colour = FG_COLOR;
    } else if (node->type == READ_ONLY_DESCRIPTION) {
      colour = READ_ONLY_DESCRIPTION_COLOR;
    }

    // Render a row
    if (*index >= menu_window_top[stack_index] &&
        *index < menu_window_bottom[stack_index]) {

      int y = (*index - menu_window_top[stack_index]) * row_pitch +
              node->menu_top;
      if (*index == menu_cursor[stack_index]) {
        ui_draw_rect(node->menu_left, y, node->menu_width, 8, HILITE_COLOR, 1);
        menu_cursor_item[stack_index] = node;
      }

      // Special symbol drawn on left edge
      if (node->symbol) {
          ui_draw_char_raw(node->symbol,
              node->menu_left+indent*8, y, colour, NULL, 0, 1);
      }

      // Sometimes, we only want to render the current item. Like when we
      // are adjusting things that affect video and we want to see the display
      // underneath the menu while we are making changes.
      if (!ui_render_current_item_only ||
          *index == menu_cursor[stack_index]) {

        char scorre[(MAX_MENU_STR - 1) * 4 + 1];
        if (node->name_petscii && raw_video_font != NULL) {

          ui_draw_text_petscii(node->name,
                               node->menu_left + (indent + 1) * 8, y, colour);
        } else {
          ui_draw_text(testo_della_voce(node,
                                        *index == menu_cursor[stack_index] &&
                                            stack_index == current_menu,
                                        indent, scorre, sizeof(scorre)),
             node->menu_left + (indent + 1) * 8, y, colour);
        }

        if (node->type == READ_ONLY_HEADING &&
          node->displayed_value[0] != '\0') {
          ui_draw_text(node->displayed_value,
                 node->menu_left + node->menu_width -
                   ui_text_width(node->displayed_value),
                 y, colour);
        } else if (node->type == FOLDER) {
          if (node->is_expanded)
            ui_draw_text("-", node->menu_left + (indent)*8, y, colour);
          else
            ui_draw_text("+", node->menu_left + (indent)*8, y, colour);
        } else if (node->type == TOGGLE) {
          if (node->value) {
            if (node->custom_toggle_label[1][0] == '\0') {
               ui_draw_text("On",
                         node->menu_left + node->menu_width -
                         ui_text_width("On"), y, colour);
            } else {
               ui_draw_text(node->custom_toggle_label[1],
                         node->menu_left + node->menu_width -
                         ui_text_width(node->custom_toggle_label[1]), y,
                                       colour);
            }
          } else {
            if (node->custom_toggle_label[0][0] == '\0') {
               ui_draw_text("Off", node->menu_left + node->menu_width -
                         ui_text_width("Off"), y, colour);
            } else {
               ui_draw_text(node->custom_toggle_label[0],
                         node->menu_left + node->menu_width -
                         ui_text_width(node->custom_toggle_label[0]), y,
                                       colour);
            }
          }
        } else if (node->type == CHECKBOX) {
          if (node->value)
            ui_draw_text("True", node->menu_left + node->menu_width -
                                     ui_text_width("True"),
                         y, colour);
          else
            ui_draw_text("False", node->menu_left + node->menu_width -
                                      ui_text_width("False"),
                         y, colour);
        } else if (node->type == RANGE) {
          if (node->divisor == 1) {
             sprintf(node->scratch, "%d", node->value);
          } else {
             // TODO: Don't assume 3 decimal places. Use divisor.
             sprintf(node->scratch, "%.3f",
                (float)node->value / (float)node->divisor);
          }
          ui_draw_text(node->scratch, node->menu_left + node->menu_width -
                                          ui_text_width(node->scratch),
                       y, colour);
        } else if (node->type == MULTIPLE_CHOICE) {
          ui_draw_text(node->choices[node->value],
                       node->menu_left + node->menu_width -
                           ui_text_width(node->choices[node->value]),
                       y, colour);
        } else if (node->type == DIVIDER) {
          ui_draw_rect(node->menu_left, y + 3, node->menu_width, 2, BORDER_COLOR, 1);
        } else if (node->type == BUTTON) {
          char *dsp_string = get_button_display_str(node);
          ui_draw_text(dsp_string, node->menu_left + node->menu_width -
                                       ui_text_width(dsp_string),
                       y, colour);
        } else if (node->type == TEXTFIELD) {
          int primo, quanti, value_x;

          int fuoco = (*index == menu_cursor[stack_index]) &&
                      (stack_index == current_menu);
          ui_campo_finestra(node, fuoco, &primo, &quanti, &value_x);
          if (node->textfield_masked) {
            memset(node->scratch, '*', quanti);
          } else {
            memcpy(node->scratch, node->str_value + primo, quanti);
          }
          node->scratch[quanti] = '\0';
          if (fuoco) {
            // draw cursor underneath text
            ui_draw_rect(value_x + (node->value - primo) * 8,
                         y, 8, 8, BORDER_COLOR, 1);
          }
          ui_draw_text(node->scratch, value_x, y, colour);
        }
      }
    }

    *index = *index + 1;
    if (node->type == FOLDER && node->is_expanded &&
        node->first_child != NULL) {
      ui_render_children(node->first_child, stack_index, index, indent + 1,
                         row_pitch);
    }
    node = node->next;
  }
}

// Make the UI layer fully transparent in preparation for an OSD to
// be displayed.
void ui_make_transparent(void) {
  memset(ui_fb, TRANSPARENT_COLOR, ui_fb_h * ui_fb_pitch);
}

static void ui_draw_shadow_text(const char* txt, int *x, int *y, int col) {
  ui_draw_text(txt, *x+1, *y, 0);
  ui_draw_text(txt, *x-1, *y, 0);
  ui_draw_text(txt, *x, *y+1, 0);
  ui_draw_text(txt, *x, *y-1, 0);
  ui_draw_text(txt, *x, *y, col);
  *x = *x + strlen(txt) *8;
}







static const uint8_t ui_frecce_scorrimento[2][8] = {
  {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18},
  {0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18},
};

static void ui_disegna_freccia(int quale, int x, int y, int colore) {
  int r, c;
  if (x < 0 || y < 0 || x > ui_fb_w - 8 || y > ui_fb_h - 8) {
    return;
  }
  for (r = 0; r < 8; r++) {
    uint8_t riga = ui_frecce_scorrimento[quale][r];
    for (c = 0; c < 8; c++) {
      if (riga & (0x80 >> c)) {
        ui_fb[x + c + (y + r) * ui_fb_pitch] = colore;
      }
    }
  }
}

static void ui_render_scroll_indicators(int menu_stack_index, int total_rows) {
  struct menu_item *root = &menu_roots[menu_stack_index];
  int window_top = menu_window_top[menu_stack_index];
  int window_bottom = menu_window_bottom[menu_stack_index];
  int prima = root->scroll_text_first_row;
  int fine = root->scroll_text_end_row;
  int first_text_row;
  int text_end;
  int marker_x;


  if (prima < 0 && root->sub_id == MENU_SUB_TEXT_CONTENTS) {
    prima = 2;
    fine = total_rows;
  }
  if (prima < 0) {
    return;
  }
  first_text_row = prima > window_top ? prima : window_top;
  text_end = fine < window_bottom ? fine : window_bottom;
  if (text_end > total_rows) {
    text_end = total_rows;
  }
  if (first_text_row >= text_end) {
    return;
  }
  marker_x = root->menu_left + root->menu_width - 8;
  if (window_top > 0) {
    ui_disegna_freccia(0, marker_x,
                       root->menu_top + (first_text_row - window_top) *
                           ui_menu_row_pitch(ui_menu_row_gap),
                       FG_COLOR);
  }
  if (window_bottom < total_rows) {
    ui_disegna_freccia(1, marker_x,
                       root->menu_top + (text_end - 1 - window_top) *
                           ui_menu_row_pitch(ui_menu_row_gap),
                       FG_COLOR);
  }




}

void ui_render_now(int menu_stack_index) {



  if (ui_fb == NULL) {
    return;
  }
  int index = 0;
  int indent = 0;

  if (menu_stack_index == -1) {
    menu_stack_index = current_menu;
    // When rendering only the top most menu, clear with transparent color
    memset(ui_fb, TRANSPARENT_COLOR, ui_fb_h * ui_fb_pitch);
  }

  struct menu_item *ptr = menu_roots[menu_stack_index].first_child;

  // background conditional upon mode
  if (!ui_transparent) {
     ui_draw_rect(ptr->menu_left, ptr->menu_top,
                  ptr->menu_width, ptr->menu_height,
                  BG_COLOR, 1);
  }

  // border
  ui_draw_rect(ptr->menu_left - 1, ptr->menu_top - 1, ptr->menu_width + 2,
               ptr->menu_height + 2, BORDER_COLOR, 0);

  // menu text
  ui_render_children(ptr, menu_stack_index, &index, indent,
                     ui_menu_row_pitch(ui_menu_row_gap));

  max_index[menu_stack_index] = index;
  ui_render_scroll_indicators(menu_stack_index, index);

  if (menu_cursor[menu_stack_index] >= max_index[menu_stack_index]) {
    menu_cursor[menu_stack_index] = max_index[menu_stack_index] - 1;
    cursor_pos_updated();
  }

  // Reveal dimensions in top left corner
  if (ui_transparent && ui_transparent_layer >= 0) {
    char str1[32];
    char str2[32];
    int dpx, dpy, fbw, fbh, dw, dh, sw, sh;

    // We're drawing into the UI layer so get it's fb dims.
    circle_get_fbl_dimensions(FB_LAYER_UI,
                              &dpx, &dpy,
                              &fbw, &fbh,
                              &sw, &sh,
                              &dw, &dh);

    // We can use the 1st display canvas info because our UI layer
    // mirrors it's dimensions all the time.
    int cx = canvas_state[VIC_INDEX].left + sw / 2 - 18 * 8 / 2;
    int cy = canvas_state[VIC_INDEX].top + sh / 2 - 7 * 10 / 2;

    // Now get info about the layer we are djusting
    circle_get_fbl_dimensions(ui_transparent_layer,
                              &dpx, &dpy,
                              &fbw, &fbh,
                              &sw, &sh,
                              &dw, &dh);

    int qx = cx;
    int qy = cy;

    sprintf (str1,"Display: %dx%d", dpx, dpy);
    ui_draw_shadow_text(str1, &qx, &qy, 1);

    qx = cx; qy+=10;
    // Unscaled frame buffer
    sprintf (str1, "FB: %d x %d", sw, sh);
    ui_draw_shadow_text(str1, &qx, &qy, 1);
    qx = qx + 10;

    // Scaled frame buffer. Show green dimension if it is
    // an even multiple of the unscaled frame buffer.
    qx = cx; qy+=10;
    sprintf (str1, "%d", dw);
    sprintf (str2, "%d", dh);
    ui_draw_shadow_text("SFB:", &qx, &qy, 1);
    qx = qx + 8;
    ui_draw_shadow_text(str1, &qx, &qy, dw % sw == 0 ? 5 : 1);
    ui_draw_shadow_text("x", &qx, &qy, 1);
    ui_draw_shadow_text(str2, &qx, &qy, dh % sh == 0 ? 5 : 1);
    qx = qx + 8;
    if (dw % sw == 0) {
       sprintf (str1, "x%d,", dw/sw);
       ui_draw_shadow_text(str1, &qx, &qy, 5);
    } else {
       ui_draw_shadow_text("*", &qx, &qy, 1);
    }
    if (dh % sh == 0) {
       sprintf (str1, "x%d", dh/sh);
       ui_draw_shadow_text(str1, &qx, &qy, 5);
    } else {
       ui_draw_shadow_text("*", &qx, &qy, 1);
    }

    qx = cx; qy+=20;
    ui_draw_shadow_text("Use , and . for", &qx, &qy, 1);
    qx = cx; qy+=10;
    ui_draw_shadow_text("-/+1 increments.", &qx, &qy, 1);
  }
}

// This function will traverse recursively all nodes in the node list
// starting at 'node'.  It fills in the render_index for each node as it
// goes and records the node that matches the current cursor index into
// menu_cursor_item.  No rendering is done here.  It used when
// we need to find the node matching the cursor position, taking into
// account all items that have been expanded/contracted.
static void ui_traverse_children(struct menu_item *node, int *index) {
  while (node != NULL) {
    node->render_index = *index;

    if (*index >= menu_window_top[current_menu] &&
        *index < menu_window_bottom[current_menu]) {
      if (*index == menu_cursor[current_menu]) {
        menu_cursor_item[current_menu] = node;
      }
    }

    *index = *index + 1;
    if (node->type == FOLDER && node->is_expanded &&
        node->first_child != NULL) {
      ui_traverse_children(node->first_child, index);
    }
    node = node->next;
  }
}

// This function will traverse recursively all child nodes in the current
// active menu. See ui_traverse_child for more details on when this
// is useful.  It also records the max index for the current menu taking
// into account all items that have been expanded/contracted.
static void ui_traverse(void) {
  int index = 0;
  struct menu_item *ptr = menu_roots[current_menu].first_child;

  ui_traverse_children(ptr, &index);

  max_index[current_menu] = index;

  if (menu_cursor[current_menu] >= max_index[current_menu]) {
    menu_cursor[current_menu] = max_index[current_menu] - 1;
    cursor_pos_updated();
  }
}

static void ui_clear_child_menu(struct menu_item *node) {
  if (node != NULL && node->type == FOLDER) {
    ui_clear_child_menu(node->first_child);
  }

  while (node != NULL) {
    struct menu_item *next = node->next;
    free(node->nome_lungo);
    free(node);
    node = next;
  }
}

static void ui_clear_menu(int menu_index) {
  ui_campo_toccato = NULL;
  ui_invio_reset();
  struct menu_item *node = &menu_roots[menu_index];
  ui_clear_child_menu(node->first_child);
  node->first_child = NULL;
}

struct menu_item *ui_pop_menu(void) {
  struct menu_item *menu_to_pop = &menu_roots[current_menu];
  ui_clear_menu(current_menu);
  current_menu--;

  if (menu_to_pop->on_popped_off) {
    // Notify pop happened (new_root/old_root)
    menu_to_pop->on_popped_off(&menu_roots[current_menu], menu_to_pop);
  }

  if (menu_roots[current_menu].on_popped_to) {
    // Notify pop happened (new_root/old_root)
    menu_to_pop->on_popped_to(&menu_roots[current_menu], menu_to_pop);
  }

  if (current_menu < 0) {
    printf("FATAL ERROR: tried to pop last menu\n");
    return NULL;
  }
  return &menu_roots[current_menu];
}

// left + border_w brings us to the left of gfx area
// then we center the menu width inside the gfx_w area
static int calc_root_menu_left() {
   return
       canvas_state[VIC_INDEX].left +
          canvas_state[VIC_INDEX].border_w +
             canvas_state[VIC_INDEX].gfx_w / 2 -
                menu_width_chars * 8 / 2;
}

// top + border_h brings us to the top of the gfx area
// then we center the menu height inside the gfx_h area
// BUT must take into account raster_skip since we don't
// double the height of the UI frame buffer like we do
// the main display.
static int calc_root_menu_top() {
   int raster_skip = canvas_state[VIC_INDEX].raster_skip;

   int ui_top = canvas_state[VIC_INDEX].first_displayed_line +
       canvas_state[VIC_INDEX].max_border_h / raster_skip;

   return ui_top + canvas_state[VIC_INDEX].gfx_h / 2 /
                      canvas_state[VIC_INDEX].raster_skip -
                         menu_height_chars * 8 / 2;
}

struct menu_item *ui_push_menu(int w_chars, int h_chars) {

  int menu_width = w_chars * 8;


  int menu_height = ui_menu_altezza(h_chars);
  if (w_chars < 0)
    menu_width = menu_width_chars * 8;

  current_menu++;
  if (current_menu >= NUM_MENU_ROOTS) {
    printf("FATAL ERROR: tried to push menu beyond NUM_MENU_ROOTS\n");
    return NULL;
  }
  ui_clear_menu(current_menu);

  // Client must set callback on each push so clear here.
  menu_roots[current_menu].scroll_text_first_row = -1;
  menu_roots[current_menu].scroll_text_end_row = -1;
  menu_roots[current_menu].on_value_changed = NULL;
  menu_roots[current_menu].on_popped_off = NULL;
  menu_roots[current_menu].on_popped_to = NULL;
  menu_roots[current_menu].left_right_listener_func = NULL;
  menu_roots[current_menu].key_listener_func = NULL;

  // Set dimensions
  menu_roots[current_menu].menu_width = menu_width;
  menu_roots[current_menu].menu_height = menu_height;
  menu_roots[current_menu].menu_rows = h_chars;

  if (w_chars == -2) {
    menu_roots[current_menu].menu_left = calc_root_menu_left();
  } else if (w_chars == -1) {
    // Inherit the root menu's left
    menu_roots[current_menu].menu_left = menu_roots[0].menu_left;
  } else {
    // Center this smaller menu inside the bounds of the root
    menu_roots[current_menu].menu_left =
       menu_roots[0].menu_left + (menu_roots[0].menu_width - menu_width) / 2;
  }

  if (h_chars == -2) {
    menu_roots[current_menu].menu_top = calc_root_menu_top();
  } else if (h_chars == -1) {
    // Inherit the root menu's top
    menu_roots[current_menu].menu_top = menu_roots[0].menu_top;
  } else {
    // Center this smaller menu inside the bounds of the root
    menu_roots[current_menu].menu_top =
       menu_roots[0].menu_top + (menu_roots[0].menu_height - menu_height) / 2;
  }

  menu_cursor[current_menu] = 0;
  menu_window_top[current_menu] = 0;


  menu_window_bottom[current_menu] =
      ui_menu_visible_rows(menu_height, ui_menu_row_gap);

  return &menu_roots[current_menu];
}

void ui_set_on_value_changed_callback(void (*callback)(struct menu_item *)) {
  on_value_changed = callback;
}

int emu_is_ui_activated(void) {
  return ui_enabled;
}

static struct menu_item *ui_push_dialog_header(int is_error) {
  struct menu_item *root = ui_push_menu(30, 4);
  if (is_error) {
    ui_menu_add_button(MENU_ERROR_DIALOG, root, "Error");
  } else {
    ui_menu_add_button(MENU_INFO_DIALOG, root, "Info");
  }
  ui_menu_add_divider(root);
  return root;
}

// Attach this callback to any OSD dialog
void glob_osd_popped(struct menu_item *new_root,
                     struct menu_item *old_root) {
  ui_disable_osd();
}




int ui_osd_attivo(void) {
  return osd_active;
}



struct menu_item *ui_menu_in_cima(void) {
  return current_menu >= 0 ? &menu_roots[current_menu] : NULL;
}




static void a_capo_in_voci(char *testo, struct menu_item *root, int id) {
  char *riga = testo;
  char *dopo;
  while ((dopo = strchr(riga, '\n')) != NULL) {
    *dopo = '\0';
    ui_menu_add_button(id, root, riga);
    riga = dopo + 1;
  }
  ui_menu_add_button(id, root, riga);
}








static int ui_messaggio_prima_del_menu(const char *tipo, const char *format,
                                       va_list args) {
  char buffer[256];

  if (menu_roots[0].type == FOLDER) {
    return 0;
  }
  vsnprintf(buffer, sizeof(buffer), format, args);
  printf("[UI] %s prima del menu: %s\n", tipo, buffer);
  return 1;
}

void ui_error(const char *format, ...) {
  va_list prima;
  va_start(prima, format);
  if (ui_messaggio_prima_del_menu("errore", format, prima)) {
    va_end(prima);
    return;
  }
  va_end(prima);
  struct menu_item *root = ui_push_dialog_header(1);
  // Don't show layer info when we want to show error.
  ui_transparent_layer = 0;
  if (!ui_enabled) {
     // We were called without the UI being up. Make this an OSD.
     ui_enable_osd();
     root->on_popped_off = glob_osd_popped;
  }
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 255, format, args);
  va_end(args);
  a_capo_in_voci(buffer, root, MENU_ERROR_DIALOG);
  ui_render_single_frame();
}

void ui_info(const char *format, ...) {
  va_list prima;
  va_start(prima, format);
  if (ui_messaggio_prima_del_menu("avviso", format, prima)) {
    va_end(prima);
    return;
  }
  va_end(prima);
  struct menu_item *root = ui_push_dialog_header(0);
  // Don't show layer info when we want to show info.
  ui_transparent_layer = 0;
  if (!ui_enabled) {
     // We were called without the UI being up. Make this an OSD.
     ui_enable_osd();
     root->on_popped_off = glob_osd_popped;
  }
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 255, format, args);
  va_end(args);
  a_capo_in_voci(buffer, root, MENU_INFO_DIALOG);
  ui_render_single_frame();
}



#define CONF_LARGHEZZA 32
#define CONF_COLONNE (CONF_LARGHEZZA - 2)


#define CONF_MAX_RIGHE 20
#define CONF_MAX_ALTEZZA 24

void ui_confirm_wrapped_labels(char *title, const char *txt, int ok_value,
                               int ok_id, const char *ok_label,
                               const char *cancel_label) {




  char righe[CONF_MAX_RIGHE][CONF_COLONNE + 2];
  int n_righe = 0;
  char buf[512];
  char line[CONF_COLONNE + 2];
  strncpy(buf, txt, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  line[0] = '\0';

  int line_pos = 0;
  char* word = strtok(buf," ");
  while (word && n_righe < CONF_MAX_RIGHE) {
     int word_len = strlen(word);
     if (line_pos + word_len < CONF_COLONNE) {
        strcat(line, word);
        strcat(line, " ");
        line_pos += word_len + 1;
     } else {
        strcpy(righe[n_righe++], line);
        strncpy(line, word, CONF_COLONNE);
        line[CONF_COLONNE] = '\0';
        strcat(line, " ");
        line_pos = word_len + 1;
     }
     word = strtok(NULL," ");
  }
  if (strlen(line) > 0 && n_righe < CONF_MAX_RIGHE) {
      strcpy(righe[n_righe++], line);
  }




  int con_pulsanti = (ok_id >= 0);
  int altezza = 1   + 1   + n_righe + 1
                + (con_pulsanti ? 2 : 0);
  if (altezza > CONF_MAX_ALTEZZA) {
    altezza = CONF_MAX_ALTEZZA;
  }

  struct menu_item *root = ui_push_menu(CONF_LARGHEZZA, altezza);

  root->scroll_text_first_row = 2;
  root->scroll_text_end_row = 2 + n_righe;
  struct menu_item *child = ui_menu_add_read_only(root, title,
                                                   READ_ONLY_HEADING);

  ui_menu_add_divider(root);
  for (int i = 0; i < n_righe; i++) {
    child = ui_menu_add_read_only(root, righe[i], READ_ONLY_DESCRIPTION);
  }

  ui_menu_add_divider(root);
  if (con_pulsanti) {
    child = ui_menu_add_button(MENU_CONFIRM_OK, root, ok_label);
    child->value = ok_value;
    child->sub_id = ok_id;

    ui_menu_add_button(MENU_CONFIRM_CANCEL, root, cancel_label);
  }

  ui_menu_fit_height(20);
  ui_select_first_interactive_item();

  ui_render_single_frame();
}

void ui_confirm_wrapped(char *title, const char *txt, int ok_value, int ok_id) {
  ui_confirm_wrapped_labels(title, txt, ok_value, ok_id, "OK", "CANCEL");
}






int ui_keyboard_shift_active(void) {
  return keyboard_shift != 0;
}

void ui_keyboard_clear_shift(void) {
  keyboard_shift = 0;
}

void ui_menu_set_button_value_fitted(struct menu_item *item,
                                     const char *value, int indent) {
  size_t available;
  size_t value_len;
  size_t menu_chars;
  size_t label_end;
  int width;

  if (item == NULL || item->type != BUTTON) {
    return;
  }
  if (value == NULL) {
    value = "";
  }

  strncpy(item->str_value, value, MAX_STR_VAL_LEN - 1);
  item->str_value[MAX_STR_VAL_LEN - 1] = '\0';
  item->prefer_str = 1;


  width = item->menu_width > 0 ? item->menu_width
                               : menu_roots[current_menu].menu_width;
  menu_chars = width > 0 ? (size_t)width / 8U : 0U;
  label_end = (size_t)(indent >= 0 ? indent : 0) + 1U + strlen(item->name);
  available = menu_chars > label_end + 2U ? menu_chars - label_end - 2U : 0U;
  if (available >= MAX_DSP_VAL_LEN) {
    available = MAX_DSP_VAL_LEN - 1U;
  }

  value_len = strlen(item->str_value);
  if (value_len <= available) {
    memcpy(item->displayed_value, item->str_value, value_len + 1U);
  } else if (available >= 4U) {
    memcpy(item->displayed_value, item->str_value, available - 3U);
    memcpy(item->displayed_value + available - 3U, "...", 4U);
  } else {
    size_t dots = available < 3U ? available : 3U;
    memset(item->displayed_value, '.', dots);
    item->displayed_value[dots] = '\0';
  }
}


void ui_error_wrapped(const char *txt) {
  ui_confirm_wrapped_labels("Error", txt, 0, -1, "OK", "CANCEL");
}



void ui_confirm_wrapped_cancel_default(char *title, const char *txt,
                                       int ok_value, int ok_id) {
  struct menu_item *it;
  ui_confirm_wrapped_labels(title, txt, ok_value, ok_id, "OK", "CANCEL");
  if (ok_id < 0) {
    return;
  }
  for (it = menu_roots[current_menu].first_child; it != NULL; it = it->next) {
    if (it->id == MENU_CONFIRM_CANCEL) {
      it->value = ok_value;
      it->sub_id = ok_id;
    }
  }
  ui_action_corpo(ACTION_Down);
  ui_render_single_frame();
}

// These nav functions are really inefficient...but oh well.


void ui_page_down() {
  int menu_index = current_menu;
  int visible_rows = menu_window_bottom[current_menu] -
                     menu_window_top[current_menu];
  for (int n=0;n<visible_rows && current_menu == menu_index;n++) {
    ui_action(ACTION_Down);
  }
}

void ui_page_up() {
  int menu_index = current_menu;
  int visible_rows = menu_window_bottom[current_menu] -
                     menu_window_top[current_menu];
  for (int n=0;n<visible_rows && current_menu == menu_index;n++) {
    ui_action(ACTION_Up);
  }
}


















void ui_to_top() {
  while (menu_cursor[current_menu] != 0) {
    int prima = menu_cursor[current_menu];
    ui_action(ACTION_Up);
    if (menu_cursor[current_menu] == prima) break;
  }
}

void ui_to_bottom() {
  while (menu_cursor[current_menu] < max_index[current_menu] - 1) {
    int prima = menu_cursor[current_menu];
    ui_action(ACTION_Down);
    if (menu_cursor[current_menu] == prima) break;
  }
}

void ui_find_first(char letter) {

  int start_index = menu_cursor[current_menu];




  int mosse = 0;
  int tetto = max_index[current_menu] + 2;


  letter = tolower((unsigned char)letter);

  while(1) {
    int prima = menu_cursor[current_menu];

    if (mosse++ > tetto) break;

    // Move down or wrap around to the top if we hit the bottom.
    if (menu_cursor[current_menu] >= max_index[current_menu] - 1) {
       ui_to_top();
    } else {
       ui_action(ACTION_Down);
    }

    // Did we get back to where we started? Bail.
    if (menu_cursor[current_menu] == start_index) break;

    if (menu_cursor[current_menu] == prima) break;

    // We need to recompute max_index and the cursor after each move.
    ui_traverse();

    // Did this match our criteria? Bail.
    struct menu_item *cur = menu_cursor_item[current_menu];
    if (cur == NULL) break;
    char *name = cur->name;
    if (name[0] != '\0' && tolower((unsigned char)name[0]) == letter) break;
  }
}

// Meant to be called immediately after a menu push to position
// the cursor to a known location. Also useful after a call to
// ui_to_top() to do the same.
void ui_set_cur_pos(int pos) {



  ui_traverse();
  while(menu_cursor[current_menu] < pos &&
        menu_cursor[current_menu] < max_index[current_menu] - 1) {
    int prima = menu_cursor[current_menu];
    ui_action(ACTION_Down);

    // We need to recompute max_index and the cursor after each move.
    ui_traverse();


    if (menu_cursor[current_menu] == prima) break;
  }
}




void ui_focus_item(struct menu_item *item) {
  if (item == NULL) {
    return;
  }
  ui_traverse();
  if (item->render_index >= 0) {
    ui_to_top();
    ui_set_cur_pos(item->render_index);
  }
}


struct menu_item *ui_menu_root(void) {
  return current_menu >= 0 ? &menu_roots[0] : NULL;
}

struct menu_item* ui_find_item_by_id(struct menu_item *node, int id) {
  if (node == NULL) {
    return NULL;
  }

  while (node != NULL) {
    if (node->id == id) return node;
    if (node->type == FOLDER) {
       struct menu_item *found = ui_find_item_by_id(node->first_child, id);
       if (found) return found;
    }
    node = node->next;
  }

  return NULL;
}

void ui_enable_osd(void) {



  joy_alza_tutto();
  emux_alza_la_tastiera();
  osd_active = 1;
  ui_enabled = 1;

  if (!ui_mouse_sessione) {
    ui_mouse_sessione_apri();
  }
  ui_alt = 0;


  ui_stato_normale();
  ui_make_transparent();
  circle_present_fbl(FB_LAYER_MASK(FB_LAYER_UI), 1  );
  circle_show_fbl(FB_LAYER_UI);
}

void ui_disable_osd(void) {
  osd_active = 0;
  lettore_mostrato = 0;
  lettore_rimette_vic();
  // We don't set ui_enabled to 0 here. We rely on
  // pop and toggle to dismiss OSDs which does the
  // right thing.
  circle_hide_fbl(FB_LAYER_UI);
}

void ui_dismiss_osd_if_active(void) {
  if (osd_active) {
     ui_pop_all_and_toggle();
     ui_disable_osd();
  }
}

void ui_set_render_current_item_only(int v) {
  ui_render_current_item_only = v;
}






















static FILE *passi_f = NULL;
static int passi_rotto = 0;

static void passi_riga(const char *verbo, const char *cosa) {








  if (passi_rotto) {
    return;
  }
  if (passi_f == NULL) {







    unlink("/PASSI-PRIMA.TXT");
    link("/PASSI.TXT", "/PASSI-PRIMA.TXT");


    passi_f = fopen("/PASSI.TXT", "w");
    if (passi_f == NULL) {
      passi_rotto = 1;
      return;
    }
    fprintf(passi_f,
            "L'ULTIMA COSA CHE LA MACCHINA STAVA FACENDO.\n"
            "Se l'ultima riga dice \"PRIMA: X\" e non c'e' il suo\n"
            "\"FATTO: X\", e' li' che si e' fermata.\n"
            "Il giro di prima sta in PASSI-PRIMA.TXT.\n\n");
  }
  fprintf(passi_f, "%8lu ms  %s %s\n", circle_get_ticks() / 1000, verbo,
          cosa);
  fflush(passi_f);
  fsync(fileno(passi_f));
}

void passo_prima(const char *cosa) { passi_riga("PRIMA:", cosa); }
void passo_fatto(const char *cosa) { passi_riga("FATTO:", cosa); }
void passo_nota(const char *cosa)  { passi_riga("NOTA: ", cosa); }





extern const char *volatile bmc_core0_dove;
void passo_nota_c0(const char *cosa) {
  char riga[176];
  const char *c0 = bmc_core0_dove;
  snprintf(riga, sizeof riga, "%s  [core 0: %s]", cosa, c0 != NULL ? c0 : "?");
  passi_riga("NOTA: ", riga);
}




int emu_get_keyboard_shiftlock(void) { return emux_get_shiftlock(); }

void emu_quick_func_interrupt(int button_assignment) {
  pending_emu_quick_func = button_assignment;
}

// These will revert back to 0 when the user moves off the
// current item.
void ui_canvas_reveal_temp(int layer) {
  if (layer == FB_LAYER_VIC && vic_showing) {
    ui_transparent = 1;
    ui_transparent_layer = layer;
    ui_set_render_current_item_only(1);
  }
  else if (layer == FB_LAYER_VDC && vdc_showing) {
    ui_transparent = 1;
    ui_transparent_layer = layer;
    ui_set_render_current_item_only(1);
  }
}

void emu_exit(void) {
  // We should never get here.  If we do, it's probably
  // because essential roms are missing.  So display a message
  // to that effect.
  int i;
  uint8_t *fb;
  int fb_pitch;
  int fb_width = 320;
  int fb_height = 240;

  circle_alloc_fbl(FB_LAYER_VIC, 0 /* indexed */, &fb,
                      fb_width, fb_height, &fb_pitch);
  circle_clear_fbl(FB_LAYER_VIC);
  circle_show_fbl(FB_LAYER_VIC);

  video_font = (uint8_t *)&font8x8_basic;
  for (i = 0; i < 256; ++i) {
    video_font_translate[i] = (8 * (i & 0x7f));
  }

  int x = 0;
  int y = 3;
  switch (emux_machine_class) {
    case BMC64_MACHINE_CLASS_VIC20:
      ui_draw_text_buf("VIC20 (Vice)", x, y, 1, fb, fb_pitch, 1);
      break;
    case BMC64_MACHINE_CLASS_C64:
      ui_draw_text_buf("C64 (Vice)", x, y, 1, fb, fb_pitch, 1);
      break;
    case BMC64_MACHINE_CLASS_SCPU64:
      ui_draw_text_buf("SCPU64 (Vice)", x, y, 1, fb, fb_pitch, 1);
      break;
    case BMC64_MACHINE_CLASS_C128:
      ui_draw_text_buf("C128 (Vice)", x, y, 1, fb, fb_pitch, 1);
      break;
    case BMC64_MACHINE_CLASS_PLUS4:
      ui_draw_text_buf("PLUS4 (Vice)", x, y, 1, fb, fb_pitch, 1);
      break;
    case BMC64_MACHINE_CLASS_PLUS4EMU:
      ui_draw_text_buf("PLUS4 (Plus4Emu)", x, y, 1, fb, fb_pitch, 1);
      break;
    case BMC64_MACHINE_CLASS_PET:
      ui_draw_text_buf("PET (Vice)", x, y, 1, fb, fb_pitch, 1);
      break;
  }
  y += 8;
  ui_draw_text_buf("Emulator failed to start.", x, y, 1, fb, fb_pitch, 1);
  y += 8;
  ui_draw_text_buf("This most likely means you are missing", x, y, 1, fb,
                   fb_pitch, 1);
  y += 8;
  ui_draw_text_buf("ROM files. Or you have specified an", x, y, 1, fb,
                   fb_pitch, 1);
  y += 8;
  ui_draw_text_buf("invalid kernal, chargen or basic", x, y, 1, fb, fb_pitch, 1);
  y += 8;
  ui_draw_text_buf("ROM.  See the documentation.", x, y, 1, fb,
                   fb_pitch, 1);

  if (emux_machine_class != BMC64_MACHINE_CLASS_C64) {
     y += 16;
     ui_draw_text_buf("Hold Ctrl/Commodore + F7 for 5 seconds,", x, y, 1, fb,
                   fb_pitch, 1);
     y += 8;
     ui_draw_text_buf("then release F7 to reset back to C64.", x, y, 1, fb,
                   fb_pitch, 1);
  }

  circle_set_palette_fbl(FB_LAYER_VIC, 0, COLOR16(0, 0, 0));
  circle_set_palette_fbl(FB_LAYER_VIC, 1, COLOR16(255, 255, 255));
  circle_update_palette_fbl(FB_LAYER_VIC);
  circle_present_fbl(FB_LAYER_MASK(FB_LAYER_VIC), 0);
}

static void ui_update_children(struct menu_item *node,
                               int top, int left) {
  while (node != NULL) {
    node->menu_top = top;
    node->menu_left = left;

    if (node->type == FOLDER && node->first_child != NULL) {
      ui_update_children(node->first_child, top, left);
    }
    node = node->next;
  }
}







static void ui_aggiorna_misure(struct menu_item *node, int top, int left,
                               int width, int height) {
  while (node != NULL) {
    node->menu_top = top;
    node->menu_left = left;
    node->menu_width = width;
    node->menu_height = height;

    if (node->type == FOLDER && node->first_child != NULL) {
      ui_aggiorna_misure(node->first_child, top, left, width, height);
    }
    node = node->next;
  }
}

static void ui_reflow_menu_window(int menu_index) {
  int capacity = ui_menu_visible_rows(
      menu_roots[menu_index].menu_height, ui_menu_row_gap);
  int top = menu_window_top[menu_index];
  int max_top;

  if (capacity < 1) {
    capacity = 1;
  }
  max_top = max_index[menu_index] > capacity
                ? max_index[menu_index] - capacity
                : 0;
  if (top > max_top) {
    top = max_top;
  }
  if (menu_cursor[menu_index] < top) {
    top = menu_cursor[menu_index];
  } else if (menu_cursor[menu_index] >= top + capacity) {
    top = menu_cursor[menu_index] - capacity + 1;
  }
  if (top < 0) {
    top = 0;
  } else if (top > max_top) {
    top = max_top;
  }
  menu_window_top[menu_index] = top;
  menu_window_bottom[menu_index] = top + capacity;
}

static void ui_apply_menu_row_gap_layout(void) {
  int menu_index;

  if (current_menu < 0) {
    return;
  }

  for (menu_index = 0; menu_index <= current_menu; ++menu_index) {
    struct menu_item *root = &menu_roots[menu_index];
    int height = root->menu_rows <= 0
                     ? menu_height_chars * UI_MENU_GLYPH_HEIGHT
                     : ui_menu_altezza(root->menu_rows);

    root->menu_height = height;
    if (menu_index > 0) {
      root->menu_top = root->menu_rows <= 0
                           ? menu_roots[0].menu_top
                           : menu_roots[0].menu_top +
                                 (menu_roots[0].menu_height - height) / 2;
    }
    ui_aggiorna_misure(root->first_child, root->menu_top, root->menu_left,
                       root->menu_width, root->menu_height);
    ui_reflow_menu_window(menu_index);
  }
}














void ui_geometry_changed(int dpx, int dpy,
                         int fbw, int fbh,
                         int sw, int sh,
                         int dw, int dh) {

  // For the UI, we don't want to double the height like we do
  // with the actual display, so we take raster_skip into account
  // here.
  fbh = fbh / canvas_state[VIC_INDEX].raster_skip;

  // When the ui geometry changes, we need to update some menu
  // fields to match.
  if (fbw != ui_fb_w || fbh != ui_fb_h) {
     // Destroy old fb.
     if (ui_fb) {
        circle_free_fbl(FB_LAYER_UI);
     }

     lettore_mostrato = 0;
     lettore_rimette_vic();
     circle_alloc_fbl(FB_LAYER_UI, 0 /* indexed */, &ui_fb,
                      fbw, fbh, &ui_fb_pitch);
     circle_clear_fbl(FB_LAYER_UI);
    circle_set_palette32_fbl(FB_LAYER_UI, READ_ONLY_DESCRIPTION_COLOR,
              0xFFE0E0E0);
    circle_update_palette_fbl(FB_LAYER_UI);
     ui_fb_w = fbw;
     ui_fb_h = fbh;
   }
   menu_roots[0].menu_top = calc_root_menu_top();
   menu_roots[0].menu_left = calc_root_menu_left();
   ui_update_children(&menu_roots[0],
      menu_roots[0].menu_top, menu_roots[0].menu_left);
}
