/*
 * mousedrv.h
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

#ifndef RASPI_MOUSEDRV_H
#define RASPI_MOUSEDRV_H

#include "types.h"
#include "mouse.h"

extern int mousedrv_resources_init(mouse_func_t *funcs);
extern int mousedrv_cmdline_options_init(void);
extern void mousedrv_init(void);

extern void mousedrv_mouse_changed(void);

#define MOUSEDRV_HAS_POLL
extern void mousedrv_poll(void);
extern int mousedrv_poll_scaled(float *delta_x, float *delta_y);
extern void mousedrv_clear_pending(void);

extern int mousedrv_get_mouse_type(void);
extern int mousedrv_mouse_type_to_joyport_id(int type);

#endif
