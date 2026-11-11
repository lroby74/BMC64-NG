#ifndef BMX_MENU_QUICK_ACCESS_H
#define BMX_MENU_QUICK_ACCESS_H

#include <stddef.h>

struct menu_item;

#define MENU_QUICK_ACCESS_SLOT_COUNT 5

struct menu_quick_access_slot {
  int id;
  int sub_id;
};

struct menu_quick_access_state {
  struct menu_quick_access_slot slots[MENU_QUICK_ACCESS_SLOT_COUNT];
};

void menu_quick_access_init(struct menu_quick_access_state *state);
int menu_quick_access_set(struct menu_quick_access_state *state, int slot,
                          int id, int sub_id);
const struct menu_quick_access_slot *menu_quick_access_get(
    const struct menu_quick_access_state *state, int slot);

int menu_quick_access_slot_from_menu_id(int id);
int menu_quick_access_menu_id_for_slot(int slot);
const char *menu_quick_access_id_name(int id);
int menu_quick_access_id_from_name(const char *name, int *id);

int menu_quick_access_item_supported(const struct menu_item *item);
struct menu_item *menu_quick_access_find(struct menu_item *root, int id,
                                         int sub_id, int expand_path);
int menu_quick_access_format_path(const struct menu_item *root,
                                  const struct menu_item *target,
                                  char *output, size_t output_size);
void menu_quick_access_fit_path(const char *path, char *output,
                                size_t output_size);

#endif
