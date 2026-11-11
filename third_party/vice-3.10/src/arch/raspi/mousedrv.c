/*
 * mousedrv.c
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

#include "mousedrv.h"

#include <stdio.h>

#include "emux_api.h"
#include "joyport.h"
#include "resources.h"


#include "circle.h"

#define MOUSE_SENSITIVITY_DEFAULT 100
#define MOUSE_SENSITIVITY_MIN 10
#define MOUSE_SENSITIVITY_MAX 200

/* Circle delivers USB mouse movement on core 0; VICE consumes it on core 1. */
static int mouse_pending_x;
static int mouse_pending_y;
static int mouse_sensitivity = MOUSE_SENSITIVITY_DEFAULT;
static int bmx_mouse_type = BMX_MOUSE_TYPE_DEFAULT;
static int mouse_resources_registered;
static mouse_func_t mouse_funcs;

static int set_mouse_sensitivity(int value, void *param) {
  (void)param;

  if (value < MOUSE_SENSITIVITY_MIN || value > MOUSE_SENSITIVITY_MAX) {
    return -1;
  }
  __atomic_store_n(&mouse_sensitivity, value, __ATOMIC_RELEASE);
  return 0;
}

static int set_bmx_mouse_type(int value, void *param) {
  (void)param;

  if (value < 0 || value >= BMX_MOUSE_TYPE_NUM) {
    return -1;
  }
  bmx_mouse_type = value;
  return 0;
}

static const resource_int_t mouse_resources_int[] = {
    {"MouseSensitivity", MOUSE_SENSITIVITY_DEFAULT, RES_EVENT_SAME, NULL,
     &mouse_sensitivity, set_mouse_sensitivity, NULL},
    {"BMXMouseType", BMX_MOUSE_TYPE_DEFAULT, RES_EVENT_SAME, NULL,
     &bmx_mouse_type, set_bmx_mouse_type, NULL},
    RESOURCE_INT_LIST_END};

int mousedrv_get_mouse_type(void) { return bmx_mouse_type; }

int mousedrv_mouse_type_to_joyport_id(int type) {
  switch (type) {
    case BMX_MOUSE_TYPE_1351:
      return JOYPORT_ID_MOUSE_1351;
    case BMX_MOUSE_TYPE_NEOS:
      return JOYPORT_ID_MOUSE_NEOS;
    case BMX_MOUSE_TYPE_AMIGA:
      return JOYPORT_ID_MOUSE_AMIGA;
    case BMX_MOUSE_TYPE_CX22:
      return JOYPORT_ID_MOUSE_CX22;
    case BMX_MOUSE_TYPE_ST:
      return JOYPORT_ID_MOUSE_ST;
    case BMX_MOUSE_TYPE_SMART:
      return JOYPORT_ID_MOUSE_SMART;
    case BMX_MOUSE_TYPE_DIGITAL:
      return JOYPORT_ID_MOUSE_DIGITAL;
    case BMX_MOUSE_TYPE_MICROMYS:
    default:
      return JOYPORT_ID_MOUSE_MICROMYS;
  }
}

static int take_scaled_mouse_delta(float *delta_x, float *delta_y) {
  int raw_x = __atomic_exchange_n(&mouse_pending_x, 0, __ATOMIC_ACQ_REL);
  int raw_y = __atomic_exchange_n(&mouse_pending_y, 0, __ATOMIC_ACQ_REL);
  float scale = (float)__atomic_load_n(&mouse_sensitivity, __ATOMIC_ACQUIRE) /
                100.0f;

  *delta_x = (float)raw_x * scale;
  *delta_y = (float)raw_y * scale;
  return raw_x != 0 || raw_y != 0;
}

int mousedrv_resources_init(mouse_func_t *funcs) {
  mouse_funcs.mbl = funcs->mbl;
  mouse_funcs.mbr = funcs->mbr;
  mouse_funcs.mbm = funcs->mbm;
  mouse_funcs.mbu = funcs->mbu;
  mouse_funcs.mbd = funcs->mbd;

  if (!mouse_resources_registered) {
    if (resources_register_int(mouse_resources_int) < 0) {
      return -1;
    }
    mouse_resources_registered = 1;
  }

  return 0;
}

int mousedrv_cmdline_options_init(void) { return 0; }

void mousedrv_init(void) {}

void mousedrv_mouse_changed(void) {}

void mousedrv_poll(void) {
  float delta_x;
  float delta_y;

  if (take_scaled_mouse_delta(&delta_x, &delta_y)) {
    mouse_move(delta_x, delta_y);
  }
}

int mousedrv_poll_scaled(float *delta_x, float *delta_y) {
  return take_scaled_mouse_delta(delta_x, delta_y);
}

void mousedrv_clear_pending(void) {
  __atomic_exchange_n(&mouse_pending_x, 0, __ATOMIC_ACQ_REL);
  __atomic_exchange_n(&mouse_pending_y, 0, __ATOMIC_ACQ_REL);
}

void emu_mouse_move(int x, int y) {
  __atomic_fetch_add(&mouse_pending_x, x, __ATOMIC_RELAXED);
  __atomic_fetch_add(&mouse_pending_y, y, __ATOMIC_RELAXED);
}

void emu_mouse_button_left(int pressed) {
  mouse_funcs.mbl(pressed);
}

void emu_mouse_button_right(int pressed) {
  mouse_funcs.mbr(pressed);
}

void emu_mouse_button_middle(int pressed) {
  mouse_funcs.mbm(pressed);
}

void emu_mouse_wheel_up(int pressed) {
  mouse_funcs.mbu(pressed);
}

void emu_mouse_wheel_down(int pressed) {
  mouse_funcs.mbd(pressed);
}
