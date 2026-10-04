


/** \file   kbd.c
 * \brief   Headless UI keyboard stuff
 *
 * \author  Marco van den Heuvel <blackystardust68@yahoo.com>
 * \author  Michael C. Martin <mcmartin@gmail.com>
 * \author  Oliver Schaertel
 * \author  pottendo <pottendo@gmx.net>
 * \author  Bas Wassink <b.wassink@ziggo.nl>
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

#include "vice.h"

#include <stdio.h>
#include "lib.h"
#include "log.h"
#include "ui.h"

/* UNIX-specific; for kbd_arch_get_host_mapping */
#include <locale.h>
#include <string.h>


#include "keyboard.h"
#include "keymap.h"
#include "kbd.h"


int kbd_arch_get_host_mapping(void)
{
    /* printf("%s\n", __func__); */

    return KBD_MAPPING_US;
}


/** \brief  Initialize keyboard handling
 */

void kbd_arch_shutdown(void)
{
    /* printf("%s\n", __func__); */

    /* Also don't call kbd_hotkey_shutdown() here */
}




/** \brief  Initialize the hotkeys
 *
 * This allocates an initial hotkeys array of HOTKEYS_SIZE_INIT elements
 */
void kbd_hotkey_init(void)
{
    /* printf("%s\n", __func__); */
}

/** \brief  Clean up memory used by the hotkeys array
 */
void kbd_hotkey_shutdown(void)
{
    /* printf("%s\n", __func__); */
}
