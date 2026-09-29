


























#include "menu_text_layout.h"

#include <stddef.h>

#include "keycodes.h"
#include "menu.h"

typedef struct {
  long key;
  char normal;
  char shifted;
} menu_text_key_t;


static const menu_text_key_t us_text_keys[] = {
 {KEYCODE_1, '1', '!'}, {KEYCODE_2, '2', '@'}, {KEYCODE_3, '3', '#'},
 {KEYCODE_4, '4', '$'}, {KEYCODE_5, '5', '%'}, {KEYCODE_6, '6', '^'},
 {KEYCODE_7, '7', '&'}, {KEYCODE_8, '8', '*'}, {KEYCODE_9, '9', '('},
 {KEYCODE_0, '0', ')'}, {KEYCODE_Dash, '-', '_'}, {KEYCODE_Equals, '=', '+'},
 {KEYCODE_LeftBracket, '[', '{'}, {KEYCODE_RightBracket, ']', '}'},
 {KEYCODE_BackSlash, '\\', '|'}, {KEYCODE_SemiColon, ';', ':'},
 {KEYCODE_SingleQuote, '\'', '"'}, {KEYCODE_BackQuote, '`', '~'},
 {KEYCODE_Comma, ',', '<'}, {KEYCODE_Period, '.', '>'},
 {KEYCODE_Slash, '/', '?'}, {KEYCODE_Space, ' ', ' '},
};




static const menu_text_key_t it_text_keys[] = {
 {KEYCODE_1, '1', '!'}, {KEYCODE_2, '2', '"'}, {KEYCODE_3, '3', '\0'},
 {KEYCODE_4, '4', '$'}, {KEYCODE_5, '5', '%'}, {KEYCODE_6, '6', '&'},
 {KEYCODE_7, '7', '/'}, {KEYCODE_8, '8', '('}, {KEYCODE_9, '9', ')'},
 {KEYCODE_0, '0', '='}, {KEYCODE_Dash, '\'', '?'},
 {KEYCODE_Equals, '\0', '^'},
 {KEYCODE_LeftBracket, '\0', '\0'}, {KEYCODE_RightBracket, '+', '*'},
 {KEYCODE_SemiColon, '\0', '\0'}, {KEYCODE_SingleQuote, '\0', '\0'},
 {KEYCODE_BackSlash, '\0', '\0'}, {KEYCODE_BackQuote, '\\', '|'},
 {KEYCODE_KP_BackSlash, '<', '>'},
 {KEYCODE_Comma, ',', ';'}, {KEYCODE_Period, '.', ':'},
 {KEYCODE_Slash, '-', '_'}, {KEYCODE_Space, ' ', ' '},
};



static const menu_text_key_t uk_text_keys[] = {
 {KEYCODE_1, '1', '!'}, {KEYCODE_2, '2', '"'}, {KEYCODE_3, '3', '\0'},
 {KEYCODE_4, '4', '$'}, {KEYCODE_5, '5', '%'}, {KEYCODE_6, '6', '^'},
 {KEYCODE_7, '7', '&'}, {KEYCODE_8, '8', '*'}, {KEYCODE_9, '9', '('},
 {KEYCODE_0, '0', ')'}, {KEYCODE_Dash, '-', '_'}, {KEYCODE_Equals, '=', '+'},
 {KEYCODE_LeftBracket, '[', '{'}, {KEYCODE_RightBracket, ']', '}'},
 {KEYCODE_BackSlash, '#', '~'}, {KEYCODE_SemiColon, ';', ':'},
 {KEYCODE_SingleQuote, '\'', '@'}, {KEYCODE_BackQuote, '`', '\0'},
 {KEYCODE_KP_BackSlash, '\\', '|'},
 {KEYCODE_Comma, ',', '<'}, {KEYCODE_Period, '.', '>'},
 {KEYCODE_Slash, '/', '?'}, {KEYCODE_Space, ' ', ' '},
};




static const menu_text_key_t de_text_keys[] = {
 {KEYCODE_1, '1', '!'}, {KEYCODE_2, '2', '"'}, {KEYCODE_3, '3', '\0'},
 {KEYCODE_4, '4', '$'}, {KEYCODE_5, '5', '%'}, {KEYCODE_6, '6', '&'},
 {KEYCODE_7, '7', '/'}, {KEYCODE_8, '8', '('}, {KEYCODE_9, '9', ')'},
 {KEYCODE_0, '0', '='}, {KEYCODE_Dash, '\0', '?'},
 {KEYCODE_Equals, '\0', '`'},
 {KEYCODE_LeftBracket, '\0', '\0'}, {KEYCODE_RightBracket, '+', '*'},
 {KEYCODE_SemiColon, '\0', '\0'}, {KEYCODE_SingleQuote, '\0', '\0'},
 {KEYCODE_BackSlash, '#', '\''}, {KEYCODE_BackQuote, '^', '\0'},
 {KEYCODE_KP_BackSlash, '<', '>'},
 {KEYCODE_Comma, ',', ';'}, {KEYCODE_Period, '.', ':'},
 {KEYCODE_Slash, '-', '_'},
 {KEYCODE_y, 'z', 'Z'}, {KEYCODE_z, 'y', 'Y'},
 {KEYCODE_Space, ' ', ' '},
};



static const menu_text_key_t positional_text_keys[] = {
 {KEYCODE_1, '1', '!'}, {KEYCODE_2, '2', '"'}, {KEYCODE_3, '3', '#'},
 {KEYCODE_4, '4', '$'}, {KEYCODE_5, '5', '%'}, {KEYCODE_6, '6', '&'},
 {KEYCODE_7, '7', '\''}, {KEYCODE_8, '8', '('}, {KEYCODE_9, '9', ')'},
 {KEYCODE_0, '0', '0'}, {KEYCODE_Dash, '+', '+'}, {KEYCODE_Equals, '-', '-'},
 {KEYCODE_LeftBracket, '@', '@'}, {KEYCODE_RightBracket, '*', '*'},
 {KEYCODE_BackSlash, '=', '='}, {KEYCODE_SemiColon, ':', '['},
 {KEYCODE_SingleQuote, ';', ']'}, {KEYCODE_Comma, ',', '<'},
 {KEYCODE_Period, '.', '>'}, {KEYCODE_Slash, '/', '?'},
 {KEYCODE_Space, ' ', ' '},
};


static const menu_text_key_t maxi_text_keys[] = {
 {KEYCODE_1, '1', '!'}, {KEYCODE_2, '2', '"'}, {KEYCODE_3, '3', '#'},
 {KEYCODE_4, '4', '$'}, {KEYCODE_5, '5', '%'}, {KEYCODE_6, '6', '&'},
 {KEYCODE_7, '7', '\''}, {KEYCODE_8, '8', '('}, {KEYCODE_9, '9', ')'},
 {KEYCODE_0, '0', '0'}, {KEYCODE_KP_Add, '+', '+'},
 {KEYCODE_KP_Subtract, '-', '-'}, {KEYCODE_LeftBracket, ':', '['},
 {KEYCODE_SemiColon, '*', '*'}, {KEYCODE_BackSlash, '@', '@'},
 {KEYCODE_Equals, '=', '='}, {KEYCODE_RightBracket, ';', ']'},
 {KEYCODE_Comma, ',', '<'}, {KEYCODE_Period, '.', '>'},
 {KEYCODE_Slash, '/', '?'}, {KEYCODE_Space, ' ', ' '},
};

#define QUANTI(t) ((size_t)(sizeof(t) / sizeof((t)[0])))

char menu_text_layout_key_to_char(long key, int shifted,
                                  int keyboard_mapping, int keyboard_layout) {
  const menu_text_key_t *text_keys = us_text_keys;
  size_t text_key_count = QUANTI(us_text_keys);
  size_t index;

  if (keyboard_mapping == KEYBOARD_MAPPING_POS) {
    text_keys = positional_text_keys;
    text_key_count = QUANTI(positional_text_keys);
  } else if (keyboard_mapping == KEYBOARD_MAPPING_MAXI) {
    text_keys = maxi_text_keys;
    text_key_count = QUANTI(maxi_text_keys);
  } else {


    switch (keyboard_layout) {
    case MENU_TEXT_LAYOUT_IT:
      text_keys = it_text_keys;
      text_key_count = QUANTI(it_text_keys);
      break;
    case MENU_TEXT_LAYOUT_UK:
      text_keys = uk_text_keys;
      text_key_count = QUANTI(uk_text_keys);
      break;
    case MENU_TEXT_LAYOUT_DE:
      text_keys = de_text_keys;
      text_key_count = QUANTI(de_text_keys);
      break;
    default:
      break;
    }
  }

  for (index = 0; index < text_key_count; index++) {
    if (text_keys[index].key == key) {
      return shifted ? text_keys[index].shifted : text_keys[index].normal;
    }
  }
  return '\0';
}
