//
// driver_circle.cpp
//
// Driver interface for Circle network driver
// by R. Stange <rsta2@gmx.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
// Alternatively, this software may be distributed under the terms of BSD
// license.
//

/*
 * The "struct brcmf_*_le" definitions in this file are:
 *
 * Copyright (c) 2012 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

extern "C" {

#include "includes.h"
#include "common.h"
#include "driver.h"
#include "eloop.h"

}

#ifdef BMC64_WLAN_TRACE
#define BMX_WPA_LOG(_fmt, ...) wpa_printf(MSG_INFO, "bmx-wpa: " _fmt, ##__VA_ARGS__)
#else
#define BMX_WPA_LOG(_fmt, ...)
#endif

#include <circle/netdevice.h>
#include <circle/macaddress.h>
#include <circle/string.h>
#include <wlan/bcm4343.h>
#include <assert.h>

typedef u16 __le16;
typedef u32 __le32;

struct brcmf_bss_info_le
{
	__le32 version;		/* version field */
#define	BRCMF_BSS_INFO_VERSION	109 /* curr ver of brcmf_bss_info_le struct */
	__le32 length;		/* byte length of data in this record,
				 * starting at version and including IEs
				 */
	u8 BSSID[ETH_ALEN];
	__le16 beacon_period;	/* units are Kusec */
	__le16 capability;	/* Capability information */
	u8 SSID_len;
	u8 SSID[32];
	struct {
		__le32 count;	/* # rates in this set */
		u8 rates[16];	/* rates in 500kbps units w/hi bit set if basic */
	} rateset;		/* supported rates */
	__le16 chanspec;	/* chanspec for bss */
	__le16 atim_window;	/* units are Kusec */
	u8 dtim_period;		/* DTIM period */
	__le16 RSSI;		/* receive signal strength (in dBm) */
	s8 phy_noise;		/* noise (in dBm) */

	u8 n_cap;		/* BSS is 802.11N Capable */
	/* 802.11N BSS Capabilities (based on HT_CAP_*): */
	__le32 nbss_cap;
	u8 ctl_ch;		/* 802.11N BSS control channel number */
	__le32 reserved32[1];	/* Reserved for expansion of BSS properties */
	u8 flags;		/* flags */
	u8 reserved[3];		/* Reserved for expansion of BSS properties */
#define BRCMF_MCSSET_LEN		16
	u8 basic_mcs[BRCMF_MCSSET_LEN];	/* 802.11N BSS required MCS set */

	__le16 ie_offset;	/* offset at which IEs start, from beginning */
	__le32 ie_length;	/* byte length of Information Elements */
	__le16 SNR;		/* average SNR of during frame reception */
	/* Add new fields here */
	/* variable length Information Elements */
};

struct brcmf_escan_result_le
{
	__le32 buflen;
	__le32 version;
	__le16 sync_id;
	__le16 bss_count;
	struct brcmf_bss_info_le bss_info_le;
};

#define BRCMF_ESCAN_RESULT_HEADER_SIZE \
	(sizeof (brcmf_escan_result_le) - sizeof (brcmf_bss_info_le))

#define CIRCLE_BSS_CACHE_SIZE	16

struct circle_bss_entry
{
	int valid;
	u8 bssid[ETH_ALEN];
	u8 ssid[32];
	size_t ssid_len;
	int freq;
	int chan;
	u16 chanspec;
	int level;
	struct os_reltime seen;
	unsigned scan_generation;
};

struct circle_wlan_state
{
	circle_bss_entry current_bss;
	circle_bss_entry pending_bss;
	circle_bss_entry cache[CIRCLE_BSS_CACHE_SIZE];
	struct os_reltime last_scan_time;
	unsigned scan_generation;
	unsigned join_generation;
	unsigned assoc_generation;
	int driver_connected;
};

struct wpa_driver_circle_data
{
	void *ctx;
	CBcm4343Device *netdev;
	size_t ssid_len;
	u8 ssid[32];
	int country_set;
	int scan_pending;
	circle_wlan_state wlan_state;
};

static void wpa_driver_circle_scan_timeout (void *eloop_ctx, void *timeout_ctx);

static const char countries[][3] =
{
	"AD","AE","AF","AI","AL","AM","AN","AR","AS","AT","AU","AW","AZ",
	"BA","BB","BD","BE","BF","BG","BH","BL","BM","BN","BO","BR","BS",
	"BT","BY","BZ","CA","CF","CH","CI","CL","CN","CO","CR","CU","CX",
	"CY","CZ","DE","DK","DM","DO","DZ","EC","EE","EG","ES","ET","FI",
	"FM","FR","GB","GD","GE","GF","GH","GL","GP","GR","GT","GU","GY",
	"HK","HN","HR","HT","HU","ID","IE","IL","IN","IR","IS","IT","JM",
	"JO","JP","KE","KH","KN","KP","KR","KW","KY","KZ","LB","LC","LI",
	"LK","LS","LT","LU","LV","MA","MC","MD","ME","MF","MH","MK","MN",
	"MO","MP","MQ","MR","MT","MU","MV","MW","MX","MY","NG","NI","NL",
	"NO","NP","NZ","OM","PA","PE","PF","PG","PH","PK","PL","PM","PR",
	"PT","PW","PY","QA","RE","RO","RS","RU","RW","SA","SE","SG","SI",
	"SK","SN","SR","SV","SY","TC","TD","TG","TH","TN","TR","TT","TW",
	"TZ","UA","UG","US","UY","UZ","VC","VE","VI","VN","VU","WF","WS",
	"YE","YT","ZA","ZW"
};

static int is_valid_country_code (const char *alpha2)
{
	assert (alpha2 != 0);

	for (unsigned i = 0; i < sizeof countries / sizeof countries[0]; i++)
	{
		if (   countries[i][0] == alpha2[0]
		    && countries[i][1] == alpha2[1])
		{
			return 1;
		}
	}

	return 0;
}

static int wpa_driver_circle_get_bssid (void *priv, u8 *bssid)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	assert (drv->netdev != 0);
	const CMACAddress *mac = drv->netdev->GetBSSID ();

	assert (mac != 0);
	assert (bssid != 0);
	mac->CopyTo (bssid);

	return 0;
}

static int wpa_driver_circle_get_ssid (void *priv, u8 *ssid)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	assert (ssid != 0);
	if (drv->ssid_len > 0)
	{
		os_memcpy (ssid, drv->ssid, drv->ssid_len);
	}

	return drv->ssid_len;
}

static CString format_addr (const u8 *addr)
{
	static const u8 zero[ETH_ALEN] = {0};
	if (addr == 0)
	{
		addr = zero;
	}

	CString Number;
	CString Address;
	for (int i = 0; i < ETH_ALEN; i++)
	{
		Number.Format ("%02X", addr[i]);
		Address.Append (Number);
	}

	return Address;
}

static int freq2chan (int freq)
{
	if (freq == 2484)
	{
		return 14;
	}

	if (2412 <= freq && freq <= 2472 && (freq - 2407) % 5 == 0)
	{
		return (freq - 2407) / 5;
	}

	if (5000 <= freq && freq <= 5900 && (freq - 5000) % 5 == 0)
	{
		return (freq - 5000) / 5;
	}

	return 0;
}

static u16 brcmf_chanspec_2g20 (int chan)
{
	if (chan < 1 || chan > 14)
	{
		return 0;
	}

	/*
	 * Broadcom 11n chanspec layout as used by brcmfmac for 2.4 GHz,
	 * 20 MHz, no sideband. Prefer the firmware-provided scan chanspec
	 * when available; this helper is only preparation for later fallback.
	 */
	return (u16) (0x1000 | 0x0800 | 0x0000 | chan);
}

static void circle_bss_clear (circle_bss_entry *bss)
{
	os_memset (bss, 0, sizeof *bss);
}

static void circle_note_scan_start (wpa_driver_circle_data *drv)
{
	drv->wlan_state.scan_generation++;
	os_get_reltime (&drv->wlan_state.last_scan_time);
	for (unsigned i = 0; i < CIRCLE_BSS_CACHE_SIZE; i++)
	{
		circle_bss_clear (&drv->wlan_state.cache[i]);
	}
}

static void circle_note_scan_bss (wpa_driver_circle_data *drv,
				  const brcmf_bss_info_le *bss, int freq)
{
	unsigned slot = 0;
	unsigned count = CIRCLE_BSS_CACHE_SIZE;

	for (unsigned i = 0; i < count; i++)
	{
		if (!drv->wlan_state.cache[i].valid)
		{
			slot = i;
			break;
		}
		slot = i;
	}

	circle_bss_clear (&drv->wlan_state.cache[slot]);
	drv->wlan_state.cache[slot].valid = 1;
	os_memcpy (drv->wlan_state.cache[slot].bssid, bss->BSSID, ETH_ALEN);
	drv->wlan_state.cache[slot].ssid_len = bss->SSID_len < sizeof drv->wlan_state.cache[slot].ssid
					     ? bss->SSID_len
					     : sizeof drv->wlan_state.cache[slot].ssid;
	os_memcpy (drv->wlan_state.cache[slot].ssid, bss->SSID,
		   drv->wlan_state.cache[slot].ssid_len);
	drv->wlan_state.cache[slot].freq = freq;
	drv->wlan_state.cache[slot].chan = freq2chan (freq);
	drv->wlan_state.cache[slot].chanspec =
		bss->chanspec ? bss->chanspec : brcmf_chanspec_2g20 (drv->wlan_state.cache[slot].chan);
	drv->wlan_state.cache[slot].level = bss->RSSI;
	drv->wlan_state.cache[slot].scan_generation = drv->wlan_state.scan_generation;
	os_get_reltime (&drv->wlan_state.cache[slot].seen);
}

static const circle_bss_entry *circle_find_scan_bss (wpa_driver_circle_data *drv,
						     const u8 *bssid,
						     const u8 *ssid,
						     size_t ssid_len,
						     int freq)
{
	for (unsigned i = 0; i < CIRCLE_BSS_CACHE_SIZE; i++)
	{
		const circle_bss_entry *entry = &drv->wlan_state.cache[i];
		if (!entry->valid)
		{
			continue;
		}
		if (bssid != 0 && os_memcmp (entry->bssid, bssid, ETH_ALEN) != 0)
		{
			continue;
		}
		if (entry->ssid_len != ssid_len || os_memcmp (entry->ssid, ssid, ssid_len) != 0)
		{
			continue;
		}
		if (freq > 0 && entry->freq != freq)
		{
			continue;
		}
		return entry;
	}

	return 0;
}

static void circle_note_pending_bss (wpa_driver_circle_data *drv,
				     const u8 *bssid,
				     const u8 *ssid,
				     size_t ssid_len,
				     int freq)
{
	circle_bss_clear (&drv->wlan_state.pending_bss);

	const circle_bss_entry *entry =
		circle_find_scan_bss (drv, bssid, ssid, ssid_len, freq);
	if (entry == 0)
	{
		BMX_WPA_LOG("associate no fresh scan cache bssid=%s freq=%d gen=%u",
			    (const char *) format_addr (bssid), freq,
			    drv->wlan_state.scan_generation);
		return;
	}

	drv->wlan_state.pending_bss = *entry;
	BMX_WPA_LOG("associate selected cache bssid=%s freq=%d chan=%d chanspec=0x%04X gen=%u",
		    (const char *) format_addr (entry->bssid), entry->freq,
		    entry->chan, entry->chanspec, entry->scan_generation);
}

static void circle_note_driver_disconnect (wpa_driver_circle_data *drv,
					   const char *reason)
{
#if defined(BMC64_DEBUG_PROFILE) || defined(BMC64_WLAN_TRACE)
	int old_scan_pending = drv->scan_pending;
#endif
	int old_current_valid = drv->wlan_state.current_bss.valid;
	int old_pending_valid = drv->wlan_state.pending_bss.valid;
	int old_connected = drv->wlan_state.driver_connected;
	int full_disconnect = old_connected || old_current_valid || old_pending_valid;

	if (drv->scan_pending && full_disconnect)
	{
		drv->scan_pending = 0;
		eloop_cancel_timeout (wpa_driver_circle_scan_timeout, drv, drv->ctx);
	}

	circle_bss_clear (&drv->wlan_state.current_bss);
	circle_bss_clear (&drv->wlan_state.pending_bss);
	drv->wlan_state.driver_connected = 0;
	drv->wlan_state.join_generation++;
	drv->wlan_state.assoc_generation++;
	drv->ssid_len = 0;

#if defined(BMC64_DEBUG_PROFILE) || defined(BMC64_WLAN_TRACE)
	wpa_printf(MSG_INFO,
		   "bmx-wpa: disconnect-cleanup reason=%s connected=%d scan_pending=%d current=%d pending=%d full=%d join_gen=%u assoc_gen=%u",
		   reason != 0 ? reason : "-",
		   old_connected, old_scan_pending, old_current_valid,
		   old_pending_valid, full_disconnect,
		   drv->wlan_state.join_generation, drv->wlan_state.assoc_generation);
#endif
}

static int wpa_driver_circle_set_key (void *priv, wpa_driver_set_key_params *params)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);
	assert (drv->netdev != 0);

	wpa_alg alg = params->alg;
	const u8 *addr = params->addr;
	int key_idx = params->key_idx;
	int set_tx = params->set_tx;
	const u8 *seq = params->seq;
	size_t seq_len = params->seq_len;
	const u8 *key = params->key;
	size_t key_len = params->key_len;

	CString Address = format_addr (addr);

	if (alg == WPA_ALG_NONE)
	{
		BMX_WPA_LOG("clear key idx=%d addr=%s flags=0x%X",
			    key_idx, (const char *) Address,
			    (unsigned) params->key_flag);
		if (key_idx < 0 || key_idx > 5)
		{
			return -1;
		}
		if (!drv->netdev->Control ("clearkey %d", key_idx))
		{
			return -1;
		}
		return 0;
	}

	u8 key_tkip[32];
	if (alg == WPA_ALG_TKIP && key_len == 32)
	{
		// swap MIC keys, see set_key comment in driver.h
		os_memcpy (key_tkip, key, 16);
		os_memcpy (key_tkip+16, key+24, 8);
		os_memcpy (key_tkip+24, key+16, 8);

		key = key_tkip;
	}

	assert (alg == WPA_ALG_TKIP || alg == WPA_ALG_CCMP);
	CString Key (alg == WPA_ALG_TKIP ? "tkip:" : "ccmp:");
	CString Number;

	assert (key_len > 0);
	for (unsigned i = 0; i < key_len; i++)
	{
		Number.Format ("%02X", key[i]);
		Key.Append (Number);
	}

	Key.Append ("@");

	assert (seq_len > 1);
	for (int i = seq_len-1; i >= 0; i--)
	{
		Number.Format ("%02X", seq[i]);
		Key.Append (Number);
	}

	CString Command;
	if (set_tx)
	{
		assert (key_idx == 0);
		Command = "txkey";
	}
	else
	{
		assert (key_idx <= 3);
		Command.Format ("rxkey%u", key_idx);
	}

	if (!drv->netdev->Control ("%s %s %s", (const char *) Command,
				   (const char *) Address, (const char *) Key))
	{
		return -1;
	}

	return 0;
}

static void wpa_driver_circle_event_handler (ether_event_type_t		 type,
					     const ether_event_params_t *params,
					     void			*context)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) context;
	assert (drv != 0);

	BMX_WPA_LOG("event type=%u", (unsigned) type);

	wpa_event_data data;
	memset (&data, 0, sizeof data);

	switch (type)
	{
	case ether_event_link:		// ignore
		break;

	case ether_event_scan_complete:
		if (drv->scan_pending)
		{
			drv->scan_pending = 0;
			eloop_cancel_timeout (wpa_driver_circle_scan_timeout, drv, drv->ctx);
			wpa_supplicant_event (drv->ctx, EVENT_SCAN_RESULTS, 0);
		}
		break;

	case ether_event_disassoc:
		circle_note_driver_disconnect (drv, "event-disassoc");
		wpa_supplicant_event (drv->ctx, EVENT_DISASSOC, 0);
		break;

	case ether_event_deauth:
		circle_note_driver_disconnect (drv, "event-deauth");
		wpa_supplicant_event (drv->ctx, EVENT_DEAUTH, 0);
		break;

	case ether_event_mic_error:
		assert (params != 0);
		data.michael_mic_failure.unicast = !params->mic_error.group;
		data.michael_mic_failure.src = params->mic_error.addr;
		wpa_supplicant_event (drv->ctx, EVENT_MICHAEL_MIC_FAILURE, &data);
		break;

	default:
		wpa_printf (MSG_DEBUG, "Unhandled event %u", type);
		break;
	}
}

static void *wpa_driver_circle_init (void *ctx, const char *ifname)
{
	BMX_WPA_LOG("driver init ifname=%s", ifname ? ifname : "(null)");

	CNetDevice *netdev = CNetDevice::GetNetDevice (NetDeviceTypeWLAN);
	if (netdev == 0)
	{
		BMX_WPA_LOG("driver init failed: no wlan netdev");
		return 0;
	}

	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) os_zalloc (sizeof *drv);
	if (drv == 0)
	{
		BMX_WPA_LOG("driver init failed: alloc");
		return 0;
	}

	drv->ctx = ctx;
	drv->netdev = (CBcm4343Device *) netdev;	// netdev can only be of this type
	drv->country_set = 0;
	drv->scan_pending = 0;

	drv->netdev->RegisterEventHandler (wpa_driver_circle_event_handler, drv);

	BMX_WPA_LOG("driver init ok");

	return drv;
}

static void wpa_driver_circle_deinit (void *priv)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	drv->netdev->RegisterEventHandler (0, 0);
	eloop_cancel_timeout (wpa_driver_circle_scan_timeout, drv, drv->ctx);

	os_free (drv);
}

#define SCAN_DURATION_SECS	5
#define SCAN_FALLBACK_TIMEOUT_SECS	12

static void wpa_driver_circle_scan_timeout (void *eloop_ctx, void *timeout_ctx)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) eloop_ctx;
	assert (drv != 0);

	assert (drv->netdev != 0);
	drv->scan_pending = 0;
	drv->netdev->Control ("escan 0");

	BMX_WPA_LOG("scan timeout fallback: posting results");

	wpa_supplicant_event (timeout_ctx, EVENT_SCAN_RESULTS, 0);
}

static int wpa_driver_circle_scan2 (void *priv, wpa_driver_scan_params *params)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	assert (params != 0);

	// TODO: allow scan params
	//assert (params->num_ssids == 0);
	assert (params->extra_ies == 0);
	assert (params->extra_ies_len == 0);
	//assert (params->freqs == 0);

	assert (drv->netdev != 0);
	// increase scan duration here to be sure, scan is not started again
	circle_note_scan_start (drv);
	BMX_WPA_LOG("scan2 start num_ssids=%u", (unsigned) params->num_ssids);
	if (!drv->netdev->Control ("escan %u", SCAN_DURATION_SECS+2))
	{
		BMX_WPA_LOG("scan2 failed: escan control");
		return -1;
	}

	eloop_cancel_timeout (wpa_driver_circle_scan_timeout, drv, drv->ctx);
	drv->scan_pending = 1;
	eloop_register_timeout (SCAN_FALLBACK_TIMEOUT_SECS, 0,
				wpa_driver_circle_scan_timeout, drv, drv->ctx);

	BMX_WPA_LOG("scan2 scheduled");

	return 0;
}

static int chanspec2freq (u16 chanspec)
{
	u8 chan = chanspec & 0xFF;

	if (1 <= chan && chan <= 14)
	{
		static const int low_freqs[] =
		{
			2412, 2417, 2422, 2427, 2432, 2437, 2442,
			2447, 2452, 2457, 2462, 2467, 2472, 2484
		};

		return low_freqs[chan-1];
	}

	if (32 <= chan && chan <= 173)
	{
		return 5160 + (chan-32) * 5;
	}

	return -1;
}

#define MAX_SCAN_RESULTS	128

static bool circle_scan_result_bounds (const u8 *buf, unsigned len,
				       const brcmf_escan_result_le **scan_res,
				       size_t *scan_len)
{
	assert (buf != 0);
	assert (scan_res != 0);
	assert (scan_len != 0);

	if (len < BRCMF_ESCAN_RESULT_HEADER_SIZE)
	{
		BMX_WPA_LOG("drop short scan result len=%u", len);
		return false;
	}

	const brcmf_escan_result_le *res = (const brcmf_escan_result_le *) buf;
	if (   res->buflen < BRCMF_ESCAN_RESULT_HEADER_SIZE
	    || res->buflen > len)
	{
		BMX_WPA_LOG("drop malformed scan result len=%u buflen=%u",
			    len, (unsigned) res->buflen);
		return false;
	}

	*scan_res = res;
	*scan_len = res->buflen;

	return true;
}

static bool circle_scan_bss_bounds (const brcmf_bss_info_le *bss, size_t remaining,
				    size_t *bss_len)
{
	assert (bss != 0);
	assert (bss_len != 0);

	if (remaining < sizeof *bss)
	{
		BMX_WPA_LOG("drop short scan bss remaining=%u", (unsigned) remaining);
		return false;
	}

	if (   bss->version != BRCMF_BSS_INFO_VERSION
	    || bss->length < sizeof *bss
	    || bss->length > remaining
	    || bss->SSID_len > sizeof bss->SSID)
	{
		BMX_WPA_LOG("drop malformed scan bss version=%u length=%u remaining=%u ssid_len=%u",
			    (unsigned) bss->version, (unsigned) bss->length,
			    (unsigned) remaining, (unsigned) bss->SSID_len);
		return false;
	}

	if (   bss->ie_offset > bss->length
	    || bss->ie_length > bss->length - bss->ie_offset
	    || (bss->ie_length != 0 && bss->ie_offset < sizeof *bss))
	{
		BMX_WPA_LOG("drop malformed scan bss ies offset=%u length=%u record=%u",
			    (unsigned) bss->ie_offset, (unsigned) bss->ie_length,
			    (unsigned) bss->length);
		return false;
	}

	*bss_len = bss->length;

	return true;
}

static wpa_scan_results *wpa_driver_circle_get_scan_results2 (void *priv)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	wpa_scan_res **res_vector =
		(wpa_scan_res **) os_zalloc (MAX_SCAN_RESULTS * sizeof (wpa_scan_res *));
	if (res_vector == 0)
	{
		return 0;
	}

	wpa_scan_results *results = (wpa_scan_results *) os_zalloc (sizeof (wpa_scan_results));
	if (results == 0)
	{
		os_free (res_vector);

		return 0;
	}
	results->res = res_vector;
	results->num = 0;

	unsigned len;
	u8 buf[FRAME_BUFFER_SIZE];
	assert (drv->netdev != 0);
	while (drv->netdev->ReceiveScanResult (buf, &len))
	{
		BMX_WPA_LOG("scan result frame len=%u", len);

		// remove remaining scan messages, if vector is full
		if (results->num == MAX_SCAN_RESULTS)
		{
			continue;
		}

		const brcmf_escan_result_le *scan_res;
		size_t scan_len;
		if (!circle_scan_result_bounds (buf, len, &scan_res, &scan_len))
		{
			continue;
		}

		const brcmf_bss_info_le *bss = &scan_res->bss_info_le;
		size_t remaining = scan_len - BRCMF_ESCAN_RESULT_HEADER_SIZE;
		for (unsigned i = 0; i < scan_res->bss_count; i++)
		{
			size_t bss_len;
			if (!circle_scan_bss_bounds (bss, remaining, &bss_len))
			{
				break;
			}

			int freq = chanspec2freq (bss->chanspec);
			if (freq > 0 && results->num < MAX_SCAN_RESULTS)
			{
				wpa_scan_res *res =
					(wpa_scan_res *) os_zalloc (sizeof (wpa_scan_res) + bss->ie_length);
				if (res == 0)
				{
					break;
				}

				os_memset (res, 0, sizeof *res);

				res->flags = WPA_SCAN_LEVEL_DBM | WPA_SCAN_QUAL_INVALID;
				os_memcpy (res->bssid, bss->BSSID, ETH_ALEN);
				res->freq = freq;
				res->beacon_int = bss->beacon_period;
				res->caps = bss->capability;
				res->noise = bss->phy_noise;
				res->level = bss->RSSI;
				// TODO: set res->tsf
				// TODO: set res->age

				// append IEs
				res->ie_len = bss->ie_length;
				os_memcpy ((u8 *) res + sizeof *res, (u8 *) bss + bss->ie_offset,
					   bss->ie_length);

				results->res[results->num] = res;
				results->num++;

				char ssid[33];
				unsigned ssid_len = bss->SSID_len < sizeof ssid - 1 ? bss->SSID_len : sizeof ssid - 1;
				os_memcpy (ssid, bss->SSID, ssid_len);
				ssid[ssid_len] = '\0';
				u8 own_mac[ETH_ALEN] = {0};
				const CMACAddress *own = drv->netdev->GetMACAddress ();
				if (own != 0) { own->CopyTo (own_mac); }
				circle_note_scan_bss (drv, bss, freq);
				BMX_WPA_LOG("scan bss bssid=" MACSTR " mac=" MACSTR " ssid='%s' freq=%d chan=%d chanspec=0x%04X rssi=%d",
					    MAC2STR(bss->BSSID), MAC2STR(own_mac), ssid, freq, freq2chan (freq),
					    bss->chanspec, (int) bss->RSSI);
			}

			bss = (const brcmf_bss_info_le *) ((const u8 *) bss + bss_len);
			remaining -= bss_len;
		}
	}

	BMX_WPA_LOG("scan results num=%u", (unsigned) results->num);

	return results;
}

static int wpa_driver_circle_deauthenticate (void *priv, const u8 *addr, u16 reason_code)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	BMX_WPA_LOG("deauthenticate reason=%u", (unsigned) reason_code);

	circle_note_driver_disconnect (drv, "deauthenticate");

	assert (drv->netdev != 0);
	if (!drv->netdev->Control ("disassoc %d", (int) reason_code))
	{
		BMX_WPA_LOG("deauthenticate best-effort disassoc failed");
	}

	return 0;
}

static int wpa_driver_circle_sta_disassociate (void *priv, const u8 *own_addr,
					       const u8 *addr, u16 reason)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	BMX_WPA_LOG("sta_disassociate reason=%u", (unsigned) reason);

	circle_note_driver_disconnect (drv, "sta-disassociate");

	assert (drv->netdev != 0);
	if (!drv->netdev->Control ("disassoc %d", (int) reason))
	{
		BMX_WPA_LOG("sta_disassociate best-effort disassoc failed");
	}

	return 0;
}

static int wpa_driver_circle_associate (void *priv, wpa_driver_associate_params *params)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);
	assert (params != 0);

	CString BSSID ("FFFFFFFFFFFF");
	if (params->bssid != 0)
	{
		BSSID = "";
		for (unsigned i = 0; i < ETH_ALEN; i++)
		{
			CString Number;
			Number.Format ("%02X", (unsigned) params->bssid[i]);

			BSSID.Append (Number);
		}
	}

	char ssid[32+1];
	assert (params->ssid != 0);
	assert (params->ssid_len < sizeof ssid);
	os_memcpy (ssid, params->ssid, params->ssid_len);
	ssid[params->ssid_len] = '\0';

	CString SSID (ssid);
	SSID.Replace (" ", "\\x20");

	int chan = freq2chan (params->freq.freq);

	u8 own_mac[ETH_ALEN] = {0};
	const CMACAddress *own = drv->netdev->GetMACAddress ();
	if (own != 0) { own->CopyTo (own_mac); }
	BMX_WPA_LOG("associate bssid=%s mac=" MACSTR " ssid='%s' freq=%d chan=%d auth_alg=0x%X wpa_ie_len=%u",
		    (const char *) BSSID, MAC2STR(own_mac), ssid, params->freq.freq,
		    chan, params->auth_alg,
		    (unsigned) params->wpa_ie_len);
	if (params->freq.freq > 0 && chan == 0)
	{
		BMX_WPA_LOG("associate unsupported join freq=%d; using firmware scan",
			    params->freq.freq);
	}

	if (!(params->auth_alg & WPA_AUTH_ALG_OPEN))
	{
		wpa_printf (MSG_ERROR, "Auth algorithm not supported (0x%X)", params->auth_alg);

		return -1;
	}

	CString Auth ("off");
	if (params->wpa_ie != 0 && params->wpa_ie_len > 0)
	{
		Auth = "";
		assert (params->wpa_ie != 0);
		for (unsigned i = 0; i < params->wpa_ie_len; i++)
		{
			CString Number;
			Number.Format ("%02X", (unsigned) params->wpa_ie[i]);

			Auth.Append (Number);
		}
	}

	if (!drv->country_set)
	{
		wpa_printf (MSG_ERROR, "Country code not set");

		return -1;
	}

	assert (drv->netdev != 0);
	drv->wlan_state.join_generation++;
	circle_note_pending_bss (drv, params->bssid, params->ssid,
				 params->ssid_len, params->freq.freq);
	u16 join_chanspec = drv->wlan_state.pending_bss.valid
			  ? drv->wlan_state.pending_bss.chanspec
			  : 0;
	if (join_chanspec != 0)
	{
		BMX_WPA_LOG("associate using scan chanspec=0x%04X", join_chanspec);
	}
	else
	{
		BMX_WPA_LOG("associate using firmware channel scan");
	}
	if (!drv->netdev->Control ("join %s %s 0x%04X %s", (const char *) SSID,
				   (const char *) BSSID, join_chanspec,
				   (const char *) Auth))
	{
		BMX_WPA_LOG("associate failed: join control");
		return -1;
	}

	os_memcpy (drv->ssid, params->ssid, params->ssid_len);
	drv->ssid_len = params->ssid_len;
	drv->wlan_state.current_bss = drv->wlan_state.pending_bss;
	drv->wlan_state.driver_connected = 1;
	drv->wlan_state.assoc_generation++;

	wpa_supplicant_event (drv->ctx, EVENT_ASSOC, 0);

	BMX_WPA_LOG("associate ok");

	return 0;
}

static int wpa_driver_circle_set_country (void *priv, const char *alpha2)
{
	wpa_driver_circle_data *drv = (wpa_driver_circle_data *) priv;
	assert (drv != 0);

	char country[3];
	assert (alpha2 != 0);
	country[0] = alpha2[0];
	country[1] = alpha2[1];
	country[2] = '\0';

	if (!is_valid_country_code (alpha2))
	{
		wpa_printf (MSG_ERROR, "Invalid country code: '%s'", country);

		return -1;
	}

	wpa_printf (MSG_INFO, "Setting country code to '%s'", country);

	assert (drv->netdev != 0);
	if (!drv->netdev->Control ("country %s", country))
	{
		BMX_WPA_LOG("set_country failed country=%s", country);
		return -1;
	}

	drv->country_set = 1;

	BMX_WPA_LOG("set_country ok country=%s", country);

	return 0;
}

extern "C" const struct wpa_driver_ops wpa_driver_circle_ops =
{
	.name = "circle",
	.desc = "Circle WLAN driver",
	.get_bssid = wpa_driver_circle_get_bssid,
	.get_ssid = wpa_driver_circle_get_ssid,
	.set_key = wpa_driver_circle_set_key,
	.init = wpa_driver_circle_init,
	.deinit = wpa_driver_circle_deinit,
	.deauthenticate = wpa_driver_circle_deauthenticate,
	.associate = wpa_driver_circle_associate,
	.get_scan_results2 = wpa_driver_circle_get_scan_results2,
	.set_country = wpa_driver_circle_set_country,
	.scan2 = wpa_driver_circle_scan2,
	.sta_disassoc = wpa_driver_circle_sta_disassociate,
};
