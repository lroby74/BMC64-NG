/*
 * WPA Supplicant / Example program entrypoint
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 *
 * Modified for Circle by R. Stange <rsta2@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

extern "C" {

#include "includes.h"

#include "common.h"
#include "wpa_supplicant_i.h"

}

#ifdef BMC64_WLAN_TRACE
#define BMX_WPA_LOG(_fmt, ...) wpa_printf(MSG_INFO, "bmx-wpa: " _fmt, ##__VA_ARGS__)
#else
#define BMX_WPA_LOG(_fmt, ...)
#endif

static struct wpa_global *global = NULL;

int wpa_supplicant_is_connected (void)
{
	if (   global == NULL
	    || global->ifaces == NULL)
		return 0;

	return    global->ifaces->wpa_state == WPA_COMPLETED
	       || global->ifaces->wpa_state == WPA_GROUP_HANDSHAKE;
}

int wpa_supplicant_main (const char *confname)
{
	struct wpa_interface iface;
	int exitcode = 0;
	struct wpa_params params;

	memset(&params, 0, sizeof(params));
	params.wpa_debug_level = MSG_INFO;

	BMX_WPA_LOG("main start conf=%s", confname ? confname : "(null)");

	global = wpa_supplicant_init(&params);
	if (global == NULL) {
		BMX_WPA_LOG("init failed");
		return -1;
	}

	memset(&iface, 0, sizeof(iface));
	iface.driver = "circle";
	iface.ifname = "wlan0";
	iface.confname = confname;

	if (wpa_supplicant_add_iface(global, &iface, NULL) == NULL) {
		BMX_WPA_LOG("add_iface failed");
		exitcode = -1;
	}
	else {
		BMX_WPA_LOG("add_iface ok");
	}

	if (exitcode == 0) {
		BMX_WPA_LOG("run enter");
		exitcode = wpa_supplicant_run(global);
		BMX_WPA_LOG("run exit code=%d", exitcode);
	}

	wpa_supplicant_deinit(global);
	global = NULL;

	BMX_WPA_LOG("main exit code=%d", exitcode);

	return exitcode;
}
