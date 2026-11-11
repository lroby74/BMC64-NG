/*
 * menu_usb.c
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

#include "menu_usb.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// RASPI includes
#include "emux_api.h"
#include "joy.h"
#include "menu.h"
#include "menu_key_binding.h"
#include "ui.h"
#include "keycodes.h"
#include "usb_gamepad_defaults.h"
#include "gamepad_button_capture.h"

extern int usb_pref[MAX_USB_DEVICES];
extern int usb_x_axis[MAX_USB_DEVICES];
extern int usb_y_axis[MAX_USB_DEVICES];
extern float usb_x_thresh[MAX_USB_DEVICES];
extern float usb_y_thresh[MAX_USB_DEVICES];
extern int usb_button_assignments[MAX_USB_DEVICES][MAX_USB_BUTTONS];

// Pre-shifted values for button bits for logical ops.
// Should find out whether this is more efficient than
// just doing the shift in place.
extern int usb_button_bits[MAX_USB_BUTTONS];

struct menu_item *raw_buttons_item;
struct menu_item *raw_hats_item[MAX_USB_HATS];
struct menu_item *raw_axes_item[MAX_USB_AXES];

// For pot x y as buttons
struct menu_item *potx_high_item;
struct menu_item *potx_low_item;
struct menu_item *poty_high_item;
struct menu_item *poty_low_item;

struct menu_item *define_bindings_item;
static struct menu_item *usb_pref_items[MAX_USB_DEVICES];
static struct menu_item *usb_x_axis_items[MAX_USB_DEVICES];
static struct menu_item *usb_y_axis_items[MAX_USB_DEVICES];
static struct menu_item *usb_x_thresh_items[MAX_USB_DEVICES];
static struct menu_item *usb_y_thresh_items[MAX_USB_DEVICES];
static struct menu_item *usb_button_items[MAX_USB_DEVICES][MAX_USB_BUTTONS];











#define USB_MAPPING_AUTOMATIC 0
#define USB_MAPPING_CUSTOM 1
static int usb_mapping_modes[MAX_USB_DEVICES];
static int usb_mapping_initialized;
static int configured_device = -1;
static struct menu_item *mapping_mode_items[MAX_USB_DEVICES];
static struct menu_item *assign_by_function_item;
static struct menu_item *function_items[NUM_BUTTON_ASSIGNMENTS];
static struct menu_item *clear_button_item;
static struct menu_item *capture_status_item;
static int capture_assignment = BTN_ASSIGN_UNDEF;
static BmxGamepadButtonCapture button_capture;

#if MAX_USB_DEVICES != BMX_GAMEPAD_CAPTURE_MAX_DEVICES
#error "Gamepad capture device count must match the USB mapping device count"
#endif

static void show_button_assignment_menu(int device);
static void show_function_assignment_menu(int device);

// Set to one when we are listening for raw usb values for config





int want_raw_usb = 0;
int want_raw_usb_device = 0;

// Conver this value to log2(b) for display purposes
// Shows user which button index is being pressed on raw screen
static int map_button_value(int b) {
  if (b == 0)
    return -1;
  return log2(b);
}

static void raw_popped(struct menu_item *new_root,
                       struct menu_item* old_root) {
  // This is our raw monitor being popped.
  // Turn off raw monitoring.
  want_raw_usb = 0;
}

static void show_usb_monitor(int device) {
  int i;
  char scratch[16];

  want_raw_usb = 1;
  want_raw_usb_device = device;

  struct menu_item *root = ui_push_menu(-1, -1);
  // We need to get notified of pop to turn off monitoring
  root->on_popped_off = raw_popped;

  raw_buttons_item =
      ui_menu_add_button_with_value(MENU_TEXT, root, "Button #", 0, "", "");
  // We want to show button index so have to show log base 2 of value
  raw_buttons_item->map_value_func = map_button_value;

  for (i = 0; i < joy_num_hats[device]; i++) {
    sprintf(scratch, "Hat %d", i);
    raw_hats_item[i] =
        ui_menu_add_button_with_value(MENU_TEXT, root, scratch, 0, "", "");
  }

  for (i = 0; i < joy_num_axes[device]; i++) {
    sprintf(scratch, "Axis %d", i);
    raw_axes_item[i] =
        ui_menu_add_button_with_value(MENU_TEXT, root, scratch, 0, "", "");
  }
}

// Called after key bindings menu was popped. Need to update the choice
// descriptions in the menu items so it reflects potential changes.
static void update_key_binding_descriptions(struct menu_item *new_root,
                                            struct menu_item *old_root) {
  // new_root will be this menu's root
  char scratch[32];
  struct menu_item *child = new_root->first_child;
  while (child != NULL) {
     if (child->id >= MENU_USB_0_BTN_ASSIGN && child->id <= MENU_USB_1_BTN_ASSIGN) {
        for (int n = 0; n < 6; n++) {
           sprintf (scratch, "Key %d (%s)", n+1,
              keycode_to_string(key_bindings[n]));
           strcpy(child->choices[BTN_ASSIGN_CUSTOM_KEY_1 + n],
              scratch);
        }
     }
     child = child->next;
  }
}



static int button_assignment_available(int assignment) {
  if (assignment == BTN_ASSIGN_SWAP_PORTS &&
      emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
    return 0;
  }
  if (assignment == BTN_ASSIGN_CART_FREEZE &&
      emux_machine_class != BMC64_MACHINE_CLASS_C64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_SCPU64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_C128) {
    return 0;
  }
  if ((assignment == BTN_ASSIGN_ACTIVE_DISPLAY ||
       assignment == BTN_ASSIGN_PIP_LOCATION ||
       assignment == BTN_ASSIGN_PIP_SWAP ||
       assignment == BTN_ASSIGN_40_80_COLUMN) &&
      emux_machine_class != BMC64_MACHINE_CLASS_C128) {
    return 0;
  }
  if (assignment == BTN_ASSIGN_VKBD_TOGGLE &&
      emux_machine_class == BMC64_MACHINE_CLASS_PET) {
    return 0;
  }
  return 1;
}

static void format_button_assignment_name(int assignment, char *buffer,
                                          size_t size) {
  if (assignment >= BTN_ASSIGN_CUSTOM_KEY_1 &&
      assignment <= BTN_ASSIGN_CUSTOM_KEY_6) {
    int binding = assignment - BTN_ASSIGN_CUSTOM_KEY_1;
    snprintf(buffer, size, "Key %d (%s)", binding + 1,
             keycode_to_string(key_bindings[binding]));
  } else {
    snprintf(buffer, size, "%s", function_to_string(assignment));
  }
}

static void format_function_buttons(int device, int assignment, char *buffer,
                                    size_t size) {
  int count = 0;
  size_t used;

  for (int button = 0; button < joy_num_buttons[device]; button++) {
    if (usb_button_assignments[device][button] == assignment) {
      count++;
    }
  }

  if (count == 0) {
    snprintf(buffer, size, "None");
    return;
  }

  used = (size_t)snprintf(buffer, size, "%s", count == 1 ? "Button "
                                                          : "Buttons ");
  if (used >= size) {
    return;
  }
  count = 0;
  for (int button = 0; button < joy_num_buttons[device]; button++) {
    int written;
    if (usb_button_assignments[device][button] != assignment) {
      continue;
    }
    written = snprintf(buffer + used, size - used, "%s%d",
                       count == 0 ? "" : ",", button);
    if (written < 0 || (size_t)written >= size - used) {
      buffer[size - 1U] = '\0';
      return;
    }
    used += (size_t)written;
    count++;
  }
}

static void refresh_function_items(int device) {
  char buttons[96];
  int device_available = joy_num_pads > device && joy_num_buttons[device] > 0;

  for (int assignment = BTN_ASSIGN_FIRE;
       assignment < NUM_BUTTON_ASSIGNMENTS; assignment++) {
    if (function_items[assignment] == NULL) {
      continue;
    }
    format_function_buttons(device, assignment, buttons, sizeof buttons);
    ui_menu_set_button_value_fitted(function_items[assignment], buttons, 1);
    function_items[assignment]->disabled =
        !device_available || !button_assignment_available(assignment);
  }

  if (clear_button_item != NULL) {
    clear_button_item->disabled = !device_available;
  }
}




static void refresh_usb_menu_values(int device) {
  if (configured_device != device || usb_pref_items[device] == NULL) {
    return;
  }
  if (mapping_mode_items[device] != NULL) {
    mapping_mode_items[device]->value = usb_mapping_modes[device];
  }
  usb_pref_items[device]->value = usb_pref[device];
  usb_x_axis_items[device]->value = usb_x_axis[device];
  usb_y_axis_items[device]->value = usb_y_axis[device];
  usb_x_thresh_items[device]->value = (int)(usb_x_thresh[device] * 100.0f);
  usb_y_thresh_items[device]->value = (int)(usb_y_thresh[device] * 100.0f);

  for (int button = 0; button < joy_num_buttons[device]; button++) {
    if (usb_button_items[device][button] != NULL) {
      usb_button_items[device][button]->value =
          usb_button_assignments[device][button];
    }
  }
  refresh_function_items(device);
}

static void mark_mapping_custom(int device) {
  usb_mapping_modes[device] = USB_MAPPING_CUSTOM;
  refresh_usb_menu_values(device);
}


static void apply_automatic_mapping(int device) {
  usb_gamepad_reset_to_defaults(device);
}

void menu_usb_mapping_initialize(void) {
  for (int device = 0; device < MAX_USB_DEVICES; device++) {
    usb_mapping_modes[device] = USB_MAPPING_AUTOMATIC;
  }
  usb_mapping_initialized = 0;
  bmx_gamepad_button_capture_init(&button_capture);
}



static int mappa_di_una_volta(int device) {
  if (usb_pref[device] != USB_PREF_ANALOG || usb_x_axis[device] != 0 ||
      usb_y_axis[device] != 1 ||
      (int)(usb_x_thresh[device] * 100.0f + 0.5f) != 50 ||
      (int)(usb_y_thresh[device] * 100.0f + 0.5f) != 50) {
    return 0;
  }
  for (int button = 0; button < MAX_USB_BUTTONS; button++) {
    if (usb_button_assignments[device][button] !=
        (button == 0 ? BTN_ASSIGN_FIRE : BTN_ASSIGN_UNDEF)) {
      return 0;
    }
  }
  return 1;
}




void menu_usb_mapping_finish_load(const int *mode_was_loaded,
                                  const int *loaded_mode,
                                  const int *mapping_was_loaded) {
  for (int device = 0; device < MAX_USB_DEVICES; device++) {
    if (mode_was_loaded[device]) {
      usb_mapping_modes[device] = loaded_mode[device] == USB_MAPPING_AUTOMATIC
                                      ? USB_MAPPING_AUTOMATIC
                                      : USB_MAPPING_CUSTOM;
    } else if (!mapping_was_loaded[device] || mappa_di_una_volta(device)) {
      usb_mapping_modes[device] = USB_MAPPING_AUTOMATIC;
    } else {
      usb_mapping_modes[device] = USB_MAPPING_CUSTOM;
    }
    if (usb_mapping_modes[device] == USB_MAPPING_AUTOMATIC) {
      apply_automatic_mapping(device);
    }
  }
  usb_mapping_initialized = 1;
}

int menu_usb_mapping_mode(int device) {
  return usb_mapping_modes[device];
}



void menu_usb_gamepad_info_changed(void) {
  if (!usb_mapping_initialized) {
    return;
  }
  for (int device = 0; device < MAX_USB_DEVICES; device++) {
    if (usb_mapping_modes[device] == USB_MAPPING_AUTOMATIC) {
      apply_automatic_mapping(device);
    }
    refresh_usb_menu_values(device);
  }
}

static void mapping_menu_popped(struct menu_item *new_root,
                                struct menu_item *old_root) {
  (void)new_root;
  (void)old_root;
  if (configured_device >= 0 && configured_device < MAX_USB_DEVICES) {
    usb_pref_items[configured_device] = NULL;
    mapping_mode_items[configured_device] = NULL;
    memset(usb_button_items[configured_device], 0,
           sizeof(usb_button_items[0]));
  }
  configured_device = -1;
  assign_by_function_item = NULL;
  memset(function_items, 0, sizeof(function_items));
  clear_button_item = NULL;
  capture_status_item = NULL;
  capture_assignment = BTN_ASSIGN_UNDEF;
  bmx_gamepad_button_capture_cancel(&button_capture);
}

static void assignment_menu_popped(struct menu_item *new_root,
                                   struct menu_item *old_root) {
  (void)new_root;
  (void)old_root;
  if (configured_device >= 0 && configured_device < MAX_USB_DEVICES) {
    memset(usb_button_items[configured_device], 0,
           sizeof(usb_button_items[0]));
  }
  memset(function_items, 0, sizeof(function_items));
  clear_button_item = NULL;
  capture_status_item = NULL;
  capture_assignment = BTN_ASSIGN_UNDEF;
  bmx_gamepad_button_capture_cancel(&button_capture);
}


//





static void scrivi_diagnosi_pad(int dev) {
  unsigned char desc[512];
  FILE *fp;
  int n;

  fp = fopen("/BMC64-DIAG.TXT", "a");
  if (fp == NULL) {
    return;
  }

  fprintf(fp, "gamepad %d: %s\n", dev + 1,
          joy_num_pads > dev ? "detected" : "NOT detected");
  fprintf(fp, "gamepad %d: %d axes, %d hats, %d buttons\n", dev + 1,
          joy_num_axes[dev], joy_num_hats[dev], joy_num_buttons[dev]);

  n = circle_gamepad_descriptor(dev, desc, sizeof(desc));
  if (n <= 0) {



    fprintf(fp, "gamepad %d: no HID report descriptor "
                "(driver for a particular controller)\n", dev + 1);
  } else {
    int i;
    fprintf(fp, "gamepad %d: HID report descriptor, %d bytes\n", dev + 1, n);
    for (i = 0; i < n; i++) {
      fprintf(fp, "%02x%s", desc[i], ((i % 16) == 15) ? "\n" : " ");
    }
    if ((n % 16) != 0) {
      fprintf(fp, "\n");
    }
  }

  fclose(fp);
}

static void menu_usb_value_changed(struct menu_item *item) {
  struct menu_item* tmp_item;

  switch (item->id) {
  case MENU_USB_0_DIAG:
  case MENU_USB_1_DIAG:
  case MENU_USB_2_DIAG:
  case MENU_USB_3_DIAG:
    scrivi_diagnosi_pad(item->id - MENU_USB_0_DIAG);
    return;
  case MENU_USB_0_PREF:
  case MENU_USB_1_PREF:
  case MENU_USB_2_PREF:
  case MENU_USB_3_PREF:
    usb_pref[item->id - MENU_USB_0_PREF] = item->value;
    mark_mapping_custom(item->id - MENU_USB_0_PREF);
    break;
  case MENU_USB_0_X_AXIS:
  case MENU_USB_1_X_AXIS:
  case MENU_USB_2_X_AXIS:
  case MENU_USB_3_X_AXIS:
    usb_x_axis[item->id - MENU_USB_0_X_AXIS] = item->value;
    mark_mapping_custom(item->id - MENU_USB_0_X_AXIS);
    break;
  case MENU_USB_0_Y_AXIS:
  case MENU_USB_1_Y_AXIS:
  case MENU_USB_2_Y_AXIS:
  case MENU_USB_3_Y_AXIS:
    usb_y_axis[item->id - MENU_USB_0_Y_AXIS] = item->value;
    mark_mapping_custom(item->id - MENU_USB_0_Y_AXIS);
    break;
  case MENU_USB_0_WATCH_RAW:
  case MENU_USB_1_WATCH_RAW:
  case MENU_USB_2_WATCH_RAW:
  case MENU_USB_3_WATCH_RAW:
    show_usb_monitor(item->id - MENU_USB_0_WATCH_RAW);
    break;
  case MENU_USB_0_RESET_DEFAULTS:
  case MENU_USB_1_RESET_DEFAULTS:
  case MENU_USB_2_RESET_DEFAULTS:
  case MENU_USB_3_RESET_DEFAULTS: {
    int device = item->id - MENU_USB_0_RESET_DEFAULTS;
    usb_gamepad_reset_to_defaults(device);

    usb_mapping_modes[device] = USB_MAPPING_AUTOMATIC;
    refresh_usb_menu_values(device);
    break;
  }



  case MENU_USB_0_MAPPING_MODE:
  case MENU_USB_1_MAPPING_MODE:
  case MENU_USB_2_MAPPING_MODE:
  case MENU_USB_3_MAPPING_MODE: {
    int device = item->id - MENU_USB_0_MAPPING_MODE;
    usb_mapping_modes[device] = item->value == USB_MAPPING_AUTOMATIC
                                    ? USB_MAPPING_AUTOMATIC
                                    : USB_MAPPING_CUSTOM;
    if (usb_mapping_modes[device] == USB_MAPPING_AUTOMATIC) {
      apply_automatic_mapping(device);
    }
    refresh_usb_menu_values(device);
    break;
  }
  case MENU_USB_ASSIGN_BY_BUTTON:
    show_button_assignment_menu(item->sub_id);
    break;
  case MENU_USB_ASSIGN_BY_FUNCTION:
    show_function_assignment_menu(item->sub_id);
    break;
  case MENU_USB_FUNCTION_ASSIGN:
  case MENU_USB_CLEAR_BUTTON:
    capture_assignment = item->id == MENU_USB_FUNCTION_ASSIGN
                             ? item->sub_id
                             : BTN_ASSIGN_UNDEF;
    bmx_gamepad_button_capture_cancel(&button_capture);
    if (configured_device < 0 || configured_device >= MAX_USB_DEVICES ||
        joy_num_pads <= configured_device ||
        joy_num_buttons[configured_device] <= 0 ||
        !bmx_gamepad_button_capture_begin(&button_capture, configured_device,
                                          capture_assignment)) {
      if (capture_status_item != NULL) {
        ui_menu_set_button_value_fitted(capture_status_item,
                                        "Gamepad unavailable", 1);
      }
    }
    break;
  case MENU_USB_0_BTN_ASSIGN:
  case MENU_USB_1_BTN_ASSIGN:
  case MENU_USB_2_BTN_ASSIGN:
  case MENU_USB_3_BTN_ASSIGN:
    usb_button_assignments[item->id - MENU_USB_0_BTN_ASSIGN][item->sub_id] = item->value;
    mark_mapping_custom(item->id - MENU_USB_0_BTN_ASSIGN);
    break;
  case MENU_POTX_HIGH:
    pot_x_high_value = item->value;
    break;
  case MENU_POTX_LOW:
    pot_x_low_value = item->value;
    break;
  case MENU_POTY_HIGH:
    pot_y_high_value = item->value;
    break;
  case MENU_POTY_LOW:
    pot_y_low_value = item->value;
    break;
  case MENU_USB_0_X_THRESH:
  case MENU_USB_1_X_THRESH:
  case MENU_USB_2_X_THRESH:
  case MENU_USB_3_X_THRESH:
    usb_x_thresh[item->id - MENU_USB_0_X_THRESH] = ((float)item->value) / 100.0f;
    mark_mapping_custom(item->id - MENU_USB_0_X_THRESH);
    break;
  case MENU_USB_0_Y_THRESH:
  case MENU_USB_1_Y_THRESH:
  case MENU_USB_2_Y_THRESH:
  case MENU_USB_3_Y_THRESH:
    usb_y_thresh[item->id - MENU_USB_0_Y_THRESH] = ((float)item->value) / 100.0f;
    mark_mapping_custom(item->id - MENU_USB_0_Y_THRESH);
    break;
  case MENU_CONFIGURE_KEY_BINDINGS:
    tmp_item = ui_push_menu(-1,-1);
    build_keybinding_menu(tmp_item);
    tmp_item->on_popped_off = update_key_binding_descriptions;
    break;
  default:
    break;
  }
}

static void add_button_choices(struct menu_item *tmp_item) {
  tmp_item->num_choices = NUM_BUTTON_ASSIGNMENTS;
  strcpy(tmp_item->choices[BTN_ASSIGN_UNDEF], function_to_string(BTN_ASSIGN_UNDEF));
  strcpy(tmp_item->choices[BTN_ASSIGN_FIRE], function_to_string(BTN_ASSIGN_FIRE));
  strcpy(tmp_item->choices[BTN_ASSIGN_MENU], function_to_string(BTN_ASSIGN_MENU));
  strcpy(tmp_item->choices[BTN_ASSIGN_WARP], function_to_string(BTN_ASSIGN_WARP));
  strcpy(tmp_item->choices[BTN_ASSIGN_STATUS_TOGGLE], function_to_string(BTN_ASSIGN_STATUS_TOGGLE));
  strcpy(tmp_item->choices[BTN_ASSIGN_SWAP_PORTS], function_to_string(BTN_ASSIGN_SWAP_PORTS));

  // These are only available for USB buttons, not as hotkeys
  strcpy(tmp_item->choices[BTN_ASSIGN_UP], function_to_string(BTN_ASSIGN_UP));
  strcpy(tmp_item->choices[BTN_ASSIGN_DOWN], function_to_string(BTN_ASSIGN_DOWN));
  strcpy(tmp_item->choices[BTN_ASSIGN_LEFT], function_to_string(BTN_ASSIGN_LEFT));
  strcpy(tmp_item->choices[BTN_ASSIGN_RIGHT], function_to_string(BTN_ASSIGN_RIGHT));
  strcpy(tmp_item->choices[BTN_ASSIGN_POTX], function_to_string(BTN_ASSIGN_POTX));
  strcpy(tmp_item->choices[BTN_ASSIGN_POTY], function_to_string(BTN_ASSIGN_POTY));

  // Back to avail for all
  strcpy(tmp_item->choices[BTN_ASSIGN_TAPE_MENU], function_to_string(BTN_ASSIGN_TAPE_MENU));
  strcpy(tmp_item->choices[BTN_ASSIGN_CART_MENU], function_to_string(BTN_ASSIGN_CART_MENU));
  strcpy(tmp_item->choices[BTN_ASSIGN_CART_FREEZE], function_to_string(BTN_ASSIGN_CART_FREEZE));
  strcpy(tmp_item->choices[BTN_ASSIGN_RESET_MENU], function_to_string(BTN_ASSIGN_RESET_MENU));
  strcpy(tmp_item->choices[BTN_ASSIGN_RESET_HARD], function_to_string(BTN_ASSIGN_RESET_HARD));
  strcpy(tmp_item->choices[BTN_ASSIGN_RESET_SOFT], function_to_string(BTN_ASSIGN_RESET_SOFT));

  // More just for USB buttons
  strcpy(tmp_item->choices[BTN_ASSIGN_RUN_STOP_BACK], function_to_string(BTN_ASSIGN_RUN_STOP_BACK));

  // More for all
  strcpy(tmp_item->choices[BTN_ASSIGN_ACTIVE_DISPLAY], function_to_string(BTN_ASSIGN_ACTIVE_DISPLAY));
  strcpy(tmp_item->choices[BTN_ASSIGN_PIP_LOCATION], function_to_string(BTN_ASSIGN_PIP_LOCATION));
  strcpy(tmp_item->choices[BTN_ASSIGN_PIP_SWAP], function_to_string(BTN_ASSIGN_PIP_SWAP));
  strcpy(tmp_item->choices[BTN_ASSIGN_40_80_COLUMN], function_to_string(BTN_ASSIGN_40_80_COLUMN));
  strcpy(tmp_item->choices[BTN_ASSIGN_VKBD_TOGGLE], function_to_string(BTN_ASSIGN_VKBD_TOGGLE));
  strcpy(tmp_item->choices[BTN_ASSIGN_FLUSH_DISK], function_to_string(BTN_ASSIGN_FLUSH_DISK));

  char scratch[32];
  for (int n = 0; n < 6; n++) {
     sprintf (scratch, "Key %d (%s)", n+1, keycode_to_string(key_bindings[n]));
     strcpy(tmp_item->choices[BTN_ASSIGN_CUSTOM_KEY_1 + n], scratch);
  }

  if (emux_machine_class == BMC64_MACHINE_CLASS_VIC20) {
    tmp_item->choice_disabled[BTN_ASSIGN_SWAP_PORTS] = 1;
  }


  if (emux_machine_class != BMC64_MACHINE_CLASS_C64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_SCPU64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_C128) {
    tmp_item->choice_disabled[BTN_ASSIGN_CART_FREEZE] = 1;
  }

  if (emux_machine_class != BMC64_MACHINE_CLASS_C128) {
    tmp_item->choice_disabled[BTN_ASSIGN_ACTIVE_DISPLAY] = 1;
    tmp_item->choice_disabled[BTN_ASSIGN_PIP_LOCATION] = 1;
    tmp_item->choice_disabled[BTN_ASSIGN_PIP_SWAP] = 1;
    tmp_item->choice_disabled[BTN_ASSIGN_40_80_COLUMN] = 1;
  }

  if (emux_machine_class == BMC64_MACHINE_CLASS_PET) {
    tmp_item->choice_disabled[BTN_ASSIGN_VKBD_TOGGLE] = 1;
  }
}



static void show_button_assignment_menu(int device) {
  struct menu_item *root = ui_push_menu(-1, -1);
  struct menu_item *item;
  char name[MAX_MENU_STR];

  if (root == NULL) {
    return;
  }
  root->on_popped_off = assignment_menu_popped;
  memset(usb_button_items[device], 0, sizeof(usb_button_items[0]));

  snprintf(name, sizeof name, "USB %d: By Button", device + 1);
  item = ui_menu_add_button(MENU_TEXT, root, name);
  item->disabled = 1;
  ui_menu_add_divider(root);

  if (joy_num_buttons[device] <= 0) {
    item = ui_menu_add_button(MENU_TEXT, root, "No buttons detected");
    item->disabled = 1;
    return;
  }

  for (int button = 0; button < joy_num_buttons[device]; button++) {
    snprintf(name, sizeof name, "Button %d Function", button);
    item = ui_menu_add_multiple_choice(MENU_USB_0_BTN_ASSIGN + device, root,
                                       name);
    add_button_choices(item);
    item->value = usb_button_assignments[device][button];
    item->on_value_changed = menu_usb_value_changed;
    item->sub_id = button;
    usb_button_items[device][button] = item;
  }
}




static void show_function_assignment_menu(int device) {
  struct menu_item *root = ui_push_menu(-1, -1);
  struct menu_item *item;
  char name[MAX_MENU_STR];

  if (root == NULL) {
    return;
  }
  root->on_popped_off = assignment_menu_popped;
  memset(function_items, 0, sizeof(function_items));

  snprintf(name, sizeof name, "USB %d: By Function", device + 1);
  item = ui_menu_add_button(MENU_TEXT, root, name);
  item->disabled = 1;
  capture_status_item = ui_menu_add_button_with_value(
      MENU_TEXT, root, "Status", 0, "", "Select a function");
  capture_status_item->disabled = 1;
  ui_menu_add_divider(root);

  for (int assignment = BTN_ASSIGN_FIRE;
       assignment < NUM_BUTTON_ASSIGNMENTS; assignment++) {
    format_button_assignment_name(assignment, name, sizeof name);
    item = ui_menu_add_button_with_value(MENU_USB_FUNCTION_ASSIGN, root, name,
                                         assignment, "", "");
    item->sub_id = assignment;
    item->on_value_changed = menu_usb_value_changed;
    function_items[assignment] = item;
  }

  ui_menu_add_divider(root);
  clear_button_item =
      ui_menu_add_button(MENU_USB_CLEAR_BUTTON, root, "Clear a Button...");
  clear_button_item->on_value_changed = menu_usb_value_changed;
  refresh_function_items(device);
}

void build_usb_menu(int dev, struct menu_item *root) {
  struct menu_item *usb_pref_item;
  struct menu_item *x_axis_item;
  struct menu_item *y_axis_item;
  struct menu_item *x_thresh_item;
  struct menu_item *y_thresh_item;
  struct menu_item *tmp_item;
  char scratch[80];
  const char *profile_name = NULL;
  int j;



  configured_device = dev;
  root->on_popped_off = mapping_menu_popped;
  memset(usb_button_items[dev], 0, sizeof(usb_button_items[0]));
  memset(function_items, 0, sizeof(function_items));

  if (joy_num_pads > dev) {
    profile_name = usb_gamepad_default_profile_display_name_for_device(dev);
    if (profile_name != NULL) {
      sprintf(scratch, "USB %d: %s", dev + 1, profile_name);
      ui_menu_add_button(MENU_TEXT, root, scratch);
    } else {
      sprintf(scratch, "USB %d: DETECTED", dev + 1);
      ui_menu_add_button(MENU_TEXT, root, scratch);
    }
    sprintf(scratch, "USB %d: %d axes, %d hats, %d buttons", dev + 1,
            joy_num_axes[dev], joy_num_hats[dev], joy_num_buttons[dev]);
    ui_menu_add_button(MENU_TEXT, root, scratch);
  } else {
    sprintf(scratch, "USB %d: NOT DETECTED", dev + 1);
    ui_menu_add_button(MENU_TEXT, root, scratch);
  }

  ui_menu_add_divider(root);

  mapping_mode_items[dev] =
      ui_menu_add_multiple_choice(MENU_USB_0_MAPPING_MODE + dev, root,
                                  "Mapping Mode");
  mapping_mode_items[dev]->num_choices = 2;
  strcpy(mapping_mode_items[dev]->choices[USB_MAPPING_AUTOMATIC],
         "Automatic");
  strcpy(mapping_mode_items[dev]->choices[USB_MAPPING_CUSTOM], "Custom");
  mapping_mode_items[dev]->value = usb_mapping_modes[dev];
  mapping_mode_items[dev]->on_value_changed = menu_usb_value_changed;
  usb_pref_item = usb_pref_items[dev] =
      ui_menu_add_multiple_choice(MENU_USB_0_PREF+dev, root, "");
  snprintf(usb_pref_item->name, MAX_MENU_STR, "USB %d Directions", dev+1);
  usb_pref_item->value = usb_pref[dev];

  x_axis_item = usb_x_axis_items[dev] = ui_menu_add_range(MENU_USB_0_X_AXIS+dev, root, "",
                                  0, 12, 1, usb_x_axis[dev]);
  snprintf(x_axis_item->name, MAX_MENU_STR, "USB %d Analog X #", dev+1);
  y_axis_item = usb_y_axis_items[dev] = ui_menu_add_range(MENU_USB_0_Y_AXIS+dev, root, "",
                                  0, 12, 1, usb_y_axis[dev]);
  snprintf(y_axis_item->name, MAX_MENU_STR, "USB %d Analog Y #", dev+1);
  x_thresh_item = usb_x_thresh_items[dev] = ui_menu_add_range(MENU_USB_0_X_THRESH+dev, root,
                                    "", 10, 90, 1,
                                    (int)(usb_x_thresh[dev] * 100.0f));
  snprintf(x_thresh_item->name, MAX_MENU_STR, "USB %d Analog X Threshold % #", dev+1);
  y_thresh_item = usb_y_thresh_items[dev] = ui_menu_add_range(MENU_USB_0_Y_THRESH+dev, root,
                                    "", 10, 90, 1,
                                    (int)(usb_y_thresh[dev] * 100.0f));
  snprintf(y_thresh_item->name, MAX_MENU_STR, "USB %d Analog Y Threshold % #", dev+1);

  ui_menu_add_divider(root);
  tmp_item = ui_menu_add_button(MENU_USB_0_WATCH_RAW+dev, root,
                                "");
  snprintf(tmp_item->name, MAX_MENU_STR, "Monitor Raw USB %d data...", dev+1);
  tmp_item->on_value_changed = menu_usb_value_changed;

  tmp_item = ui_menu_add_button(MENU_USB_0_DIAG+dev, root, "");
  snprintf(tmp_item->name, MAX_MENU_STR, "Write USB %d info to BMC64-DIAG.TXT", dev+1);
  tmp_item->on_value_changed = menu_usb_value_changed;

  if (joy_num_pads > dev) {
    ui_menu_add_divider(root);
    tmp_item = ui_menu_add_button(MENU_USB_0_RESET_DEFAULTS+dev, root,
                                  "Reset to Default Bindings");
    tmp_item->on_value_changed = menu_usb_value_changed;
    ui_menu_add_divider(root);
  }



  tmp_item = ui_menu_add_button(MENU_USB_ASSIGN_BY_BUTTON, root,
                                "Assignments by Button...");
  tmp_item->sub_id = dev;
  tmp_item->on_value_changed = menu_usb_value_changed;
  assign_by_function_item = tmp_item =
      ui_menu_add_button(MENU_USB_ASSIGN_BY_FUNCTION, root,
                         "Assignments by Function...");
  tmp_item->sub_id = dev;
  tmp_item->on_value_changed = menu_usb_value_changed;

  ui_menu_add_divider(root);
  define_bindings_item =
     ui_menu_add_button(MENU_CONFIGURE_KEY_BINDINGS, root, "Define Key Bindings");

  ui_menu_add_divider(root);
  potx_high_item = ui_menu_add_range(MENU_POTX_HIGH, root, "POT X Up Value", 0,
                                     255, 1, pot_x_high_value);
  potx_low_item = ui_menu_add_range(MENU_POTX_LOW, root, "POT X Down Value", 0,
                                    255, 1, pot_x_low_value);
  poty_high_item = ui_menu_add_range(MENU_POTY_HIGH, root, "POT Y Up Value", 0,
                                     255, 1, pot_y_high_value);
  poty_low_item = ui_menu_add_range(MENU_POTY_LOW, root, "POT Y Down Value", 0,
                                    255, 1, pot_y_low_value);

  usb_pref_item->num_choices = 3;
  strcpy(usb_pref_item->choices[0], "Analog+POTX/Y Buttons");
  strcpy(usb_pref_item->choices[1], "Hat+POTX/Y Buttons");
  strcpy(usb_pref_item->choices[2], "Hat+POTX/Y Paddles");

  define_bindings_item->on_value_changed = menu_usb_value_changed;
  usb_pref_item->on_value_changed = menu_usb_value_changed;
  x_axis_item->on_value_changed = menu_usb_value_changed;
  y_axis_item->on_value_changed = menu_usb_value_changed;
  x_thresh_item->on_value_changed = menu_usb_value_changed;
  y_thresh_item->on_value_changed = menu_usb_value_changed;

  potx_high_item->on_value_changed = menu_usb_value_changed;
  potx_low_item->on_value_changed = menu_usb_value_changed;
  poty_high_item->on_value_changed = menu_usb_value_changed;
  poty_low_item->on_value_changed = menu_usb_value_changed;
}

int emu_wants_raw_usb(void) {
  return ui_enabled && want_raw_usb;
}

void emu_set_raw_usb(int device,
                     unsigned buttons,
                     const int hats[MAX_USB_HATS],
                     const int axes[MAX_USB_AXES]) {
  // Don't do too much here. This is from an isr.  Just set values for paint to
  // update.
  int i;
  if (device == want_raw_usb_device) {
    raw_buttons_item->value = buttons;
    for (i = 0; i < joy_num_hats[device]; i++) {
      raw_hats_item[i]->value = hats[i];
    }
    for (i = 0; i < joy_num_axes[device]; i++) {
      raw_axes_item[i]->value = axes[i];
    }
  }
}

// Compares the previous button state for 'button_num' with
// the current state and will return a press or release event
// for that button if the button has a button assignment.
// If no button assignment is present or if there is no
// change to the button state, btn_assignemnt will be set to
// BTN_ASSIGN_UNDEF.  Otherwise, it will be set to the button
// assignmnt and is_press will be set to 1 for a down event,
// 0 for an up event.  Returns -1 when button_num has reached
// or exceeded the number of buttons known to be available for
// the given usb gamepad device.  Otherwise, returns 0.
// Caller should keep calling this method until -1 is returned,
// otherwise, previous button state will not be correctly
// recorded.
int emu_button_function(int device, int button_num, unsigned buttons,
                        int* btn_assignment, int* is_press) {
  if (button_num >= joy_num_buttons[device]) {
     // No more buttons
     joy_prev_buttons[device] = buttons;
     return -1;
  }

  unsigned prev_buttons = joy_prev_buttons[device];
  if ((prev_buttons & usb_button_bits[button_num]) &&
      !(buttons & usb_button_bits[button_num])) {
    // Was down, now up.
    *is_press = 0;
    *btn_assignment = usb_button_assignments[device][button_num];
    return 0;
  } else if (!(prev_buttons & usb_button_bits[button_num]) &&
              (buttons & usb_button_bits[button_num])) {
    // Was up, now down.
    *is_press = 1;
    *btn_assignment = usb_button_assignments[device][button_num];
    return 0;
  }

  *btn_assignment = BTN_ASSIGN_UNDEF;
  *is_press = 0;
  return 0;
}

int add_pot_values(int *value, int potx, int poty) {
  if (potx) {
     *value |= (pot_x_low_value << 5);
  } else {
     *value |= (pot_x_high_value << 5);
  }
  if (poty) {
     *value |= (pot_y_low_value << 13);
  } else {
     *value |= (pot_y_high_value << 13);
  }
  return 0;
}

int emu_add_button_values(int dev, unsigned b) {
  int i;
  int value = (pot_x_high_value << 5) | (pot_y_high_value << 13);

  for (i = 0; i < joy_num_buttons[dev]; i++) {
    if (b & usb_button_bits[i]) {
      int j = usb_button_assignments[dev][i];
      switch (j) {
      case BTN_ASSIGN_FIRE:
        value |= 0x10;
        break;
      case BTN_ASSIGN_POTX:
        value &= ~(pot_x_high_value << 5);
        value |= (pot_x_low_value << 5);
        break;
      case BTN_ASSIGN_POTY:
        value &= ~(pot_y_high_value << 13);
        value |= (pot_y_low_value << 13);
        break;
      case BTN_ASSIGN_UP:
        value |= 0x1;
        break;
      case BTN_ASSIGN_DOWN:
        value |= 0x2;
        break;
      case BTN_ASSIGN_LEFT:
        value |= 0x4;
        break;
      case BTN_ASSIGN_RIGHT:
        value |= 0x8;
        break;
      }
    }
  }
  return value;
}

long emu_get_key_binding(int index) {
  return key_bindings[index];
}




int emu_gamepad_button_capture_report(unsigned device, unsigned buttons,
                                      unsigned button_count) {
  int captured = bmx_gamepad_button_capture_report(
      &button_capture, device, buttons, button_count);
  if (captured && device < MAX_USB_DEVICES) {

    joy_prev_buttons[device] = buttons;
  }
  return captured;
}



void menu_usb_capture_refresh(void) {
  BmxGamepadCaptureState state;
  unsigned device;
  unsigned button;
  int assignment;
  char assignment_name[MAX_MENU_STR];
  char status[MAX_DSP_VAL_LEN];

  if (bmx_gamepad_button_capture_result(&button_capture, &device, &button,
                                        &assignment)) {
    if (device < MAX_USB_DEVICES && button < MAX_USB_BUTTONS &&
        assignment >= BTN_ASSIGN_UNDEF &&
        assignment < NUM_BUTTON_ASSIGNMENTS) {
      usb_button_assignments[device][button] = assignment;
      mark_mapping_custom((int)device);
      format_button_assignment_name(assignment, assignment_name,
                                    sizeof assignment_name);
      if (assignment == BTN_ASSIGN_UNDEF) {
        snprintf(status, sizeof status, "Button %u cleared", button);
      } else {
        snprintf(status, sizeof status, "Button %u -> %s", button,
                 assignment_name);
      }
      if (capture_status_item != NULL) {
        ui_menu_set_button_value_fitted(capture_status_item, status, 1);
      }
    }
    bmx_gamepad_button_capture_finish(&button_capture);
    return;
  }

  state = bmx_gamepad_button_capture_state(&button_capture);
  if (state == BMX_GAMEPAD_CAPTURE_IDLE || capture_status_item == NULL) {
    return;
  }
  if (configured_device < 0 || configured_device >= MAX_USB_DEVICES ||
      joy_num_pads <= configured_device ||
      joy_num_buttons[configured_device] <= 0) {
    bmx_gamepad_button_capture_cancel(&button_capture);
    ui_menu_set_button_value_fitted(capture_status_item,
                                    "Gamepad unavailable", 1);
    return;
  }

  if (capture_assignment == BTN_ASSIGN_UNDEF) {
    snprintf(assignment_name, sizeof assignment_name, "Clear");
  } else {
    format_button_assignment_name(capture_assignment, assignment_name,
                                  sizeof assignment_name);
  }
  switch (state) {
  case BMX_GAMEPAD_CAPTURE_WAIT_NEUTRAL:
    snprintf(status, sizeof status, "%s: release buttons", assignment_name);
    break;
  case BMX_GAMEPAD_CAPTURE_WAIT_PRESS:
    snprintf(status, sizeof status, "%s: press button", assignment_name);
    break;
  case BMX_GAMEPAD_CAPTURE_WAIT_RELEASE:
    snprintf(status, sizeof status, "%s: release to assign", assignment_name);
    break;
  case BMX_GAMEPAD_CAPTURE_COMPLETE:
  case BMX_GAMEPAD_CAPTURE_IDLE:
    return;
  }
  ui_menu_set_button_value_fitted(capture_status_item, status, 1);
}


























































