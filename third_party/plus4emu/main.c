#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>
#include "plus4lib/plus4emu.h"
#include "../common/circle.h"
#include "../common/emux_api.h"
#include "../common/keycodes.h"
#include "../common/overlay.h"
#include "../common/demo.h"
#include "../common/menu.h"
#include "../common/kbd.h"

static Plus4VM            *vm = NULL;













static unsigned char p4_giu[128];








static char disco_nell_otto[MAX_STR_VAL_LEN] = "";





static char disco_nella_nove[MAX_STR_VAL_LEN] = "";

static void manda_tasto(int codice, int premuto) {
  if (codice < 0 || codice >= 128) {
    return;
  }
  p4_giu[codice] = premuto ? 1 : 0;
  Plus4VM_KeyboardEvent(vm, codice, premuto);
}
static Plus4VideoDecoder  *videoDecoder = NULL;

#define TEXT_LINE_LEN 80




#define P4_DRIVE_NIENTE 2










#define MAX_KEY_SYM 0x108
#define P4_SHIFT 15








#define TF_SHIFT_MIO     1
#define TF_SHIFT_LIBERO  8
#define TF_SHIFT_TOLTO  16
#define TF_ALTRA_RIGA   32
#define TF_SHIFT_FERMO  64
#define TF_TENUTE (TF_SHIFT_MIO | TF_SHIFT_LIBERO | TF_SHIFT_TOLTO | \
                   TF_ALTRA_RIGA | TF_SHIFT_FERMO)



static int default_bmc64_keycode_to_plus4emu(long keycode);


static int keysymToP4Code[MAX_KEY_SYM];
static int keysymBandiere[MAX_KEY_SYM];







static int keysymP4Shiftato[MAX_KEY_SYM];
static int keysymBandiereShiftate[MAX_KEY_SYM];









static int p4_dato[MAX_KEY_SYM];
static int bandiere_date[MAX_KEY_SYM];

static int shift_fisico = 0;



static int shift_fermo = 0;


static void tastiera_azzera_stato(void) {
  int c;
  for (c = 0; c < MAX_KEY_SYM; c++) {
    p4_dato[c] = -1;
    bandiere_date[c] = 0;
  }
  shift_fisico = 0;
  shift_fermo = 0;
}




static void mappa_azzera(void) {
  int i;
  for (i = 0; i < MAX_KEY_SYM; i++) {
    keysymToP4Code[i] = default_bmc64_keycode_to_plus4emu(i);
    keysymBandiere[i] = TF_SHIFT_LIBERO;
    keysymP4Shiftato[i] = -1;
    keysymBandiereShiftate[i] = 0;
  }
}



static int mappa_riga(char *line) {
  char *sym_name;
  char *pezzo;
  int p4code;
  int bandiere;
  int keysym;





  pezzo = strchr(line, '#');
  if (pezzo != NULL) {
    *pezzo = '\0';
  }

  sym_name = strtok(line, " \t\r\n");
  if (sym_name == NULL) {
    return 0;
  }
  pezzo = strtok(NULL, " \t\r\n");
  if (pezzo == NULL) {
    return 0;
  }
  p4code = atoi(pezzo);
  pezzo = strtok(NULL, " \t\r\n");
  bandiere = (pezzo != NULL) ? atoi(pezzo) : 0;






  bandiere &= TF_TENUTE;
  if (bandiere == 0) {
    bandiere = TF_SHIFT_LIBERO;
  }

  keysym = (int)kbd_arch_keyname_to_keynum(sym_name);
  if (keysym <= 0 || keysym >= MAX_KEY_SYM) {
    printf("WARNING: Ignoring keysym %s\n", sym_name);
    return 0;
  }
  if (p4code < 0 || p4code >= 128) {
    printf("WARNING: keysym %s, codice %d fuori dalla matrice\n",
           sym_name, p4code);
    return 0;
  }




  if ((keysymBandiere[keysym] & TF_ALTRA_RIGA) &&
      keysymP4Shiftato[keysym] < 0) {
    keysymP4Shiftato[keysym] = p4code;
    keysymBandiereShiftate[keysym] = bandiere;
  } else {
    keysymToP4Code[keysym] = p4code;
    keysymBandiere[keysym] = bandiere;
    keysymP4Shiftato[keysym] = -1;
    keysymBandiereShiftate[keysym] = 0;
  }
  return 1;
}










static void rifai_lo_shift(void) {
  int mio = 0;
  int tolto = 0;
  int i;

  for (i = 0; i < MAX_KEY_SYM; i++) {
    if (p4_dato[i] < 0) {
      continue;
    }
    if (bandiere_date[i] & TF_SHIFT_MIO) {
      mio = 1;
    }
    if (bandiere_date[i] & TF_SHIFT_TOLTO) {
      tolto = 1;
    }
  }
  manda_tasto(P4_SHIFT,
              (!tolto && (shift_fisico || mio)) || shift_fermo);
}


static void tasto_evento(int keysym, int premuto) {
  int p4code;
  int bandiere;

  if (keysym < 0 || keysym >= MAX_KEY_SYM) {
    return;
  }

  if (premuto) {


    if (shift_fisico && keysymP4Shiftato[keysym] >= 0) {
      p4code = keysymP4Shiftato[keysym];
      bandiere = keysymBandiereShiftate[keysym];
    } else {
      p4code = keysymToP4Code[keysym];
      bandiere = keysymBandiere[keysym];
    }
  } else {

    p4code = p4_dato[keysym];
    bandiere = bandiere_date[keysym];
    p4_dato[keysym] = -1;
    bandiere_date[keysym] = 0;
  }

  if (p4code < 0) {
    return;
  }




  if (bandiere & TF_SHIFT_FERMO) {
    if (premuto) {
      shift_fermo = !shift_fermo;
      rifai_lo_shift();
    }
    return;
  }

  if (p4code == P4_SHIFT) {
    shift_fisico = premuto;
    if (premuto) {
      p4_dato[keysym] = p4code;
      bandiere_date[keysym] = bandiere;
    }
    rifai_lo_shift();
    return;
  }

  if (premuto) {






    if (p4_dato[keysym] >= 0 && p4_dato[keysym] != p4code) {
      manda_tasto(p4_dato[keysym], 0);
    }
    p4_dato[keysym] = p4code;
    bandiere_date[keysym] = bandiere;



    rifai_lo_shift();
    manda_tasto(p4code, 1);
  } else {


    manda_tasto(p4code, 0);
    rifai_lo_shift();
  }
}


// Global state variables
static uint8_t *fb_buf;
static int fb_pitch;





static const unsigned char p4_tavolozza[128 * 3] = {
  0x00, 0x00, 0x00, 0x27, 0x27, 0x27, 0x60, 0x0F, 0x10, 0x00, 0x40, 0x3F,
  0x56, 0x04, 0x66, 0x00, 0x4B, 0x00, 0x1A, 0x1A, 0x8C, 0x35, 0x34, 0x00,
  0x53, 0x1E, 0x00, 0x47, 0x28, 0x00, 0x18, 0x43, 0x00, 0x61, 0x08, 0x34,
  0x00, 0x47, 0x1E, 0x04, 0x29, 0x7A, 0x28, 0x13, 0x8F, 0x08, 0x48, 0x00,
  0x00, 0x00, 0x00, 0x37, 0x37, 0x37, 0x6F, 0x1E, 0x1F, 0x00, 0x4F, 0x4E,
  0x65, 0x13, 0x75, 0x04, 0x5A, 0x05, 0x2A, 0x2A, 0x9C, 0x44, 0x44, 0x00,
  0x63, 0x2E, 0x00, 0x56, 0x38, 0x00, 0x28, 0x52, 0x00, 0x70, 0x17, 0x43,
  0x00, 0x56, 0x2E, 0x14, 0x38, 0x8A, 0x38, 0x22, 0x9E, 0x17, 0x58, 0x00,
  0x00, 0x00, 0x00, 0x43, 0x43, 0x43, 0x7C, 0x2B, 0x2C, 0x0B, 0x5C, 0x5B,
  0x72, 0x20, 0x82, 0x11, 0x67, 0x11, 0x36, 0x37, 0xA8, 0x51, 0x50, 0x00,
  0x6F, 0x3A, 0x00, 0x63, 0x44, 0x00, 0x34, 0x5F, 0x00, 0x7D, 0x24, 0x50,
  0x0A, 0x63, 0x3A, 0x21, 0x45, 0x96, 0x45, 0x2F, 0xAB, 0x24, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x8D, 0x3C, 0x3D, 0x1C, 0x6D, 0x6C,
  0x83, 0x31, 0x93, 0x22, 0x78, 0x22, 0x47, 0x48, 0xB9, 0x62, 0x61, 0x00,
  0x80, 0x4B, 0x11, 0x74, 0x55, 0x00, 0x45, 0x70, 0x00, 0x8E, 0x35, 0x61,
  0x1B, 0x74, 0x4B, 0x32, 0x56, 0xA7, 0x56, 0x40, 0xBC, 0x35, 0x76, 0x00,
  0x00, 0x00, 0x00, 0x79, 0x79, 0x79, 0xB2, 0x61, 0x62, 0x40, 0x91, 0x90,
  0xA7, 0x55, 0xB7, 0x46, 0x9D, 0x47, 0x6C, 0x6C, 0xDE, 0x86, 0x86, 0x14,
  0xA5, 0x70, 0x35, 0x99, 0x7A, 0x22, 0x6A, 0x94, 0x15, 0xB3, 0x59, 0x86,
  0x3F, 0x98, 0x70, 0x56, 0x7B, 0xCC, 0x7A, 0x64, 0xE1, 0x59, 0x9A, 0x22,
  0x00, 0x00, 0x00, 0xA9, 0xA9, 0xA9, 0xE1, 0x90, 0x91, 0x70, 0xC1, 0xC0,
  0xD7, 0x85, 0xE7, 0x76, 0xCC, 0x76, 0x9C, 0x9C, 0xFF, 0xB6, 0xB6, 0x44,
  0xD5, 0xA0, 0x65, 0xC8, 0xA9, 0x52, 0x9A, 0xC4, 0x45, 0xE2, 0x89, 0xB5,
  0x6F, 0xC8, 0xA0, 0x86, 0xAA, 0xFB, 0xAA, 0x94, 0xFF, 0x89, 0xCA, 0x52,
  0x00, 0x00, 0x00, 0xC7, 0xC7, 0xC7, 0xFF, 0xAF, 0xB0, 0x8F, 0xE0, 0xDF,
  0xF6, 0xA3, 0xFF, 0x94, 0xEB, 0x95, 0xBA, 0xBA, 0xFF, 0xD4, 0xD4, 0x62,
  0xF3, 0xBE, 0x83, 0xE7, 0xC8, 0x70, 0xB8, 0xE2, 0x63, 0xFF, 0xA7, 0xD4,
  0x8D, 0xE7, 0xBE, 0xA4, 0xC9, 0xFF, 0xC8, 0xB3, 0xFF, 0xA8, 0xE8, 0x70,
  0x00, 0x00, 0x00, 0xFA, 0xFA, 0xFA, 0xFF, 0xE2, 0xE3, 0xC2, 0xFF, 0xFF,
  0xFF, 0xD6, 0xFF, 0xC7, 0xFF, 0xC8, 0xED, 0xED, 0xFF, 0xFF, 0xFF, 0x95,
  0xFF, 0xF1, 0xB6, 0xFF, 0xFB, 0xA3, 0xEB, 0xFF, 0x96, 0xFF, 0xDA, 0xFF,
  0xC0, 0xFF, 0xF1, 0xD7, 0xFC, 0xFF, 0xFB, 0xE6, 0xFF, 0xDB, 0xFF, 0xA3,
};


static uint16_t riga16[512];

static uint8_t *mappa565 = NULL;

static uint16_t p4_rgb565(int r, int g, int b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}


static void p4_prepara_tavolozza(void) {
  int i;

  for (i = 0; i < 128; i++) {
    circle_set_palette_fbl(FB_LAYER_VIC, (uint8_t)i,
        p4_rgb565(p4_tavolozza[i * 3], p4_tavolozza[i * 3 + 1],
                  p4_tavolozza[i * 3 + 2]));
  }
  circle_update_palette_fbl(FB_LAYER_VIC);

  if (mappa565 != NULL) {
    return;
  }
  mappa565 = (uint8_t *)malloc(65536);
  if (mappa565 == NULL) {
    return;
  }


  for (int v = 0; v < 65536; v++) {
    int r = ((v >> 11) & 0x1F) << 3;
    int g = ((v >> 5) & 0x3F) << 2;
    int b = (v & 0x1F) << 3;
    int migliore = 0;
    long distanza = 1L << 30;
    for (i = 0; i < 128; i++) {
      int dr = r - p4_tavolozza[i * 3];
      int dg = g - p4_tavolozza[i * 3 + 1];
      int db = b - p4_tavolozza[i * 3 + 2];
      long d = (long)dr * dr * 3 + (long)dg * dg * 6 + (long)db * db;
      if (d < distanza) {
        distanza = d;
        migliore = i;
      }
    }
    mappa565[v] = (uint8_t)migliore;
  }
}
static int ui_trap;
static int wait_vsync;
static int ui_warp;
static int joy_latch_value[2];
static int is_tape_motor;
static int is_tape_motor_tick;
static int is_tape_seeking;
static int is_tape_seeking_dir;
static int is_tape_seeking_tick;




static int p4_nastro_svelto = 0;
static double tape_counter_offset;
static char last_iec_dir[256];
static int vertical_res;
static int raster_low;
static int time_advance;

// Things that need to be saved and restored.
int reset_tape_with_cpu = 1;
int tape_feedback = 0;
int ram_size = 64;
int sid_model = 0;


int sid_card = 0;
int sid_write_access = 0;
int sid_digiblaster = 0;
int drive_model_8 = 0;





int drive_model_9 = P4_DRIVE_NIENTE;






int iec_8 = 0;
int iec_9 = 0;
int attach_3plus1_roms = 0;
int keyboard_mapping = 1;



int keyboard_layout = 0;
char rom_basic[MAX_STR_VAL_LEN];
char rom_kernal[MAX_STR_VAL_LEN];
char rom_c0_lo[MAX_STR_VAL_LEN];
char rom_c0_hi[MAX_STR_VAL_LEN];
char rom_c1_lo[MAX_STR_VAL_LEN];
char rom_c1_hi[MAX_STR_VAL_LEN];
char rom_c2_lo[MAX_STR_VAL_LEN];
char rom_c2_hi[MAX_STR_VAL_LEN];
char rom_1541[MAX_STR_VAL_LEN];
char rom_1551[MAX_STR_VAL_LEN];
char rom_1581[MAX_STR_VAL_LEN];
int rom_basic_off;
int rom_kernal_off;
int rom_c0_lo_off;
int rom_c0_hi_off;
int rom_c1_lo_off;
int rom_c1_hi_off;
int rom_c2_lo_off;
int rom_c2_hi_off;
int rom_1541_off;
int rom_1551_off;
int rom_1581_lo_off;
int rom_1581_hi_off;
int color_brightness = 1000;
int color_contrast = 666;
int color_gamma = 800;
int color_tint = 1000;
int color_saturation = 1000;
int raster_skip = 1;
int crt_filter = 1;

static struct menu_item *sid_model_item;
static struct menu_item *sid_card_item;
static struct menu_item *sid_write_access_item;
static struct menu_item *sid_digiblaster_item;
static struct menu_item *tape_feedback_item;
static struct menu_item *ram_size_item;
static struct menu_item *drive_model_8_item;
static struct menu_item *drive_model_9_item;
static struct menu_item *attach_3plus1_roms_item;
static struct menu_item *keyboard_mapping_item;
static struct menu_item *keyboard_layout_item;

static struct menu_item *c0_lo_item;
static struct menu_item *c0_hi_item;
static struct menu_item *c1_lo_item;
static struct menu_item *c1_hi_item;
static struct menu_item *c2_lo_item;
static struct menu_item *c2_hi_item;

static struct menu_item *c0_lo_offset_item;
static struct menu_item *c0_hi_offset_item;
static struct menu_item *c1_lo_offset_item;
static struct menu_item *c1_hi_offset_item;
static struct menu_item *c2_lo_offset_item;
static struct menu_item *c2_hi_offset_item;

static uint32_t prev_drive_state;
static int drive_led_colors[4];

#define COLOR16(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)

static void init_video(void);

static int p4_isspace(char c) {
  return (c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

static void rtrim(char *txt) {
  if (!txt) return;
  int p=strlen(txt)-1;
  while (p4_isspace(txt[p])) { txt[p] = '\0'; p--; }
}

static char* ltrim(char *txt) {
  if (!txt) return NULL;
  int p=0;
  while (p4_isspace(txt[p])) { p++; }
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

static void set_video_font(void) {
  int i;

  // Temporary for now. Need to figure out how to get this from the emulator's
  // ROM. Just read the file ourselves.
  uint8_t* chargen = malloc(4096); // never freed
  FILE* fp = fopen(rom_kernal, "r");
  fseek(fp, 0x1000, SEEK_SET);
  fread(chargen,1,4096,fp);
  fclose(fp);

  video_font = &chargen[0x400];
  raw_video_font = &chargen[0x000];
  for (i = 0; i < 256; ++i) {
    video_font_translate[i] = 8 * ascii_to_petscii[i];
  }
}

static void apply_sid_config() {
  int sid_flags = sid_model_item->choice_ints[sid_model_item->value];
  if (sid_write_access_item->value) sid_flags |= 0x2;
  Plus4VM_SetSIDConfiguration(vm, sid_flags, sid_digiblaster_item->value, 0);





  Plus4VM_SetEnableSIDEmulation(vm, sid_card_item ?
                                    sid_card_item->value : 0);
}



static int carica_banco_cartuccia(int banco, struct menu_item *voce,
                                  int scarto) {
  if (voce == NULL || strlen(voce->str_value) == 0) {
    return 1;
  }
  if (Plus4VM_LoadROM(vm, banco, voce->str_value, scarto) != PLUS4EMU_SUCCESS) {
    printf("plus4emu: banco %d, '%s' (+%d) NON si carica\n",
           banco, voce->str_value, scarto);
    return 0;
  }
  return 1;
}

static int apply_rom_config() {
  if (Plus4VM_LoadROM(vm, 0x00, rom_basic, 0) != PLUS4EMU_SUCCESS)
    return 1;

  if (Plus4VM_LoadROM(vm, 0x01, rom_kernal, 0) != PLUS4EMU_SUCCESS)
    return 1;

  if (Plus4VM_LoadROM(vm, 0x10, rom_1541, 0) != PLUS4EMU_SUCCESS)
    return 1;

  Plus4VM_LoadROM(vm, 0x20, rom_1551, 0);
  Plus4VM_LoadROM(vm, 0x30, rom_1581, 0);
  Plus4VM_LoadROM(vm, 0x31, rom_1581, 16384);




  carica_banco_cartuccia(2, c0_lo_item, c0_lo_offset_item->value);
  carica_banco_cartuccia(3, c0_hi_item, c0_hi_offset_item->value);
  carica_banco_cartuccia(4, c1_lo_item, c1_lo_offset_item->value);
  carica_banco_cartuccia(5, c1_hi_item, c1_hi_offset_item->value);
  carica_banco_cartuccia(6, c2_lo_item, c2_lo_offset_item->value);
  carica_banco_cartuccia(7, c2_hi_item, c2_hi_offset_item->value);

  return 0;
}

static int apply_settings() {
  // Here, we should make whatever calls are necessary to configure the VM
  // according to any settings that were loaded.
  apply_sid_config();
  Plus4VM_SetTapeFeedbackLevel(vm, tape_feedback);
  Plus4VM_SetRAMConfiguration(vm, ram_size, 0x99999999UL);
  return apply_rom_config();
}

// This is only used for the latch func for row/col which
// is really only used for the virtual keyboard. Even USB
// GPIO ends up using the keymap so it must match the
// kernel's keysym table.
static int rowColToP4Code[8][8] = {
 {0, 8,  16, 24, 32, 48, 43, 56},
 {1, 9,  17, 25, 33, 41, 49, 52},
 {2, 10, 18, 26, 34, 42, 50, 58},
 {3, 11, 19, 27, 35, 51, 40, 59},
 {4, 12, 20, 28, 36, 44, 15, 60},
 {5, 13, 21, 29, 37, 45, 53, 61},
 {6, 14, 22, 30, 38, 46, 54, 62},
 {7, 15, 23, 31, 39, 47, 55, 63},
};

//     0: Del          1: Return       2: £            3: Help
//     4: F1           5: F2           6: F3           7: @
//     8: 3            9: W           10: A           11: 4
//    12: Z           13: S           14: E           15: Shift
//    16: 5           17: R           18: D           19: 6
//    20: C           21: F           22: T           23: X
//    24: 7           25: Y           26: G           27: 8
//    28: B           29: H           30: U           31: V
//    32: 9           33: I           34: J           35: 0
//    36: M           37: K           38: O           39: N
//    40: Down        41: P           42: L           43: Up
//    44: .           45: :           46: -           47: ,
//    48: Left        49: *           50: ;           51: Right
//    52: Esc         53: =           54: +           55: /
//    56: 1           57: Home        58: Ctrl        59: 2
//    60: Space       61: C=          62: Q           63: Stop
static int default_bmc64_keycode_to_plus4emu(long keycode) {
   switch (keycode) {
      case KEYCODE_Backspace:
         return 0;
      case KEYCODE_Return:
         return 1;
      case KEYCODE_BackSlash:
         return 2;
      case KEYCODE_F7:
         return 3;
      case KEYCODE_F1:
         return 4;
      case KEYCODE_F2:
         return 5;
      case KEYCODE_F3:
         return 6;
      case KEYCODE_Insert:
         return 7;
      case KEYCODE_3:
         return 8;
      case KEYCODE_w:
         return 9;
      case KEYCODE_a:
         return 10;
      case KEYCODE_4:
         return 11;
      case KEYCODE_z:
         return 12;
      case KEYCODE_s:
         return 13;
      case KEYCODE_e:
         return 14;
      case KEYCODE_LeftShift:
      case KEYCODE_RightShift:
         return 15;
      case KEYCODE_5:
         return 16;
      case KEYCODE_r:
         return 17;
      case KEYCODE_d:
         return 18;
      case KEYCODE_6:
         return 19;
      case KEYCODE_c:
         return 20;
      case KEYCODE_f:
         return 21;
      case KEYCODE_t:
         return 22;
      case KEYCODE_x:
         return 23;
      case KEYCODE_7:
         return 24;
      case KEYCODE_y:
         return 25;
      case KEYCODE_g:
         return 26;
      case KEYCODE_8:
         return 27;
      case KEYCODE_b:
         return 28;
      case KEYCODE_h:
         return 29;
      case KEYCODE_u:
         return 30;
      case KEYCODE_v:
         return 31;
      case KEYCODE_9:
         return 32;
      case KEYCODE_i:
         return 33;
      case KEYCODE_j:
         return 34;
      case KEYCODE_0:
         return 35;
      case KEYCODE_m:
         return 36;
      case KEYCODE_k:
         return 37;
      case KEYCODE_o:
         return 38;
      case KEYCODE_n:
         return 39;
      case KEYCODE_Down:
         return 40;
      case KEYCODE_p:
         return 41;
      case KEYCODE_l:
         return 42;
      case KEYCODE_Up:
         return 43;
      case KEYCODE_Period:
         return 44;
      case KEYCODE_SemiColon:
         return 45;
      case KEYCODE_LeftBracket:
         return 46;
      case KEYCODE_Comma:
         return 47;
      case KEYCODE_Left:
         return 48;
      case KEYCODE_Dash:
         return 49;
      case KEYCODE_SingleQuote:
         return 50;
      case KEYCODE_Right:
         return 51;
      case KEYCODE_BackQuote:
         return 52;
      case KEYCODE_RightBracket:
         return 53;
      case KEYCODE_Equals:
         return 54;
      case KEYCODE_Slash:
         return 55;
      case KEYCODE_1:
         return 56;
      case KEYCODE_Home:
         return 57;
      case KEYCODE_Tab:
         return 58;
      case KEYCODE_2:
         return 59;
      case KEYCODE_Space:
         return 60;
      case KEYCODE_LeftControl:
         return 61;
      case KEYCODE_q:
         return 62;
      case KEYCODE_Escape:
         return 63;
      default:
         return -1;
   }
}

static void errorMessage(const char *fmt, ...)
{
  va_list args;
  fprintf(stderr, " *** Plus/4 error: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  Plus4VM_Destroy(vm);
  exit(-1);
}

static void vmError(void)
{
  fprintf(stderr, " *** Plus/4 error: %s\n", Plus4VM_GetLastErrorMessage(vm));
  Plus4VM_Destroy(vm);
  exit(-1);
}

static void audioOutputCallback(void *userData,
                                const int16_t *buf, size_t nFrames)
{
  if (!ui_warp)
     circle_sound_write((int16_t*)buf, nFrames);
}

static void videoLineCallback(void *userData,
                              int lineNum, const Plus4VideoLineData *lineData)
{
   if (raster_skip == 1) {
      lineNum = lineNum / 2 - raster_low;
   } else {
      lineNum = lineNum - raster_low;
   }
   if (lineNum >= 0 && lineNum < vertical_res && mappa565 != NULL) {


     uint8_t *dest = fb_buf + lineNum * fb_pitch;
     int i;
     Plus4VideoDecoder_DecodeLine(videoDecoder, (uint8_t *)riga16, 384,
                                  lineData);
     for (i = 0; i < 384; i++) {
       dest[i] = mappa565[riga16[i]];
     }
   }
}














#define AVVIO_FERMO    0
#define AVVIO_AL_READY 1
#define AVVIO_AL_FINE  2

static int  avvio_stato = AVVIO_FERMO;
static int  avvio_battiti = 0;
static int  avvio_visto_caricare = 0;

static char avvio_da_battere[48];
static int  avvio_lettera = 0;
static int  avvio_fase = 0;




static int tasto_plus4(char c, int *maiusc) {
  *maiusc = 0;
  switch (c) {
    case 'A': return 10;   case 'D': return 18;   case 'L': return 42;
    case 'N': return 39;   case 'O': return 38;   case 'R': return 17;
    case 'U': return 30;
    case '0': return 35;   case '1': return 56;   case '2': return 59;
    case '8': return 27;
    case ',': return 47;   case '*': return 49;   case ':': return 45;
    case '$': *maiusc = 1; return 11;
    case '"': *maiusc = 1; return 59;
    case '\n': return 1;
    default:  return -1;
  }
}

#define PLUS4_MAIUSC 15



static int  avvio_reset_in_coda = 0;
static char avvio_disco_in_coda[512];
static char avvio_comando[48];
static char avvio_poi[16];


#define AVVIO_OGNI       10
#define AVVIO_MAX_READY  (50 * 15)
#define AVVIO_MAX_FINE   (50 * 600)
#define AVVIO_MAX_MUTO   (50 * 10)

static const char *ultima_volta(const char *dove, const char *cosa) {
  const char *p = dove, *ultimo = NULL;
  while ((p = strstr(p, cosa)) != NULL) { ultimo = p; p++; }
  return ultimo;
}




static int avvio_warp_prima = 0;

static void avvio_finito(void) {
  avvio_stato = AVVIO_FERMO;
  emux_set_warp(avvio_warp_prima);
}

static void avvio_parti(const char *comando, const char *poi) {
  avvio_warp_prima = ui_warp;
  emux_set_warp(1);
  strncpy(avvio_comando, comando, sizeof(avvio_comando) - 1);
  avvio_comando[sizeof(avvio_comando) - 1] = '\0';
  strncpy(avvio_poi, poi ? poi : "", sizeof(avvio_poi) - 1);
  avvio_poi[sizeof(avvio_poi) - 1] = '\0';
  avvio_stato = AVVIO_AL_READY;
  avvio_battiti = 0;
  avvio_visto_caricare = 0;
}



static void avvio_batti_tasti(void) {
  int codice, maiusc;

  if (avvio_da_battere[0] == '\0') { return; }
  if ((avvio_fase++ & 3) != 0) { return; }

  codice = tasto_plus4(avvio_da_battere[avvio_lettera], &maiusc);
  if ((avvio_fase & 7) == 1) {

    if (codice >= 0) {
      if (maiusc) { manda_tasto(PLUS4_MAIUSC, 1); }
      manda_tasto(codice, 1);
    }
  } else {

    if (codice >= 0) {
      manda_tasto(codice, 0);
      if (maiusc) { manda_tasto(PLUS4_MAIUSC, 0); }
    }
    avvio_lettera++;
    if (avvio_da_battere[avvio_lettera] == '\0') {
      avvio_da_battere[0] = '\0';
      avvio_lettera = 0;
    }
  }
}

static void avvio_scrivi(const char *testo) {
  strncpy(avvio_da_battere, testo, sizeof(avvio_da_battere) - 1);
  avvio_da_battere[sizeof(avvio_da_battere) - 1] = '\0';
  avvio_lettera = 0;
  avvio_fase = 0;
}


static void avvio_incolla_se_serve(void) {
  if (avvio_reset_in_coda) {
    avvio_reset_in_coda = 0;

    Plus4VM_Reset(vm, 0);
    if (avvio_disco_in_coda[0] != '\0') {
      int esito = emux_attach_disk_image(8, avvio_disco_in_coda);
      if (esito == 0) {
        avvio_parti("LOAD\"*\",8,1\n", "RUN\n");
      }
      avvio_disco_in_coda[0] = '\0';
    }
  }
}

static void avvio_batti(void) {
  static char schermo[1200];
  const char *pronto, *carica, *cerca;

  avvio_batti_tasti();
  if (avvio_stato == AVVIO_FERMO) { return; }
  avvio_battiti++;
  if ((avvio_battiti % AVVIO_OGNI) != 0) { return; }
  if (avvio_da_battere[0] != '\0') { return; }
  if (Plus4VM_CopyText(vm, schermo, sizeof(schermo), -1, -2) < 0) { return; }

  if (avvio_stato == AVVIO_AL_READY) {
    if (strstr(schermo, "READY.") != NULL) {
      avvio_scrivi(avvio_comando);
      if (avvio_poi[0] == '\0') {
        avvio_finito();
      } else {
        avvio_stato = AVVIO_AL_FINE;
        avvio_battiti = 0;
        avvio_visto_caricare = 0;
      }
    } else if (avvio_battiti > AVVIO_MAX_READY) {
      avvio_finito();
    }
    return;
  }



  carica = ultima_volta(schermo, "LOADING");
  cerca  = ultima_volta(schermo, "SEARCHING");
  pronto = ultima_volta(schermo, "READY.");
  if (carica != NULL) {
    avvio_visto_caricare = 1;
    avvio_battiti = 0;
  }
  if (strstr(schermo, "FILE NOT FOUND") != NULL ||
      strstr(schermo, "DEVICE NOT PRESENT") != NULL) {
    avvio_finito();
    return;
  }



  if (avvio_visto_caricare && pronto != NULL && carica != NULL &&
      pronto > carica) {
    avvio_scrivi(avvio_poi);
    avvio_finito();
    return;
  }



  if (avvio_visto_caricare && pronto == NULL && carica == NULL &&
      cerca == NULL && avvio_battiti > AVVIO_MAX_MUTO) {
    avvio_finito();
    return;
  }
  if (avvio_battiti > AVVIO_MAX_FINE) {
    avvio_finito();
  }
}









static void misura_la_velocita(void) {
  static unsigned long inizio = 0;
  static unsigned quanti = 0;
  unsigned long adesso = circle_get_ticks();
  unsigned long passato;

  quanti++;
  if (inizio == 0) {
    inizio = adesso;
    return;
  }
  passato = adesso - inizio;
  if (passato < 1000000UL) {
    return;
  }

  raspi_meter_fps_x10 = (unsigned)(((unsigned long long)quanti * 10000000ULL)
                                   / passato);
  {
    double nominale = emux_calculate_fps();
    if (nominale > 0.1) {
      raspi_meter_speed_pct =
          (unsigned)((raspi_meter_fps_x10 / 10.0) * 100.0 / nominale + 0.5);
    }
  }


  raspi_meter_frame_avg_us = passato / (quanti ? quanti : 1);
  inizio = adesso;
  quanti = 0;
}



















static unsigned long passo_prossimo_us = 0;


static uint32_t p4_strati_pronti = 0;

static void aspetta_il_passo(void) {
  unsigned long periodo;
  double fps;

  if (ui_warp) {


    passo_prossimo_us = 0;
    return;
  }
  fps = emux_calculate_fps();
  if (fps < 1.0) {
    return;
  }
  periodo = (unsigned long)(1000000.0 / fps + 0.5);
  if (passo_prossimo_us == 0) {
    passo_prossimo_us = circle_get_ticks() + periodo;
    return;
  }
  while ((long)(passo_prossimo_us - circle_get_ticks()) > 0) {

    circle_yield();
  }
  passo_prossimo_us += periodo;
  if ((long)(circle_get_ticks() - passo_prossimo_us) > (long)(periodo * 4)) {
    passo_prossimo_us = circle_get_ticks() + periodo;
  }
}









static void alza_tutti_i_tasti(void) {
  int c;

  for (c = 0; c < 128; c++) {
    manda_tasto(c, 0);
  }
  tastiera_azzera_stato();
}




static int p4_alza_i_tasti = 0;
static int p4_quanto_aspetto = 0;











static void aggiorna_barra_drive(void) {
  static int acceso_prima = -1;
  static int colori_prima[2] = { -1, -1 };
  static int montato_prima[2] = { -1, -1 };
  static int traccia_prima[2] = { -1, -1 };
  int colori[4];
  uint64_t teste;
  int u;


  int acceso = ((drive_model_8 == P4_DRIVE_NIENTE) ? 0 : 1) |
               ((drive_model_9 == P4_DRIVE_NIENTE) ? 0 : 2);

  colori[0] = 0; colori[1] = 0; colori[2] = 0; colori[3] = 0;







  for (u = 0; u < 2; u++) {
    if (Plus4VM_GetFloppyDriveType(vm, u) == 4) {
      colori[u] = EMUX_LED_ROSSO_SX | EMUX_DRIVE_LED2 | EMUX_LED_BARRA;
    }
  }




  if (acceso_prima != acceso || colori_prima[0] != colori[0] ||
      colori_prima[1] != colori[1]) {
    acceso_prima = acceso;
    colori_prima[0] = colori[0];
    colori_prima[1] = colori[1];
    emux_enable_drive_status(acceso, colori);
  }

  teste = Plus4VM_GetDriveHeadPositions(vm);
  for (u = 0; u < 2; u++) {
    const char *disco = (u == 0) ? disco_nell_otto : disco_nella_nove;
    int montato = (disco[0] != '\0') ? 1 : 0;
    int posizione;

    if (montato_prima[u] != montato) {
      montato_prima[u] = montato;
      emux_display_drive_image(u, (char *)disco);
    }



    posizione = (int)((teste >> (16 * u)) & 0xFFFFu);
    if (posizione != 0xFFFF) {
      int traccia = (posizione >> 8) & 0x7F;
      if (traccia != traccia_prima[u]) {
        traccia_prima[u] = traccia;
        emux_display_drive_track(u, traccia * 2);
      }
    }
  }
}






static void videoFrameCallback(void *userData)
{
  misura_la_velocita();















  if (kbd_aspetta_ancora()) {
    if (p4_alza_i_tasti < 3) {
      p4_alza_i_tasti = 3;
    }
    if (++p4_quanto_aspetto >= 600) {
      p4_alza_i_tasti = 0;
    }
  } else {
    p4_quanto_aspetto = 0;
  }
  if (p4_alza_i_tasti > 0) {
    p4_alza_i_tasti--;
    alza_tutti_i_tasti();
  }










  menu_poweroff_tick();

  avvio_batti();






  {








    static unsigned long warp_ultima = 0;
    int mostra = 1;
    if (ui_warp) {
      const unsigned long ora_warp = circle_get_ticks();
      mostra = (ora_warp - warp_ultima >= 100000UL);
      if (mostra) {
        warp_ultima = ora_warp;
      }
    }
    if (mostra) {
      p4_strati_pronti |= FB_LAYER_MASK(FB_LAYER_VIC);
    }
    if (p4_strati_pronti != 0) {
      int sync = !ui_warp;
      if (p4_strati_pronti & (FB_LAYER_MASK(FB_LAYER_UI) |
                              FB_LAYER_MASK(FB_LAYER_STATUS))) {
        sync = 1;
      }
      circle_present_fbl(p4_strati_pronti, sync);
      p4_strati_pronti = 0;
    }
  }



  aspetta_il_passo();

  // Something is waiting for vsync, ack and return.
  if (wait_vsync) {
    wait_vsync = 0;
    return;
  }

  emux_ensure_video();

  bmc_giri[1]++;
  bmc_dove = DOVE_EMULA;
  // This render will handle any OSDs we have. ODSs don't pause emulation.
  if (ui_enabled) {
    // The only way we can be here and have ui_enabled=1
    // is for an osd to be enabled.
    ui_render_now(-1); // only render top most menu
    p4_strati_pronti |= FB_LAYER_MASK(FB_LAYER_UI);
    ui_check_key();
  }

  if (statusbar_showing || vkbd_showing) {
    overlay_check();
    if (overlay_dirty) {
       p4_strati_pronti |= FB_LAYER_MASK(FB_LAYER_STATUS);
       overlay_dirty = 0;
    }
  }

  circle_yield();
  circle_check_gpio();

  int reset_demo = 0;

  circle_lock_acquire();



  if (pending_emu_key.lost) {
    pending_emu_key.lost = 0;
    alza_tutti_i_tasti();
    printf("[TAS] coda piena, alzo tutti i tasti\n");
  }
  while (pending_emu_key.head != pending_emu_key.tail) {









    int i = pending_emu_key.head & PENDING_EMU_KEY_MASK;
    reset_demo = 1;
    if (vkbd_enabled) {
      // Kind of nice to have virtual keyboard's state
      // stay in sync with changes happening from USB
      // key events.
      vkbd_sync_event(pending_emu_key.key[i], pending_emu_key.pressed[i]);
    }









    tasto_evento(pending_emu_key.key[i], pending_emu_key.pressed[i]);
    pending_emu_key.head++;
  }

  // Joystick event dequeue
  while (pending_emu_joy.head != pending_emu_joy.tail) {
    int i = pending_emu_joy.head & 0x7f;
    reset_demo = 1;
    if (vkbd_enabled) {
      int value = pending_emu_joy.value[i];
      int devd = pending_emu_joy.device[i];
      switch (pending_emu_joy.type[i]) {
      case PENDING_EMU_JOY_TYPE_ABSOLUTE:
        if (!vkbd_press[devd]) {
           if (value & 0x1 && !vkbd_up[devd]) {
             vkbd_up[devd] = 1;
             vkbd_nav_up();
           } else if (!(value & 0x1) && vkbd_up[devd]) {
             vkbd_up[devd] = 0;
           }
           if (value & 0x2 && !vkbd_down[devd]) {
             vkbd_down[devd] = 1;
             vkbd_nav_down();
           } else if (!(value & 0x2) && vkbd_down[devd]) {
             vkbd_down[devd] = 0;
           }
           if (value & 0x4 && !vkbd_left[devd]) {
             vkbd_left[devd] = 1;
             vkbd_nav_left();
           } else if (!(value & 0x4) && vkbd_left[devd]) {
             vkbd_left[devd] = 0;
           }
           if (value & 0x8 && !vkbd_right[devd]) {
             vkbd_right[devd] = 1;
             vkbd_nav_right();
           } else if (!(value & 0x8) && vkbd_right[devd]) {
             vkbd_right[devd] = 0;
           }
        }
        if (value & 0x10 && !vkbd_press[devd]) vkbd_nav_press(1, devd);
        else if (!(value & 0x10) && vkbd_press[devd]) vkbd_nav_press(0, devd);
        break;
      }
    } else {
      int port = pending_emu_joy.port[i]-1;
      int oldv = joy_latch_value[port];
      switch (pending_emu_joy.type[i]) {
      case PENDING_EMU_JOY_TYPE_ABSOLUTE:
        // If new bit is 0 and old bit is 1, it is an up event
        // If new bit is 1 and old bit is 0, it is a down event
        joy_latch_value[port] = pending_emu_joy.value[i];
        break;
      case PENDING_EMU_JOY_TYPE_AND:
        // If new bit is 0 and old bit is 1, it is an up event
        joy_latch_value[port] &= pending_emu_joy.value[i];
        break;
      case PENDING_EMU_JOY_TYPE_OR:
        // If new bit is 1 and old bit is 0, it is a down event
        joy_latch_value[port] |= pending_emu_joy.value[i];
        break;
      default:
        break;
      }

      //    72: Joy2 Up     73: Joy2 Down   74: Joy2 Left   75: Joy2 Right
      //    79: Joy2 Fire
      //    80: Joy1 Up     81: Joy1 Down   82: Joy1 Left   83: Joy1 Right
      //    86: Joy1 Fire

      int newv = joy_latch_value[port];
      if (!(newv & 0x01) && (oldv & 0x01)) {
        manda_tasto(72 + 8*port, 0);
      } else if ((newv & 0x01) && !(oldv & 0x01)) {
        manda_tasto(72 + 8*port, 1);
      }
      if (!(newv & 0x02) && (oldv & 0x02)) {
        manda_tasto(73 + 8*port, 0);
      } else if ((newv & 0x02) && !(oldv & 0x02)) {
        manda_tasto(73 + 8*port, 1);
      }
      if (!(newv & 0x04) && (oldv & 0x04)) {
        manda_tasto(74 + 8*port, 0);
      } else if ((newv & 0x04) && !(oldv & 0x04)) {
        manda_tasto(74 + 8*port, 1);
      }
      if (!(newv & 0x08) && (oldv & 0x08)) {
        manda_tasto(75 + 8*port, 0);
      } else if ((newv & 0x08) && !(oldv & 0x08)) {
        manda_tasto(75 + 8*port, 1);
      }
      if (!(newv & 0x10) && (oldv & 0x10)) {
        manda_tasto(79 + 7*port, 0);
      } else if ((newv & 0x10) && !(oldv & 0x10)) {
        manda_tasto(79 + 7*port, 1);
      }
    }
    pending_emu_joy.head++;
  }

  if (ui_trap) {
      ui_trap = 0;
      circle_lock_release();
      emu_pause_trap(0, NULL);
      circle_lock_acquire();
  }

  circle_lock_release();

  ui_handle_toggle_or_quick_func();

  if (reset_demo) {
    demo_reset_timeout();
  }

  if (raspi_demo_mode) {
    demo_check();
  }

  if (is_tape_motor) {
     // Plus4Emu doesn't have a rewind/fastforward state so we
     // fake it here.
     if (is_tape_seeking) {
        is_tape_seeking_tick--;
        if (is_tape_seeking_tick == 0) {
          double pos = Plus4VM_GetTapePosition(vm);
          double newpos = pos + 1 * is_tape_seeking_dir;
          double len = Plus4VM_GetTapeLength(vm);
          if (newpos < 0)
             newpos = 0;
          else if (newpos > len)
             newpos = len;

          if (Plus4VM_TapeSeek(vm, newpos) != PLUS4EMU_SUCCESS) {
             is_tape_seeking = 0;
             is_tape_motor = 0;
          }

          int showing = (int)pos - (int)tape_counter_offset;
          if (showing < 0) showing += 1000;
          emux_display_tape_counter(showing);
          is_tape_seeking_tick = p4_nastro_svelto ? 2 : 5;
        }
     }
     is_tape_motor_tick--;
     if (is_tape_motor_tick == 0) {
       double pos = Plus4VM_GetTapePosition(vm);
       int showing = (int)pos - (int)tape_counter_offset;
       if (showing < 0) showing += 1000;
       emux_display_tape_counter(showing);
       is_tape_motor_tick = 50;
     }
  }

  aggiorna_barra_drive();





  uint32_t drive_state = Plus4VM_GetFloppyDriveLEDState(vm);
  if (drive_state != prev_drive_state) {
    int u;
    for (u = 0; u < 2; u++) {
      unsigned int b = (drive_state >> (8 * u)) & 0xFFu;
      if (Plus4VM_GetFloppyDriveType(vm, u) == 4) {



        emux_display_drive_led(u, (b & 1u) ? 1000 : 0, (b & 2u) ? 1000 : 0);
      } else {
        emux_display_drive_led(u, b ? 1000 : 0, 0);
      }
    }
    prev_drive_state = drive_state;
  }
}





static const char *i_file_simbolici[] = {
  "/PLUS4EMU/rpi_sym.vkm",
  "/PLUS4EMU/rpi_sym_it.vkm",
  "/PLUS4EMU/rpi_sym_uk.vkm",
  "/PLUS4EMU/rpi_sym_de.vkm",
};
#define N_DISPOSIZIONI ((int)(sizeof(i_file_simbolici) / \
                              sizeof(i_file_simbolici[0])))

static void load_keymap(void) {
  FILE *fp = NULL;




  char line[256];



  mappa_azzera();
  alza_tutti_i_tasti();

  if (keyboard_mapping_item->value == KEYBOARD_MAPPING_SYM) {
     int quale = keyboard_layout_item != NULL
                   ? keyboard_layout_item->value : keyboard_layout;
     if (quale < 0 || quale >= N_DISPOSIZIONI) {
        quale = 0;
     }
     fp = fopen(i_file_simbolici[quale], "r");
     if (fp == NULL) {
        printf("WARNING: manca %s\n", i_file_simbolici[quale]);
     }
  } else if (keyboard_mapping_item->value == KEYBOARD_MAPPING_POS) {
     fp = fopen("/PLUS4EMU/rpi_pos.vkm", "r");
  } else if (keyboard_mapping_item->value == KEYBOARD_MAPPING_MAXI) {
     fp = fopen("/PLUS4EMU/rpi_maxi_pos.vkm", "r");
  }
  if (fp != NULL) {
    while (fgets(line, sizeof(line) - 1, fp)) {
      mappa_riga(line);
    }
    fclose(fp);
  } else {
    printf ("WARNING: No keymap found, using default\n");
  }
}

static void machine_kbd_init(void) {
  // Discover these keys from the keymap.
  commodore_key_sym = KEYCODE_LeftControl;
  ctrl_key_sym = KEYCODE_Tab;
  restore_key_sym = KEYCODE_Home;
  commodore_key_sym_set = 0;
  ctrl_key_sym_set = 0;
  restore_key_sym_set = 0;

  for (int i=0;i<MAX_KEY_SYM;i++) {
     if (keysymToP4Code[i] == 61 && !commodore_key_sym_set) {
        commodore_key_sym = i;
        commodore_key_sym_set = 1;
     } else if (keysymToP4Code[i] == 58 && !ctrl_key_sym_set) {
        ctrl_key_sym = i;
        ctrl_key_sym_set = 1;
     } else if (keysymToP4Code[i] == 57 && !restore_key_sym_set) {
        restore_key_sym = i;
        restore_key_sym_set = 1;
     }
  }
}

// This is made to look like VICE's main entry point so our
// Plus4Emu version of EmulatorCore can look more or less the same
// as the Vice version.







static void applica_modello_drive(int unita, int modello);
static void reset_freddo(void) {
  Plus4VM_Reset(vm, 1);
  if (disco_nell_otto[0] == '\0') {
    applica_modello_drive(8, drive_model_8);
  }
  if (disco_nella_nove[0] == '\0') {
    applica_modello_drive(9, drive_model_9);
  }
}

int main_program(int argc, char **argv)
{
  (void) argc;
  (void) argv;

  printf ("Init\n");

  strcpy (last_iec_dir, ".");

  // Default keymap
  mappa_azzera();
  tastiera_azzera_stato();

  int timing = circle_get_machine_timing();

  vm = Plus4VM_Create();
  if (!vm)
    errorMessage("could not create Plus/4 emulator object");

  Plus4VM_SetAudioOutputCallback(vm, &audioOutputCallback, NULL);
  if (Plus4VM_SetAudioOutputQuality(vm, 1) != PLUS4EMU_SUCCESS)
    vmError();

  int audioSampleRate;
  int fragsize;
  int fragnr;
  int channels = 1; // Only mono for plus4emu

  circle_sound_init(NULL, &audioSampleRate, &fragsize, &fragnr, &channels);
  if (Plus4VM_SetAudioSampleRate(vm, audioSampleRate) != PLUS4EMU_SUCCESS)
    vmError();

  if (Plus4VM_SetWorkingDirectory(vm, ".") != PLUS4EMU_SUCCESS)
    vmError();
  /* enable read-write IEC level drive emulation for unit 8 */
  Plus4VM_SetIECDriveReadOnlyMode(vm, 0);

  emux_detach_disk(8);

  videoDecoder =
      Plus4VideoDecoder_Create(&videoLineCallback, &videoFrameCallback, NULL);
  if (!videoDecoder)
    errorMessage("could not create video decoder object");
  Plus4VM_SetVideoOutputCallback(vm, &Plus4VideoDecoder_VideoCallback,
                                 (void *) videoDecoder);

  vic_enabled = 1; // really TED

  init_video();
  ui_init_menu(); // loads settings
  emux_geometry_changed(FB_LAYER_VIC);

  load_keymap();
  machine_kbd_init();

  strcpy(rom_basic,"/PLUS4EMU/p4_basic.rom");
  strcpy(rom_1541,"/PLUS4EMU/dos1541.rom");
  strcpy(rom_1551,"/PLUS4EMU/dos1551.rom");
  strcpy(rom_1581,"/PLUS4EMU/dos1581.rom");

  // Global settings vars have been restored by our load settings hook.
  // Use them to configure the VM.
  if (apply_settings()) {
     return -1;
  }

  set_video_font();

  printf ("Enter emulation loop\n");
  reset_freddo();

  // Fake two core init complete. Temp solution to get sound back for plus4emu.
  circle_kernel_core_init_complete(1);
  circle_kernel_core_init_complete(2);

  circle_boot_complete();

  assert(time_advance > 0);
  for(;;) {
    Plus4VM_Run(vm, time_advance);
    avvio_incolla_se_serve();
  }

  Plus4VM_Destroy(vm);
  Plus4VideoDecoder_Destroy(videoDecoder);
  return 0;
}

// Begin emu_api impl.

void emu_machine_init(int raster_skip_enabled, int raster_skip2_enabled) {
  emux_machine_class = BMC64_MACHINE_CLASS_PLUS4EMU;

  raster_skip = raster_skip_enabled ? 2 : 1;

  // For plus4emu, raster_skip can't be turned off
  // at runtime. There's no line dupe like in our
  // VICE mod.
}



int emux_handle_reu_image_change(const char *path) {
   (void)path;
   return -1;
}

int emux_save_reu_image(const char *path) {
   (void)path;
   return -1;
}



int emux_prepare_shutdown(void) {
   return 0;
}

void emux_reu_image_loaded(int size_kb) {
   (void)size_kb;
}

int emux_handle_ide64_image_change(int device, const char *path) {
   (void)device;
   (void)path;
   return -1;
}



void emux_flush_disks_now(fullpath_func f_fullpath) {
  (void)f_fullpath;
}

void emux_trap_main_loop_ui(void) {
  circle_lock_acquire();
  ui_trap = 1;
  circle_lock_release();
}

void emux_trap_main_loop(void (*trap_func)(uint16_t, void *data), void* data) {
}

void emux_kbd_set_latch_keyarr(int row, int col, int pressed) {
  int p4code = rowColToP4Code[col][row];
  if (p4code >= 0) {
    manda_tasto(p4code, pressed);
  }
}

static int iec_acceso(int unita) {
  return (unita == 8) ? iec_8 : ((unita == 9) ? iec_9 : 0);
}




static void cartella_iec(int n) {
  Plus4VM_SetWorkingDirectory(vm, last_iec_dir);
  Plus4VM_SetDiskImageFile(vm, n, "", 0);
  Plus4VM_SetDiskImageFile(vm, n, "", 1);
}

static void applica_modello_drive(int unita, int modello) {
  char *disco = (unita == 9) ? disco_nella_nove : disco_nell_otto;
  struct menu_item *voce = (unita == 9) ? drive_model_9_item
                                        : drive_model_8_item;
  int n = (unita == 9) ? 1 : 0;

  if (unita == 9) {
    drive_model_9 = modello;
  } else {
    drive_model_8 = modello;
  }
  if (voce != NULL) {
    voce->value = modello;
  }
  if (modello == P4_DRIVE_NIENTE) {



    disco[0] = '\0';






    Plus4VM_SetDiskImageFile(vm, n, "", 1);
    Plus4VM_SetDiskImageFile(vm, n, "", 0);


    if (iec_acceso(unita)) {
      cartella_iec(n);
    }
    return;
  }
  if (disco[0] != '\0') {
    Plus4VM_SetDiskImageFile(vm, n, disco, modello);
  } else if (iec_acceso(unita)) {

    cartella_iec(n);
  } else if (unita <= 9) {









    Plus4VM_SetEmptyFloppyDrive(vm, n, modello);
  }
}

int emux_attach_disk_image(int unit, char *filename) {






  int modello = (unit == 8) ? drive_model_8
                            : ((unit == 9) ? drive_model_9 : 0);

  if (unit < 8 || unit > 11) {
    return 1;
  }



  if (modello == P4_DRIVE_NIENTE) {
    modello = 0;
    if (unit == 8 || unit == 9) {
      applica_modello_drive(unit, 0);
    }
  }
  if (Plus4VM_SetDiskImageFile(vm, unit-8, filename, modello)
        != PLUS4EMU_SUCCESS) {
    return 1;
  }
  if (unit == 8) {
    strncpy(disco_nell_otto, filename, MAX_STR_VAL_LEN - 1);
    disco_nell_otto[MAX_STR_VAL_LEN - 1] = '\0';
  } else if (unit == 9) {
    strncpy(disco_nella_nove, filename, MAX_STR_VAL_LEN - 1);
    disco_nella_nove[MAX_STR_VAL_LEN - 1] = '\0';
  }



  return 0;
}

void emux_detach_disk(int unit) {
  if (unit < 8 || unit > 11) {
    return;
  }
  if (unit == 8) {
    disco_nell_otto[0] = '\0';
  } else if (unit == 9) {
    disco_nella_nove[0] = '\0';
  }




  if (unit <= 9) {
    applica_modello_drive(unit, unit == 9 ? drive_model_9 : drive_model_8);
    return;
  }
  Plus4VM_SetWorkingDirectory(vm, last_iec_dir);
  Plus4VM_SetDiskImageFile(vm, unit-8, "", 0);
}














static int p4_nastro_attaccato = 0;

int emux_tape_presente(void) {
  return p4_nastro_attaccato;
}

int emux_attach_tape_image(char *filename) {
  is_tape_seeking = 0;
  is_tape_motor = 0;
  emux_display_tape_counter(0);
  emux_display_tape_motor_status(EMUX_TAPE_STOP);
  if (Plus4VM_SetTapeFileName(vm, filename) != PLUS4EMU_SUCCESS) {
    p4_nastro_attaccato = 0;
    return 1;
  }
  p4_nastro_attaccato = (filename != NULL && filename[0] != '\0');
  return 0;
}

void emux_detach_tape(void) {
  is_tape_seeking = 0;
  is_tape_motor = 0;
  emux_display_tape_counter(0);
  emux_display_tape_motor_status(EMUX_TAPE_STOP);
  Plus4VM_SetTapeFileName(vm, "");
  p4_nastro_attaccato = 0;
}

int emux_attach_cart(int menu_id, char *filename) {
  int bank;
  int offset;
  struct menu_item* item;

  ui_info("Attaching...");

  switch (menu_id) {
     case MENU_PLUS4_CART_C0_LO_FILE:
        bank = 2;
        offset = c0_lo_offset_item->value;
        item = c0_lo_item;
        break;
     case MENU_PLUS4_CART_C0_HI_FILE:
        bank = 3;
        offset = c0_hi_offset_item->value;
        item = c0_hi_item;
        break;
     case MENU_PLUS4_CART_C1_LO_FILE:
        bank = 4;
        offset = c1_lo_offset_item->value;
        item = c1_lo_item;
        break;
     case MENU_PLUS4_CART_C1_HI_FILE:
        bank = 5;
        offset = c1_hi_offset_item->value;
        item = c1_hi_item;
        break;
     case MENU_PLUS4_CART_C2_LO_FILE:
        bank = 6;
        offset = c2_lo_offset_item->value;
        item = c2_lo_item;
        break;
     case MENU_PLUS4_CART_C2_HI_FILE:
        bank = 7;
        offset = c2_hi_offset_item->value;
        item = c2_hi_item;
        break;
     default:


        ui_pop_menu();
        ui_error("Unknown cartridge slot");
        return 1;
  }

  if (Plus4VM_LoadROM(vm, bank, filename, offset) != PLUS4EMU_SUCCESS) {
     ui_pop_menu();
     ui_error("Failed to attach cart image");
     return 1;
  } else {
     ui_pop_all_and_toggle();
     reset_freddo();
  }

  // Update attached cart name
  strncpy(item->str_value, filename, MAX_STR_VAL_LEN - 1);
  strncpy(item->displayed_value, filename, MAX_DSP_VAL_LEN - 1);
  return 0;
}

void emux_set_cart_default(void) {
}

void emux_detach_cart(int menu_id) {
  int bank;
  struct menu_item* item;
  switch (menu_id) {
     case MENU_PLUS4_DETACH_CART_C0_LO:
        bank = 2;
        item = c0_lo_item;
        break;
     case MENU_PLUS4_DETACH_CART_C0_HI:
        bank = 3;
        item = c0_hi_item;
        break;
     case MENU_PLUS4_DETACH_CART_C1_LO:
        bank = 4;
        item = c1_lo_item;
        break;
     case MENU_PLUS4_DETACH_CART_C1_HI:
        bank = 5;
        item = c1_hi_item;
        break;
     case MENU_PLUS4_DETACH_CART_C2_LO:
        bank = 6;
        item = c2_lo_item;
        break;
     case MENU_PLUS4_DETACH_CART_C2_HI:
        bank = 7;
        item = c2_hi_item;
        break;
     default:







        return;
  }
  if (item == NULL) {
     return;
  }

  Plus4VM_LoadROM(vm, bank, "", 0);
  reset_freddo();
  item->displayed_value[0] = '\0';
  item->str_value[0] = '\0';
}

static void reset_tape_drive() {
  Plus4VM_TapeSeek(vm, 0);
  Plus4VM_TapeStop(vm);
  is_tape_motor = 0;
  tape_counter_offset = 0;
  emux_display_tape_counter(0);
}

void emux_alza_la_tastiera(void) {
  alza_tutti_i_tasti();
}

void emux_reset(int isSoft) {
  if (isSoft) {
    Plus4VM_Reset(vm, 0);
  } else {
    reset_freddo();
  }
  alza_tutti_i_tasti();
  kbd_aspetta_il_rilascio();
  p4_alza_i_tasti = 3;
  p4_quanto_aspetto = 0;
  if (reset_tape_with_cpu) {
     emux_display_tape_control_status(EMUX_TAPE_STOP);
     reset_tape_drive();
     emux_display_tape_motor_status(is_tape_motor);
  }
}

int emux_save_state(char *filename) {
  if (Plus4VM_SaveState(vm, filename) != PLUS4EMU_SUCCESS) {
    return 1;
  }
  return 0;
}

int emux_load_state(char *filename) {
  if (Plus4VM_LoadState(vm, filename) != PLUS4EMU_SUCCESS) {
    return 1;
  }
  return 0;
}





static int emux_tape_control_interno(int cmd);

int emux_tape_control_veloce(int cmd, int volte) {
  p4_nastro_svelto = (volte >= 2);
  return emux_tape_control_interno(cmd);
}

int emux_tape_control(int cmd) {
  p4_nastro_svelto = 0;
  return emux_tape_control_interno(cmd);
}

static int emux_tape_control_interno(int cmd) {

    if (!p4_nastro_attaccato) {
      return -1;
    }
    emux_display_tape_control_status(cmd);
    switch (cmd) {
    case EMUX_TAPE_PLAY:
      Plus4VM_TapePlay(vm);
      is_tape_seeking = 0;
      is_tape_motor = 1;
      is_tape_motor_tick = 50;
      break;
    case EMUX_TAPE_STOP:
      Plus4VM_TapeStop(vm);
      is_tape_seeking = 0;
      is_tape_motor = 0;
      break;
    case EMUX_TAPE_REWIND:
      is_tape_seeking = 1;
      is_tape_seeking_dir = -1;
      is_tape_seeking_tick = p4_nastro_svelto ? 2 : 5;
      is_tape_motor = 1;
      is_tape_motor_tick = 50;
      break;
    case EMUX_TAPE_FASTFORWARD:
      is_tape_seeking = 1;
      is_tape_seeking_dir = 1;
      is_tape_seeking_tick = p4_nastro_svelto ? 2 : 5;
      is_tape_motor = 1;
      is_tape_motor_tick = 50;
      break;
    case EMUX_TAPE_RECORD:
      Plus4VM_TapeRecord(vm);
      is_tape_seeking = 0;
      is_tape_motor = 1;
      is_tape_motor_tick = 50;
      break;
    case EMUX_TAPE_RESET:
      reset_tape_drive();
      break;
    case EMUX_TAPE_ZERO:
      tape_counter_offset = Plus4VM_GetTapePosition(vm);
      emux_display_tape_counter(0);
      break;
    default:
      assert(0);
      break;
  }
  emux_display_tape_motor_status(is_tape_motor);
}

void emux_show_cart_osd_menu(void) {
}

unsigned long emux_calculate_timing(double fps) {
  // TODO: Enable custom timing calc in common when this if fixed.
  return 0;
}

double emux_calculate_fps() {
  // TODO: Enable custom timing calc in common when this if fixed.
  if (is_ntsc()) {
    return 60;
  }
  return 50;
}







int emux_real_drive_available(void) { return 0; }









int emux_set_sound_sample_rate(int sample_rate) {
  if (vm == NULL || sample_rate <= 0) {
    return -1;
  }
  return Plus4VM_SetAudioSampleRate(vm, (float)sample_rate) ==
                 PLUS4EMU_SUCCESS ? 0 : -1;
}






























void emux_set_real_drive(int unit) { (void)unit; }

void emux_set_real_drive_log(int acceso) { (void)acceso; }

int emux_real_drive_measure(int *v, int n) { (void)v; (void)n; return 0; }
void emux_set_real_drive_respiro(int us) { (void)us; }
void emux_set_real_drive_veloce(int acceso) { (void)acceso; }
void emux_real_drive_molla(void) { }
int emux_real_drive_recover(int unit, int *v, int n) {
  (void)unit; (void)v; (void)n; return 0;
}
int emux_real_drive_turbo(int unit, char *nome, int nome_max,
                          char *dove, int dove_max, int *v, int n) {
  (void)unit; (void)nome; (void)nome_max; (void)dove; (void)dove_max;
  (void)v; (void)n; return 0;
}

int emux_real_drive_counters(int *v, int n) { (void)v; (void)n; return 0; }

void emux_real_drive_reset_counters(void) { }

void emux_want_real_drive(int unit) { (void)unit; }
void emux_real_drive_tick(void) {}


void emux_use_emulated_drive(int unit) { (void)unit; }
int emux_is_real_drive(int unit) { (void)unit; return 0; }
void emux_refresh_drive_items(int unit) { (void)unit; }

void emux_leave_real_drive(int unit) { (void)unit; }





int emux_get_shiftlock(void) {
  return 0;
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





unsigned long emux_orologio_cpu(void) { return 0; }
void emux_sid_uscita_come_i_chip(void) { }
int emux_sid_livelli(int livello[3]) { (void)livello; return 0; }
int emux_sid_voci_mute(void) { return 0; }
void emux_sid_voci_imposta(int mute) { (void)mute; }

int emux_sid_lato(int s) { (void)s; return -1; }
int emux_sid_lato_cambia(int s) { (void)s; return -1; }
void emux_sid_corsa(int on) { (void)on; }
void emux_sid_drive_zitti(int zitti) { (void)zitti; }
int emux_sid_drive_zitti_ora(void) { return 0; }
const char *emux_sid_chi_suona(int quale) { (void)quale; return ""; }
void emux_sid_ferma(void) {}
void emux_sid_lascia_la_porta(void) {}
const char *emux_sid_modello(int chip) { (void)chip; return NULL; }



unsigned long emux_cicli_al_secondo(void) { return 0; }
const char *emux_sid_velocita(void) { return ""; }














































int emux_load_prg_file(char* filename) {
  if (Plus4VM_LoadProgram(vm, filename) != PLUS4EMU_SUCCESS) {
     return -1;
  }
  return 0;
}





int emux_autostart_file(char* filename) {
  const char *punto = strrchr(filename, '.');
  char est[8];
  int i;

  est[0] = '\0';
  if (punto != NULL) {
    for (i = 0; i < 7 && punto[i + 1] != '\0'; i++) {
      est[i] = tolower((unsigned char)punto[i + 1]);
    }
    est[i] = '\0';
  }

  if (strcmp(est, "d64") == 0 || strcmp(est, "d81") == 0 ||
      strcmp(est, "d71") == 0 || strcmp(est, "g64") == 0) {


    strncpy(avvio_disco_in_coda, filename, sizeof(avvio_disco_in_coda) - 1);
    avvio_disco_in_coda[sizeof(avvio_disco_in_coda) - 1] = '\0';
    avvio_reset_in_coda = 1;
    return 0;
  }

  if (Plus4VM_LoadProgram(vm, filename) != PLUS4EMU_SUCCESS) {
     return -1;
  }
  avvio_parti("RUN\n", NULL);
  return 0;
}

void emux_drive_change_model(int unit) {
}











void emux_add_drive_option(struct menu_item* parent, int drive) {
  struct menu_item *voce = NULL;

  if (drive == 8) {
    voce = ui_menu_add_multiple_choice(MENU_DRIVE_TYPE_8, parent,
                                       "Drive Model");
    voce->value = drive_model_8;
    drive_model_8_item = voce;
  } else if (drive == 9) {
    voce = ui_menu_add_multiple_choice(MENU_DRIVE_TYPE_9, parent,
                                       "Drive Model");
    voce->value = drive_model_9;
    drive_model_9_item = voce;
  }
  if (voce == NULL) {
    return;
  }
  voce->num_choices = 3;
  strcpy(voce->choices[0], "1541");
  strcpy(voce->choices[1], "1551");
  strcpy(voce->choices[2], "None");
}

void emux_create_disk(struct menu_item* item, fullpath_func fullpath) {
  // Not supported for plus/4
}

void emux_create_tape(struct menu_item* item, fullpath_func fullpath) {
  // Not supported for plus/4
}

void emux_set_joy_port_device(int port_num, int dev_id) {
}

void emux_set_joy_pot_x(int port,int value) {
  // Not supported on plus4emu
}

void emux_set_joy_pot_y(int port, int value) {
  // Not supported on plus4emu
}

void emux_add_tape_options(struct menu_item* parent) {
  tape_feedback_item =
      ui_menu_add_range(MENU_TAPE_FEEDBACK, parent,
          "Tape Audible Feedback Level", 0, 10, 1, tape_feedback);
}





static void aggiorna_voce_layout(void) {
  if (keyboard_layout_item == NULL) {
    return;
  }
  keyboard_layout_item->disabled =
      (keyboard_mapping_item->value != KEYBOARD_MAPPING_SYM);
}






static void aggiorna_tastiera_del_menu(void) {
  if (keyboard_mapping_item != NULL) {
    ui_set_keyboard_mapping(keyboard_mapping_item->value);
  }
  if (keyboard_layout_item != NULL) {
    ui_set_keyboard_layout(keyboard_layout_item->value);
  }
}

void emux_add_keyboard_options(struct menu_item* parent) {
  keyboard_mapping_item = ui_menu_add_multiple_choice(
      MENU_KEYBOARD_MAPPING, parent, "Mapping");
  keyboard_mapping_item->num_choices = 3;

  keyboard_mapping_item->value = keyboard_mapping;
  strcpy(keyboard_mapping_item->choices[KEYBOARD_MAPPING_SYM], "Symbolic");
  strcpy(keyboard_mapping_item->choices[KEYBOARD_MAPPING_POS], "Positional");
  strcpy(keyboard_mapping_item->choices[KEYBOARD_MAPPING_MAXI], "Maxi Positional");








  keyboard_layout_item = ui_menu_add_multiple_choice(
      MENU_KEYBOARD_LAYOUT, parent, "Layout");
  keyboard_layout_item->num_choices = N_DISPOSIZIONI;
  strcpy(keyboard_layout_item->choices[0], "US");
  strcpy(keyboard_layout_item->choices[1], "Italiana");
  strcpy(keyboard_layout_item->choices[2], "British");
  strcpy(keyboard_layout_item->choices[3], "Deutsch");
  keyboard_layout_item->value =
      (keyboard_layout >= 0 && keyboard_layout < N_DISPOSIZIONI)
        ? keyboard_layout : 0;
  aggiorna_voce_layout();
  aggiorna_tastiera_del_menu();
}

void emux_add_sound_options(struct menu_item* parent) {


  sid_card_item = ui_menu_add_toggle(MENU_SIDCART_ENABLE, parent,
      "SID Card", sid_card);










  struct menu_item* child = sid_model_item =
      ui_menu_add_multiple_choice(MENU_SID_MODEL, parent, "Sid Model");
  child->num_choices = 2;
  child->value = sid_model;
  strcpy(child->choices[0], "8580");
  child->choice_ints[0] = 0; // 8580 for sidflags
  strcpy(child->choices[1], "6581");
  child->choice_ints[1] = 1;

  // Write access at $d400-d41f
  sid_write_access_item =
      ui_menu_add_toggle(MENU_SID_WRITE_D400, parent,
          "Write Access $D400-D41F", sid_write_access);

  // Digiblaster
  sid_digiblaster_item =
      ui_menu_add_toggle(MENU_SID_DIGIBLASTER, parent,
          "Enable Digiblaster", sid_digiblaster);
}

void emux_video_color_setting_changed(int display_num) {
  Plus4VideoDecoder_UpdatePalette(videoDecoder);
  // Plus4Emu doesn't use an indexed palette so we have to allow
  // the decoder to draw a frame after we change a color param.
  wait_vsync = 1;
  do {
    Plus4VM_Run(vm, 2000);
  } while (wait_vsync);
}

void emux_set_color_brightness(int display_num, int value) {
  // Incoming 0-2000, Outgoing -.5 - .5
  // Default 0
  color_brightness = value;
  double v = value;
  v = v - 1000;
  v = v / 2000;
  Plus4VideoDecoder_SetBrightness(videoDecoder,v,v,v,v);
}

void emux_set_color_contrast(int display_num, int value) {
  // Incoming 0-2000, Outgoing .5 - 2.0
  // Default 1
  color_contrast = value;
  double v = value / 1333.33d;
  v = v + .5;
  if (v < .5) v = .5;
  if (v > 2.0) v = 2.0;
  Plus4VideoDecoder_SetContrast(videoDecoder,v,v,v,v);
}

void emux_set_color_gamma(int display_num, int value) {
  // Incoming 0-4000, Outgoing .25 - 4.0
  // Default 1
  color_gamma = value;
  double v = value / 1066.66d;
  v = v + .25;
  if (v < .25) v = .25;
  if (v > 4.0) v = 4.0;
  Plus4VideoDecoder_SetGamma(videoDecoder,v,v,v,v);
}

void emux_set_color_tint(int display_num, int value) {
  // Incoming 0-2000, Outgoing -180, 180
  // Default 0
  color_tint = value;
  double v = value / 5.555d;
  v = v - 180;
  if (v < -180) v = -180;
  if (v > 180) v = 180;
  Plus4VideoDecoder_SetHueShift(videoDecoder,v);
}

void emux_set_color_saturation(int display_num, int value) {






  double v;
  color_saturation = value;
  v = value / 1000.0d;
  if (v < 0.0) v = 0.0;
  if (v > 2.0) v = 2.0;
  Plus4VideoDecoder_SetSaturation(videoDecoder, v);
}

int emux_get_color_brightness(int display_num) {
  return color_brightness;
}

int emux_get_color_contrast(int display_num) {
  return color_contrast;
}

int emux_get_color_gamma(int display_num) {
  return color_gamma;
}

int emux_get_color_tint(int display_num) {
  return color_tint;
}

int emux_get_color_saturation(int display_num) {
  return color_saturation;
}

void emux_set_video_cache(int value) {
  // Ignore for plus/4
}

void emux_set_hw_scale(int value) {
  // Ignore for plus/4
}

struct menu_item* emux_add_palette_options(int menu_id,
                                           struct menu_item* parent) {








  return NULL;
}

static void menu_value_changed(struct menu_item *item) {
  // Forward to our emux_ handler
  emux_handle_menu_change(item);
}

void emux_add_machine_options(struct menu_item* parent) {
  // TODO : Memory and cartridge configurations
  // C16-16k
  // C16-64k
  // Plus/4-64k

  ram_size_item =
      ui_menu_add_multiple_choice(MENU_MEMORY, parent, "Memory");
  ram_size_item->num_choices = 5;

  switch (ram_size) {
    case 16:
      ram_size_item->value = 0;
      break;
    case 32:
      ram_size_item->value = 1;
      break;
    case 64:
      ram_size_item->value = 2;
      break;
    case 256:
      ram_size_item->value = 3;
      break;
    case 1024:
    default:
      ram_size_item->value = 4;
      break;
  }

  strcpy(ram_size_item->choices[0], "16k");
  strcpy(ram_size_item->choices[1], "32k");
  strcpy(ram_size_item->choices[2], "64k");
  strcpy(ram_size_item->choices[3], "256k");
  strcpy(ram_size_item->choices[4], "1024k");
  ram_size_item->choice_ints[0] = 16;
  ram_size_item->choice_ints[1] = 32;
  ram_size_item->choice_ints[2] = 64;
  ram_size_item->choice_ints[3] = 256;
  ram_size_item->choice_ints[4] = 1024;
  ram_size_item->on_value_changed = menu_value_changed;

  attach_3plus1_roms_item = ui_menu_add_toggle(MENU_PLUS4_3PLUS1_ROMS, parent,
          "Attach 3Plus1 ROMs", attach_3plus1_roms);
}

struct menu_item* emux_add_cartridge_options(struct menu_item* root) {
  struct menu_item* parent = ui_menu_add_folder(root, "Cartridge");

  ui_menu_add_divider(parent);
  c0_lo_item = ui_menu_add_button_with_value(MENU_TEXT, parent,
     "C0 LO:",0,rom_c0_lo,"");
  c0_lo_item->prefer_str = 1;
  strncpy(c0_lo_item->displayed_value, rom_c0_lo, MAX_DSP_VAL_LEN - 1);
  ui_menu_add_button(MENU_PLUS4_ATTACH_CART_C0_LO, parent, "Attach...");
  c0_lo_offset_item = ui_menu_add_range(MENU_PLUS4_ATTACH_CART_C0_LO_OFFSET,
     parent, "Offset", 0, 16384, 16384, rom_c0_lo_off);
  ui_menu_add_button(MENU_PLUS4_DETACH_CART_C0_LO, parent, "Detach");
  ui_menu_add_divider(parent);

  c0_hi_item = ui_menu_add_button_with_value(MENU_TEXT, parent,
      "C0 HI:",0,rom_c0_hi,"");
  c0_hi_item->prefer_str = 1;
  strncpy(c0_hi_item->displayed_value, rom_c0_hi, MAX_DSP_VAL_LEN - 1);
  ui_menu_add_button(MENU_PLUS4_ATTACH_CART_C0_HI, parent, "Attach...");
  c0_hi_offset_item = ui_menu_add_range(MENU_PLUS4_ATTACH_CART_C0_HI_OFFSET,
     parent, "Offset", 0, 16384, 16384, rom_c0_hi_off);
  ui_menu_add_button(MENU_PLUS4_DETACH_CART_C0_HI, parent, "Detach");
  ui_menu_add_divider(parent);

  c1_lo_item = ui_menu_add_button_with_value(MENU_TEXT, parent,
      "C1 LO:",0,rom_c1_lo,"");
  c1_lo_item->prefer_str = 1;
  strncpy(c1_lo_item->displayed_value, rom_c1_lo, MAX_DSP_VAL_LEN - 1);
  ui_menu_add_button(MENU_PLUS4_ATTACH_CART_C1_LO, parent, "Attach...");
  c1_lo_offset_item = ui_menu_add_range(MENU_PLUS4_ATTACH_CART_C1_LO_OFFSET,
      parent, "Offset", 0, 16384, 16384, rom_c1_lo_off);
  ui_menu_add_button(MENU_PLUS4_DETACH_CART_C1_LO, parent, "Detach");
  ui_menu_add_divider(parent);

  c1_hi_item = ui_menu_add_button_with_value(MENU_TEXT, parent,
      "C1 HI:",0,rom_c1_hi,"");
  c1_hi_item->prefer_str = 1;
  strncpy(c1_hi_item->displayed_value, rom_c1_hi, MAX_DSP_VAL_LEN - 1);
  ui_menu_add_button(MENU_PLUS4_ATTACH_CART_C1_HI, parent, "Attach...");
  c1_hi_offset_item = ui_menu_add_range(MENU_PLUS4_ATTACH_CART_C1_HI_OFFSET,
      parent, "Offset", 0, 16384, 16384, rom_c1_hi_off);
  ui_menu_add_button(MENU_PLUS4_DETACH_CART_C1_HI, parent, "Detach");
  ui_menu_add_divider(parent);

  c2_lo_item = ui_menu_add_button_with_value(MENU_TEXT, parent,
      "C2 LO:",0,rom_c2_lo,"");
  c2_lo_item->prefer_str = 1;
  strncpy(c2_lo_item->displayed_value, rom_c2_lo, MAX_DSP_VAL_LEN - 1);
  ui_menu_add_button(MENU_PLUS4_ATTACH_CART_C2_LO, parent, "Attach...");
  c2_lo_offset_item = ui_menu_add_range(MENU_PLUS4_ATTACH_CART_C2_LO_OFFSET,
      parent, "Offset", 0, 16384, 16384, rom_c2_lo_off);
  ui_menu_add_button(MENU_PLUS4_DETACH_CART_C2_LO, parent, "Detach");
  ui_menu_add_divider(parent);

  c2_hi_item = ui_menu_add_button_with_value(MENU_TEXT, parent,
      "C2 HI:",0,rom_c2_hi,"");
  c2_hi_item->prefer_str = 1;
  strncpy(c2_hi_item->displayed_value, rom_c2_hi, MAX_DSP_VAL_LEN - 1);
  ui_menu_add_button(MENU_PLUS4_ATTACH_CART_C2_HI, parent, "Attach...");
  c2_hi_offset_item = ui_menu_add_range(MENU_PLUS4_ATTACH_CART_C2_HI_OFFSET,
      parent, "Offset", 0, 16384, 16384, rom_c2_hi_off);
  ui_menu_add_button(MENU_PLUS4_DETACH_CART_C2_HI, parent, "Detach");

  return parent;
}

void emux_set_warp(int warp) {
  ui_warp = warp;
}

void emux_change_palette(int display_num, int palette_index) {
  // Never called for Plus4Emu
}

int emux_handle_rom_change(struct menu_item* item, fullpath_func fullpath) {
  return 0;
}

void emux_set_iec_dir(int unit, char* dir) {
  Plus4VM_SetWorkingDirectory(vm, dir);
  strcpy (last_iec_dir, dir);
}


const char *emux_get_engine_version(void) {
  return "plus4emu";
}

void emux_set_int(IntSetting setting, int value) {
  switch (setting) {
    case Setting_DatasetteResetWithCPU:
       reset_tape_with_cpu = value;
       break;
    case Setting_Datasette:
       // Not applicable
       break;
    case Setting_VideoFilter:
       crt_filter = value;
       break;
    default:
       printf ("Unhandled set int %d\n", setting);
  }
}

void emux_set_int_1(IntSetting setting, int value, int param) {
  switch (setting) {
     case Setting_FileSystemDeviceN:
        // Nothing to do
        break;
     case Setting_IECDeviceN:



        if (param == 8 || param == 9) {
          if (param == 8) {
            iec_8 = value ? 1 : 0;
          } else {
            iec_9 = value ? 1 : 0;
          }
          if ((param == 9 ? disco_nella_nove : disco_nell_otto)[0] == '\0') {
            applica_modello_drive(param, param == 9 ? drive_model_9
                                                    : drive_model_8);
          }
        }
        break;
     case Setting_DriveNType:






        if (param == 8 || param == 9) {
          if (value == 1541) {
            applica_modello_drive(param, 0);
          } else if (value == 1551) {
            applica_modello_drive(param, 1);
          } else if (value == 0) {


            applica_modello_drive(param, P4_DRIVE_NIENTE);
          }
        }
        break;
     default:
        printf ("Unhandled set int_1 %d\n", setting);
        break;
  }
}

void emux_get_int(IntSetting setting, int* dest) {
   switch (setting) {
      case Setting_WarpMode:
          *dest = ui_warp;
          break;
      case Setting_DatasetteResetWithCPU:
          *dest = reset_tape_with_cpu;
          break;
      case Setting_DriveSoundEmulation:
      case Setting_DriveSoundEmulationVolume:
      case Setting_TapeSoundEmulation:
      case Setting_TapeSoundEmulationVolume:
          *dest = 0;
          // Not applicable
          break;
      case Setting_VideoFilter:
          *dest = crt_filter;
          break;
      default:
          printf ("WARNING: Tried to get unsupported setting %d\n",setting);
          break;
   }
}

void emux_get_int_1(IntSetting setting, int* dest, int param) {
  switch (setting) {
     case Setting_FileSystemDeviceN:
        *dest = 1;
        return;
     case Setting_IECDeviceN:



        *dest = (param == 8) ? iec_8 : ((param == 9) ? iec_9 : 0);
        break;
     default:
        printf ("Unhandled get int_1 %d\n", setting);
  }
}

void emux_get_string_1(StringSetting setting, const char** dest, int param) {
  switch (setting) {
     case Setting_FSDeviceNDir:
        *dest = last_iec_dir;
        break;
     default:
        printf ("Unhandled set string_1 %d\n", setting);
        break;
  }
}

int emux_save_settings(void) {
  // All our  additional settings are handled by emux_save_additional_settings
  // Nothing to do here.
  return 0;
}

void emux_log_settings_file(const char *filename) {
   printf("Writing settings file `%s'.\n", filename);
}

void emux_log_riga(const char *riga) {
   printf("%s\n", riga);
}

// Handle any menu item we've created for this emulator.
int emux_handle_menu_change(struct menu_item* item) {
  switch (item->id) {
    case MENU_SIDCART_ENABLE:
    case MENU_SID_MODEL:
    case MENU_SID_WRITE_D400:
    case MENU_SID_DIGIBLASTER:
      apply_sid_config();
      return 1;
    case MENU_TAPE_FEEDBACK:
      Plus4VM_SetTapeFeedbackLevel(vm, item->value);
      return 1;
    case MENU_MEMORY:
      Plus4VM_SetRAMConfiguration(vm,
         ram_size_item->choice_ints[ram_size_item->value], 0x99999999UL);
      apply_rom_config();
      return 1;
    case MENU_DRIVE_TYPE_8:


      applica_modello_drive(8, item->value);
      return 1;
    case MENU_DRIVE_TYPE_9:
      applica_modello_drive(9, item->value);
      return 1;
    case MENU_PLUS4_3PLUS1_ROMS:
      if (item->value) {
         strcpy (c0_lo_item->str_value,"/PLUS4EMU/3plus1.rom");
         strcpy (c0_lo_item->displayed_value,"/PLUS4EMU/3plus1.rom");
         c0_lo_offset_item->value = 0;
         strcpy (c0_hi_item->str_value,"/PLUS4EMU/3plus1.rom");
         strcpy (c0_hi_item->displayed_value,"/PLUS4EMU/3plus1.rom");
         c0_hi_offset_item->value = 16384;
      } else {
         strcpy (c0_lo_item->str_value,"");
         strcpy (c0_lo_item->displayed_value,"");
         c0_lo_offset_item->value = 0;
         strcpy (c0_hi_item->str_value,"");
         strcpy (c0_hi_item->displayed_value,"");
         c0_hi_offset_item->value = 0;
      }






      if (!carica_banco_cartuccia(2, c0_lo_item, c0_lo_offset_item->value) ||
          !carica_banco_cartuccia(3, c0_hi_item, c0_hi_offset_item->value)) {
         item->value = 0;
         c0_lo_item->str_value[0] = '\0';
         c0_lo_item->displayed_value[0] = '\0';
         c0_lo_offset_item->value = 0;
         c0_hi_item->str_value[0] = '\0';
         c0_hi_item->displayed_value[0] = '\0';
         c0_hi_offset_item->value = 0;
         Plus4VM_LoadROM(vm, 2, "", 0);
         Plus4VM_LoadROM(vm, 3, "", 0);
         ui_error("Need /PLUS4EMU/3plus1.rom - 32K, lo then hi");
         return 1;
      }
      reset_freddo();
      return 1;
    case MENU_KEYBOARD_MAPPING:
      aggiorna_voce_layout();
      aggiorna_tastiera_del_menu();
      load_keymap();
      machine_kbd_init();
      return 1;
    case MENU_KEYBOARD_LAYOUT:
      aggiorna_tastiera_del_menu();
      load_keymap();
      machine_kbd_init();
      return 1;
    default:
      return 0;
  }
}

int emux_handle_quick_func(int button_func, fullpath_func fullpath) {
  return 0;
}

// For Plus4emu, we grab additional settings from the same txt file.
void emux_load_additional_settings() {
  // NOTE: This is called before any menu items have been constructed.

  strcpy(rom_c0_lo, "");
  strcpy(rom_c0_hi, "");
  strcpy(rom_c1_lo, "");
  strcpy(rom_c1_hi, "");
  strcpy(rom_c2_lo, "");
  strcpy(rom_c2_hi, "");

  FILE *fp;
  fp = fopen("/settings-plus4emu.txt", "r");
  if (fp == NULL) {
     return;
  }

  char name_value[256];
  size_t len;
  int value;
  int usb_btn_0_i = 0;
  int usb_btn_1_i = 0;
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

    if (strcmp(name,"sid_model") == 0) {
       sid_model = value;
    } else if (strcmp(name,"sid_card") == 0) {
       sid_card = value;
    } else if (strcmp(name,"sid_write_access") == 0) {
       sid_write_access = value;
    } else if (strcmp(name,"sid_digiblaster") == 0) {
       sid_digiblaster = value;
    } else if (strcmp(name,"reset_tape_with_cpu") == 0) {
       reset_tape_with_cpu = value;
    } else if (strcmp(name,"tape_feedback") == 0) {
       tape_feedback = value;
    } else if (strcmp(name,"ram_size") == 0) {
       ram_size = value;
    } else if (strcmp(name,"drive_model_8") == 0) {
       drive_model_8 = value;
    } else if (strcmp(name,"drive_model_9") == 0) {
       drive_model_9 = value;
    } else if (strcmp(name,"iec_8") == 0) {
       iec_8 = value ? 1 : 0;
    } else if (strcmp(name,"iec_9") == 0) {
       iec_9 = value ? 1 : 0;
    } else if (strcmp(name,"attach_3plus1_roms") == 0) {
       attach_3plus1_roms = value;
    } else if (strcmp(name,"rom_c0_lo") == 0) {
       strcpy(rom_c0_lo, value_str);
    } else if (strcmp(name,"rom_c0_hi") == 0) {
       strcpy(rom_c0_hi, value_str);
    } else if (strcmp(name,"rom_c1_lo") == 0) {
       strcpy(rom_c1_lo, value_str);
    } else if (strcmp(name,"rom_c1_hi") == 0) {
       strcpy(rom_c1_hi, value_str);
    } else if (strcmp(name,"rom_c2_lo") == 0) {
       strcpy(rom_c2_lo, value_str);
    } else if (strcmp(name,"rom_c2_hi") == 0) {
       strcpy(rom_c2_hi, value_str);
    } else if (strcmp(name,"rom_c0_lo_off") == 0) {
       rom_c0_lo_off = value;
    } else if (strcmp(name,"rom_c0_hi_off") == 0) {
       rom_c0_hi_off = value;
    } else if (strcmp(name,"rom_c1_lo_off") == 0) {
       rom_c1_lo_off = value;
    } else if (strcmp(name,"rom_c1_hi_off") == 0) {
       rom_c1_hi_off = value;
    } else if (strcmp(name,"rom_c2_lo_off") == 0) {
       rom_c2_lo_off = value;
    } else if (strcmp(name,"rom_c2_hi_off") == 0) {
       rom_c2_hi_off = value;
    } else if (strcmp(name,"color_brightness") == 0) {
       color_brightness = value;
    } else if (strcmp(name,"color_contrast") == 0) {
       color_contrast = value;
    } else if (strcmp(name,"color_gamma") == 0) {
       color_gamma = value;
    } else if (strcmp(name,"color_tint") == 0) {
       color_tint = value;
    } else if (strcmp(name,"keyboard_mapping") == 0) {
       keyboard_mapping = value;
    } else if (strcmp(name,"keyboard_layout") == 0) {
       keyboard_layout = value;
    } else if (strcmp(name,"crt_filter") == 0) {
       crt_filter = value;
    }
  }

  fclose(fp);
}

void emux_save_additional_settings(FILE *fp) {
  fprintf (fp,"sid_model=%d\n", sid_model_item->value);
  fprintf (fp,"sid_card=%d\n", sid_card_item->value);
  fprintf (fp,"sid_write_access=%d\n", sid_write_access_item->value);
  fprintf (fp,"sid_digiblaster=%d\n", sid_digiblaster_item->value);
  fprintf (fp,"reset_tape_with_cpu=%d\n", reset_tape_with_cpu);
  fprintf (fp,"tape_feedback=%d\n", tape_feedback_item->value);
  fprintf (fp,"ram_size=%d\n", ram_size_item->choice_ints[ram_size_item->value]);
  fprintf (fp,"drive_model_8=%d\n", drive_model_8_item->value);




  if (drive_model_9_item != NULL) {
    fprintf (fp,"drive_model_9=%d\n", drive_model_9_item->value);
  }

  fprintf (fp,"iec_8=%d\n", iec_8);
  fprintf (fp,"iec_9=%d\n", iec_9);
  fprintf (fp,"attach_3plus1_roms=%d\n", attach_3plus1_roms_item->value);
  if (strlen(c0_lo_item->str_value) > 0) {
     fprintf (fp,"rom_c0_lo=%s\n", c0_lo_item->str_value);
  }
  if (strlen(c0_hi_item->str_value) > 0) {
     fprintf (fp,"rom_c0_hi=%s\n", c0_hi_item->str_value);
  }
  if (strlen(c1_lo_item->str_value) > 0) {
     fprintf (fp,"rom_c1_lo=%s\n", c1_lo_item->str_value);
  }
  if (strlen(c1_hi_item->str_value) > 0) {
     fprintf (fp,"rom_c1_hi=%s\n", c1_hi_item->str_value);
  }
  if (strlen(c2_lo_item->str_value) > 0) {
     fprintf (fp,"rom_c2_lo=%s\n", c2_lo_item->str_value);
  }
  if (strlen(c2_hi_item->str_value) > 0) {
     fprintf (fp,"rom_c2_hi=%s\n", c2_hi_item->str_value);
  }
  fprintf (fp,"rom_c0_lo_off=%d\n", c0_lo_offset_item->value);
  fprintf (fp,"rom_c0_hi_off=%d\n", c0_hi_offset_item->value);
  fprintf (fp,"rom_c1_lo_off=%d\n", c1_lo_offset_item->value);
  fprintf (fp,"rom_c1_hi_off=%d\n", c1_hi_offset_item->value);
  fprintf (fp,"rom_c2_lo_off=%d\n", c2_lo_offset_item->value);
  fprintf (fp,"rom_c2_hi_off=%d\n", c2_hi_offset_item->value);
  fprintf (fp,"color_brightness=%d\n", color_brightness);
  fprintf (fp,"color_contrast=%d\n", color_contrast);
  fprintf (fp,"color_gamma=%d\n", color_gamma);
  fprintf (fp,"tintcolor_=%d\n", color_tint);
  fprintf (fp,"keyboard_mapping=%d\n", keyboard_mapping_item->value);
  fprintf (fp,"keyboard_layout=%d\n", keyboard_layout_item->value);
  fprintf (fp,"crt_filter=%d\n", crt_filter);
}

void emux_get_default_color_setting(int *brightness, int *contrast,
                                    int *gamma, int *tint, int *saturation) {
  *brightness = 1000;
  *contrast = 666;
  *gamma = 800;
  *tint = 1000;
  *saturation = 1000;
}

int emux_handle_loaded_setting(char *name, char* value_str, int value) {
  return 0;
}

void emux_menu_about_to_activate(void) {}

void emux_load_settings_done(void) {





  applica_modello_drive(8, drive_model_8);
  applica_modello_drive(9, drive_model_9);
}

static void init_video(void) {
  if (is_ntsc()) {
     vertical_res = 242 * raster_skip;
     raster_low = 18 * raster_skip;
  } else {
     vertical_res = 288 * raster_skip;
     raster_low = 0;
  }

  // BMC64 Video Init

  if (circle_alloc_fbl(FB_LAYER_VIC, 0  , &fb_buf,
                              384, vertical_res, &fb_pitch)) {
    printf ("Failed to create video buf.\n");
    assert(0);
  }
  circle_clear_fbl(FB_LAYER_VIC);
  circle_show_fbl(FB_LAYER_VIC);
  p4_prepara_tavolozza();

  canvas_state[VIC_INDEX].gfx_w = 40*8;
  canvas_state[VIC_INDEX].gfx_h = 25*8 * raster_skip;
  canvas_state[VIC_INDEX].raster_skip = raster_skip;

  if (is_ntsc()) {
    canvas_state[VIC_INDEX].max_padding_w = 0;
    canvas_state[VIC_INDEX].max_padding_h = 0;
    canvas_state[VIC_INDEX].max_border_w = 32;
    canvas_state[VIC_INDEX].max_border_h = 22 * raster_skip;
    time_advance = 1666;
    Plus4VM_SetVideoClockFrequency(vm, 14318180);
    strcpy(rom_kernal,"/PLUS4EMU/p4_ntsc.rom");
    Plus4VideoDecoder_SetNTSCMode(videoDecoder, 1);
  } else {
    canvas_state[VIC_INDEX].max_padding_w = 0;
    canvas_state[VIC_INDEX].max_padding_h = 0;
    canvas_state[VIC_INDEX].max_border_w = 32;
    canvas_state[VIC_INDEX].max_border_h = 40 * raster_skip;
    time_advance = 2000;
    Plus4VM_SetVideoClockFrequency(vm, 17734475);
    strcpy(rom_kernal,"/PLUS4EMU/p4kernal.rom");
    Plus4VideoDecoder_SetNTSCMode(videoDecoder, 0);
  }
}

void emux_add_userport_joys(struct menu_item* parent) {
}

uint8_t circle_get_userport_ddr(void) {
  return 0;
}

uint8_t circle_get_userport(void) {
  return 0xff;
}

void circle_set_userport(uint8_t value) {
}

