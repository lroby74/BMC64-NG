/*
 * wpa_supplicant/hostapd / Debug prints
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 *
 * Modified for Circle by R. Stange <rsta2@gmx.net>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"

int wpa_debug_level = MSG_INFO;
int wpa_debug_show_keys = 0;
int wpa_debug_timestamp = 0;
int wpa_debug_syslog = 0;

#ifndef CONFIG_NO_STDOUT_DEBUG

int wpa_debug_open_file (const char *path) { return 0; }
int wpa_debug_reopen_file (void) { return 0; }
void wpa_debug_close_file (void) {}
void wpa_debug_setup_stdout (void) {}
void wpa_debug_stop_log (void) {}

void wpa_printf (int level, const char *fmt, ...)
{
	if (level >= wpa_debug_level)
	{
		va_list ap;
		va_start (ap, fmt);

		vprintf (fmt, ap);

		va_end (ap);
	}
}

void wpa_hexdump (int level, const char *title, const void *buf, size_t len)
{
	if (level >= wpa_debug_level)
	{
		os_hexdump (title, buf, len);
	}
}

void wpa_hexdump_key (int level, const char *title, const void *buf, size_t len)
{
	if (   wpa_debug_show_keys
	    && level >= wpa_debug_level)
	{
		os_hexdump (title, buf, len);
	}
}

void wpa_hexdump_ascii (int level, const char *title, const void *buf, size_t len)
{
	if (level >= wpa_debug_level)
	{
		os_hexdump (title, buf, len);
	}
}

void wpa_hexdump_ascii_key (int level, const char *title, const void *buf, size_t len)
{
	if (   wpa_debug_show_keys
	    && level >= wpa_debug_level)
	{
		os_hexdump (title, buf, len);
	}
}

#endif

#ifndef CONFIG_NO_WPA_MSG

void wpa_msg (void *ctx, int level, const char *fmt, ...)
{
	if (level >= wpa_debug_level)
	{
		va_list ap;
		va_start (ap, fmt);

		vprintf (fmt, ap);

		va_end (ap);
	}
}

void wpa_msg_ctrl (void *ctx, int level, const char *fmt, ...) {}

void wpa_msg_register_ifname_cb (wpa_msg_get_ifname_func func) {}

#endif
