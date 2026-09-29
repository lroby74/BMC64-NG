/** \file   ui.h
 * \brief   Main Gtk3 UI code - header
 *
 * \author  Marco van den Heuvel <blackystardust68@yahoo.com>
 */

/*
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

#ifndef VICE_UI_H
#define VICE_UI_H

#include "vice.h"

#include "videoarch.h"
#include "palette.h"




enum {
    PRIMARY_WINDOW,
    SECONDARY_WINDOW,
    MONITOR_WINDOW
};




void ui_dispatch_events(void);




int  ui_pause_active(void);
void ui_pause_enable(void);
bool ui_pause_loop_iteration(void);
void ui_pause_disable(void);
void ui_pause_toggle(void);

void ui_update_lightpen(void);

void ui_enable_crt_controls(int enabled);
void ui_enable_mixer_controls(int enabled);





const char *ui_action_get_name(int action);
int         ui_action_get_id(const char *name);
void        ui_action_trigger(int action);

video_canvas_t *ui_get_active_canvas(void);

#endif
