#include "menu_quick_access.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "menu.h"








enum {
#define BMX_MENU_ID(name) quick_posto_##name,
#include "menu_ids.inc"
#undef BMX_MENU_ID
  quick_posti
};
#define BMX_MENU_ID(name) \
  typedef char quick_al_suo_posto_##name[(int)(name) == (int)quick_posto_##name ? 1 : -1];
#include "menu_ids.inc"
#undef BMX_MENU_ID

static const int quick_access_menu_ids[MENU_QUICK_ACCESS_SLOT_COUNT] = {
    MENU_QUICK_ACCESS_SLOT_1,
    MENU_QUICK_ACCESS_SLOT_2,
    MENU_QUICK_ACCESS_SLOT_3,
    MENU_QUICK_ACCESS_SLOT_4,
    MENU_QUICK_ACCESS_SLOT_5,
};

void menu_quick_access_init(struct menu_quick_access_state *state) {
  int i;
  if (state == NULL) return;
  for (i = 0; i < MENU_QUICK_ACCESS_SLOT_COUNT; ++i) {
    state->slots[i].id = MENU_ID_DO_NOTHING;
    state->slots[i].sub_id = MENU_SUB_NONE;
  }
}

int menu_quick_access_set(struct menu_quick_access_state *state, int slot,
                          int id, int sub_id) {
  if (state == NULL || slot < 0 || slot >= MENU_QUICK_ACCESS_SLOT_COUNT) {
    return 0;
  }
  state->slots[slot].id = id;
  state->slots[slot].sub_id = sub_id;
  return 1;
}

const struct menu_quick_access_slot *menu_quick_access_get(
    const struct menu_quick_access_state *state, int slot) {
  if (state == NULL || slot < 0 || slot >= MENU_QUICK_ACCESS_SLOT_COUNT) {
    return NULL;
  }
  return &state->slots[slot];
}

int menu_quick_access_slot_from_menu_id(int id) {
  int i;
  for (i = 0; i < MENU_QUICK_ACCESS_SLOT_COUNT; ++i) {
    if (quick_access_menu_ids[i] == id) return i;
  }
  return -1;
}

int menu_quick_access_menu_id_for_slot(int slot) {
  if (slot < 0 || slot >= MENU_QUICK_ACCESS_SLOT_COUNT) {
    return MENU_ID_DO_NOTHING;
  }
  return quick_access_menu_ids[slot];
}

const char *menu_quick_access_id_name(int id) {
  switch (id) {
#define BMX_MENU_ID(name) case name: return #name;
#include "menu_ids.inc"
#undef BMX_MENU_ID
    default: return NULL;
  }
}

int menu_quick_access_id_from_name(const char *name, int *id) {
  if (name == NULL || id == NULL) return 0;
#define BMX_MENU_ID(menu_id) \
  if (strcmp(name, #menu_id) == 0) { \
    *id = menu_id; \
    return 1; \
  }
#include "menu_ids.inc"
#undef BMX_MENU_ID
  return 0;
}





static int excluded_id(int id) {
  switch (id) {
    case MENU_ABOUT:
    case MENU_LICENSE:
    case MENU_SHORTCUTS:
    case MENU_QUICK_ACCESS_SLOT_1:
    case MENU_QUICK_ACCESS_SLOT_2:
    case MENU_QUICK_ACCESS_SLOT_3:
    case MENU_QUICK_ACCESS_SLOT_4:
    case MENU_QUICK_ACCESS_SLOT_5:
    case MENU_TEXT:
    case MENU_ERROR_DIALOG:
    case MENU_INFO_DIALOG:
    case MENU_CONFIRM_OK:
    case MENU_CONFIRM_CANCEL:
    case MENU_CONFIRM_KEYBOARD_EDITOR_CONFLICT:
    case MENU_CONFIRM_KEYBOARD_EDITOR_RESTORE:
    case MENU_REBOOT_PI:
    case MENU_POWEROFF:
      return 1;
    default:
      return 0;
  }
}

int menu_quick_access_item_supported(const struct menu_item *item) {
  if (item == NULL || item->id == MENU_ID_DO_NOTHING ||
      excluded_id(item->id)) {
    return 0;
  }
  switch (item->type) {
    case TOGGLE:
    case CHECKBOX:
    case MULTIPLE_CHOICE:
    case BUTTON:
    case RANGE:
      return 1;
    default:
      return 0;
  }
}

static struct menu_item *find_in(struct menu_item *node, int id, int sub_id,
                                 int expand_path) {
  while (node != NULL) {
    if (node->id == id && node->sub_id == sub_id &&
        menu_quick_access_item_supported(node)) {
      return node;
    }
    if (node->type == FOLDER && node->first_child != NULL) {
      struct menu_item *found =
          find_in(node->first_child, id, sub_id, expand_path);
      if (found != NULL) {
        if (expand_path) node->is_expanded = 1;
        return found;
      }
    }
    node = node->next;
  }
  return NULL;
}

struct menu_item *menu_quick_access_find(struct menu_item *root, int id,
                                         int sub_id, int expand_path) {
  if (root == NULL || root->type != FOLDER) return NULL;
  return find_in(root->first_child, id, sub_id, expand_path);
}

static int append_component(char *output, size_t output_size,
                            size_t *length, const char *name) {
  size_t name_length;
  static const char separator[] = " / ";
  if (name == NULL || name[0] == '\0') return 1;
  name_length = strlen(name);
  if (*length != 0) {
    if (*length + sizeof(separator) > output_size) return 0;
    memcpy(output + *length, separator, sizeof(separator) - 1);
    *length += sizeof(separator) - 1;
  }
  if (*length + name_length + 1 > output_size) return 0;
  memcpy(output + *length, name, name_length);
  *length += name_length;
  output[*length] = '\0';
  return 1;
}

static int format_path_in(const struct menu_item *node,
                          const struct menu_item *target,
                          char *output, size_t output_size, size_t *length) {
  while (node != NULL) {
    size_t saved_length = *length;
    if (node == target) {
      return append_component(output, output_size, length, node->name);
    }
    if (node->type == FOLDER && node->first_child != NULL) {
      if (append_component(output, output_size, length, node->name) &&
          format_path_in(node->first_child, target, output, output_size,
                         length)) {
        return 1;
      }
      *length = saved_length;
      output[*length] = '\0';
    }
    node = node->next;
  }
  return 0;
}

int menu_quick_access_format_path(const struct menu_item *root,
                                  const struct menu_item *target,
                                  char *output, size_t output_size) {
  size_t length = 0;
  if (output == NULL || output_size == 0) return 0;
  output[0] = '\0';
  if (root == NULL || root->type != FOLDER || target == NULL) return 0;
  return format_path_in(root->first_child, target, output, output_size,
                        &length);
}

void menu_quick_access_fit_path(const char *path, char *output,
                                size_t output_size) {
  size_t length;
  if (output == NULL || output_size == 0) return;
  if (path == NULL) path = "";
  length = strlen(path);
  if (length < output_size) {
    memcpy(output, path, length + 1);
  } else if (output_size <= 4) {
    size_t dots = output_size - 1;
    memset(output, '.', dots);
    output[dots] = '\0';
  } else {
    size_t tail = output_size - 4;
    memcpy(output, "...", 3);
    memcpy(output + 3, path + length - tail, tail);
    output[output_size - 1] = '\0';
  }
}
