
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#include "compat.h"

#include "portable-endian.h"

#include <smb2.h>
#include <libsmb2.h>
#include "libsmb2-private.h"


static int
l1(char c)
{
        int i = 0;
        while (c & 0x80) {
                i++;
                c <<= 1;
        }
        return i;
}








static int
validate_utf8_cp(const char **utf8, uint16_t *ret)
{
        int c = *(*utf8)++;
        int l, l_tmp;
        uint32_t cp;
        l = l_tmp = l1(c);
 
        switch (l) {
        case 0:

                *ret = c & 0x7f;
                return 1;
        case 1:

                return -1;
        case 2:
        case 3:
        case 4:
                cp = c & (0x7f >> l);



                while(--l_tmp) {
                        c = *(*utf8)++;
                        if (l1(c) != 1) {
                                return -1;
                        }
                        cp <<= 6;
                        cp |= (c & 0x3f);
                }


                switch (l) {
                case 2:
                        if (cp < 0x80) return -1;
                        break;
                case 3:
                        if (cp < 0x800) return -1;
                        break;
                case 4:
                        if (cp < 0x10000) return -1;
                        break;
                default: break;
                }


                if (cp < 0xd800 || (cp - 0xe000) < 0x2000) {

                        *ret = cp;
                        return 1;
                } else if (cp < 0xe000) {

                        return -1;
                } else if (cp < 0x110000) {
                        cp -= 0x10000;
                        *ret = 0xd800 | (cp >> 10);
                        *(ret+1) = 0xdc00 | (cp & 0x3ff) ;
                        return 2;
                } else {

                        return -1;
                }
        }
        return -1;
}




static int
validate_utf8_str(const char *utf8)
{
        const char *u = utf8;
        int i = 0;
        int cp_length;
        uint16_t cp[2];

        while (*u) {
                cp_length = validate_utf8_cp(&u, cp);
                if (cp_length < 0) {
                        return -1;
                }
                i += cp_length;
        }
        return i;
}


struct smb2_utf16 *
smb2_utf8_to_utf16(const char *utf8)
{
        struct smb2_utf16 *utf16;
        int i, len;

        len = validate_utf8_str(utf8);
        if (len < 0) {
                return NULL;
        }

        utf16 = (struct smb2_utf16 *)(malloc(offsetof(struct smb2_utf16, val) + 2 * len));
        if (utf16 == NULL) {
                return NULL;
        }

        utf16->len = len;
        i = 0;
        while (i < len) {
                switch(validate_utf8_cp(&utf8, &utf16->val[i])) {
                case 1:
                    utf16->val[i] = htole16(utf16->val[i]);
                    i += 1;
                    break;
                case 2:
                    utf16->val[i] = htole16(utf16->val[i]);
                    utf16->val[i+1] = htole16(utf16->val[i+1]);
                    i += 2;
                    break;
                default:

                    break;
                }
        }

        return utf16;
}

static int
utf16_size(const uint16_t *utf16, size_t utf16_len)
{
        int length = 0;
        const uint16_t *utf16_end = utf16 + utf16_len;
        while (utf16 < utf16_end) {
                uint32_t code = le16toh(*utf16++);

                if (code < 0x80) {
                        length += 1;
                } else if (code < 0x800) {
                        length += 2;
                } else if (code < 0xd800 || code - 0xe000 < 0x2000) {
                        length += 3;
                } else if (code < 0xdc00) {
                        uint32_t trail;
                        if (utf16 == utf16_end) {
                                return length + 3;
                        }

                        trail = le16toh(*utf16);
                        if (trail - 0xdc00 < 0x400) {
                                code = 0x10000 + ((code & 0x3ff) << 10) + (trail & 0x3ff);
                                if (code < 0x10000) {
                                        length += 3;
                                } else {
                                        length += 4;
                                }
                                utf16++;
                        } else {
                                length += 3;
                        }
                } else {
                        length += 3;
                }
        }

        return length;
}




const char *
smb2_utf16_to_utf8(const uint16_t *utf16, size_t utf16_len)
{
        int utf8_len = 1;
        char *str, *tmp;
        const uint16_t *utf16_end;
        

        utf8_len += utf16_size(utf16, utf16_len);
        str = tmp = (char*)malloc(utf8_len);
        if (str == NULL) {
                return NULL;
        }
        str[utf8_len - 1] = 0;

        utf16_end = utf16 + utf16_len;
        while (utf16 < utf16_end) {
                uint32_t code = le16toh(*utf16++);

                if (code < 0x80) {
                        *tmp++ = code;
                } else if (code < 0x800) {
                        *tmp++ = 0xc0 |  (code >> 6);
                        *tmp++ = 0x80 | ((code     ) & 0x3f);
                } else if (code < 0xD800 || code - 0xe000 < 0x2000) {
                        *tmp++ = 0xe0 |  (code >> 12);
                        *tmp++ = 0x80 | ((code >>  6) & 0x3f);
                        *tmp++ = 0x80 | ((code      ) & 0x3f);
                } else if (code < 0xdc00) {
                        uint32_t trail;
                        if (utf16 == utf16_end) {
                                *tmp++ = 0xef; *tmp++ = 0xbf; *tmp++ = 0xbd;
                                return str;
                        }

                        trail = le16toh(*utf16);
                        if (trail - 0xdc00 < 0x400) {
                                code = 0x10000 + ((code & 0x3ff) << 10) + (trail & 0x3ff);
                                if (code < 0x10000) {
                                        *tmp++ = 0xe0 |  (code >> 12);
                                        *tmp++ = 0x80 | ((code >>  6) & 0x3f);
                                        *tmp++ = 0x80 | ((code      ) & 0x3f);
                                } else {
                                        *tmp++ = 0xF0 | (code >> 18);
                                        *tmp++ = 0x80 | ((code >> 12) & 0x3F);
                                        *tmp++ = 0x80 | ((code >> 6) & 0x3F);
                                        *tmp++ = 0x80 | (code & 0x3F);
                                }
                                utf16++;
                        } else {

                                *tmp++ = 0xef; *tmp++ = 0xbf; *tmp++ = 0xbd;
                        }
                } else {

                        *tmp++ = 0xef; *tmp++ = 0xbf; *tmp++ = 0xbd;
                }
        }

        return str;
}
