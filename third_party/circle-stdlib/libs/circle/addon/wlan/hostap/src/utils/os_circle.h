//
// os_circle.h
//
// OS specific functions for Circle
// by R. Stange <rsta2@gmx.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
// Alternatively, this software may be distributed under the terms of BSD
// license.
//
#ifndef _os_circle_h
#define _os_circle_h

#include <circle/alloc.h>
#include <circle/stdarg.h>
#include <circle/util.h>
#include <circle/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __LITTLE_ENDIAN	1234
#define __BYTE_ORDER	__LITTLE_ENDIAN
#define WPA_TYPES_DEFINED

#define NULL		0

#define __force

#define ENOMEM          1
#define EINVAL          2
#define EBUSY           3
#define EAGAIN          4
#define EIO             5
#define EOPNOTSUPP	6
#define ENOTCONN	7
#define ECANCELED	8

#define bswap_16	bswap16
#define bswap_32	bswap32

typedef signed long	time_t;

struct in_addr
{
        u32	s_addr;
};

#define abs		__wpa_abs
#define isblank		__wpa_isblank
#define isprint		__wpa_isprint
#define isspace		__wpa_isspace
#define strrchr		__wpa_strrchr
#define strdup		__wpa_strdup
#define strtol		__wpa_strtol
#define sscanf		__wpa_sscanf
#define snprintf	__wpa_snprintf
#define vsnprintf	__wpa_vsnprintf
#define printf		__wpa_printf
#define vprintf		__wpa_vprintf
#define qsort		__wpa_qsort
#define abort		__wpa_abort

int abs (int i);

int isblank (int c);
int isprint (int c);
int isspace (int c);

char *strrchr (const char *s, int c);
char *strdup (const char *s);

long strtol (const char *nptr, char **endptr, int base);

int sscanf (const char *str, const char *format, ...);

int snprintf (char *str, size_t size, const char *format, ...);
int vsnprintf (char *str, size_t size, const char *format, va_list ap);
int printf (const char *format, ...);
int vprintf (const char *format, va_list ap);

void qsort (void *base, size_t nmemb, size_t size,
	    int (*compare) (const void *, const void *));

void abort (void);

void os_hexdump (const char *title, const void *p, size_t len);

#ifdef __cplusplus
}
#endif

#endif
