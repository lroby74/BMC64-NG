





#ifndef BMX_GAMEPAD_MENU_NAVIGATION_H
#define BMX_GAMEPAD_MENU_NAVIGATION_H

#include <stdint.h>

#define BMX_GAMEPAD_MENU_UP UINT32_C(0x01)
#define BMX_GAMEPAD_MENU_DOWN UINT32_C(0x02)
#define BMX_GAMEPAD_MENU_LEFT UINT32_C(0x04)
#define BMX_GAMEPAD_MENU_RIGHT UINT32_C(0x08)
#define BMX_GAMEPAD_MENU_ALL UINT32_C(0x0f)

typedef struct {
  uint32_t active_directions;
} BmxGamepadMenuNavigation;

void bmx_gamepad_menu_navigation_init(BmxGamepadMenuNavigation *navigation);
void bmx_gamepad_menu_navigation_update(
    BmxGamepadMenuNavigation *navigation, int menu_active,
    uint32_t current_directions, uint32_t *pressed_directions,
    uint32_t *released_directions);

#endif
