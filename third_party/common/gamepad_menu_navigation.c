





#include "gamepad_menu_navigation.h"

#include <string.h>

void bmx_gamepad_menu_navigation_init(BmxGamepadMenuNavigation *navigation) {
  memset(navigation, 0, sizeof(*navigation));
}

void bmx_gamepad_menu_navigation_update(
    BmxGamepadMenuNavigation *navigation, int menu_active,
    uint32_t current_directions, uint32_t *pressed_directions,
    uint32_t *released_directions) {
  uint32_t previous = navigation->active_directions;
  uint32_t current = current_directions & BMX_GAMEPAD_MENU_ALL;

  *pressed_directions = 0U;
  *released_directions = 0U;
  if (!menu_active) {
    navigation->active_directions = 0U;
    return;
  }

  *pressed_directions = current & ~previous;
  *released_directions = previous & ~current;
  navigation->active_directions = current;
}
