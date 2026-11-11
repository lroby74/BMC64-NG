/*
 * keycodes.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "keycodes.h"

// Must default these to sane values in case the emulator
// does not start and we do not discover what these
// keys are supposed to be.
signed long commodore_key_sym = KEYCODE_LeftControl;
signed long restore_key_sym = KEYCODE_PrintScreen;
signed long ctrl_key_sym = KEYCODE_Tab;
int commodore_key_sym_set;
int restore_key_sym_set;
int ctrl_key_sym_set;


raw_keycode_func_t raw_keycode_func = 0;

const char* keycode_to_string(long keycode) {
  switch (keycode) {
    case KEYCODE_a:
       return "a";
    case KEYCODE_b:
       return "b";
    case KEYCODE_c:
       return "c";
    case KEYCODE_d:
       return "d";
    case KEYCODE_e:
       return "e";
    case KEYCODE_f:
       return "f";
    case KEYCODE_g:
       return "g";
    case KEYCODE_h:
       return "h";
    case KEYCODE_i:
       return "i";
    case KEYCODE_j:
       return "j";
    case KEYCODE_k:
       return "k";
    case KEYCODE_l:
       return "l";
    case KEYCODE_m:
       return "m";
    case KEYCODE_n:
       return "n";
    case KEYCODE_o:
       return "o";
    case KEYCODE_p:
       return "p";
    case KEYCODE_q:
       return "q";
    case KEYCODE_r:
       return "r";
    case KEYCODE_s:
       return "s";
    case KEYCODE_t:
       return "t";
    case KEYCODE_u:
       return "u";
    case KEYCODE_v:
       return "v";
    case KEYCODE_w:
       return "w";
    case KEYCODE_x:
       return "x";
    case KEYCODE_y:
       return "y";
    case KEYCODE_z:
       return "z";
    case KEYCODE_1:
       return "1";
    case KEYCODE_2:
       return "2";
    case KEYCODE_3:
       return "3";
    case KEYCODE_4:
       return "4";
    case KEYCODE_5:
       return "5";
    case KEYCODE_6:
       return "6";
    case KEYCODE_7:
       return "7";
    case KEYCODE_8:
       return "8";
    case KEYCODE_9:
       return "9";
    case KEYCODE_0:
       return "0";
    case KEYCODE_Return:
       return "Return";
    case KEYCODE_Escape:
       return "Esc";
    case KEYCODE_Backspace:
       return "Backspace";
    case KEYCODE_Tab:
       return "Tab";
    case KEYCODE_Space:
       return "Space";
    case KEYCODE_Dash:
       return "-";
    case KEYCODE_Equals:
       return "=";
    case KEYCODE_LeftBracket:
       return "{";
    case KEYCODE_RightBracket:
       return "}";
    case KEYCODE_BackSlash:
       return "\\";
    case KEYCODE_Pound:
       return "Pound";
    case KEYCODE_SemiColon:
       return ";";
    case KEYCODE_SingleQuote:
       return "'";
    case KEYCODE_BackQuote:
       return "`";
    case KEYCODE_Comma:
       return ",";
    case KEYCODE_Period:
       return ".";
    case KEYCODE_Slash:
       return "/";
    case KEYCODE_CapsLock:
       return "CapsLock";
    case KEYCODE_F1:
       return "F1";
    case KEYCODE_F2:
       return "F2";
    case KEYCODE_F3:
       return "F3";
    case KEYCODE_F4:
       return "F4";
    case KEYCODE_F5:
       return "F5";
    case KEYCODE_F6:
       return "F6";
    case KEYCODE_F7:
       return "F7";
    case KEYCODE_F8:
       return "F8";
    case KEYCODE_F9:
       return "F9";
    case KEYCODE_F10:
       return "F10";
    case KEYCODE_F11:
       return "F11";
    case KEYCODE_F12:
       return "F12";
    case KEYCODE_ScrollLock:
       return "ScrollLock";
    case KEYCODE_Pause:
       return "Pause";
    case KEYCODE_Insert:
       return "Insert";
    case KEYCODE_Home:
       return "Home";
    case KEYCODE_PrintScreen:
       return "PrtScn";
    case KEYCODE_PageUp:
       return "PgUp";
    case KEYCODE_Delete:
       return "Del";
    case KEYCODE_End:
       return "End";
    case KEYCODE_PageDown:
       return "PgDown";
    case KEYCODE_Right:
       return "Right";
    case KEYCODE_Left:
       return "Left";
    case KEYCODE_Down:
       return "Down";
    case KEYCODE_Up:
       return "Up";
    case KEYCODE_NumLock:
       return "NumLock";
    case KEYCODE_KP_Divide:
       return "KP Divide";
    case KEYCODE_KP_Multiply:
       return "KP Multiply";
    case KEYCODE_KP_Subtract:
       return "KP Subtract";
    case KEYCODE_KP_Add:
       return "KP Add";
    case KEYCODE_KP_Enter:
       return "KP Enter";
    case KEYCODE_KP1:
       return "KP 1";
    case KEYCODE_KP2:
       return "KP 2";
    case KEYCODE_KP3:
       return "KP 3";
    case KEYCODE_KP4:
       return "KP 4";
    case KEYCODE_KP5:
       return "KP 5";
    case KEYCODE_KP6:
       return "KP 6";
    case KEYCODE_KP7:
       return "KP 7";
    case KEYCODE_KP8:
       return "KP 8";
    case KEYCODE_KP9:
       return "KP 9";
    case KEYCODE_KP0:
       return "KP 0";
    case KEYCODE_KP_Decimal:
       return "KP Decimal";
    case KEYCODE_KP_BackSlash:
       return "KP BackSlash";
    case KEYCODE_Application:
       return "App";
    case KEYCODE_LeftShift:
       return "LeftShift";
    case KEYCODE_RightShift:
       return "RightShift";
    case KEYCODE_LeftControl:
       return "LeftControl";
    case KEYCODE_RightControl:
       return "RightControl";
    case KEYCODE_LeftAlt:
       return "LeftAlt";
    case KEYCODE_RightAlt:
       return "RightAlt";
    case KEYCODE_LeftSuper:
       return "LeftSuper";
    case KEYCODE_RightSuper:
       return "RightSuper";
    default:



       {
         static char sconosciuto[16];
         sprintf(sconosciuto, "HID 0x%02X", (unsigned)(keycode & 0xffff));
         return sconosciuto;
       }
  }
}




int keycode_format_keycap(long keycode, int german_layout,
                          char *buffer, size_t buffer_size) {
  const char *label = NULL;
  int written;

  if (buffer == NULL || buffer_size == 0) {
    return 0;
  }

  if (german_layout) {
    switch (keycode) {
      case KEYCODE_y: label = "z"; break;
      case KEYCODE_z: label = "y"; break;
      case KEYCODE_Dash: label = "\xDF"; break;       /* sharp s */
      case KEYCODE_Equals: label = "\xB4"; break;     /* acute */
      case KEYCODE_LeftBracket: label = "\xFC"; break;
      case KEYCODE_RightBracket: label = "+"; break;
      case KEYCODE_BackSlash:
      case KEYCODE_Pound: label = "#"; break;
      case KEYCODE_SemiColon: label = "\xF6"; break;
      case KEYCODE_SingleQuote: label = "\xE4"; break;
      case KEYCODE_BackQuote: label = "^"; break;
      case KEYCODE_Slash: label = "-"; break;
      case KEYCODE_KP_BackSlash: label = "<"; break;
      default: break;
    }
  } else {
    switch (keycode) {
      case KEYCODE_LeftBracket: label = "["; break;
      case KEYCODE_RightBracket: label = "]"; break;
      case KEYCODE_BackSlash: label = "\\"; break;
      case KEYCODE_Pound: label = "#"; break;
      default: break;
    }
  }

  if (label == NULL) {
    label = keycode_to_string(keycode);
  }
  if (label == NULL || *label == '\0' || strcmp(label, "undefined") == 0) {
    return keycode_format_vkm_token(keycode, buffer, buffer_size);
  }
  written = snprintf(buffer, buffer_size, "%s", label);
  return written >= 0 && (size_t)written < buffer_size;
}

int keycode_format_vkm_token(long keycode, char *buffer, size_t buffer_size) {
  const char *token = NULL;
  int written;

  if (buffer == NULL || buffer_size == 0) {
    return 0;
  }

  switch (keycode) {
    case KEYCODE_a: token = "a"; break;
    case KEYCODE_b: token = "b"; break;
    case KEYCODE_c: token = "c"; break;
    case KEYCODE_d: token = "d"; break;
    case KEYCODE_e: token = "e"; break;
    case KEYCODE_f: token = "f"; break;
    case KEYCODE_g: token = "g"; break;
    case KEYCODE_h: token = "h"; break;
    case KEYCODE_i: token = "i"; break;
    case KEYCODE_j: token = "j"; break;
    case KEYCODE_k: token = "k"; break;
    case KEYCODE_l: token = "l"; break;
    case KEYCODE_m: token = "m"; break;
    case KEYCODE_n: token = "n"; break;
    case KEYCODE_o: token = "o"; break;
    case KEYCODE_p: token = "p"; break;
    case KEYCODE_q: token = "q"; break;
    case KEYCODE_r: token = "r"; break;
    case KEYCODE_s: token = "s"; break;
    case KEYCODE_t: token = "t"; break;
    case KEYCODE_u: token = "u"; break;
    case KEYCODE_v: token = "v"; break;
    case KEYCODE_w: token = "w"; break;
    case KEYCODE_x: token = "x"; break;
    case KEYCODE_y: token = "y"; break;
    case KEYCODE_z: token = "z"; break;
    case KEYCODE_1: token = "1"; break;
    case KEYCODE_2: token = "2"; break;
    case KEYCODE_3: token = "3"; break;
    case KEYCODE_4: token = "4"; break;
    case KEYCODE_5: token = "5"; break;
    case KEYCODE_6: token = "6"; break;
    case KEYCODE_7: token = "7"; break;
    case KEYCODE_8: token = "8"; break;
    case KEYCODE_9: token = "9"; break;
    case KEYCODE_0: token = "0"; break;
    case KEYCODE_Return: token = "Return"; break;
    case KEYCODE_Escape: token = "Escape"; break;
    case KEYCODE_Backspace: token = "BackSpace"; break;
    case KEYCODE_Tab: token = "Tab"; break;
    case KEYCODE_Space: token = "Space"; break;
    case KEYCODE_Dash: token = "Dash"; break;
    case KEYCODE_Equals: token = "Equals"; break;
    case KEYCODE_LeftBracket: token = "LeftBracket"; break;
    case KEYCODE_RightBracket: token = "RightBracket"; break;
    case KEYCODE_BackSlash: token = "BackSlash"; break;
    case KEYCODE_Pound: token = "Pound"; break;
    case KEYCODE_SemiColon: token = "SemiColon"; break;
    case KEYCODE_SingleQuote: token = "SingleQuote"; break;
    case KEYCODE_BackQuote: token = "BackQuote"; break;
    case KEYCODE_Comma: token = "Comma"; break;
    case KEYCODE_Period: token = "Period"; break;
    case KEYCODE_Slash: token = "Slash"; break;
    case KEYCODE_CapsLock: token = "CapsLock"; break;
    case KEYCODE_F1: token = "F1"; break;
    case KEYCODE_F2: token = "F2"; break;
    case KEYCODE_F3: token = "F3"; break;
    case KEYCODE_F4: token = "F4"; break;
    case KEYCODE_F5: token = "F5"; break;
    case KEYCODE_F6: token = "F6"; break;
    case KEYCODE_F7: token = "F7"; break;
    case KEYCODE_F8: token = "F8"; break;
    case KEYCODE_F9: token = "F9"; break;
    case KEYCODE_F10: token = "F10"; break;
    case KEYCODE_F11: token = "F11"; break;
    case KEYCODE_F12: token = "F12"; break;
    case KEYCODE_PrintScreen: token = "PrtScn"; break;
    case KEYCODE_ScrollLock: token = "ScrollLock"; break;
    case KEYCODE_Pause: token = "Pause"; break;
    case KEYCODE_Insert: token = "Insert"; break;
    case KEYCODE_Home: token = "Home"; break;
    case KEYCODE_PageUp: token = "PageUp"; break;
    case KEYCODE_Delete: token = "Del"; break;
    case KEYCODE_End: token = "End"; break;
    case KEYCODE_PageDown: token = "PageDown"; break;
    case KEYCODE_Right: token = "Right"; break;
    case KEYCODE_Left: token = "Left"; break;
    case KEYCODE_Down: token = "Down"; break;
    case KEYCODE_Up: token = "Up"; break;
    case KEYCODE_NumLock: token = "NumLock"; break;
    case KEYCODE_KP_Divide: token = "KP_Divide"; break;
    case KEYCODE_KP_Multiply: token = "KP_Multiply"; break;
    case KEYCODE_KP_Subtract: token = "KP_Subtract"; break;
    case KEYCODE_KP_Add: token = "KP_Add"; break;
    case KEYCODE_KP_Enter: token = "KP_Enter"; break;
    case KEYCODE_KP1: token = "KP_1"; break;
    case KEYCODE_KP2: token = "KP_2"; break;
    case KEYCODE_KP3: token = "KP_3"; break;
    case KEYCODE_KP4: token = "KP_4"; break;
    case KEYCODE_KP5: token = "KP_5"; break;
    case KEYCODE_KP6: token = "KP_6"; break;
    case KEYCODE_KP7: token = "KP_7"; break;
    case KEYCODE_KP8: token = "KP_8"; break;
    case KEYCODE_KP9: token = "KP_9"; break;
    case KEYCODE_KP0: token = "KP_0"; break;
    case KEYCODE_KP_Decimal: token = "KP_Decimal"; break;
    case KEYCODE_KP_BackSlash: token = "KP_BackSlash"; break;
    case KEYCODE_Application: token = "Application"; break;
    case KEYCODE_LeftShift: token = "Shift_L"; break;
    case KEYCODE_RightShift: token = "Shift_R"; break;
    case KEYCODE_LeftControl: token = "Control_L"; break;
    case KEYCODE_RightControl: token = "Control_R"; break;
    case KEYCODE_LeftAlt: token = "Alt_L"; break;
    case KEYCODE_RightAlt: token = "Alt_R"; break;
    case KEYCODE_LeftSuper: token = "Super_L"; break;
    case KEYCODE_RightSuper: token = "Super_R"; break;
    default: break;
  }

  if (token != NULL) {
    written = snprintf(buffer, buffer_size, "%s", token);
    return written >= 0 && (size_t)written < buffer_size;
  }
  if (keycode > 0 && keycode <= 0xff) {
    written = snprintf(buffer, buffer_size, "HID_%02lX", keycode);
    return written >= 0 && (size_t)written < buffer_size;
  }

  buffer[0] = '\0';
  return 0;
}

long keycode_from_vkm_token(const char *token) {
  char *end = NULL;
  long number;

  if (token == NULL || *token == '\0') {
    return -1;
  }
  if (token[1] == '\0') {
    if (token[0] >= 'a' && token[0] <= 'z') {
      return KEYCODE_a + token[0] - 'a';
    }
    if (token[0] >= '1' && token[0] <= '9') {
      return KEYCODE_1 + token[0] - '1';
    }
    if (token[0] == '0') {
      return KEYCODE_0;
    }
  }
  if (strncmp(token, "HID_", 4) == 0 && strlen(token) == 6) {
    number = strtol(token + 4, &end, 16);
    return end == token + 6 && number > 0 && number <= 0xff ? number : -1;
  }
  if (token[0] == 'F') {
    number = strtol(token + 1, &end, 10);
    if (*end == '\0' && number >= 1 && number <= 12) {
      return KEYCODE_F1 + number - 1;
    }
  }
  if (strncmp(token, "KP_", 3) == 0 && strlen(token) == 4) {
    if (token[3] >= '1' && token[3] <= '9') {
      return KEYCODE_KP1 + token[3] - '1';
    }
    if (token[3] == '0') {
      return KEYCODE_KP0;
    }
  }

#define TOKEN(name, keycode) if (strcmp(token, name) == 0) return keycode
  TOKEN("Return", KEYCODE_Return);
  TOKEN("Escape", KEYCODE_Escape);
  TOKEN("BackSpace", KEYCODE_Backspace);
  TOKEN("Tab", KEYCODE_Tab);
  TOKEN("Space", KEYCODE_Space);
  TOKEN("Dash", KEYCODE_Dash);
  TOKEN("Equals", KEYCODE_Equals);
  TOKEN("LeftBracket", KEYCODE_LeftBracket);
  TOKEN("RightBracket", KEYCODE_RightBracket);
  TOKEN("BackSlash", KEYCODE_BackSlash);
  TOKEN("Pound", KEYCODE_Pound);
  TOKEN("SemiColon", KEYCODE_SemiColon);
  TOKEN("SingleQuote", KEYCODE_SingleQuote);
  TOKEN("BackQuote", KEYCODE_BackQuote);
  TOKEN("Comma", KEYCODE_Comma);
  TOKEN("Period", KEYCODE_Period);
  TOKEN("Slash", KEYCODE_Slash);
  TOKEN("CapsLock", KEYCODE_CapsLock);
  TOKEN("PrtScn", KEYCODE_PrintScreen);
  TOKEN("ScrollLock", KEYCODE_ScrollLock);
  TOKEN("Pause", KEYCODE_Pause);
  TOKEN("Insert", KEYCODE_Insert);
  TOKEN("Home", KEYCODE_Home);
  TOKEN("PageUp", KEYCODE_PageUp);
  TOKEN("Del", KEYCODE_Delete);
  TOKEN("End", KEYCODE_End);
  TOKEN("PageDown", KEYCODE_PageDown);
  TOKEN("Right", KEYCODE_Right);
  TOKEN("Left", KEYCODE_Left);
  TOKEN("Down", KEYCODE_Down);
  TOKEN("Up", KEYCODE_Up);
  TOKEN("NumLock", KEYCODE_NumLock);
  TOKEN("KP_Divide", KEYCODE_KP_Divide);
  TOKEN("KP_Multiply", KEYCODE_KP_Multiply);
  TOKEN("KP_Subtract", KEYCODE_KP_Subtract);
  TOKEN("KP_Add", KEYCODE_KP_Add);
  TOKEN("KP_Enter", KEYCODE_KP_Enter);
  TOKEN("KP_Decimal", KEYCODE_KP_Decimal);
  TOKEN("KP_BackSlash", KEYCODE_KP_BackSlash);
  TOKEN("Application", KEYCODE_Application);
  TOKEN("Shift_L", KEYCODE_LeftShift);
  TOKEN("Shift_R", KEYCODE_RightShift);
  TOKEN("Control_L", KEYCODE_LeftControl);
  TOKEN("Control_R", KEYCODE_RightControl);
  TOKEN("Alt_L", KEYCODE_LeftAlt);
  TOKEN("Alt_R", KEYCODE_RightAlt);
  TOKEN("Super_L", KEYCODE_LeftSuper);
  TOKEN("Super_R", KEYCODE_RightSuper);
  /* Legacy spellings still present in shipped and third-party maps. */
  TOKEN("Delete", KEYCODE_PageUp);
  TOKEN("Apostrophe", KEYCODE_SingleQuote);
  TOKEN("ISO_Left_Tab", KEYCODE_Tab);
  TOKEN("KP_Tab", KEYCODE_Tab);
  TOKEN("LP_Subtract", KEYCODE_KP_Subtract);
  TOKEN("KP_Separator", KEYCODE_KP_Decimal);
  TOKEN("Page_Down", KEYCODE_PageDown);
  TOKEN("Clear", KEYCODE_NumLock);
#undef TOKEN
  return -1;
}
