/** \file   archdep_rename.c
 * \brief   Rename a file
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
#include "archdep_defs.h"

#include <stdio.h>

#include "archdep_rename.h"


/** \brief  Rename \a oldpath to \a newpath
 *
 * \param[in]   oldpath old name
 * \param[in]   newpath new name
 *
 * \return  0 on success, -1 on error
 */
int archdep_rename(const char *oldpath, const char *newpath)
{
#ifdef RASPI_COMPILE



    FILE *in, *out;
    char buf[512];
    size_t n;

    in = fopen(oldpath, "rb");
    if (in == NULL) {
        return -1;
    }
    out = fopen(newpath, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    remove(oldpath);
    return 0;
#else
    return rename(oldpath, newpath);
#endif
}
