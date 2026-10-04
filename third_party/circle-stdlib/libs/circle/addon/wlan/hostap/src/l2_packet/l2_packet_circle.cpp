//
// l2_packet_circle.cpp
//
// Layer2 packet handling interface for Circle
// by R. Stange <rsta2@gmx.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
// Alternatively, this software may be distributed under the terms of BSD
// license.
//

extern "C" {

#include "includes.h"
#include "common.h"
#include "eloop.h"
#include "l2_packet.h"

}

#include <circle/net/netsubsystem.h>
#include <circle/net/netdevlayer.h>
#include <circle/net/linklayer.h>
#include <circle/atomic.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <assert.h>
#include <string.h>

#define SOCK_FD		1

struct l2_packet_data
{
	unsigned short protocol;
	void (*rx_callback) (void *ctx, const u8 *src_addr, const u8 *buf, size_t len);
	void *rx_callback_ctx;
	u8 own_addr[ETH_ALEN];
	CLinkLayer *link;
};

static void l2_packet_receive (int sock, void *eloop_ctx, void *sock_ctx);

static int auth_active;
static volatile int pending_8021x_tx;
static l2_packet_data *active_l2;

enum
{
	BMX_L2_EAPOL_UNKNOWN,
	BMX_L2_EAPOL_M1,
	BMX_L2_EAPOL_M2,
	BMX_L2_EAPOL_M3,
	BMX_L2_EAPOL_M4,
	BMX_L2_EAPOL_GROUP1,
	BMX_L2_EAPOL_GROUP2,
	BMX_L2_EAPOL_OTHER
};

#ifdef BMC64_WLAN_TRACE
#define BMX_L2_LOG(_fmt, ...) wpa_printf (MSG_INFO, "bmx-eapol: " _fmt, ##__VA_ARGS__)
#else
#define BMX_L2_LOG(_fmt, ...)
#endif
#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE_LOG)
#define BMX_L2_STATE_LOG(_fmt, ...) wpa_printf (MSG_INFO, "bmx-eapol: " _fmt, ##__VA_ARGS__)
#else
#define BMX_L2_STATE_LOG(_fmt, ...)
#endif

static u16 BMXL2BE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static unsigned BMXL2EAPOLMsg (const u8 *pBuffer, size_t nLength)
{
	if (pBuffer == 0 || nLength < 99 || pBuffer[1] != 3)
	{
		return BMX_L2_EAPOL_UNKNOWN;
	}

	u16 nKeyInfo = BMXL2BE16 (pBuffer+5);
	bool bPairwise = (nKeyInfo & 0x0008) != 0;
	bool bInstall = (nKeyInfo & 0x0040) != 0;
	bool bACK = (nKeyInfo & 0x0080) != 0;
	bool bMIC = (nKeyInfo & 0x0100) != 0;
	bool bSecure = (nKeyInfo & 0x0200) != 0;
	bool bEncrypted = (nKeyInfo & 0x1000) != 0;

	if (bPairwise)
	{
		if (bACK && !bMIC) return BMX_L2_EAPOL_M1;
		if (!bACK && bMIC && !bSecure) return BMX_L2_EAPOL_M2;
		if (bACK && bMIC && bInstall && bSecure && bEncrypted) return BMX_L2_EAPOL_M3;
		if (!bACK && bMIC && bSecure) return BMX_L2_EAPOL_M4;
		return BMX_L2_EAPOL_OTHER;
	}

	if (bACK && bMIC && bSecure) return BMX_L2_EAPOL_GROUP1;
	if (!bACK && bMIC && bSecure) return BMX_L2_EAPOL_GROUP2;
	return BMX_L2_EAPOL_OTHER;
}

static const char *BMXL2EAPOLMsgName (unsigned nMsg)
{
	switch (nMsg)
	{
	case BMX_L2_EAPOL_M1:	return "4way-1/4";
	case BMX_L2_EAPOL_M2:	return "4way-2/4";
	case BMX_L2_EAPOL_M3:	return "4way-3/4";
	case BMX_L2_EAPOL_M4:	return "4way-4/4";
	case BMX_L2_EAPOL_GROUP1: return "group-1/2";
	case BMX_L2_EAPOL_GROUP2: return "group-2/2";
	case BMX_L2_EAPOL_OTHER: return "key-other";
	default:		return "unknown";
	}
}

static boolean BMXL2DropInactiveEAPOL (unsigned nMsg)
{
#if 0
	return    nMsg == BMX_L2_EAPOL_M3
	       || nMsg == BMX_L2_EAPOL_M4
	       || nMsg == BMX_L2_EAPOL_GROUP1
	       || nMsg == BMX_L2_EAPOL_GROUP2;
#endif
	return FALSE;
}

extern "C" void bmx_l2_note_pending_8021x_tx (void)
{
	AtomicIncrement (&pending_8021x_tx);
}

extern "C" void bmx_l2_note_done_8021x_tx (void)
{
	int pending = AtomicGet (&pending_8021x_tx);
	if (pending <= 0)
	{
		AtomicSet (&pending_8021x_tx, 0);
		return;
	}

	AtomicDecrement (&pending_8021x_tx);
}

extern "C" int bmx_l2_wait_pending_8021x_tx (unsigned timeout_ms)
{
	unsigned start = CTimer::GetClockTicks ();
	unsigned timeout_us = timeout_ms * 1000;

	while (AtomicGet (&pending_8021x_tx) > 0)
	{
		unsigned elapsed = CTimer::GetClockTicks () - start;
		if (elapsed >= timeout_us)
		{
			wpa_printf (MSG_INFO,
				    "bmx-eapol: pending tx wait timeout pending=%d ms=%u",
				    AtomicGet (&pending_8021x_tx), timeout_ms);
			AtomicSet (&pending_8021x_tx, 0);
			return 0;
		}

		CScheduler::Get ()->Yield ();
		CTimer::SimpleMsDelay (1);
	}

	return 1;
}

extern "C" void bmx_l2_flush_8021x_rx (const char *pReason)
{
	l2_packet_data *l2 = active_l2;
	if (l2 == 0 || l2->link == 0)
	{
		return;
	}

	u8 Buffer[FRAME_BUFFER_SIZE];
	unsigned nResultLength;
	CMACAddress Sender;
	unsigned nFlushed = 0;
	while (nFlushed < 64 && l2->link->ReceiveRaw (Buffer, &nResultLength, &Sender))
	{
		nFlushed++;
		if ((nFlushed & 7) == 0)
		{
			CScheduler::Get ()->Yield ();
		}
	}

	if (nFlushed != 0)
	{
		BMX_L2_STATE_LOG ("rx flush reason=%s raw=%u",
				  pReason ? pReason : "-", nFlushed);
	}
}

static void bmx_l2_flush_receive_path (l2_packet_data *l2, const char *pReason)
{
	CNetSubSystem *pNetSubSystem = CNetSubSystem::Get ();
	if (pNetSubSystem != 0)
	{
		CNetDeviceLayer *pNetDeviceLayer = pNetSubSystem->GetNetDeviceLayer ();
		if (pNetDeviceLayer != 0)
		{
			pNetDeviceLayer->FlushReceiveQueue ();
		}

		CLinkLayer *pLinkLayer = pNetSubSystem->GetLinkLayer ();
		if (pLinkLayer != 0)
		{
			pLinkLayer->FlushRawReceiveQueue ();
		}
	}

	active_l2 = l2;
	bmx_l2_flush_8021x_rx (pReason);
}

#ifdef BMC64_WLAN_TRACE
static unsigned s_nBMXEAPOLTraceSeq;

static u16 BMXEAPOLBE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static const char *BMXEAPOLTypeName (unsigned nType)
{
	switch (nType)
	{
	case 0:
		return "packet";
	case 1:
		return "start";
	case 2:
		return "logoff";
	case 3:
		return "key";
	case 4:
		return "asf-alert";
	default:
		return "unknown";
	}
}

static const char *BMXEAPOLKeyDescName (unsigned nDesc)
{
	switch (nDesc)
	{
	case 2:
		return "rsn";
	case 254:
		return "wpa";
	default:
		return "unknown";
	}
}

static const char *BMXEAPOLKeyMsgName (u16 nKeyInfo)
{
	bool bPairwise = (nKeyInfo & 0x0008) != 0;
	bool bInstall = (nKeyInfo & 0x0040) != 0;
	bool bACK = (nKeyInfo & 0x0080) != 0;
	bool bMIC = (nKeyInfo & 0x0100) != 0;
	bool bSecure = (nKeyInfo & 0x0200) != 0;
	bool bEncrypted = (nKeyInfo & 0x1000) != 0;

	if (bPairwise)
	{
		if (bACK && !bMIC)
		{
			return "4way-1/4";
		}
		if (!bACK && bMIC && !bSecure)
		{
			return "4way-2/4";
		}
		if (bACK && bMIC && bInstall && bSecure && bEncrypted)
		{
			return "4way-3/4";
		}
		if (!bACK && bMIC && bSecure)
		{
			return "4way-4/4";
		}

		return "4way-key";
	}

	if (bACK && bMIC && bSecure)
	{
		return "group-1/2";
	}
	if (!bACK && bMIC && bSecure)
	{
		return "group-2/2";
	}

	return "group-key";
}

static unsigned BMXEAPOLTracePayload (const char *pStage, const u8 *pPeer,
				      const u8 *pBuffer, size_t nLength)
{
	unsigned nSeq = ++s_nBMXEAPOLTraceSeq;

	if (pBuffer == 0 || nLength < 4)
	{
		BMX_L2_LOG ("%s seq=%u peer=" MACSTR " len=%u truncated",
			    pStage, nSeq, MAC2STR (pPeer), (unsigned) nLength);
		return nSeq;
	}

	unsigned nVersion = pBuffer[0];
	unsigned nType = pBuffer[1];
	unsigned nEAPOLLen = BMXEAPOLBE16 (pBuffer+2);
	if (nType != 3 || nLength < 99)
	{
		BMX_L2_LOG ("%s seq=%u peer=" MACSTR " len=%u version=%u type=%s(%u) eapol_len=%u",
			    pStage, nSeq, MAC2STR (pPeer), (unsigned) nLength,
			    nVersion, BMXEAPOLTypeName (nType), nType, nEAPOLLen);
		return nSeq;
	}

	unsigned nDesc = pBuffer[4];
	u16 nKeyInfo = BMXEAPOLBE16 (pBuffer+5);
	unsigned nKeyLength = BMXEAPOLBE16 (pBuffer+7);
	unsigned nKeyDataLength = BMXEAPOLBE16 (pBuffer+97);

	BMX_L2_LOG ("%s seq=%u peer=" MACSTR " len=%u version=%u eapol_len=%u desc=%s(%u) msg=%s key_info=0x%04X pairwise=%u install=%u ack=%u mic=%u secure=%u encr=%u request=%u error=%u key_len=%u replay=%02X%02X%02X%02X%02X%02X%02X%02X key_data_len=%u",
		    pStage, nSeq, MAC2STR (pPeer), (unsigned) nLength,
		    nVersion, nEAPOLLen, BMXEAPOLKeyDescName (nDesc), nDesc,
		    BMXEAPOLKeyMsgName (nKeyInfo), nKeyInfo,
		    (nKeyInfo & 0x0008) != 0, (nKeyInfo & 0x0040) != 0,
		    (nKeyInfo & 0x0080) != 0, (nKeyInfo & 0x0100) != 0,
		    (nKeyInfo & 0x0200) != 0, (nKeyInfo & 0x1000) != 0,
		    (nKeyInfo & 0x0800) != 0, (nKeyInfo & 0x0400) != 0,
		    nKeyLength,
		    pBuffer[9], pBuffer[10], pBuffer[11], pBuffer[12],
		    pBuffer[13], pBuffer[14], pBuffer[15], pBuffer[16],
		    nKeyDataLength);

	return nSeq;
}
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_EAPOL_STAGE_L2_RX,
	BMX_EAPOL_STAGE_L2_TX,
	BMX_EAPOL_STAGE_BCM_TX_ENTER,
	BMX_EAPOL_STAGE_BCM_TX_QUEUED,
	BMX_EAPOL_STAGE_BCM_TX_CALL,
	BMX_EAPOL_STAGE_BCM_TX_RETURN,
	BMX_EAPOL_STAGE_BCM_TX_ERROR,
	BMX_EAPOL_STAGE_BCM_RX_DEQUEUE,
	BMX_EAPOL_STAGE_BCM_RX_ENQUEUE,
	BMX_EAPOL_STAGE_BCM_ETHERIQ,
	BMX_EAPOL_STAGE_WL_TX_QGET,
	BMX_EAPOL_STAGE_WL_TX_SDIO,
	BMX_EAPOL_STAGE_WL_TX_SDIO_DONE,
	BMX_EAPOL_STAGE_WL_TX_SDIO_ERROR,
	BMX_EAPOL_STAGE_WL_RX_SDIO,
	BMX_EAPOL_STAGE_WPA_RX3,
	BMX_EAPOL_STAGE_WPA_TX4,
	BMX_EAPOL_STAGE_WPA_KEY_COMPLETE,
	BMX_EAPOL_STAGE_LINK_RX,
	BMX_EAPOL_STAGE_LINK_DROP_MAC,
	BMX_EAPOL_STAGE_LINK_DROP_PROTO,
	BMX_EAPOL_STAGE_LINK_RAW_ENQUEUE,
	BMX_EAPOL_STAGE_LINK_RAW_DEQUEUE,
	BMX_EAPOL_STAGE_L2_RX_CALLBACK_ENTER,
	BMX_EAPOL_STAGE_L2_RX_CALLBACK_RETURN,
	BMX_EAPOL_STAGE_WPA_RX_EAPOL,
	BMX_EAPOL_STAGE_WPA_DISPATCH_M1,
	BMX_EAPOL_STAGE_WPA_DISPATCH_M3,
	BMX_EAPOL_STAGE_WPA_REJECT,
	BMX_EAPOL_STAGE_COUNT
};

enum
{
	BMX_EAPOL_MSG_UNKNOWN,
	BMX_EAPOL_MSG_M1,
	BMX_EAPOL_MSG_M2,
	BMX_EAPOL_MSG_M3,
	BMX_EAPOL_MSG_M4,
	BMX_EAPOL_MSG_GROUP1,
	BMX_EAPOL_MSG_GROUP2,
	BMX_EAPOL_MSG_PAIRWISE_OTHER,
	BMX_EAPOL_MSG_GROUP_OTHER,
	BMX_EAPOL_MSG_NONKEY,
	BMX_EAPOL_MSG_TRUNCATED,
	BMX_EAPOL_MSG_COUNT
};

enum
{
	BMX_EAPOL_WPA_EVENT_WPA_RX3 = 1,
	BMX_EAPOL_WPA_EVENT_WPA_TX4,
	BMX_EAPOL_WPA_EVENT_RSN_RX3,
	BMX_EAPOL_WPA_EVENT_RSN_TX4,
	BMX_EAPOL_WPA_EVENT_KEY_COMPLETE
};

static const unsigned BMX_EAPOL_LOW_EVENT_COUNT = 48;

struct BMXEAPOLLowEvent
{
	unsigned nTimeUS;
	unsigned nAuthGeneration;
	int nAuthRelMS;
	unsigned nStage;
	unsigned nMsg;
	unsigned nLength;
	u16 nKeyInfo;
	u16 nKeyDataLength;
	u8 Replay[8];
	int nTXSeq;
	int nTXWindow;
	unsigned nFCMask;
	unsigned nQueueLength;
};

struct BMXEAPOLLowSummary
{
	unsigned nAuthGeneration;
	unsigned nAuthStartUS;
	unsigned nAuthEndUS;
	unsigned nEvents;
	unsigned Counts[BMX_EAPOL_STAGE_COUNT][BMX_EAPOL_MSG_COUNT];
	unsigned nWPAKeyComplete;
	unsigned nWPACompleteSecure;
	unsigned nWPACompleteState;
	unsigned nWPACompleteReplaySet;
	unsigned nPairwiseKeyInstalls;
	unsigned nGroupKeyInstalls;
	unsigned nPairwiseKeyMSLast;
	unsigned nPairwiseKeyMSMin;
	unsigned nPairwiseKeyMSMax;
	unsigned nGroupKeyMSLast;
	unsigned nGroupKeyMSMin;
	unsigned nGroupKeyMSMax;
	unsigned nLastEventWrite;
	unsigned nLastEventCount;
	bool bDumped;
	BMXEAPOLLowEvent LastEvents[BMX_EAPOL_LOW_EVENT_COUNT];
};

static BMXEAPOLLowSummary s_BMXEAPOLLow;
static unsigned s_nBMXEAPOLLowAuthGeneration;

extern "C" void bmx_wlan_debug_dump (const char *pReason);
extern "C" void bmx_wlan_debug_auth_start (unsigned nAuthGeneration);

static u16 BMXEAPOLLowBE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static const char *BMXEAPOLLowStageName (unsigned nStage)
{
	switch (nStage)
	{
	case BMX_EAPOL_STAGE_L2_RX: return "l2-rx";
	case BMX_EAPOL_STAGE_L2_TX: return "l2-tx";
	case BMX_EAPOL_STAGE_BCM_TX_ENTER: return "bcm-tx-enter";
	case BMX_EAPOL_STAGE_BCM_TX_QUEUED: return "bcm-tx-queued";
	case BMX_EAPOL_STAGE_BCM_TX_CALL: return "bcm-tx-call";
	case BMX_EAPOL_STAGE_BCM_TX_RETURN: return "bcm-tx-return";
	case BMX_EAPOL_STAGE_BCM_TX_ERROR: return "bcm-tx-error";
	case BMX_EAPOL_STAGE_BCM_RX_DEQUEUE: return "bcm-rx-dequeue";
	case BMX_EAPOL_STAGE_BCM_RX_ENQUEUE: return "bcm-rx-enqueue";
	case BMX_EAPOL_STAGE_BCM_ETHERIQ: return "bcm-etheriq";
	case BMX_EAPOL_STAGE_WL_TX_QGET: return "wl-tx-qget";
	case BMX_EAPOL_STAGE_WL_TX_SDIO: return "wl-tx-sdio";
	case BMX_EAPOL_STAGE_WL_TX_SDIO_DONE: return "wl-tx-sdio-done";
	case BMX_EAPOL_STAGE_WL_TX_SDIO_ERROR: return "wl-tx-sdio-error";
	case BMX_EAPOL_STAGE_WL_RX_SDIO: return "wl-rx-sdio";
	case BMX_EAPOL_STAGE_WPA_RX3: return "wpa-rx3";
	case BMX_EAPOL_STAGE_WPA_TX4: return "wpa-tx4";
	case BMX_EAPOL_STAGE_WPA_KEY_COMPLETE: return "wpa-complete";
	case BMX_EAPOL_STAGE_LINK_RX: return "link-rx";
	case BMX_EAPOL_STAGE_LINK_DROP_MAC: return "link-drop-mac";
	case BMX_EAPOL_STAGE_LINK_DROP_PROTO: return "link-drop-proto";
	case BMX_EAPOL_STAGE_LINK_RAW_ENQUEUE: return "link-raw-enqueue";
	case BMX_EAPOL_STAGE_LINK_RAW_DEQUEUE: return "link-raw-dequeue";
	case BMX_EAPOL_STAGE_L2_RX_CALLBACK_ENTER: return "l2-rx-cb-enter";
	case BMX_EAPOL_STAGE_L2_RX_CALLBACK_RETURN: return "l2-rx-cb-return";
	case BMX_EAPOL_STAGE_WPA_RX_EAPOL: return "wpa-rx-eapol";
	case BMX_EAPOL_STAGE_WPA_DISPATCH_M1: return "wpa-dispatch-m1";
	case BMX_EAPOL_STAGE_WPA_DISPATCH_M3: return "wpa-dispatch-m3";
	case BMX_EAPOL_STAGE_WPA_REJECT: return "wpa-reject";
	default: return "unknown";
	}
}

static const char *BMXEAPOLLowMsgName (unsigned nMsg)
{
	switch (nMsg)
	{
	case BMX_EAPOL_MSG_M1: return "4way-1/4";
	case BMX_EAPOL_MSG_M2: return "4way-2/4";
	case BMX_EAPOL_MSG_M3: return "4way-3/4";
	case BMX_EAPOL_MSG_M4: return "4way-4/4";
	case BMX_EAPOL_MSG_GROUP1: return "group-1/2";
	case BMX_EAPOL_MSG_GROUP2: return "group-2/2";
	case BMX_EAPOL_MSG_PAIRWISE_OTHER: return "4way-other";
	case BMX_EAPOL_MSG_GROUP_OTHER: return "group-other";
	case BMX_EAPOL_MSG_NONKEY: return "nonkey";
	case BMX_EAPOL_MSG_TRUNCATED: return "truncated";
	default: return "unknown";
	}
}

static unsigned BMXEAPOLLowClassifyKeyInfo (u16 nKeyInfo)
{
	bool bPairwise = (nKeyInfo & 0x0008) != 0;
	bool bInstall = (nKeyInfo & 0x0040) != 0;
	bool bACK = (nKeyInfo & 0x0080) != 0;
	bool bMIC = (nKeyInfo & 0x0100) != 0;
	bool bSecure = (nKeyInfo & 0x0200) != 0;
	bool bEncrypted = (nKeyInfo & 0x1000) != 0;

	if (bPairwise)
	{
		if (bACK && !bMIC) return BMX_EAPOL_MSG_M1;
		if (!bACK && bMIC && !bSecure) return BMX_EAPOL_MSG_M2;
		if (bACK && bMIC && bInstall && bSecure && bEncrypted) return BMX_EAPOL_MSG_M3;
		if (!bACK && bMIC && bSecure) return BMX_EAPOL_MSG_M4;
		return BMX_EAPOL_MSG_PAIRWISE_OTHER;
	}

	if (bACK && bMIC && bSecure) return BMX_EAPOL_MSG_GROUP1;
	if (!bACK && bMIC && bSecure) return BMX_EAPOL_MSG_GROUP2;
	return BMX_EAPOL_MSG_GROUP_OTHER;
}

static void BMXEAPOLLowReset (void)
{
	memset (&s_BMXEAPOLLow, 0, sizeof s_BMXEAPOLLow);
}

static void BMXEAPOLLowRememberEvent (unsigned nStage, unsigned nMsg, unsigned nLength,
				      u16 nKeyInfo, const u8 *pReplay,
				      unsigned nKeyDataLength, int nTXSeq,
				      int nTXWindow, unsigned nFCMask,
				      unsigned nQueueLength)
{
	BMXEAPOLLowEvent *pEvent =
		&s_BMXEAPOLLow.LastEvents[s_BMXEAPOLLow.nLastEventWrite % BMX_EAPOL_LOW_EVENT_COUNT];
	pEvent->nTimeUS = CTimer::GetClockTicks ();
	pEvent->nAuthGeneration = s_BMXEAPOLLow.nAuthGeneration;
	pEvent->nAuthRelMS =    s_BMXEAPOLLow.nAuthStartUS != 0
			      && pEvent->nTimeUS >= s_BMXEAPOLLow.nAuthStartUS
			    ? (int) ((pEvent->nTimeUS - s_BMXEAPOLLow.nAuthStartUS) / 1000)
			    : -1;
	pEvent->nStage = nStage;
	pEvent->nMsg = nMsg;
	pEvent->nLength = nLength;
	pEvent->nKeyInfo = nKeyInfo;
	pEvent->nKeyDataLength = nKeyDataLength;
	if (pReplay != 0)
	{
		memcpy (pEvent->Replay, pReplay, sizeof pEvent->Replay);
	}
	else
	{
		memset (pEvent->Replay, 0, sizeof pEvent->Replay);
	}
	pEvent->nTXSeq = nTXSeq;
	pEvent->nTXWindow = nTXWindow;
	pEvent->nFCMask = nFCMask;
	pEvent->nQueueLength = nQueueLength;

	s_BMXEAPOLLow.nLastEventWrite++;
	if (s_BMXEAPOLLow.nLastEventCount < BMX_EAPOL_LOW_EVENT_COUNT)
	{
		s_BMXEAPOLLow.nLastEventCount++;
	}
}

static void BMXEAPOLLowRecordPayload (unsigned nStage, const u8 *pBuffer,
				      unsigned nLength, int nTXSeq,
				      int nTXWindow, unsigned nFCMask,
				      unsigned nQueueLength)
{
	unsigned nMsg = BMX_EAPOL_MSG_TRUNCATED;
	u16 nKeyInfo = 0;
	unsigned nKeyDataLength = 0;
	const u8 *pReplay = 0;

	if (nStage >= BMX_EAPOL_STAGE_COUNT)
	{
		return;
	}

	if (pBuffer != 0 && nLength >= 4)
	{
		unsigned nType = pBuffer[1];
		if (nType != 3)
		{
			nMsg = BMX_EAPOL_MSG_NONKEY;
		}
		else if (nLength >= 99)
		{
			nKeyInfo = BMXEAPOLLowBE16 (pBuffer+5);
			nKeyDataLength = BMXEAPOLLowBE16 (pBuffer+97);
			pReplay = pBuffer+9;
			nMsg = BMXEAPOLLowClassifyKeyInfo (nKeyInfo);
		}
	}

	s_BMXEAPOLLow.Counts[nStage][nMsg]++;
	s_BMXEAPOLLow.nEvents++;
	BMXEAPOLLowRememberEvent (nStage, nMsg, nLength, nKeyInfo, pReplay,
				  nKeyDataLength, nTXSeq, nTXWindow, nFCMask,
				  nQueueLength);
}

static void BMXEAPOLLowDump (const char *pReason)
{
#if !defined(BMC64_WLAN_LOW_IMPACT_TRACE_LOG)
	if (pReason == 0 || strcmp (pReason, "anomaly-second-key-complete") != 0)
	{
		return;
	}
#endif
	unsigned nDurationUS = 0;
	if (s_BMXEAPOLLow.nAuthEndUS >= s_BMXEAPOLLow.nAuthStartUS)
	{
		nDurationUS = s_BMXEAPOLLow.nAuthEndUS - s_BMXEAPOLLow.nAuthStartUS;
	}

	wpa_printf (MSG_INFO,
		    "bmx-eapol-low: summary reason=%s gen=%u events=%u auth_us=%u "
		    "l2_rx_m1=%u l2_rx_m3=%u wl_rx_m1=%u wl_rx_m3=%u "
		    "bcm_iq_m1=%u bcm_iq_m3=%u bcm_rxenq_m1=%u bcm_rxenq_m3=%u "
		    "bcm_rxdq_m1=%u bcm_rxdq_m3=%u "
		    "link_rx_m1=%u link_rx_m3=%u link_rawq_m1=%u link_rawq_m3=%u "
		    "link_rawdq_m1=%u link_rawdq_m3=%u l2_cb_in_m1=%u l2_cb_in_m3=%u "
		    "l2_tx_m2=%u l2_tx_m4=%u bcm_txret_m2=%u bcm_txret_m4=%u "
		    "wl_txdone_m2=%u wl_txdone_m4=%u "
		    "wpa_any_m1=%u wpa_any_m3=%u wpa_dispatch_m1=%u wpa_dispatch_m3=%u "
		    "wpa_reject_m1=%u wpa_reject_m3=%u wpa_rx3=%u wpa_tx4=%u "
		    "complete=%u secure=%u state=%u replay_set=%u "
		    "key_pair_count=%u key_pair_ms_last=%u min=%u max=%u "
		    "key_group_count=%u key_group_ms_last=%u min=%u max=%u "
		    "nonkey=%u truncated=%u",
		    pReason, s_BMXEAPOLLow.nAuthGeneration,
		    s_BMXEAPOLLow.nEvents, nDurationUS,
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_RX][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_RX][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WL_RX_SDIO][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WL_RX_SDIO][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_ETHERIQ][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_ETHERIQ][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_RX_ENQUEUE][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_RX_ENQUEUE][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_RX_DEQUEUE][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_RX_DEQUEUE][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_LINK_RX][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_LINK_RX][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_LINK_RAW_ENQUEUE][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_LINK_RAW_ENQUEUE][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_LINK_RAW_DEQUEUE][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_LINK_RAW_DEQUEUE][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_RX_CALLBACK_ENTER][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_RX_CALLBACK_ENTER][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_TX][BMX_EAPOL_MSG_M2],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_TX][BMX_EAPOL_MSG_M4],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_TX_RETURN][BMX_EAPOL_MSG_M2],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_BCM_TX_RETURN][BMX_EAPOL_MSG_M4],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WL_TX_SDIO_DONE][BMX_EAPOL_MSG_M2],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WL_TX_SDIO_DONE][BMX_EAPOL_MSG_M4],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_RX_EAPOL][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_RX_EAPOL][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_DISPATCH_M1][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_DISPATCH_M3][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_REJECT][BMX_EAPOL_MSG_M1],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_REJECT][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_RX3][BMX_EAPOL_MSG_M3],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_TX4][BMX_EAPOL_MSG_M4],
		    s_BMXEAPOLLow.nWPAKeyComplete,
		    s_BMXEAPOLLow.nWPACompleteSecure,
		    s_BMXEAPOLLow.nWPACompleteState,
		    s_BMXEAPOLLow.nWPACompleteReplaySet,
		    s_BMXEAPOLLow.nPairwiseKeyInstalls,
		    s_BMXEAPOLLow.nPairwiseKeyMSLast,
		    s_BMXEAPOLLow.nPairwiseKeyMSMin,
		    s_BMXEAPOLLow.nPairwiseKeyMSMax,
		    s_BMXEAPOLLow.nGroupKeyInstalls,
		    s_BMXEAPOLLow.nGroupKeyMSLast,
		    s_BMXEAPOLLow.nGroupKeyMSMin,
		    s_BMXEAPOLLow.nGroupKeyMSMax,
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_RX][BMX_EAPOL_MSG_NONKEY]
		      + s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_TX][BMX_EAPOL_MSG_NONKEY],
		    s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_RX][BMX_EAPOL_MSG_TRUNCATED]
		      + s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_L2_TX][BMX_EAPOL_MSG_TRUNCATED]);

	unsigned nToPrint = s_BMXEAPOLLow.nLastEventCount;
	if (nToPrint > 24)
	{
		nToPrint = 24;
	}
	unsigned nStart = s_BMXEAPOLLow.nLastEventWrite - nToPrint;
	for (unsigned i = 0; i < nToPrint; i++)
	{
		const BMXEAPOLLowEvent *pEvent =
			&s_BMXEAPOLLow.LastEvents[(nStart+i) % BMX_EAPOL_LOW_EVENT_COUNT];
		wpa_printf (MSG_INFO,
			    "bmx-eapol-low: event idx=%u t=%u gen=%u rel=%dms stage=%s msg=%s len=%u key=0x%04x replay=%02x%02x%02x%02x%02x%02x%02x%02x data=%u txseq=%d txwin=%d fcmask=0x%x qlen=%u",
			    i, pEvent->nTimeUS, pEvent->nAuthGeneration,
			    pEvent->nAuthRelMS,
			    BMXEAPOLLowStageName (pEvent->nStage),
			    BMXEAPOLLowMsgName (pEvent->nMsg),
			    pEvent->nLength, pEvent->nKeyInfo,
			    pEvent->Replay[0], pEvent->Replay[1],
			    pEvent->Replay[2], pEvent->Replay[3],
			    pEvent->Replay[4], pEvent->Replay[5],
			    pEvent->Replay[6], pEvent->Replay[7],
			    pEvent->nKeyDataLength,
			    pEvent->nTXSeq, pEvent->nTXWindow,
			    pEvent->nFCMask, pEvent->nQueueLength);
	}

	s_BMXEAPOLLow.bDumped = true;
}

static void BMXEAPOLLowAuthStart (void)
{
	if (s_BMXEAPOLLow.bDumped)
	{
		BMXEAPOLLowReset ();
	}
	s_nBMXEAPOLLowAuthGeneration++;
	if (s_nBMXEAPOLLowAuthGeneration == 0)
	{
		s_nBMXEAPOLLowAuthGeneration++;
	}
	s_BMXEAPOLLow.nAuthGeneration = s_nBMXEAPOLLowAuthGeneration;
	s_BMXEAPOLLow.nAuthStartUS = CTimer::GetClockTicks ();
	bmx_wlan_debug_auth_start (s_BMXEAPOLLow.nAuthGeneration);
}

static void BMXEAPOLLowAuthEnd (void)
{
	s_BMXEAPOLLow.nAuthEndUS = CTimer::GetClockTicks ();
}

extern "C" void bmx_eapol_low_frame (unsigned nStage, const unsigned char *pFrame,
				     unsigned nLength, int nTXSeq, int nTXWindow,
				     unsigned nFCMask, unsigned nQueueLength)
{
	if (pFrame == 0 || nLength < 18)
	{
		return;
	}
	if (BMXEAPOLLowBE16 (pFrame+12) != 0x888E)
	{
		return;
	}

	BMXEAPOLLowRecordPayload (nStage, pFrame+14, nLength-14, nTXSeq,
				  nTXWindow, nFCMask, nQueueLength);
}

extern "C" void bmx_eapol_low_payload (unsigned nStage, const unsigned char *pPayload,
				       unsigned nLength)
{
	BMXEAPOLLowRecordPayload (nStage, pPayload, nLength, -1, -1, 0, 0);
}

extern "C" void bmx_eapol_low_wpa_frame (unsigned nStage, int nState,
					 unsigned nKeyInfo,
					 const unsigned char *pReplay,
					 unsigned nKeyDataLength, int nReplaySet)
{
	unsigned nMsg = BMXEAPOLLowClassifyKeyInfo ((u16) nKeyInfo);

	(void) nState;
	(void) nReplaySet;

	if (nStage >= BMX_EAPOL_STAGE_COUNT)
	{
		return;
	}

	s_BMXEAPOLLow.Counts[nStage][nMsg]++;
	s_BMXEAPOLLow.nEvents++;
	BMXEAPOLLowRememberEvent (nStage, nMsg, 0, (u16) nKeyInfo, pReplay,
				  nKeyDataLength, -1, -1, 0, 0);
}

extern "C" void bmx_eapol_low_wpa_reject (const char *pReason, int nState,
					  unsigned nKeyInfo,
					  const unsigned char *pReplay,
					  unsigned nKeyDataLength,
					  int nReplaySet)
{
	unsigned nMsg = BMXEAPOLLowClassifyKeyInfo ((u16) nKeyInfo);

	s_BMXEAPOLLow.Counts[BMX_EAPOL_STAGE_WPA_REJECT][nMsg]++;
	s_BMXEAPOLLow.nEvents++;
	BMXEAPOLLowRememberEvent (BMX_EAPOL_STAGE_WPA_REJECT, nMsg, 0,
				  (u16) nKeyInfo, pReplay, nKeyDataLength,
				  -1, -1, 0, 0);
	wpa_printf (MSG_INFO,
		    "bmx-eapol-low: wpa-reject reason=%s state=%d msg=%s key=0x%04x replay=%02x%02x%02x%02x%02x%02x%02x%02x data=%u replay_set=%d",
		    pReason != 0 ? pReason : "unknown", nState,
		    BMXEAPOLLowMsgName (nMsg), nKeyInfo,
		    pReplay != 0 ? pReplay[0] : 0,
		    pReplay != 0 ? pReplay[1] : 0,
		    pReplay != 0 ? pReplay[2] : 0,
		    pReplay != 0 ? pReplay[3] : 0,
		    pReplay != 0 ? pReplay[4] : 0,
		    pReplay != 0 ? pReplay[5] : 0,
		    pReplay != 0 ? pReplay[6] : 0,
		    pReplay != 0 ? pReplay[7] : 0,
		    nKeyDataLength, nReplaySet);
}

extern "C" void bmx_eapol_low_wpa_event (unsigned nEvent, int nState,
					 unsigned nKeyInfo,
					 const unsigned char *pReplay,
					 unsigned nKeyDataLength, int nSecure,
					 int nReplaySet)
{
	unsigned nStage = BMX_EAPOL_STAGE_WPA_KEY_COMPLETE;
	unsigned nMsg = BMX_EAPOL_MSG_UNKNOWN;

	switch (nEvent)
	{
	case BMX_EAPOL_WPA_EVENT_WPA_RX3:
	case BMX_EAPOL_WPA_EVENT_RSN_RX3:
		nStage = BMX_EAPOL_STAGE_WPA_RX3;
		nMsg = BMX_EAPOL_MSG_M3;
		break;
	case BMX_EAPOL_WPA_EVENT_WPA_TX4:
	case BMX_EAPOL_WPA_EVENT_RSN_TX4:
		nStage = BMX_EAPOL_STAGE_WPA_TX4;
		nMsg = BMX_EAPOL_MSG_M4;
		break;
	case BMX_EAPOL_WPA_EVENT_KEY_COMPLETE:
		nStage = BMX_EAPOL_STAGE_WPA_KEY_COMPLETE;
		nMsg = BMX_EAPOL_MSG_M4;
		s_BMXEAPOLLow.nWPAKeyComplete++;
		s_BMXEAPOLLow.nWPACompleteSecure = nSecure;
		s_BMXEAPOLLow.nWPACompleteState = nState;
		s_BMXEAPOLLow.nWPACompleteReplaySet = nReplaySet;
		break;
	default:
		return;
	}

	s_BMXEAPOLLow.Counts[nStage][nMsg]++;
	BMXEAPOLLowRememberEvent (nStage, nMsg, 0, (u16) nKeyInfo, pReplay,
				  nKeyDataLength, -1, -1, 0, 0);
	if (   nEvent == BMX_EAPOL_WPA_EVENT_KEY_COMPLETE
	    && s_BMXEAPOLLow.nWPAKeyComplete > 1
	    && !s_BMXEAPOLLow.bDumped)
	{
		s_BMXEAPOLLow.nAuthEndUS = CTimer::GetClockTicks ();
		BMXEAPOLLowDump ("anomaly-second-key-complete");
		bmx_wlan_debug_dump ("anomaly-second-key-complete");
	}
}

extern "C" void bmx_eapol_low_key_install (unsigned nKeyID, int bPairwise, unsigned nMS)
{
	(void) nKeyID;

	if (bPairwise)
	{
		s_BMXEAPOLLow.nPairwiseKeyInstalls++;
		s_BMXEAPOLLow.nPairwiseKeyMSLast = nMS;
		if (   s_BMXEAPOLLow.nPairwiseKeyMSMin == 0
		    || nMS < s_BMXEAPOLLow.nPairwiseKeyMSMin)
		{
			s_BMXEAPOLLow.nPairwiseKeyMSMin = nMS;
		}
		if (nMS > s_BMXEAPOLLow.nPairwiseKeyMSMax)
		{
			s_BMXEAPOLLow.nPairwiseKeyMSMax = nMS;
		}
	}
	else
	{
		s_BMXEAPOLLow.nGroupKeyInstalls++;
		s_BMXEAPOLLow.nGroupKeyMSLast = nMS;
		if (s_BMXEAPOLLow.nGroupKeyMSMin == 0 || nMS < s_BMXEAPOLLow.nGroupKeyMSMin)
		{
			s_BMXEAPOLLow.nGroupKeyMSMin = nMS;
		}
		if (nMS > s_BMXEAPOLLow.nGroupKeyMSMax)
		{
			s_BMXEAPOLLow.nGroupKeyMSMax = nMS;
		}
	}
}
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_DHCP_STAGE_CLIENT_TX,
	BMX_DHCP_STAGE_CLIENT_RX_RAW,
	BMX_DHCP_STAGE_CLIENT_RX_VALID,
	BMX_DHCP_STAGE_CLIENT_RX_DROP,
	BMX_DHCP_STAGE_CLIENT_TIMEOUT,
	BMX_DHCP_STAGE_NETDEV_TX,
	BMX_DHCP_STAGE_NETDEV_TX_FAILED,
	BMX_DHCP_STAGE_NETDEV_TX_DONE,
	BMX_DHCP_STAGE_NETDEV_RX,
	BMX_DHCP_STAGE_LINK_RX,
	BMX_DHCP_STAGE_LINK_DROP_MAC,
	BMX_DHCP_STAGE_LINK_IP,
	BMX_DHCP_STAGE_LINK_DROP_PROTO,
	BMX_DHCP_STAGE_IP_RX,
	BMX_DHCP_STAGE_IP_DROP_SHORT,
	BMX_DHCP_STAGE_IP_DROP_IHL,
	BMX_DHCP_STAGE_IP_DROP_CHECKSUM,
	BMX_DHCP_STAGE_IP_DROP_NOOWN,
	BMX_DHCP_STAGE_IP_DROP_FRAGMENT,
	BMX_DHCP_STAGE_IP_DROP_TRUNC,
	BMX_DHCP_STAGE_IP_ENQUEUE,
	BMX_DHCP_STAGE_UDP_ENTER,
	BMX_DHCP_STAGE_UDP_DROP_OWNPORT,
	BMX_DHCP_STAGE_UDP_DROP_FOREIGNPORT,
	BMX_DHCP_STAGE_UDP_DROP_FOREIGNIP,
	BMX_DHCP_STAGE_UDP_DROP_LENGTH,
	BMX_DHCP_STAGE_UDP_DROP_CHECKSUM,
	BMX_DHCP_STAGE_UDP_DROP_BCAST,
	BMX_DHCP_STAGE_UDP_ENQUEUE,
	BMX_DHCP_STAGE_UDP_DEQUEUE,
	BMX_DHCP_STAGE_BCM_TX_ENTER,
	BMX_DHCP_STAGE_BCM_TX_QUEUED,
	BMX_DHCP_STAGE_BCM_TX_CALL,
	BMX_DHCP_STAGE_BCM_TX_RETURN,
	BMX_DHCP_STAGE_BCM_TX_ERROR,
	BMX_DHCP_STAGE_BCM_RX_DEQUEUE,
	BMX_DHCP_STAGE_BCM_RX_ENQUEUE,
	BMX_DHCP_STAGE_BCM_ETHERIQ,
	BMX_DHCP_STAGE_WL_TX_QGET,
	BMX_DHCP_STAGE_WL_TX_SDIO,
	BMX_DHCP_STAGE_WL_TX_SDIO_DONE,
	BMX_DHCP_STAGE_WL_TX_SDIO_ERROR,
	BMX_DHCP_STAGE_WL_RX_SDIO,
	BMX_DHCP_STAGE_COUNT
};

enum
{
	BMX_DHCP_MSG_UNKNOWN,
	BMX_DHCP_MSG_DISCOVER,
	BMX_DHCP_MSG_OFFER,
	BMX_DHCP_MSG_REQUEST,
	BMX_DHCP_MSG_ACK,
	BMX_DHCP_MSG_NAK,
	BMX_DHCP_MSG_OTHER,
	BMX_DHCP_MSG_TRUNCATED,
	BMX_DHCP_MSG_COUNT
};

enum
{
	BMX_DHCP_REASON_NONE,
	BMX_DHCP_REASON_SHORT,
	BMX_DHCP_REASON_HEADER,
	BMX_DHCP_REASON_XID,
	BMX_DHCP_REASON_CHADDR,
	BMX_DHCP_REASON_CONFIG,
	BMX_DHCP_REASON_TIMEOUT,
	BMX_DHCP_REASON_COUNT
};

static const unsigned BMX_DHCP_LOW_EVENT_COUNT = 64;

struct BMXDHCPLowEvent
{
	unsigned nTimeUS;
	unsigned nStage;
	unsigned nMsg;
	unsigned nLength;
	unsigned nXID;
	unsigned nReason;
	u16 nSourcePort;
	u16 nDestPort;
	u8 SourceIP[4];
	u8 DestIP[4];
	int nTXSeq;
	int nTXWindow;
	unsigned nFCMask;
	unsigned nQueueLength;
	int nResult;
};

struct BMXDHCPLowSummary
{
	unsigned nStartUS;
	unsigned nEndUS;
	unsigned nEvents;
	unsigned Counts[BMX_DHCP_STAGE_COUNT][BMX_DHCP_MSG_COUNT];
	unsigned Reasons[BMX_DHCP_REASON_COUNT];
	unsigned nLastXID;
	unsigned nLastClientXID;
	unsigned nLastOfferXID;
	unsigned nLastAckXID;
	unsigned nLastEventWrite;
	unsigned nLastEventCount;
	bool bDumped;
	BMXDHCPLowEvent LastEvents[BMX_DHCP_LOW_EVENT_COUNT];
};

static BMXDHCPLowSummary s_BMXDHCPLow;

static u16 BMXDHCPLowBE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static u32 BMXDHCPLowBE32 (const u8 *p)
{
	return   (u32) p[0] << 24
	       | (u32) p[1] << 16
	       | (u32) p[2] << 8
	       | (u32) p[3];
}

static const char *BMXDHCPLowStageName (unsigned nStage)
{
	switch (nStage)
	{
	case BMX_DHCP_STAGE_CLIENT_TX: return "client-tx";
	case BMX_DHCP_STAGE_CLIENT_RX_RAW: return "client-rx-raw";
	case BMX_DHCP_STAGE_CLIENT_RX_VALID: return "client-rx-valid";
	case BMX_DHCP_STAGE_CLIENT_RX_DROP: return "client-rx-drop";
	case BMX_DHCP_STAGE_CLIENT_TIMEOUT: return "client-timeout";
	case BMX_DHCP_STAGE_NETDEV_TX: return "netdev-tx";
	case BMX_DHCP_STAGE_NETDEV_TX_FAILED: return "netdev-tx-failed";
	case BMX_DHCP_STAGE_NETDEV_TX_DONE: return "netdev-tx-done";
	case BMX_DHCP_STAGE_NETDEV_RX: return "netdev-rx";
	case BMX_DHCP_STAGE_LINK_RX: return "link-rx";
	case BMX_DHCP_STAGE_LINK_DROP_MAC: return "link-drop-mac";
	case BMX_DHCP_STAGE_LINK_IP: return "link-ip";
	case BMX_DHCP_STAGE_LINK_DROP_PROTO: return "link-drop-proto";
	case BMX_DHCP_STAGE_IP_RX: return "ip-rx";
	case BMX_DHCP_STAGE_IP_DROP_SHORT: return "ip-drop-short";
	case BMX_DHCP_STAGE_IP_DROP_IHL: return "ip-drop-ihl";
	case BMX_DHCP_STAGE_IP_DROP_CHECKSUM: return "ip-drop-checksum";
	case BMX_DHCP_STAGE_IP_DROP_NOOWN: return "ip-drop-noown";
	case BMX_DHCP_STAGE_IP_DROP_FRAGMENT: return "ip-drop-fragment";
	case BMX_DHCP_STAGE_IP_DROP_TRUNC: return "ip-drop-trunc";
	case BMX_DHCP_STAGE_IP_ENQUEUE: return "ip-enqueue";
	case BMX_DHCP_STAGE_UDP_ENTER: return "udp-enter";
	case BMX_DHCP_STAGE_UDP_DROP_OWNPORT: return "udp-drop-ownport";
	case BMX_DHCP_STAGE_UDP_DROP_FOREIGNPORT: return "udp-drop-foreignport";
	case BMX_DHCP_STAGE_UDP_DROP_FOREIGNIP: return "udp-drop-foreignip";
	case BMX_DHCP_STAGE_UDP_DROP_LENGTH: return "udp-drop-length";
	case BMX_DHCP_STAGE_UDP_DROP_CHECKSUM: return "udp-drop-checksum";
	case BMX_DHCP_STAGE_UDP_DROP_BCAST: return "udp-drop-bcast";
	case BMX_DHCP_STAGE_UDP_ENQUEUE: return "udp-enqueue";
	case BMX_DHCP_STAGE_UDP_DEQUEUE: return "udp-dequeue";
	case BMX_DHCP_STAGE_BCM_TX_ENTER: return "bcm-tx-enter";
	case BMX_DHCP_STAGE_BCM_TX_QUEUED: return "bcm-tx-queued";
	case BMX_DHCP_STAGE_BCM_TX_CALL: return "bcm-tx-call";
	case BMX_DHCP_STAGE_BCM_TX_RETURN: return "bcm-tx-return";
	case BMX_DHCP_STAGE_BCM_TX_ERROR: return "bcm-tx-error";
	case BMX_DHCP_STAGE_BCM_RX_DEQUEUE: return "bcm-rx-dequeue";
	case BMX_DHCP_STAGE_BCM_RX_ENQUEUE: return "bcm-rx-enqueue";
	case BMX_DHCP_STAGE_BCM_ETHERIQ: return "bcm-etheriq";
	case BMX_DHCP_STAGE_WL_TX_QGET: return "wl-tx-qget";
	case BMX_DHCP_STAGE_WL_TX_SDIO: return "wl-tx-sdio";
	case BMX_DHCP_STAGE_WL_TX_SDIO_DONE: return "wl-tx-sdio-done";
	case BMX_DHCP_STAGE_WL_TX_SDIO_ERROR: return "wl-tx-sdio-error";
	case BMX_DHCP_STAGE_WL_RX_SDIO: return "wl-rx-sdio";
	default: return "unknown";
	}
}

static const char *BMXDHCPLowMsgName (unsigned nMsg)
{
	switch (nMsg)
	{
	case BMX_DHCP_MSG_DISCOVER: return "discover";
	case BMX_DHCP_MSG_OFFER: return "offer";
	case BMX_DHCP_MSG_REQUEST: return "request";
	case BMX_DHCP_MSG_ACK: return "ack";
	case BMX_DHCP_MSG_NAK: return "nak";
	case BMX_DHCP_MSG_OTHER: return "other";
	case BMX_DHCP_MSG_TRUNCATED: return "truncated";
	default: return "unknown";
	}
}

static const char *BMXDHCPLowReasonName (unsigned nReason)
{
	switch (nReason)
	{
	case BMX_DHCP_REASON_SHORT: return "short";
	case BMX_DHCP_REASON_HEADER: return "header";
	case BMX_DHCP_REASON_XID: return "xid";
	case BMX_DHCP_REASON_CHADDR: return "chaddr";
	case BMX_DHCP_REASON_CONFIG: return "config";
	case BMX_DHCP_REASON_TIMEOUT: return "timeout";
	default: return "none";
	}
}

static unsigned BMXDHCPLowClassifyType (unsigned nType)
{
	switch (nType)
	{
	case 1: return BMX_DHCP_MSG_DISCOVER;
	case 2: return BMX_DHCP_MSG_OFFER;
	case 3: return BMX_DHCP_MSG_REQUEST;
	case 5: return BMX_DHCP_MSG_ACK;
	case 6: return BMX_DHCP_MSG_NAK;
	case 0: return BMX_DHCP_MSG_UNKNOWN;
	default: return BMX_DHCP_MSG_OTHER;
	}
}

static unsigned BMXDHCPLowPayloadType (const u8 *pPayload, unsigned nLength)
{
	if (pPayload == 0 || nLength < 240)
	{
		return BMX_DHCP_MSG_TRUNCATED;
	}
	if (   pPayload[236] != 99
	    || pPayload[237] != 130
	    || pPayload[238] != 83
	    || pPayload[239] != 99)
	{
		return BMX_DHCP_MSG_UNKNOWN;
	}

	for (unsigned i = 240; i < nLength;)
	{
		u8 uchOption = pPayload[i];
		if (uchOption == 255)
		{
			break;
		}
		if (uchOption == 0)
		{
			i++;
			continue;
		}
		if (i+2 > nLength)
		{
			return BMX_DHCP_MSG_TRUNCATED;
		}

		unsigned nOptionLength = pPayload[i+1];
		if (i+2+nOptionLength > nLength)
		{
			return BMX_DHCP_MSG_TRUNCATED;
		}
		if (uchOption == 53 && nOptionLength >= 1)
		{
			return BMXDHCPLowClassifyType (pPayload[i+2]);
		}

		i += 2+nOptionLength;
	}

	return BMX_DHCP_MSG_UNKNOWN;
}

static void BMXDHCPLowReset (void)
{
	memset (&s_BMXDHCPLow, 0, sizeof s_BMXDHCPLow);
}

static void BMXDHCPLowRemember (unsigned nStage, unsigned nMsg, unsigned nLength,
				unsigned nXID, const u8 *pSourceIP, const u8 *pDestIP,
				u16 nSourcePort, u16 nDestPort, unsigned nReason,
				int nTXSeq, int nTXWindow, unsigned nFCMask,
				unsigned nQueueLength, int nResult)
{
	if (nStage >= BMX_DHCP_STAGE_COUNT)
	{
		return;
	}
	if (nMsg >= BMX_DHCP_MSG_COUNT)
	{
		nMsg = BMX_DHCP_MSG_UNKNOWN;
	}
	if (nReason >= BMX_DHCP_REASON_COUNT)
	{
		nReason = BMX_DHCP_REASON_NONE;
	}

	if (   s_BMXDHCPLow.bDumped
	    && nStage == BMX_DHCP_STAGE_CLIENT_TX
	    && nMsg == BMX_DHCP_MSG_DISCOVER)
	{
		BMXDHCPLowReset ();
	}
	if (s_BMXDHCPLow.nStartUS == 0)
	{
		s_BMXDHCPLow.nStartUS = CTimer::GetClockTicks ();
	}

	s_BMXDHCPLow.Counts[nStage][nMsg]++;
	if (nReason != BMX_DHCP_REASON_NONE)
	{
		s_BMXDHCPLow.Reasons[nReason]++;
	}
	s_BMXDHCPLow.nEvents++;
	s_BMXDHCPLow.nLastXID = nXID;
	if (nStage == BMX_DHCP_STAGE_CLIENT_TX)
	{
		s_BMXDHCPLow.nLastClientXID = nXID;
	}
	if (nMsg == BMX_DHCP_MSG_OFFER)
	{
		s_BMXDHCPLow.nLastOfferXID = nXID;
	}
	else if (nMsg == BMX_DHCP_MSG_ACK)
	{
		s_BMXDHCPLow.nLastAckXID = nXID;
	}

	BMXDHCPLowEvent *pEvent =
		&s_BMXDHCPLow.LastEvents[s_BMXDHCPLow.nLastEventWrite % BMX_DHCP_LOW_EVENT_COUNT];
	memset (pEvent, 0, sizeof *pEvent);
	pEvent->nTimeUS = CTimer::GetClockTicks ();
	pEvent->nStage = nStage;
	pEvent->nMsg = nMsg;
	pEvent->nLength = nLength;
	pEvent->nXID = nXID;
	pEvent->nReason = nReason;
	pEvent->nSourcePort = nSourcePort;
	pEvent->nDestPort = nDestPort;
	if (pSourceIP != 0)
	{
		memcpy (pEvent->SourceIP, pSourceIP, sizeof pEvent->SourceIP);
	}
	if (pDestIP != 0)
	{
		memcpy (pEvent->DestIP, pDestIP, sizeof pEvent->DestIP);
	}
	pEvent->nTXSeq = nTXSeq;
	pEvent->nTXWindow = nTXWindow;
	pEvent->nFCMask = nFCMask;
	pEvent->nQueueLength = nQueueLength;
	pEvent->nResult = nResult;

	s_BMXDHCPLow.nLastEventWrite++;
	if (s_BMXDHCPLow.nLastEventCount < BMX_DHCP_LOW_EVENT_COUNT)
	{
		s_BMXDHCPLow.nLastEventCount++;
	}
}

static void BMXDHCPLowRecordPayload (unsigned nStage, const u8 *pPayload,
				     unsigned nLength, const u8 *pSourceIP,
				     const u8 *pDestIP, u16 nSourcePort,
				     u16 nDestPort, unsigned nReason,
				     int nTXSeq, int nTXWindow, unsigned nFCMask,
				     unsigned nQueueLength, int nResult)
{
	if (pPayload == 0)
	{
		return;
	}

	unsigned nMsg = BMXDHCPLowPayloadType (pPayload, nLength);
	if (nMsg == BMX_DHCP_MSG_UNKNOWN && nReason == BMX_DHCP_REASON_NONE)
	{
		return;
	}
	unsigned nXID = nLength >= 8 ? BMXDHCPLowBE32 (pPayload+4) : 0;
	BMXDHCPLowRemember (nStage, nMsg, nLength, nXID, pSourceIP, pDestIP,
			    nSourcePort, nDestPort, nReason, nTXSeq, nTXWindow,
			    nFCMask, nQueueLength, nResult);
}

static void BMXDHCPLowRecordIP (unsigned nStage, const u8 *pIP, unsigned nLength,
				unsigned nReason, int nTXSeq, int nTXWindow,
				unsigned nFCMask, unsigned nQueueLength, int nResult)
{
	if (pIP == 0 || nLength < 20)
	{
		if (nReason != BMX_DHCP_REASON_NONE)
		{
			BMXDHCPLowRemember (nStage, BMX_DHCP_MSG_TRUNCATED, nLength, 0,
					    0, 0, 0, 0, nReason, nTXSeq, nTXWindow,
					    nFCMask, nQueueLength, nResult);
		}
		return;
	}
	unsigned nIPHeaderLength = (pIP[0] & 0x0F) * 4;
	if (   (pIP[0] >> 4) != 4
	    || nIPHeaderLength < 20
	    || nLength < nIPHeaderLength+8
	    || pIP[9] != 17)
	{
		return;
	}

	const u8 *pUDP = pIP + nIPHeaderLength;
	u16 nSourcePort = BMXDHCPLowBE16 (pUDP);
	u16 nDestPort = BMXDHCPLowBE16 (pUDP+2);
	if (   nSourcePort != 67
	    && nSourcePort != 68
	    && nDestPort != 67
	    && nDestPort != 68)
	{
		return;
	}

	unsigned nUDPLength = BMXDHCPLowBE16 (pUDP+4);
	if (nUDPLength < 8 || nIPHeaderLength+nUDPLength > nLength)
	{
		BMXDHCPLowRemember (nStage, BMX_DHCP_MSG_TRUNCATED, nLength, 0,
				    pIP+12, pIP+16, nSourcePort, nDestPort,
				    BMX_DHCP_REASON_SHORT, nTXSeq, nTXWindow,
				    nFCMask, nQueueLength, nResult);
		return;
	}

	BMXDHCPLowRecordPayload (nStage, pUDP+8, nUDPLength-8, pIP+12, pIP+16,
				 nSourcePort, nDestPort, nReason, nTXSeq,
				 nTXWindow, nFCMask, nQueueLength, nResult);
}

extern "C" void bmx_dhcp_low_frame (unsigned nStage, const unsigned char *pFrame,
				    unsigned nLength, int nTXSeq, int nTXWindow,
				    unsigned nFCMask, unsigned nQueueLength)
{
	if (pFrame == 0 || nLength < 14)
	{
		return;
	}
	if (BMXDHCPLowBE16 (pFrame+12) != 0x0800)
	{
		return;
	}

	BMXDHCPLowRecordIP (nStage, pFrame+14, nLength-14, BMX_DHCP_REASON_NONE,
			    nTXSeq, nTXWindow, nFCMask, nQueueLength, 0);
}

extern "C" void bmx_dhcp_low_ip (unsigned nStage, const unsigned char *pIP,
				 unsigned nLength, unsigned nReason)
{
	BMXDHCPLowRecordIP (nStage, pIP, nLength, nReason, -1, -1, 0, 0, 0);
}

extern "C" void bmx_dhcp_low_udp (unsigned nStage, const unsigned char *pUDP,
				  unsigned nLength, const unsigned char *pSourceIP,
				  const unsigned char *pDestIP, unsigned nReason)
{
	if (pUDP == 0 || nLength < 8)
	{
		return;
	}

	u16 nSourcePort = BMXDHCPLowBE16 (pUDP);
	u16 nDestPort = BMXDHCPLowBE16 (pUDP+2);
	if (   nSourcePort != 67
	    && nSourcePort != 68
	    && nDestPort != 67
	    && nDestPort != 68)
	{
		return;
	}

	unsigned nUDPLength = BMXDHCPLowBE16 (pUDP+4);
	if (nUDPLength < 8 || nUDPLength > nLength)
	{
		BMXDHCPLowRemember (nStage, BMX_DHCP_MSG_TRUNCATED, nLength, 0,
				    pSourceIP, pDestIP, nSourcePort, nDestPort,
				    BMX_DHCP_REASON_SHORT, -1, -1, 0, 0, 0);
		return;
	}

	BMXDHCPLowRecordPayload (nStage, pUDP+8, nUDPLength-8, pSourceIP,
				 pDestIP, nSourcePort, nDestPort, nReason,
				 -1, -1, 0, 0, 0);
}

extern "C" void bmx_dhcp_low_payload (unsigned nStage, const unsigned char *pPayload,
				      unsigned nLength, const unsigned char *pSourceIP,
				      unsigned nSourcePort, unsigned nDestPort,
				      unsigned nReason)
{
	BMXDHCPLowRecordPayload (nStage, pPayload, nLength, pSourceIP, 0,
				 (u16) nSourcePort, (u16) nDestPort, nReason,
				 -1, -1, 0, 0, 0);
}

extern "C" void bmx_dhcp_low_client_packet (unsigned nStage,
					    const unsigned char *pPayload,
					    unsigned nLength, unsigned nReason,
					    int nResult)
{
	BMXDHCPLowRecordPayload (nStage, pPayload, nLength, 0, 0, 0, 0,
				 nReason, -1, -1, 0, 0, nResult);
}

extern "C" void bmx_dhcp_low_timeout (int bRequest, unsigned nTry, unsigned nXID)
{
	(void) bRequest;
	(void) nTry;
	BMXDHCPLowRemember (BMX_DHCP_STAGE_CLIENT_TIMEOUT, BMX_DHCP_MSG_UNKNOWN,
			    0, nXID, 0, 0, 0, 0, BMX_DHCP_REASON_TIMEOUT,
			    -1, -1, 0, 0, 0);
}

extern "C" void bmx_dhcp_low_dump (const char *pReason)
{
	if (pReason == 0 || s_BMXDHCPLow.nEvents == 0)
	{
		return;
	}
#if !defined(BMC64_WLAN_LOW_IMPACT_TRACE_LOG)
	if (strcmp (pReason, "bound") == 0)
	{
		s_BMXDHCPLow.bDumped = true;
		return;
	}
#endif

	s_BMXDHCPLow.nEndUS = CTimer::GetClockTicks ();
	unsigned nDurationUS = 0;
	if (s_BMXDHCPLow.nEndUS >= s_BMXDHCPLow.nStartUS)
	{
		nDurationUS = s_BMXDHCPLow.nEndUS - s_BMXDHCPLow.nStartUS;
	}

	wpa_printf (MSG_INFO,
		    "bmx-dhcp-low: summary reason=%s events=%u us=%u xid_last=%08x xid_client=%08x xid_offer=%08x xid_ack=%08x "
		    "tx_disc=%u tx_req=%u wl_rx_offer=%u bcm_rx_offer=%u netdev_rx_offer=%u link_ip_offer=%u "
		    "ip_offer=%u ip_drop_noown_offer=%u udp_enter_offer=%u udp_enqueue_offer=%u udp_dequeue_offer=%u "
		    "client_rx_offer=%u client_valid_offer=%u client_rx_ack=%u client_valid_ack=%u "
		    "drop_short=%u drop_header=%u drop_xid=%u drop_chaddr=%u drop_config=%u timeout=%u",
		    pReason, s_BMXDHCPLow.nEvents, nDurationUS,
		    s_BMXDHCPLow.nLastXID, s_BMXDHCPLow.nLastClientXID,
		    s_BMXDHCPLow.nLastOfferXID, s_BMXDHCPLow.nLastAckXID,
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_CLIENT_TX][BMX_DHCP_MSG_DISCOVER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_CLIENT_TX][BMX_DHCP_MSG_REQUEST],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_WL_RX_SDIO][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_BCM_RX_ENQUEUE][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_NETDEV_RX][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_LINK_IP][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_IP_ENQUEUE][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_IP_DROP_NOOWN][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_UDP_ENTER][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_UDP_ENQUEUE][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_UDP_DEQUEUE][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_CLIENT_RX_RAW][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_CLIENT_RX_VALID][BMX_DHCP_MSG_OFFER],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_CLIENT_RX_RAW][BMX_DHCP_MSG_ACK],
		    s_BMXDHCPLow.Counts[BMX_DHCP_STAGE_CLIENT_RX_VALID][BMX_DHCP_MSG_ACK],
		    s_BMXDHCPLow.Reasons[BMX_DHCP_REASON_SHORT],
		    s_BMXDHCPLow.Reasons[BMX_DHCP_REASON_HEADER],
		    s_BMXDHCPLow.Reasons[BMX_DHCP_REASON_XID],
		    s_BMXDHCPLow.Reasons[BMX_DHCP_REASON_CHADDR],
		    s_BMXDHCPLow.Reasons[BMX_DHCP_REASON_CONFIG],
		    s_BMXDHCPLow.Reasons[BMX_DHCP_REASON_TIMEOUT]);

	unsigned nToPrint = s_BMXDHCPLow.nLastEventCount;
	if (nToPrint > 16)
	{
		nToPrint = 16;
	}
	unsigned nStart = s_BMXDHCPLow.nLastEventWrite - nToPrint;
	for (unsigned i = 0; i < nToPrint; i++)
	{
		const BMXDHCPLowEvent *pEvent =
			&s_BMXDHCPLow.LastEvents[(nStart+i) % BMX_DHCP_LOW_EVENT_COUNT];
		wpa_printf (MSG_INFO,
			    "bmx-dhcp-low: event idx=%u t=%u stage=%s msg=%s len=%u xid=%08x ip=%u.%u.%u.%u>%u.%u.%u.%u udp=%u>%u reason=%s rc=%d txseq=%d txwin=%d fcmask=0x%x qlen=%u",
			    i, pEvent->nTimeUS,
			    BMXDHCPLowStageName (pEvent->nStage),
			    BMXDHCPLowMsgName (pEvent->nMsg),
			    pEvent->nLength, pEvent->nXID,
			    pEvent->SourceIP[0], pEvent->SourceIP[1],
			    pEvent->SourceIP[2], pEvent->SourceIP[3],
			    pEvent->DestIP[0], pEvent->DestIP[1],
			    pEvent->DestIP[2], pEvent->DestIP[3],
			    pEvent->nSourcePort, pEvent->nDestPort,
			    BMXDHCPLowReasonName (pEvent->nReason),
			    pEvent->nResult, pEvent->nTXSeq, pEvent->nTXWindow,
			    pEvent->nFCMask, pEvent->nQueueLength);
	}

	s_BMXDHCPLow.bDumped = true;
}
#endif

l2_packet_data * l2_packet_init (const char *ifname, const u8 *own_addr, unsigned short protocol,
				 void (*rx_callback) (void *ctx, const u8 *src_addr,
						      const u8 *buf, size_t len),
				 void *rx_callback_ctx, int l2_hdr)
{
	assert (own_addr == 0);
	assert (protocol == 0x888E);
	assert (l2_hdr == 0);

	l2_packet_data *l2 = (l2_packet_data *) os_zalloc (sizeof *l2);
	if (l2 == 0)
	{
		return 0;
	}

	l2->protocol = protocol;
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;

	auth_active = 0;
	active_l2 = l2;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMXEAPOLLowReset ();
	BMXDHCPLowReset ();
#endif

	const CMACAddress *mac = CNetSubSystem::Get ()->GetNetDeviceLayer ()->GetMACAddress ();
	assert (mac != 0);
	mac->CopyTo (l2->own_addr);

	l2->link = CNetSubSystem::Get ()->GetLinkLayer ();
	assert (l2->link != 0);
	if (!l2->link->EnableReceiveRaw (protocol))
	{
		os_free (l2);

		return 0;
	}

	eloop_register_read_sock (SOCK_FD, l2_packet_receive, l2, 0);
	BMX_L2_LOG ("init protocol=0x%04X", protocol);

	return l2;
}

l2_packet_data * l2_packet_init_bridge (const char *br_ifname, const char *ifname,
					const u8 *own_addr, unsigned short protocol,
					void (*rx_callback) (void *ctx, const u8 *src_addr,
							     const u8 *buf, size_t len),
					void *rx_callback_ctx, int l2_hdr)
{
	return l2_packet_init (br_ifname, own_addr, protocol, rx_callback, rx_callback_ctx, l2_hdr);
}

void l2_packet_deinit (l2_packet_data *l2)
{
	if (l2 == 0)
	{
		return;
	}

	eloop_unregister_read_sock (SOCK_FD);
	if (active_l2 == l2)
	{
		active_l2 = 0;
	}

	os_free (l2);
}

int l2_packet_get_own_addr (l2_packet_data *l2, u8 *addr)
{
	assert (l2 != 0);
	assert (addr != 0);
	os_memcpy (addr, l2->own_addr, ETH_ALEN);

	return 0;
}

int l2_packet_send (l2_packet_data *l2, const u8 *dst_addr, u16 proto, const u8 *buf, size_t len)
{
	if (l2 == 0)
	{
		return -1;
	}

	u8 Buffer[FRAME_BUFFER_SIZE];
	TEthernetHeader *pHeader = (TEthernetHeader *) Buffer;
	assert (dst_addr != 0);
	os_memcpy (pHeader->MACReceiver, dst_addr, MAC_ADDRESS_SIZE);
	os_memcpy (pHeader->MACSender, l2->own_addr, MAC_ADDRESS_SIZE);
	pHeader->nProtocolType = le2be16 (l2->protocol);

	assert (len > 0);
	if (len + sizeof (TEthernetHeader) > FRAME_BUFFER_SIZE)
	{
		return -1;
	}

	assert (buf != 0);
	os_memcpy (Buffer + sizeof (TEthernetHeader), buf, len);

	assert (l2->link != 0);
#ifdef BMC64_WLAN_TRACE
	unsigned nTraceSeq = BMXEAPOLTracePayload ("tx", dst_addr, buf, len);
#endif
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMXEAPOLLowRecordPayload (BMX_EAPOL_STAGE_L2_TX, buf, len, -1, -1, 0, 0);
#endif
	BMX_L2_LOG ("tx dst=" MACSTR " proto=0x%04X len=%u",
		    MAC2STR (dst_addr), proto, (unsigned) len);
	if (l2->protocol == ETH_P_EAPOL)
	{
		bmx_l2_note_pending_8021x_tx ();
	}
	if (!l2->link->SendRaw (Buffer, len + sizeof (TEthernetHeader)))
	{
		if (l2->protocol == ETH_P_EAPOL)
		{
			bmx_l2_note_done_8021x_tx ();
		}

		return -1;
	}
#ifdef BMC64_WLAN_TRACE
	BMX_L2_LOG ("tx submit done seq=%u len=%u", nTraceSeq, (unsigned) len);
#endif

	return 0;
}

static void l2_packet_receive (int sock, void *eloop_ctx, void *sock_ctx)
{
	assert (sock == SOCK_FD);
	l2_packet_data *l2 = (l2_packet_data *) eloop_ctx;
	assert (l2 != 0);
	assert (l2->link != 0);
	assert (l2->rx_callback != 0);

	u8 Buffer[FRAME_BUFFER_SIZE];
	unsigned nResultLength;
	CMACAddress Sender;
	while (l2->link->ReceiveRaw (Buffer, &nResultLength, &Sender))
	{
		u8 src_addr[MAC_ADDRESS_SIZE];
		Sender.CopyTo (src_addr);

#ifdef BMC64_WLAN_TRACE
		unsigned nTraceSeq = BMXEAPOLTracePayload ("rx", src_addr, Buffer, nResultLength);
#endif
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		BMXEAPOLLowRecordPayload (BMX_EAPOL_STAGE_L2_RX, Buffer, nResultLength,
					  -1, -1, 0, 0);
#endif
		unsigned nMsg = BMXL2EAPOLMsg (Buffer, nResultLength);
		if (!auth_active && BMXL2DropInactiveEAPOL (nMsg))
		{
			BMX_L2_STATE_LOG ("drop stale rx msg=%s len=%u auth_active=0",
					  BMXL2EAPOLMsgName (nMsg), nResultLength);
			CScheduler::Get ()->Yield ();
			continue;
		}

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		BMXEAPOLLowRecordPayload (BMX_EAPOL_STAGE_L2_RX_CALLBACK_ENTER,
					  Buffer, nResultLength, -1, -1, 0, 0);
#endif
		BMX_L2_LOG ("rx src=" MACSTR " len=%u", MAC2STR (src_addr), nResultLength);
		(*l2->rx_callback) (l2->rx_callback_ctx, src_addr, Buffer, nResultLength);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		BMXEAPOLLowRecordPayload (BMX_EAPOL_STAGE_L2_RX_CALLBACK_RETURN,
					  Buffer, nResultLength, -1, -1, 0, 0);
#endif
		BMX_L2_LOG ("rx dispatch done seq=%u len=%u", nTraceSeq, nResultLength);

		CScheduler::Get ()->Yield ();
	}
}

#define AUTH_DURATION_SECS	5

static void l2_packet_auth_end (void *eloop_ctx, void *timeout_ctx)
{
	auth_active = 0;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMXEAPOLLowAuthEnd ();
#endif
	BMX_L2_LOG ("auth window end");
}

void l2_packet_notify_auth_start (l2_packet_data *l2)
{
	assert (l2 != 0);

	bmx_l2_flush_receive_path (l2, "auth-start");
	auth_active = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMXEAPOLLowAuthStart ();
#endif
	BMX_L2_LOG ("auth window start");

	eloop_cancel_timeout (l2_packet_auth_end, l2, 0);
	eloop_register_timeout (AUTH_DURATION_SECS, 0, l2_packet_auth_end, l2, 0);
}

int l2_packet_get_ip_addr (l2_packet_data *l2, char *buf, size_t len)
{
	return -1;
}

int l2_packet_set_packet_filter (l2_packet_data *l2, l2_packet_filter_type type)
{
	return -1;
}

extern "C" int l2_packet_auth_active (void)
{
	return auth_active;
}
