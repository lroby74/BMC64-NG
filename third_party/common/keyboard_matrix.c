/*
 * Human-readable names for keys addressed by VICE keyboard matrix entries.
 */

#include "keyboard_matrix.h"

#include <stdio.h>
#include <string.h>

/* These are the VICE .vkm flags that force modifiers on the emulated side. */
#define VKM_VIRTUAL_SHIFT (1 << 0)
#define VKM_ALLOW_SHIFT   (1 << 3)
#define VKM_VIRTUAL_CBM   (1 << 11)
#define VKM_VIRTUAL_CTRL  (1 << 12)

#define EDITOR_MAX_TARGETS 128

struct editor_target {
  int row;
  int column;
  int flags;
};

static const char *const c64_keys[8][8] = {
  { "Delete", "Return", "Cursor Right", "F7", "F1", "F3", "F5", "Cursor Down" },
  { "3", "W", "A", "4", "Z", "S", "E", "Left Shift" },
  { "5", "R", "D", "6", "C", "F", "T", "X" },
  { "7", "Y", "G", "8", "B", "H", "U", "V" },
  { "9", "I", "J", "0", "M", "K", "O", "N" },
  { "Plus", "P", "L", "Minus", "Period", "Colon", "At", "Comma" },
  { "Pound", "Asterisk", "Semicolon", "Home", "Right Shift", "Equals", "Up Arrow", "Slash" },
  { "1", "Left Arrow", "Control", "2", "Space", "Commodore", "Q", "Run/Stop" }
};

static const char *const c128_extra_keys[3][8] = {
  { "Help", "Keypad 8", "Keypad 5", "Tab", "Keypad 2", "Keypad 4", "Keypad 7", "Keypad 1" },
  { "Escape", "Keypad Plus", "Keypad Minus", "Line Feed", "Keypad Enter", "Keypad 6", "Keypad 9", "Keypad 3" },
  { "Alt", "Keypad 0", "Keypad Period", "Cursor Up", "Cursor Down", "Cursor Left", "Cursor Right", "No Scroll" }
};

static const char *const vic20_keys[8][8] = {
  { "1", "Left Arrow", "Control", "Run/Stop", "Space", "Commodore", "Q", "2" },
  { "3", "W", "A", "Left Shift", "Z", "S", "E", "4" },
  { "5", "R", "D", "X", "C", "F", "T", "6" },
  { "7", "Y", "G", "V", "B", "H", "U", "8" },
  { "9", "I", "J", "N", "M", "K", "O", "0" },
  { "Plus", "P", "L", "Comma", "Period", "Colon", "At", "Minus" },
  { "Pound", "Asterisk", "Semicolon", "Slash", "Right Shift", "Equals", "Up Arrow", "Home" },
  { "Delete", "Return", "Cursor Right", "Cursor Down", "F1", "F3", "F5", "F7" }
};

static const char *const plus4_keys[8][8] = {
  { "Delete", "Return", "Pound", "Help", "F1", "F2", "F3", "At" },
  { "3", "W", "A", "4", "Z", "S", "E", "Shift" },
  { "5", "R", "D", "6", "C", "F", "T", "X" },
  { "7", "Y", "G", "8", "B", "H", "U", "V" },
  { "9", "I", "J", "0", "M", "K", "O", "N" },
  { "Cursor Down", "P", "L", "Cursor Up", "Period", "Colon", "Minus", "Comma" },
  { "Cursor Left", "Asterisk", "Semicolon", "Cursor Right", "Escape", "Equals", "Plus", "Slash" },
  { "1", "Home", "Control", "2", "Space", "Commodore", "Q", "Run/Stop" }
};

static const char *const pet_business_keys[10][8] = {
  { "2", "5", "8", "Minus", "Keypad 8", "Cursor Right", "^N", NULL },
  { "1", "4", "7", "0", "Keypad 7", "Up Arrow", NULL, "Keypad 9" },
  { "Escape", "S", "F", "H", "Right Bracket", "K", "Semicolon", "Keypad 5" },
  { "A", "D", "G", "J", "Return", "L", "At", "Keypad 6" },
  { "Tab", "W", "R", "Y", "Backslash", "I", "P", "Delete" },
  { "Q", "E", "T", "U", "Cursor Down", "O", "Left Bracket", "Keypad 4" },
  { "Left Shift", "C", "B", "Period", "Keypad Period", "^Y", "Right Shift", "Keypad 3" },
  { "Z", "V", "N", "Comma", "Keypad 0", "^O", "Repeat", "Keypad 2" },
  { "Reverse", "X", "Space", "M", "Home", "^U", "Slash", "Keypad 1" },
  { "Left Arrow", "3", "6", "9", "Run/Stop", "Colon", NULL, "^V" }
};

static const char *const pet_graphics_keys[10][8] = {
  { "Exclamation", "Number Sign", "Percent", "Ampersand", "Left Parenthesis", "Left Arrow", "Home", "Cursor Right" },
  { "Double Quote", "Dollar", "Apostrophe", "Backslash", "Right Parenthesis", NULL, "Cursor Down", "Delete" },
  { "Q", "E", "T", "U", "O", "Up Arrow", "7", "9" },
  { "W", "R", "Y", "I", "P", NULL, "8", "Slash" },
  { "A", "D", "G", "J", "L", NULL, "4", "6" },
  { "S", "F", "H", "K", "Colon", NULL, "5", "Asterisk" },
  { "Z", "C", "B", "M", "Semicolon", "Return", "1", "3" },
  { "X", "V", "N", "Comma", "Question Mark", NULL, "2", "Plus" },
  { "Left Shift", "At", "Right Bracket", NULL, "Greater Than", "Right Shift", "0", "Minus" },
  { "Reverse", "Left Bracket", "Space", "Less Than", "Run/Stop", NULL, "Period", "Equals" }
};

static const char *pet_business_override(BmxKeyboardMatrix matrix,
                                         int row, int column) {
  if (matrix == BMX_KEYBOARD_MATRIX_PET_BUSINESS_US) {
    if (row == 2 && column == 4) return "Semicolon";
    if (row == 2 && column == 6) return "Backslash";
    if (row == 3 && column == 6) return "Left Bracket";
    if (row == 4 && column == 4) return "At";
    if (row == 5 && column == 6) return "Right Bracket";
  } else if (matrix == BMX_KEYBOARD_MATRIX_PET_BUSINESS_DE) {
    if (row == 1 && column == 5) return "Sharp S";
    if (row == 2 && column == 4) return "U Umlaut";
    if (row == 4 && column == 4) return "O Umlaut";
    if (row == 5 && column == 6) return "A Umlaut";
  }
  return NULL;
}

static const char *matrix_key_name(BmxKeyboardMatrix matrix,
                                   int row, int column) {
  const char *override;

  if (column < 0 || column >= 8) {
    return NULL;
  }
  switch (matrix) {
    case BMX_KEYBOARD_MATRIX_C64:
      return row >= 0 && row < 8 ? c64_keys[row][column] : NULL;
    case BMX_KEYBOARD_MATRIX_C128:
      if (row >= 0 && row < 8) return c64_keys[row][column];
      return row >= 8 && row < 11 ? c128_extra_keys[row - 8][column] : NULL;
    case BMX_KEYBOARD_MATRIX_VIC20:
      return row >= 0 && row < 8 ? vic20_keys[row][column] : NULL;
    case BMX_KEYBOARD_MATRIX_PLUS4:
      return row >= 0 && row < 8 ? plus4_keys[row][column] : NULL;
    case BMX_KEYBOARD_MATRIX_PET_BUSINESS_UK:
    case BMX_KEYBOARD_MATRIX_PET_BUSINESS_US:
    case BMX_KEYBOARD_MATRIX_PET_BUSINESS_DE:
      if (row < 0 || row >= 10) return NULL;
      override = pet_business_override(matrix, row, column);
      return override != NULL ? override : pet_business_keys[row][column];
    case BMX_KEYBOARD_MATRIX_PET_GRAPHICS:
      return row >= 0 && row < 10 ? pet_graphics_keys[row][column] : NULL;
    default:
      return NULL;
  }
}

int keyboard_matrix_key_at(BmxKeyboardMatrix matrix, size_t index,
                           int *row, int *column) {
  int candidate_row;
  int candidate_column;
  size_t found = 0;
  if (row == NULL || column == NULL) {
    return 0;
  }
  for (candidate_row = 0; candidate_row < 11; ++candidate_row) {
    for (candidate_column = 0; candidate_column < 8; ++candidate_column) {
      if (matrix_key_name(matrix, candidate_row, candidate_column) == NULL) {
        continue;
      }
      if (found++ == index) {
        *row = candidate_row;
        *column = candidate_column;
        return 1;
      }
    }
  }
  return 0;
}

size_t keyboard_matrix_key_count(BmxKeyboardMatrix matrix) {
  size_t count = 0;
  int row;
  int column;
  while (keyboard_matrix_key_at(matrix, count, &row, &column)) {
    ++count;
  }
  return count;
}

static unsigned editor_name_rank(const char *name, int row, int column) {
  static const char *const remaining_order[] = {
    "Plus", "Minus", "Asterisk", "Slash", "Equals", "Pound", "At",
    "Colon", "Semicolon", "Comma", "Period", "Up Arrow", "Left Arrow",
    "Space", "Return", "Delete", "Home", "Run/Stop",
    "Cursor Up", "Cursor Down", "Cursor Left", "Cursor Right",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    "Control", "Commodore", "Left Shift", "Right Shift", "Shift",
    "Restore", "Help", "Escape", "Tab", "Line Feed", "No Scroll",
    "40/80 Column", "Caps (ASCII/DIN)", "Alt", "Repeat", "Reverse"
  };
  size_t i;
  if (name != NULL && name[1] == '\0' && name[0] >= '0' && name[0] <= '9') {
    return (unsigned)(name[0] - '0');
  }
  if (name != NULL && name[1] == '\0' && name[0] >= 'A' && name[0] <= 'Z') {
    return 20U + (unsigned)(name[0] - 'A');
  }
  for (i = 0; i < sizeof remaining_order / sizeof remaining_order[0]; ++i) {
    if (name != NULL && strcmp(name, remaining_order[i]) == 0) {
      return 100U + (unsigned)i;
    }
  }
  return 1000U + (unsigned)((row + 8) * 16 + column);
}

static int editor_base_before(BmxKeyboardMatrix matrix,
                              const struct editor_target *a,
                              const struct editor_target *b) {
  unsigned a_rank = editor_name_rank(
      matrix_key_name(matrix, a->row, a->column), a->row, a->column);
  unsigned b_rank = editor_name_rank(
      matrix_key_name(matrix, b->row, b->column), b->row, b->column);
  if (a_rank != b_rank) return a_rank < b_rank;
  if (a->row != b->row) return a->row < b->row;
  return a->column < b->column;
}

static void editor_sort_targets(BmxKeyboardMatrix matrix,
                                struct editor_target *targets,
                                size_t count) {
  size_t i;
  for (i = 1; i < count; ++i) {
    struct editor_target value = targets[i];
    size_t j = i;
    while (j > 0 && editor_base_before(matrix, &value, &targets[j - 1])) {
      targets[j] = targets[j - 1];
      --j;
    }
    targets[j] = value;
  }
}

static const struct editor_target c64_virtual_targets[] = {
  { 0, 0, VKM_VIRTUAL_SHIFT }, { 0, 2, VKM_VIRTUAL_SHIFT },
  { 0, 3, VKM_VIRTUAL_SHIFT }, { 0, 4, VKM_VIRTUAL_SHIFT },
  { 0, 5, VKM_VIRTUAL_SHIFT }, { 0, 6, VKM_VIRTUAL_SHIFT },
  { 0, 7, VKM_VIRTUAL_SHIFT }, { 1, 0, VKM_VIRTUAL_SHIFT },
  { 3, 0, VKM_VIRTUAL_SHIFT }, { 5, 5, VKM_VIRTUAL_SHIFT },
  { 5, 6, VKM_VIRTUAL_CBM },   { 5, 7, VKM_VIRTUAL_SHIFT },
  { 6, 2, VKM_VIRTUAL_SHIFT }
};

static const struct editor_target c128_virtual_targets[] = {
  { 0, 2, VKM_VIRTUAL_SHIFT }, { 0, 3, VKM_VIRTUAL_SHIFT },
  { 0, 4, VKM_VIRTUAL_SHIFT }, { 0, 5, VKM_VIRTUAL_SHIFT },
  { 0, 6, VKM_VIRTUAL_SHIFT }, { 0, 7, VKM_VIRTUAL_SHIFT },
  { 1, 0, VKM_VIRTUAL_SHIFT }, { 3, 0, VKM_VIRTUAL_SHIFT },
  { 5, 5, VKM_VIRTUAL_SHIFT }, { 5, 6, VKM_VIRTUAL_CBM },
  { 5, 7, VKM_VIRTUAL_SHIFT }, { 6, 2, VKM_VIRTUAL_SHIFT }
};

static const struct editor_target vic20_virtual_targets[] = {
  { 0, 0, VKM_VIRTUAL_SHIFT }, { 0, 7, VKM_VIRTUAL_SHIFT },
  { 1, 0, VKM_VIRTUAL_SHIFT }, { 1, 7, VKM_VIRTUAL_SHIFT },
  { 2, 0, VKM_VIRTUAL_SHIFT }, { 2, 7, VKM_VIRTUAL_SHIFT },
  { 3, 0, VKM_VIRTUAL_SHIFT }, { 3, 7, VKM_VIRTUAL_SHIFT },
  { 4, 0, VKM_VIRTUAL_SHIFT }, { 5, 3, VKM_VIRTUAL_SHIFT },
  { 5, 4, VKM_VIRTUAL_SHIFT }, { 5, 5, VKM_VIRTUAL_SHIFT },
  { 5, 6, VKM_VIRTUAL_CBM },   { 6, 2, VKM_VIRTUAL_SHIFT },
  { 6, 3, VKM_VIRTUAL_SHIFT }, { 7, 2, VKM_VIRTUAL_SHIFT },
  { 7, 3, VKM_VIRTUAL_SHIFT }, { 7, 4, VKM_VIRTUAL_SHIFT },
  { 7, 5, VKM_VIRTUAL_SHIFT }, { 7, 6, VKM_VIRTUAL_SHIFT },
  { 7, 7, VKM_VIRTUAL_SHIFT }
};

static const struct editor_target plus4_virtual_targets[] = {
  { 0, 3, VKM_VIRTUAL_SHIFT }, { 0, 4, VKM_VIRTUAL_SHIFT },
  { 0, 5, VKM_VIRTUAL_SHIFT }, { 0, 6, VKM_VIRTUAL_SHIFT },
  { 0, 7, VKM_VIRTUAL_CBM },   { 1, 0, VKM_VIRTUAL_SHIFT },
  { 1, 3, VKM_VIRTUAL_SHIFT }, { 2, 0, VKM_VIRTUAL_SHIFT },
  { 2, 3, VKM_VIRTUAL_SHIFT }, { 3, 0, VKM_VIRTUAL_SHIFT },
  { 3, 3, VKM_VIRTUAL_SHIFT }, { 4, 0, VKM_VIRTUAL_SHIFT },
  { 5, 4, VKM_VIRTUAL_SHIFT }, { 5, 5, VKM_VIRTUAL_SHIFT },
  { 5, 7, VKM_VIRTUAL_SHIFT }, { 6, 2, VKM_VIRTUAL_SHIFT },
  { 6, 7, VKM_VIRTUAL_SHIFT }, { 7, 0, VKM_VIRTUAL_SHIFT },
  { 7, 3, VKM_VIRTUAL_SHIFT }
};

static const struct editor_target pet_virtual_targets[] = {
  { 0, 0, VKM_VIRTUAL_SHIFT }, { 0, 1, VKM_VIRTUAL_SHIFT },
  { 0, 2, VKM_VIRTUAL_SHIFT }, { 0, 3, VKM_VIRTUAL_SHIFT },
  { 0, 5, VKM_VIRTUAL_SHIFT }, { 1, 0, VKM_VIRTUAL_SHIFT },
  { 1, 1, VKM_VIRTUAL_SHIFT }, { 1, 2, VKM_VIRTUAL_SHIFT },
  { 2, 6, VKM_VIRTUAL_SHIFT }, { 3, 6, VKM_VIRTUAL_SHIFT },
  { 4, 0, VKM_VIRTUAL_SHIFT }, { 5, 4, VKM_VIRTUAL_SHIFT },
  { 6, 3, VKM_VIRTUAL_SHIFT }, { 7, 3, VKM_VIRTUAL_SHIFT },
  { 8, 6, VKM_VIRTUAL_SHIFT }, { 9, 1, VKM_VIRTUAL_SHIFT },
  { 9, 2, VKM_VIRTUAL_SHIFT }, { 9, 3, VKM_VIRTUAL_SHIFT },
  { 9, 5, VKM_VIRTUAL_SHIFT }
};

static size_t editor_append_special_targets(BmxKeyboardMatrix matrix,
                                            struct editor_target *targets,
                                            size_t count) {
  if (matrix == BMX_KEYBOARD_MATRIX_C64) {
    targets[count++] = (struct editor_target){ -3, 0, 0 };
    targets[count++] = (struct editor_target){ -4, 1, 0 };
  } else if (matrix == BMX_KEYBOARD_MATRIX_C128) {
    targets[count++] = (struct editor_target){ -3, 0, 0 };
    targets[count++] = (struct editor_target){ -3, 1, 0 };
    targets[count++] = (struct editor_target){ -4, 0, 0 };
    targets[count++] = (struct editor_target){ -4, 1, 0 };
  } else if (matrix == BMX_KEYBOARD_MATRIX_VIC20) {
    targets[count++] = (struct editor_target){ -3, 0, 0 };
    targets[count++] = (struct editor_target){ -3, 1, 0 };
  }
  return count;
}

static const struct editor_target *editor_virtual_catalog(
    BmxKeyboardMatrix matrix, size_t *count) {
  switch (matrix) {
    case BMX_KEYBOARD_MATRIX_C64:
      *count = sizeof c64_virtual_targets / sizeof c64_virtual_targets[0];
      return c64_virtual_targets;
    case BMX_KEYBOARD_MATRIX_C128:
      *count = sizeof c128_virtual_targets / sizeof c128_virtual_targets[0];
      return c128_virtual_targets;
    case BMX_KEYBOARD_MATRIX_VIC20:
      *count = sizeof vic20_virtual_targets / sizeof vic20_virtual_targets[0];
      return vic20_virtual_targets;
    case BMX_KEYBOARD_MATRIX_PLUS4:
      *count = sizeof plus4_virtual_targets / sizeof plus4_virtual_targets[0];
      return plus4_virtual_targets;
    case BMX_KEYBOARD_MATRIX_PET_BUSINESS_UK:
    case BMX_KEYBOARD_MATRIX_PET_BUSINESS_US:
    case BMX_KEYBOARD_MATRIX_PET_BUSINESS_DE:
    case BMX_KEYBOARD_MATRIX_PET_GRAPHICS:
      *count = sizeof pet_virtual_targets / sizeof pet_virtual_targets[0];
      return pet_virtual_targets;
    default:
      *count = 0;
      return NULL;
  }
}

static size_t keyboard_matrix_build_editor_catalog(
    BmxKeyboardMatrix matrix, struct editor_target *targets) {
  const struct editor_target *virtual_targets;
  size_t base_count = 0;
  size_t virtual_count = 0;
  size_t virtual_start;
  size_t i;
  int row;
  int column;

  while (keyboard_matrix_key_at(matrix, base_count, &row, &column)) {
    if (base_count >= EDITOR_MAX_TARGETS) return 0;
    targets[base_count] = (struct editor_target){
      row, column, VKM_ALLOW_SHIFT
    };
    ++base_count;
  }
  editor_sort_targets(matrix, targets, base_count);
  base_count = editor_append_special_targets(matrix, targets, base_count);
  virtual_start = base_count;
  virtual_targets = editor_virtual_catalog(matrix, &virtual_count);
  if (base_count + virtual_count > EDITOR_MAX_TARGETS) return 0;
  for (i = 0; i < virtual_count; ++i) {
    if (matrix_key_name(matrix, virtual_targets[i].row,
                        virtual_targets[i].column) != NULL) {
      targets[base_count++] = virtual_targets[i];
    }
  }
  editor_sort_targets(matrix, targets + virtual_start,
                      base_count - virtual_start);
  return base_count;
}

size_t keyboard_matrix_editor_target_count(BmxKeyboardMatrix matrix) {
  struct editor_target targets[EDITOR_MAX_TARGETS];
  return keyboard_matrix_build_editor_catalog(matrix, targets);
}

int keyboard_matrix_editor_target_at(BmxKeyboardMatrix matrix, size_t index,
                                     int *row, int *column, int *flags) {
  struct editor_target targets[EDITOR_MAX_TARGETS];
  size_t count;
  if (row == NULL || column == NULL || flags == NULL) return 0;
  count = keyboard_matrix_build_editor_catalog(matrix, targets);
  if (index >= count) return 0;
  *row = targets[index].row;
  *column = targets[index].column;
  *flags = targets[index].flags;
  return 1;
}

static int special_key_name(int row, int column,
                            char *buffer, size_t buffer_size) {
  static const char *const directions[9] = {
    "Fire", "South-West", "South", "South-East", "West",
    "East", "North-West", "North", "North-East"
  };
  int written = -1;

  if ((row == -1 || row == -2) && column >= 0 && column < 9) {
    written = snprintf(buffer, buffer_size, "Joystick %c %s",
                       row == -1 ? 'A' : 'B', directions[column]);
  } else if (row == -3 && (column == 0 || column == 1)) {
    written = snprintf(buffer, buffer_size, column == 0 ? "Restore" : "Restore 2");
  } else if (row == -4 && (column == 0 || column == 1)) {
    written = snprintf(buffer, buffer_size,
                       column == 0 ? "40/80 Column" : "Caps (ASCII/DIN)");
  } else if (row == -5 && column >= 0 && column < 20) {
    written = snprintf(buffer, buffer_size, "Joyport Keypad %d", column);
  }
  return written >= 0 && (size_t)written < buffer_size;
}

int keyboard_matrix_format_emulated_key(BmxKeyboardMatrix matrix,
                                        int row, int column, int flags,
                                        char *buffer, size_t buffer_size) {
  const char *name;
  const char *shift = flags & VKM_VIRTUAL_SHIFT ? "Shift+" : "";
  const char *cbm = flags & VKM_VIRTUAL_CBM ? "Commodore+" : "";
  const char *ctrl = flags & VKM_VIRTUAL_CTRL ? "Ctrl+" : "";
  int written;

  if (buffer == NULL || buffer_size == 0) {
    return 0;
  }
  buffer[0] = '\0';
  if (row < 0) {
    return special_key_name(row, column, buffer, buffer_size);
  }
  name = matrix_key_name(matrix, row, column);
  if (name == NULL) {
    return 0;
  }
  written = snprintf(buffer, buffer_size, "%s%s%s%s", shift, cbm, ctrl, name);
  return written >= 0 && (size_t)written < buffer_size;
}
