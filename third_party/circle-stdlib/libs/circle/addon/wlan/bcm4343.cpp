//
// bcm4343.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2020-2025  R. Stange <rsta2@gmx.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <wlan/bcm4343.h>
#include <wlan/p9compat.h>
#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/net/linklayer.h>
#include <circle/net/netdevlayer.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <circle/sysconfig.h>
#include <circle/timer.h>
#include <assert.h>
#include <string.h>

#if RASPPI <= 3 && !defined (USE_SDHOST)
	#warning WLAN cannot be used parallel with SD card access in this configuration!
#endif

LOGMODULE ("bcm4343");

#ifdef BMC64_WLAN_TRACE
#define BMX_WLAN_LOG(_fmt, ...) CLogger::Get ()->Write ("bmx-wlan", LogNotice, _fmt, ##__VA_ARGS__)
#else
#define BMX_WLAN_LOG(_fmt, ...)
#endif
#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE_LOG)
#define BMX_WLAN_RECOVERY_LOG(_fmt, ...) CLogger::Get ()->Write ("bmx-wlan", LogNotice, _fmt, ##__VA_ARGS__)
#define BMX_WLAN_EVENT_LOG(_fmt, ...) CLogger::Get ()->Write ("bmx-wlan-event", LogNotice, _fmt, ##__VA_ARGS__)
#else
#define BMX_WLAN_RECOVERY_LOG(_fmt, ...)
#define BMX_WLAN_EVENT_LOG(_fmt, ...)
#endif

#ifdef BMC64_WLAN_TRACE
static const unsigned BcmTraceETHHeaderLen = 14;
static const unsigned BcmTraceIPHeaderMinLen = 20;
static const unsigned BcmTraceUDPHeaderLen = 8;
static const u16 BcmTraceETHProtIP = 0x0800;
static const u16 BcmTraceETHProtEAPOL = 0x888E;
static const u8 BcmTraceIPProtoUDP = 17;
static const u16 BcmTraceDHCPPortServer = 67;
static const u16 BcmTraceDHCPPortClient = 68;

static u16 BcmTraceBE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static u32 BcmTraceBE32 (const u8 *p)
{
	return   (u32) p[0] << 24
	       | (u32) p[1] << 16
	       | (u32) p[2] << 8
	       | (u32) p[3];
}

static const char *BcmTraceDHCPTypeName (unsigned nType)
{
	switch (nType)
	{
	case 1:	return "DISCOVER";
	case 2:	return "OFFER";
	case 3:	return "REQUEST";
	case 5:	return "ACK";
	case 6:	return "NAK";
	default:
		return "UNKNOWN";
	}
}

static unsigned BcmTraceDHCPType (const u8 *pPayload, unsigned nPayloadLen)
{
	if (nPayloadLen < 240)
	{
		return 0;
	}
	if (   pPayload[236] != 99
	    || pPayload[237] != 130
	    || pPayload[238] != 83
	    || pPayload[239] != 99)
	{
		return 0;
	}

	for (unsigned i = 240; i < nPayloadLen;)
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
		if (i+2 > nPayloadLen)
		{
			break;
		}

		unsigned nLen = pPayload[i+1];
		if (i+2+nLen > nPayloadLen)
		{
			break;
		}
		if (uchOption == 53 && nLen >= 1)
		{
			return pPayload[i+2];
		}

		i += 2+nLen;
	}

	return 0;
}

static boolean BcmTraceIsDHCPPort (u16 nSourcePort, u16 nDestPort)
{
	return    nSourcePort == BcmTraceDHCPPortServer
	       || nSourcePort == BcmTraceDHCPPortClient
	       || nDestPort == BcmTraceDHCPPortServer
	       || nDestPort == BcmTraceDHCPPortClient;
}

static void BcmTraceDHCPFrame (const char *pStage, const void *pBuffer, unsigned nLength)
{
	const u8 *pFrame = (const u8 *) pBuffer;
	if (pFrame == 0 || nLength < BcmTraceETHHeaderLen + BcmTraceIPHeaderMinLen + BcmTraceUDPHeaderLen)
	{
		return;
	}

	u16 nEtherType = BcmTraceBE16 (pFrame+12);
	if (nEtherType != BcmTraceETHProtIP)
	{
		return;
	}

	const u8 *pIP = pFrame + BcmTraceETHHeaderLen;
	unsigned nIPHeaderLen = (pIP[0] & 0x0F) * 4;
	if (   (pIP[0] >> 4) != 4
	    || nIPHeaderLen < BcmTraceIPHeaderMinLen
	    || nLength < BcmTraceETHHeaderLen + nIPHeaderLen + BcmTraceUDPHeaderLen
	    || pIP[9] != BcmTraceIPProtoUDP)
	{
		return;
	}

	const u8 *pUDP = pIP + nIPHeaderLen;
	u16 nSourcePort = BcmTraceBE16 (pUDP);
	u16 nDestPort = BcmTraceBE16 (pUDP+2);
	if (!BcmTraceIsDHCPPort (nSourcePort, nDestPort))
	{
		return;
	}

	unsigned nUDPLen = BcmTraceBE16 (pUDP+4);
	if (   nUDPLen < BcmTraceUDPHeaderLen
	    || nLength < BcmTraceETHHeaderLen + nIPHeaderLen + nUDPLen)
	{
		CLogger::Get ()->Write ("bmx-wlan-dhcp", LogNotice,
					"%s truncated len=%u udplen=%u ports=%u>%u",
					pStage, nLength, nUDPLen, nSourcePort, nDestPort);
		return;
	}

	const u8 *pDHCP = pUDP + BcmTraceUDPHeaderLen;
	unsigned nDHCPPayloadLen = nUDPLen - BcmTraceUDPHeaderLen;
	unsigned nDHCPType = BcmTraceDHCPType (pDHCP, nDHCPPayloadLen);
	u32 nXID = nDHCPPayloadLen >= 8 ? BcmTraceBE32 (pDHCP+4) : 0;
	u16 nFlags = nDHCPPayloadLen >= 12 ? BcmTraceBE16 (pDHCP+10) : 0;

	CLogger::Get ()->Write ("bmx-wlan-dhcp", LogNotice,
				"%s len=%u eth=%02X:%02X:%02X:%02X:%02X:%02X>%02X:%02X:%02X:%02X:%02X:%02X ip=%u.%u.%u.%u>%u.%u.%u.%u udp=%u>%u dhcp=%s(%u) xid=%08X flags=%04X",
				pStage, nLength,
				pFrame[6], pFrame[7], pFrame[8], pFrame[9], pFrame[10], pFrame[11],
				pFrame[0], pFrame[1], pFrame[2], pFrame[3], pFrame[4], pFrame[5],
				pIP[12], pIP[13], pIP[14], pIP[15],
				pIP[16], pIP[17], pIP[18], pIP[19],
				nSourcePort, nDestPort,
				BcmTraceDHCPTypeName (nDHCPType), nDHCPType, nXID, nFlags);
}

static const char *BcmTraceEAPOLTypeName (unsigned nType)
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

static const char *BcmTraceEAPOLKeyDescName (unsigned nDesc)
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

static const char *BcmTraceEAPOLKeyMsgName (u16 nKeyInfo)
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

static void BcmTraceEAPOLFrame (const char *pStage, const void *pBuffer, unsigned nLength)
{
	const u8 *pFrame = (const u8 *) pBuffer;
	if (pFrame == 0 || nLength < BcmTraceETHHeaderLen + 4)
	{
		return;
	}
	if (BcmTraceBE16 (pFrame+12) != BcmTraceETHProtEAPOL)
	{
		return;
	}

	const u8 *pEAPOL = pFrame + BcmTraceETHHeaderLen;
	unsigned nPayloadLen = nLength - BcmTraceETHHeaderLen;
	unsigned nVersion = pEAPOL[0];
	unsigned nType = pEAPOL[1];
	unsigned nEAPOLLen = BcmTraceBE16 (pEAPOL+2);
	if (nType != 3 || nPayloadLen < 99)
	{
		CLogger::Get ()->Write ("bmx-wlan-eapol", LogNotice,
					"%s len=%u eth=%02X:%02X:%02X:%02X:%02X:%02X>%02X:%02X:%02X:%02X:%02X:%02X version=%u type=%s(%u) eapol_len=%u",
					pStage, nLength,
					pFrame[6], pFrame[7], pFrame[8], pFrame[9], pFrame[10], pFrame[11],
					pFrame[0], pFrame[1], pFrame[2], pFrame[3], pFrame[4], pFrame[5],
					nVersion, BcmTraceEAPOLTypeName (nType), nType, nEAPOLLen);
		return;
	}

	unsigned nDesc = pEAPOL[4];
	u16 nKeyInfo = BcmTraceBE16 (pEAPOL+5);
	unsigned nKeyLength = BcmTraceBE16 (pEAPOL+7);
	unsigned nKeyDataLength = BcmTraceBE16 (pEAPOL+97);
	CLogger::Get ()->Write ("bmx-wlan-eapol", LogNotice,
				"%s len=%u eth=%02X:%02X:%02X:%02X:%02X:%02X>%02X:%02X:%02X:%02X:%02X:%02X version=%u eapol_len=%u desc=%s(%u) msg=%s key_info=0x%04X pairwise=%u install=%u ack=%u mic=%u secure=%u encr=%u request=%u error=%u key_len=%u replay=%02X%02X%02X%02X%02X%02X%02X%02X key_data_len=%u",
				pStage, nLength,
				pFrame[6], pFrame[7], pFrame[8], pFrame[9], pFrame[10], pFrame[11],
				pFrame[0], pFrame[1], pFrame[2], pFrame[3], pFrame[4], pFrame[5],
				nVersion, nEAPOLLen, BcmTraceEAPOLKeyDescName (nDesc), nDesc,
				BcmTraceEAPOLKeyMsgName (nKeyInfo), nKeyInfo,
				(nKeyInfo & 0x0008) != 0, (nKeyInfo & 0x0040) != 0,
				(nKeyInfo & 0x0080) != 0, (nKeyInfo & 0x0100) != 0,
				(nKeyInfo & 0x0200) != 0, (nKeyInfo & 0x1000) != 0,
				(nKeyInfo & 0x0800) != 0, (nKeyInfo & 0x0400) != 0,
				nKeyLength,
				pEAPOL[9], pEAPOL[10], pEAPOL[11], pEAPOL[12],
				pEAPOL[13], pEAPOL[14], pEAPOL[15], pEAPOL[16],
				nKeyDataLength);
}
#else
#define BcmTraceDHCPFrame(_stage, _buffer, _length)
#define BcmTraceEAPOLFrame(_stage, _buffer, _length)
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_EAPOL_STAGE_BCM_TX_ENTER = 2,
	BMX_EAPOL_STAGE_BCM_TX_QUEUED = 3,
	BMX_EAPOL_STAGE_BCM_TX_CALL = 4,
	BMX_EAPOL_STAGE_BCM_TX_RETURN = 5,
	BMX_EAPOL_STAGE_BCM_TX_ERROR = 6,
	BMX_EAPOL_STAGE_BCM_RX_DEQUEUE = 7,
	BMX_EAPOL_STAGE_BCM_RX_ENQUEUE = 8,
	BMX_EAPOL_STAGE_BCM_ETHERIQ = 9
};

extern "C" void bmx_eapol_low_frame (unsigned nStage, const unsigned char *pFrame,
				     unsigned nLength, int nTXSeq, int nTXWindow,
				     unsigned nFCMask, unsigned nQueueLength);

#define BcmLowEAPOLFrame(_stage, _buffer, _length) \
	bmx_eapol_low_frame ((_stage), (const unsigned char *) (_buffer), (_length), -1, -1, 0, 0)

enum
{
	BMX_DHCP_STAGE_BCM_TX_ENTER = 30,
	BMX_DHCP_STAGE_BCM_TX_QUEUED = 31,
	BMX_DHCP_STAGE_BCM_TX_CALL = 32,
	BMX_DHCP_STAGE_BCM_TX_RETURN = 33,
	BMX_DHCP_STAGE_BCM_TX_ERROR = 34,
	BMX_DHCP_STAGE_BCM_RX_DEQUEUE = 35,
	BMX_DHCP_STAGE_BCM_RX_ENQUEUE = 36,
	BMX_DHCP_STAGE_BCM_ETHERIQ = 37
};

extern "C" void bmx_dhcp_low_frame (unsigned nStage, const unsigned char *pFrame,
				    unsigned nLength, int nTXSeq, int nTXWindow,
				    unsigned nFCMask, unsigned nQueueLength);

#define BcmLowDHCPFrame(_stage, _buffer, _length) \
	bmx_dhcp_low_frame ((_stage), (const unsigned char *) (_buffer), (_length), -1, -1, 0, 0)
#else
#define BcmLowEAPOLFrame(_stage, _buffer, _length)
#define BcmLowDHCPFrame(_stage, _buffer, _length)
#endif

extern "C" void ether4330link (void);
extern "C" void ether4330hardreset (Ether *pEther, const char *pReason);
extern "C" void ether4330linkdowncleanup (Ether *pEther, const char *pReason);
extern "C" void ether4330dumptrace (Ether *pEther, const char *pReason);
extern "C" void ether4330debugauthstart (Ether *pEther, unsigned nAuthGeneration);
extern "C" int ether4330flowstatus (Ether *pEther, bmx_wlan_flow_status_t *pStatus);
extern "C" void ether4330rxhandoff (Ether *pEther, uvlong nElapsedUS);
extern "C" void bmx_l2_flush_8021x_rx (const char *pReason);

static ether_pnp_t *s_pEtherPnpHandler = 0;
static Ether s_EtherDevice;
static const unsigned WPA_LINK_SETTLE_US = 1000000;
static const unsigned WLAN_HARD_RESET_COOLDOWN_US = 15000000;
static const unsigned WLAN_HARD_RESET_ASYNC_DELAY_US = 250000;
static const unsigned WLAN_EVENT_TASK_BUDGET = 4;

static const char *BcmEventName (ether_event_type_t Type)
{
	switch (Type)
	{
	case ether_event_link:		return "link";
	case ether_event_disassoc:	return "disassoc";
	case ether_event_deauth:		return "deauth";
	case ether_event_mic_error:	return "mic_error";
	case ether_event_scan_complete:	return "scan_complete";
	default:			return "unknown";
	}
}

static boolean BcmIsContainedCleanupControl (const char *pCommand)
{
	assert (pCommand != 0);

	return   strncmp (pCommand, "clearkey ", 9) == 0
	      || strncmp (pCommand, "disassoc ", 9) == 0
	      || strncmp (pCommand, "escan ", 6) == 0;
}

static boolean BcmIsKeyControl (const char *pCommand)
{
	assert (pCommand != 0);

	if (strncmp (pCommand, "txkey ", 6) == 0 || strncmp (pCommand, "rxkey ", 6) == 0)
	{
		return TRUE;
	}

	if (strncmp (pCommand, "rxkey", 5) != 0)
	{
		return FALSE;
	}

	char chIndex = pCommand[5];
	return '0' <= chIndex && chIndex <= '3' && pCommand[6] == ' ';
}

static const char *BcmLogControlCommand (const char *pCommand)
{
	assert (pCommand != 0);

	return BcmIsKeyControl (pCommand) ? "<redacted-key-command>" : pCommand;
}

extern "C" void bmx_wlan_debug_dump (const char *pReason)
{
	ether4330dumptrace (&s_EtherDevice, pReason != 0 ? pReason : "-");
}

extern "C" void bmx_wlan_debug_auth_start (unsigned nAuthGeneration)
{
	ether4330debugauthstart (&s_EtherDevice, nAuthGeneration);
}

class CBcm4343RecoveryTask : public CTask
{
public:
	explicit CBcm4343RecoveryTask (CBcm4343Device *pDevice)
	: CTask (16 * 1024),
	  m_pDevice (pDevice)
	{
		SetName ("wlanrec");
	}

	void Run (void) override
	{
		m_ErrorStack.stackptr = ERROR_STACK_SIZE;
		SetUserData (&m_ErrorStack, TASK_USER_DATA_ERROR_STACK);

		for (;;)
		{
			CScheduler::Get ()->MsSleep (100);
			if (m_pDevice != 0)
			{
				m_pDevice->ProcessScheduledHardReset ();
			}
		}
	}

private:
	CBcm4343Device *m_pDevice;
	struct error_stack_t m_ErrorStack;
};

class CBcm4343EventTask : public CTask
{
public:
	explicit CBcm4343EventTask (CBcm4343Device *pDevice)
	: CTask (16 * 1024),
	  m_pDevice (pDevice)
	{
		SetName ("wlanevt");
	}

	void Run (void) override
	{
		m_ErrorStack.stackptr = ERROR_STACK_SIZE;
		SetUserData (&m_ErrorStack, TASK_USER_DATA_ERROR_STACK);

		for (;;)
		{
			unsigned nProcessed = 0;
			if (m_pDevice != 0)
			{
				nProcessed = m_pDevice->ProcessQueuedEvents (WLAN_EVENT_TASK_BUDGET);
			}

			if (nProcessed == 0)
			{
				CScheduler::Get ()->MsSleep (1);
			}
			else
			{
				CScheduler::Get ()->Yield ();
			}
		}
	}

private:
	CBcm4343Device *m_pDevice;
	struct error_stack_t m_ErrorStack;
};

CBcm4343Device *CBcm4343Device::s_pThis = 0;

CBcm4343Device::CBcm4343Device (const char *pFirmwarePath)
:	m_FirmwarePath (pFirmwarePath),
	m_pEventHandler (0),
	m_pEventContext (0),
	m_EventSpinLock (TASK_LEVEL),
	m_nEventHead (0),
	m_nEventTail (0),
	m_nEventCount (0),
	m_nEventNextSequence (1),
	m_nEventDropped (0),
	m_bDispatchingEvent (FALSE),
	m_StateSpinLock (TASK_LEVEL),
	m_bOpenNet (FALSE),
	m_bLinkUp (FALSE),
	m_nWPALinkUpSinceUS (0),
	m_bWPALinkSettled (FALSE),
	m_pIsConnected (0),
	m_bHardResetBeforeNextJoin (FALSE),
	m_bHardResetInProgress (FALSE),
	m_bControlBlockedUntilReset (FALSE),
	m_nLastHardResetUS (0),
	m_bHardResetScheduled (FALSE),
	m_nHardResetScheduledUS (0),
	m_pRecoveryTask (0),
	m_pEventTask (0)
{
	s_pThis = this;
	m_HardResetReason[0] = '\0';
}

CBcm4343Device::~CBcm4343Device (void)
{
	assert (s_EtherDevice.shutdown != 0);
	(*s_EtherDevice.shutdown) (&s_EtherDevice);

	delete s_EtherDevice.oq;

	s_pThis = 0;
}

boolean CBcm4343Device::Initialize (void)
{
#if RASPPI >= 5
	// Fetch local MAC address from DTB
	const CDeviceTreeBlob *pDTB = CMachineInfo::Get ()->GetDTB ();
	const TDeviceTreeNode *pWifi1Node;
	const TDeviceTreeProperty *pLocalMACAddress;

	if (   !pDTB
	    || !(pWifi1Node = pDTB->FindNode ("/axi/mmc@1100000/wifi@1"))
	    || !(pLocalMACAddress = pDTB->FindProperty (pWifi1Node, "local-mac-address"))
	    || pDTB->GetPropertyValueLength (pLocalMACAddress) != 6)
	{
		LOGERR ("Cannot get MAC address from DTB");

		return FALSE;
	}

	m_MACAddress.Set (pDTB->GetPropertyValue (pLocalMACAddress));

	m_MACAddress.CopyTo (s_EtherDevice.ea);
#endif

	p9arch_init ();
	p9chan_init (m_FirmwarePath);
	p9proc_init ();

	ether4330link ();
	assert (s_pEtherPnpHandler != 0);
	(*s_pEtherPnpHandler) (&s_EtherDevice);

	s_EtherDevice.oq = new Queue;
	memset (s_EtherDevice.oq, 0, sizeof *s_EtherDevice.oq);

	if (waserror ())
	{
		return FALSE;
	}

	assert (s_EtherDevice.attach != 0);
	(*s_EtherDevice.attach) (&s_EtherDevice);

	assert (s_EtherDevice.setevhndlr != 0);
	(*s_EtherDevice.setevhndlr) (&s_EtherDevice, LowLevelEventHandler, this);

#if RASPPI < 5
	m_MACAddress.Set (s_EtherDevice.ea);
#endif

	AddNetDevice ();

	if (m_pRecoveryTask == 0)
	{
		m_pRecoveryTask = new CBcm4343RecoveryTask (this);
	}
	if (m_pEventTask == 0)
	{
		m_pEventTask = new CBcm4343EventTask (this);
	}

	poperror ();

	return TRUE;
}

const CMACAddress *CBcm4343Device::GetMACAddress (void) const
{
	return &m_MACAddress;
}

boolean CBcm4343Device::SendFrame (const void *pBuffer, unsigned nLength)
{
	//hexdump (pBuffer, nLength, "wlantx");

	BcmTraceDHCPFrame ("bcm tx-enter", pBuffer, nLength);
	BcmTraceEAPOLFrame ("bcm tx-enter", pBuffer, nLength);
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_TX_ENTER, pBuffer, nLength);
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_TX_ENTER, pBuffer, nLength);

	Block *pBlock = allocb (nLength);
	assert (pBlock != 0);

	assert (pBlock->wp != 0);
	assert (pBuffer != 0);
	memcpy (pBlock->wp, pBuffer, nLength);
	pBlock->wp += nLength;

	assert (s_EtherDevice.oq != 0);
	pBlock->timingus = p9microseconds ();
	qpass (s_EtherDevice.oq, pBlock);
	BcmTraceDHCPFrame ("bcm tx-queued", pBuffer, nLength);
	BcmTraceEAPOLFrame ("bcm tx-queued", pBuffer, nLength);
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_TX_QUEUED, pBuffer, nLength);
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_TX_QUEUED, pBuffer, nLength);

	if (waserror ())
	{
		BcmTraceDHCPFrame ("bcm tx-error", pBuffer, nLength);
		BcmTraceEAPOLFrame ("bcm tx-error", pBuffer, nLength);
		BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_TX_ERROR, pBuffer, nLength);
		BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_TX_ERROR, pBuffer, nLength);
		return FALSE;
	}

	BcmTraceDHCPFrame ("bcm tx-call", pBuffer, nLength);
	BcmTraceEAPOLFrame ("bcm tx-call", pBuffer, nLength);
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_TX_CALL, pBuffer, nLength);
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_TX_CALL, pBuffer, nLength);

	assert (s_EtherDevice.transmit != 0);
	(*s_EtherDevice.transmit) (&s_EtherDevice);

	poperror ();

	BcmTraceDHCPFrame ("bcm tx-return", pBuffer, nLength);
	BcmTraceEAPOLFrame ("bcm tx-return", pBuffer, nLength);
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_TX_RETURN, pBuffer, nLength);
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_TX_RETURN, pBuffer, nLength);
	return TRUE;
}

boolean CBcm4343Device::ReceiveFrame (void *pBuffer, unsigned *pResultLength)
{
	assert (pBuffer != 0);
	void *pTiming = 0;
	unsigned nLength = m_RxQueue.Dequeue (pBuffer, &pTiming);
	if (nLength == 0)
	{
		return FALSE;
	}
	u64 nNowUS = p9microseconds ();
	uintptr nStartUS = reinterpret_cast<uintptr> (pTiming);
#if AARCH == 32
	u64 nElapsedUS = static_cast<u32> (nNowUS) - static_cast<u32> (nStartUS);
#else
	u64 nElapsedUS = nNowUS - nStartUS;
#endif
	ether4330rxhandoff (&s_EtherDevice, nElapsedUS);

	assert (pResultLength != 0);
	*pResultLength = nLength;

	BcmTraceDHCPFrame ("bcm rx-dequeue", pBuffer, nLength);
	BcmTraceEAPOLFrame ("bcm rx-dequeue", pBuffer, nLength);
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_RX_DEQUEUE, pBuffer, nLength);
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_RX_DEQUEUE, pBuffer, nLength);

	//hexdump (pBuffer, nLength, "wlanrx");

	return TRUE;
}

boolean CBcm4343Device::IsLinkUp (void)
{
	m_StateSpinLock.Acquire ();
	boolean bOpenNet = m_bOpenNet;
	boolean bLinkUp = m_bLinkUp;
	TBcm4343ConnectedProvider *pIsConnected = m_pIsConnected;
	m_StateSpinLock.Release ();

	if (bOpenNet)
	{
		return bLinkUp;
	}

	if (pIsConnected != 0)
	{
		boolean bWPAConnected = (*pIsConnected) ();

		m_StateSpinLock.Acquire ();
		if (!bWPAConnected || !m_bLinkUp)
		{
			m_bHardResetBeforeNextJoin = FALSE;
			m_nWPALinkUpSinceUS = 0;
			m_bWPALinkSettled = FALSE;
			m_StateSpinLock.Release ();

			return FALSE;
		}

		if (m_bWPALinkSettled)
		{
			m_StateSpinLock.Release ();
			return TRUE;
		}

		unsigned nNowUS = CTimer::GetClockTicks ();
		if (m_nWPALinkUpSinceUS == 0)
		{
			m_nWPALinkUpSinceUS = nNowUS;
			m_StateSpinLock.Release ();
			BMX_WLAN_LOG ("wpa link up; settling data path");

			return FALSE;
		}

		if (nNowUS - m_nWPALinkUpSinceUS < WPA_LINK_SETTLE_US)
		{
			m_StateSpinLock.Release ();
			return FALSE;
		}

		unsigned nSettleMS = (nNowUS - m_nWPALinkUpSinceUS) / 1000;
		m_bWPALinkSettled = TRUE;
		m_StateSpinLock.Release ();
		BMX_WLAN_LOG ("wpa data path settled after %u ms", nSettleMS);
		(void) nSettleMS;

		return TRUE;
	}

	return FALSE;
}

boolean CBcm4343Device::GetFlowStatus (bmx_wlan_flow_status_t *pStatus) const
{
	return ether4330flowstatus (&s_EtherDevice, pStatus) ? TRUE : FALSE;
}

boolean CBcm4343Device::SetMulticastFilter (const u8 Groups[][MAC_ADDRESS_SIZE])
{
	u32 nGroups = 0;
	while (Groups[nGroups][0])
	{
		nGroups++;
	}

	size_t ulSize = sizeof nGroups + nGroups * MAC_ADDRESS_SIZE;

	u8 Buffer[ulSize];
	memcpy (Buffer, &nGroups, sizeof nGroups);
	if (nGroups)
	{
		memcpy (Buffer + sizeof nGroups, Groups, nGroups * MAC_ADDRESS_SIZE);
	}

	if (waserror ())
	{
		return FALSE;
	}

	assert (s_EtherDevice.setmulticast != 0);
	(*s_EtherDevice.setmulticast) (&s_EtherDevice, Buffer, ulSize);

	poperror ();

	return TRUE;
}

void CBcm4343Device::RegisterEventHandler (TBcm4343EventHandler *pHandler, void *pContext)
{
	m_EventSpinLock.Acquire ();
	m_pEventHandler = pHandler;
	m_pEventContext = pContext;
	m_EventSpinLock.Release ();

	BMX_WLAN_EVENT_LOG ("upper handler %s", pHandler != 0 ? "registered" : "cleared");
}

void CBcm4343Device::RegisterConnectedProvider (TBcm4343ConnectedProvider *pHandler)
{
	m_StateSpinLock.Acquire ();
	m_pIsConnected = pHandler;
	m_StateSpinLock.Release ();
}

void CBcm4343Device::LowLevelEventHandler (ether_event_type_t Type,
					   const ether_event_params_t *pParams,
					   void *pContext)
{
	CBcm4343Device *pThis = (CBcm4343Device *) pContext;
	if (pThis == 0)
	{
		pThis = s_pThis;
	}
	assert (pThis != 0);

	pThis->EnqueueEvent (Type, pParams);
}

void CBcm4343Device::EnqueueEvent (ether_event_type_t Type,
				   const ether_event_params_t *pParams)
{
	TBcm4343QueuedEvent Event;
	memset (&Event, 0, sizeof Event);
	Event.Type = Type;
	if (pParams != 0)
	{
		Event.Params = *pParams;
	}
	Event.nEnqueueUS = CTimer::GetClockTicks ();
	boolean bDropped = FALSE;
	TBcm4343QueuedEvent Dropped;
	memset (&Dropped, 0, sizeof Dropped);

	m_EventSpinLock.Acquire ();

	Event.nSequence = m_nEventNextSequence++;

	if (m_nEventCount == EventQueueSize)
	{
		Dropped = m_EventQueue[m_nEventHead];
		m_nEventHead = (m_nEventHead + 1) % EventQueueSize;
		m_nEventCount--;
		m_nEventDropped++;
		bDropped = TRUE;
	}

	m_EventQueue[m_nEventTail] = Event;
	m_nEventTail = (m_nEventTail + 1) % EventQueueSize;
	m_nEventCount++;

	unsigned nDepth = m_nEventCount;

	m_EventSpinLock.Release ();

	if (bDropped)
	{
		BMX_WLAN_EVENT_LOG ("drop seq=%u type=%s total=%u",
				    Dropped.nSequence, BcmEventName (Dropped.Type),
				    m_nEventDropped);
	}
	BMX_WLAN_EVENT_LOG ("enqueue seq=%u type=%s depth=%u",
			    Event.nSequence, BcmEventName (Type), nDepth);
	(void) nDepth;
}

boolean CBcm4343Device::DequeueEvent (TBcm4343QueuedEvent *pEvent)
{
	assert (pEvent != 0);

	m_EventSpinLock.Acquire ();

	if (m_nEventCount == 0)
	{
		m_EventSpinLock.Release ();
		return FALSE;
	}

	*pEvent = m_EventQueue[m_nEventHead];
	m_nEventHead = (m_nEventHead + 1) % EventQueueSize;
	m_nEventCount--;

	m_EventSpinLock.Release ();

	return TRUE;
}

void CBcm4343Device::HandleLinkDown (ether_event_type_t Type)
{
	m_StateSpinLock.Acquire ();
	m_bLinkUp = FALSE;
	m_nWPALinkUpSinceUS = 0;
	m_bWPALinkSettled = FALSE;
	m_bHardResetBeforeNextJoin = FALSE;
	boolean bHardResetScheduled = m_bHardResetScheduled;
	boolean bHardResetInProgress = m_bHardResetInProgress;
	boolean bControlBlockedUntilReset = m_bControlBlockedUntilReset;
	m_StateSpinLock.Release ();

	BMX_WLAN_RECOVERY_LOG ("linkdown cleanup type=%s scheduled=%u in_progress=%u blocked=%u",
			       BcmEventName (Type),
			       (unsigned) bHardResetScheduled,
			       (unsigned) bHardResetInProgress,
			       (unsigned) bControlBlockedUntilReset);
	(void) bHardResetScheduled;
	(void) bHardResetInProgress;
	(void) bControlBlockedUntilReset;

	FlushReceiveQueues (BcmEventName (Type));
	ether4330linkdowncleanup (&s_EtherDevice, BcmEventName (Type));
}

void CBcm4343Device::FlushReceiveQueues (const char *pReason)
{
	m_RxQueue.Flush ();
	m_ScanResultQueue.Flush ();

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

	bmx_l2_flush_8021x_rx (pReason);

	BMX_WLAN_RECOVERY_LOG ("rx queues flushed reason=%s", pReason ? pReason : "-");
}

void CBcm4343Device::DispatchEvent (const TBcm4343QueuedEvent &Event)
{
	unsigned nStartUS = CTimer::GetClockTicks ();
	unsigned nAgeMS = (nStartUS - Event.nEnqueueUS) / 1000;
	TBcm4343EventHandler *pHandler;
	void *pContext;

	m_EventSpinLock.Acquire ();
	pHandler = m_pEventHandler;
	pContext = m_pEventContext;
	m_EventSpinLock.Release ();

	BMX_WLAN_EVENT_LOG ("dispatch seq=%u type=%s age_ms=%u",
			    Event.nSequence, BcmEventName (Event.Type), nAgeMS);
	(void) nAgeMS;

	switch (Event.Type)
	{
	case ether_event_link:
		m_StateSpinLock.Acquire ();
		m_bLinkUp = TRUE;
		m_StateSpinLock.Release ();
		break;

	case ether_event_disassoc:
	case ether_event_deauth:
		HandleLinkDown (Event.Type);
		break;

	default:
		break;
	}

	m_StateSpinLock.Acquire ();
	m_bDispatchingEvent = TRUE;
	m_StateSpinLock.Release ();
	if (pHandler != 0)
	{
		(*pHandler) (Event.Type, &Event.Params, pContext);
	}
	m_StateSpinLock.Acquire ();
	m_bDispatchingEvent = FALSE;
	m_StateSpinLock.Release ();

	unsigned nDurationMS = (CTimer::GetClockTicks () - nStartUS) / 1000;
	BMX_WLAN_EVENT_LOG ("done seq=%u type=%s dur_ms=%u",
			    Event.nSequence, BcmEventName (Event.Type),
			    nDurationMS);
	(void) nDurationMS;
}

unsigned CBcm4343Device::ProcessQueuedEvents (unsigned nBudget)
{
	unsigned nProcessed = 0;

	while (nProcessed < nBudget)
	{
		TBcm4343QueuedEvent Event;
		if (!DequeueEvent (&Event))
		{
			break;
		}

		DispatchEvent (Event);
		nProcessed++;
		CScheduler::Get ()->Yield ();
	}

	return nProcessed;
}

void CBcm4343Device::ScheduleHardReset (const char *pReason)
{
	const char *pSafeReason = pReason != 0 ? pReason : "-";
	char Reason[sizeof m_HardResetReason];

	m_StateSpinLock.Acquire ();
	strncpy (m_HardResetReason, pSafeReason, sizeof m_HardResetReason - 1);
	m_HardResetReason[sizeof m_HardResetReason - 1] = '\0';
	strncpy (Reason, m_HardResetReason, sizeof Reason - 1);
	Reason[sizeof Reason - 1] = '\0';
	m_nHardResetScheduledUS = CTimer::GetClockTicks ();
	m_bHardResetScheduled = TRUE;
	m_bControlBlockedUntilReset = TRUE;
	m_StateSpinLock.Release ();

	BMX_WLAN_RECOVERY_LOG ("hard reset scheduled reason=%s delay_ms=%u",
			      Reason, WLAN_HARD_RESET_ASYNC_DELAY_US / 1000);
}

void CBcm4343Device::ProcessScheduledHardReset (void)
{
	m_StateSpinLock.Acquire ();
	if (!m_bHardResetScheduled)
	{
		m_StateSpinLock.Release ();
		return;
	}

	unsigned nNowUS = CTimer::GetClockTicks ();
	if (nNowUS - m_nHardResetScheduledUS < WLAN_HARD_RESET_ASYNC_DELAY_US)
	{
		m_StateSpinLock.Release ();
		return;
	}

	char Reason[sizeof m_HardResetReason];
	strncpy (Reason, m_HardResetReason, sizeof Reason - 1);
	Reason[sizeof Reason - 1] = '\0';
	m_bHardResetScheduled = FALSE;
	m_StateSpinLock.Release ();

	BMX_WLAN_RECOVERY_LOG ("hard reset firing reason=%s", Reason);
	HardReset (Reason);
}

boolean CBcm4343Device::HardReset (const char *pReason)
{
	boolean bBypassCooldown = pReason != 0 && strcmp (pReason, "wlcmd-timeout") == 0;

	m_StateSpinLock.Acquire ();
	if (m_bHardResetInProgress)
	{
		m_bControlBlockedUntilReset = TRUE;
		m_StateSpinLock.Release ();
		BMX_WLAN_RECOVERY_LOG ("hard reset already running reason=%s",
			      pReason != 0 ? pReason : "-");

		return TRUE;
	}

	unsigned nNowUS = CTimer::GetClockTicks ();
	if (   !bBypassCooldown
	    && m_nLastHardResetUS != 0
	    && nNowUS - m_nLastHardResetUS < WLAN_HARD_RESET_COOLDOWN_US)
	{
		unsigned nCooldownMS =
			(WLAN_HARD_RESET_COOLDOWN_US - (nNowUS - m_nLastHardResetUS)) / 1000;
		m_bHardResetScheduled = FALSE;
		m_bHardResetBeforeNextJoin = FALSE;
		m_bControlBlockedUntilReset = FALSE;
		m_StateSpinLock.Release ();
		BMX_WLAN_RECOVERY_LOG ("hard reset request reason=%s bypass_cooldown=%u",
			      pReason != 0 ? pReason : "-", (unsigned) bBypassCooldown);
		BMX_WLAN_RECOVERY_LOG ("hard reset skipped reason=%s cooldown_ms=%u",
			      pReason != 0 ? pReason : "-", nCooldownMS);
		(void) nCooldownMS;

		return TRUE;
	}

	m_bHardResetInProgress = TRUE;
	m_bControlBlockedUntilReset = TRUE;
	m_bHardResetBeforeNextJoin = FALSE;
	m_bLinkUp = FALSE;
	m_nWPALinkUpSinceUS = 0;
	m_bWPALinkSettled = FALSE;
	m_nLastHardResetUS = nNowUS;

	m_bHardResetScheduled = FALSE;
	m_StateSpinLock.Release ();

	BMX_WLAN_RECOVERY_LOG ("hard reset request reason=%s bypass_cooldown=%u",
			      pReason != 0 ? pReason : "-", (unsigned) bBypassCooldown);
	BMX_WLAN_RECOVERY_LOG ("hard reset start reason=%s", pReason != 0 ? pReason : "-");

	if (waserror ())
	{
		BMX_WLAN_RECOVERY_LOG ("hard reset failed reason=%s err=%s",
			      pReason != 0 ? pReason : "-",
			      up->errstr != 0 ? up->errstr : "-");
		m_StateSpinLock.Acquire ();
		m_bHardResetInProgress = FALSE;
		m_bControlBlockedUntilReset = TRUE;
		m_StateSpinLock.Release ();

		return FALSE;
	}

	ether4330hardreset (&s_EtherDevice, pReason);

	poperror ();

	m_StateSpinLock.Acquire ();
	m_bHardResetInProgress = FALSE;
	m_bControlBlockedUntilReset = FALSE;
	m_StateSpinLock.Release ();

	BMX_WLAN_RECOVERY_LOG ("hard reset done reason=%s", pReason != 0 ? pReason : "-");

	return TRUE;
}

void CBcm4343Device::RequestHardResetBeforeNextJoin (const char *pReason)
{
	m_StateSpinLock.Acquire ();
	boolean bLogRequest = !m_bHardResetBeforeNextJoin;
	m_bHardResetBeforeNextJoin = TRUE;
	m_bControlBlockedUntilReset = TRUE;
	m_StateSpinLock.Release ();

	if (bLogRequest)
	{
		BMX_WLAN_RECOVERY_LOG ("hard reset requested before next join reason=%s",
			      pReason != 0 ? pReason : "-");
	}

	ScheduleHardReset (pReason);
}

boolean CBcm4343Device::Control (const char *pFormat, ...)
{
	assert (pFormat != 0);

	va_list var;
	va_start (var, pFormat);

	CString Command;
	Command.FormatV (pFormat, var);

	va_end (var);

	const char *pCommand = (const char *) Command;
	const char *pLogCommand = BcmLogControlCommand (pCommand);
	(void) pLogCommand;

	m_StateSpinLock.Acquire ();
	boolean bDispatchingEvent = m_bDispatchingEvent;
	boolean bControlBlockedUntilReset = m_bControlBlockedUntilReset;
	boolean bHardResetScheduled = m_bHardResetScheduled;
	boolean bHardResetInProgress = m_bHardResetInProgress;
	boolean bHardResetBeforeNextJoin = m_bHardResetBeforeNextJoin;
	m_StateSpinLock.Release ();

	if (bDispatchingEvent)
	{
		BMX_WLAN_EVENT_LOG ("control from dispatcher '%s'", pLogCommand);
	}

	if (   bControlBlockedUntilReset
	    || bHardResetScheduled
	    || bHardResetInProgress
	    || bHardResetBeforeNextJoin)
	{
		BMX_WLAN_RECOVERY_LOG ("control blocked '%s': reset pending scheduled=%u in_progress=%u before_join=%u",
				       pLogCommand,
				       (unsigned) bHardResetScheduled,
				       (unsigned) bHardResetInProgress,
				       (unsigned) bHardResetBeforeNextJoin);

		return FALSE;
	}

	BMX_WLAN_LOG ("control '%s'", pLogCommand);

	if (waserror ())
	{
		const char *pError = up->errstr != 0 ? up->errstr : "";
		BMX_WLAN_RECOVERY_LOG ("control failed '%s' err=%s", pLogCommand, pError);
		if (strcmp (pError, "wlcmd timeout") == 0 && BcmIsContainedCleanupControl (pCommand))
		{
			BMX_WLAN_RECOVERY_LOG ("control timeout contained '%s'", pLogCommand);
		}
		else if (   strcmp (pError, "wlcmd timeout") == 0
		    || strcmp (pError, "wifi join timeout") == 0
		    || strcmp (pError, "wlan resetting") == 0)
		{
			RequestHardResetBeforeNextJoin (
				strcmp (pError, "wlcmd timeout") == 0
				? "wlcmd-timeout"
				: strcmp (pError, "wlan resetting") == 0
				  ? "wlan-resetting" : "join-timeout");
		}

		return FALSE;
	}

	assert (s_EtherDevice.ctl != 0);
	(*s_EtherDevice.ctl) (&s_EtherDevice, (const char *) Command, 0);

	poperror ();

	return TRUE;
}

boolean CBcm4343Device::ReceiveScanResult (void *pBuffer, unsigned *pResultLength)
{
	assert (pBuffer != 0);
	unsigned nLength = m_ScanResultQueue.Dequeue (pBuffer);
	if (nLength == 0)
	{
		return FALSE;
	}

	assert (pResultLength != 0);
	*pResultLength = nLength;

	//hexdump (pBuffer, nLength, "wlanscan");

	return TRUE;
}

const CMACAddress *CBcm4343Device::GetBSSID (void)
{
	u8 BSSID[MAC_ADDRESS_SIZE];
	assert (s_EtherDevice.getbssid != 0);
	(*s_EtherDevice.getbssid) (&s_EtherDevice, BSSID);

	m_BSSID.Set (BSSID);

	return &m_BSSID;
}

boolean CBcm4343Device::JoinOpenNet (const char *pSSID)
{
	m_StateSpinLock.Acquire ();
	m_bOpenNet = m_bLinkUp = FALSE;
	m_StateSpinLock.Release ();

	RegisterEventHandler (OpenNetEventHandler, this);

	assert (pSSID != 0);
	boolean bOK = Control ("join %s %s 0 off", pSSID, "FFFFFFFFFFFF");

	m_StateSpinLock.Acquire ();
	m_bOpenNet = bOK;
	m_StateSpinLock.Release ();

	return bOK;
}

// by @sebastienNEC
boolean CBcm4343Device::CreateOpenNet (const char *pSSID, int nChannel, bool bHidden)
{
	m_StateSpinLock.Acquire ();
	m_bOpenNet = m_bLinkUp = FALSE;
	m_StateSpinLock.Release ();

	assert (pSSID != 0);
	boolean bOK = Control ("create %s %d %d", pSSID, nChannel, bHidden);

	m_StateSpinLock.Acquire ();
	m_bOpenNet = m_bLinkUp = bOK;
	m_StateSpinLock.Release ();

	return bOK;
}

boolean CBcm4343Device::DestroyOpenNet (void)
{
	m_StateSpinLock.Acquire ();
	m_bOpenNet = m_bLinkUp = FALSE;
	m_StateSpinLock.Release ();

	return Control ("down");
}

void CBcm4343Device::DumpStatus (void)
{
	char Buffer[200];

	assert (s_EtherDevice.ifstat != 0);
	long nLength = (*s_EtherDevice.ifstat) (&s_EtherDevice, Buffer, sizeof Buffer, 0);
	Buffer[nLength] = '\0';

	print (Buffer);
}

void CBcm4343Device::FrameReceived (const void *pBuffer, unsigned nLength, u64 nStartUS)
{
	assert (s_pThis != 0);
	BcmTraceDHCPFrame ("bcm rx-enqueue", pBuffer, nLength);
	BcmTraceEAPOLFrame ("bcm rx-enqueue", pBuffer, nLength);
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_RX_ENQUEUE, pBuffer, nLength);
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_RX_ENQUEUE, pBuffer, nLength);
	s_pThis->m_RxQueue.Enqueue (pBuffer, nLength,
				     reinterpret_cast<void *> (static_cast<uintptr> (nStartUS)));
}

void CBcm4343Device::ScanResultReceived (const void *pBuffer, unsigned nLength)
{
	assert (s_pThis != 0);
	BMX_WLAN_LOG ("scan result received len=%u", nLength);
	s_pThis->m_ScanResultQueue.Enqueue (pBuffer, nLength);
}

void CBcm4343Device::OpenNetEventHandler (ether_event_type_t Type,
					  const ether_event_params_t *pParams,
					  void *pContext)
{
	CBcm4343Device *pThis = (CBcm4343Device *) pContext;
	if (pThis == 0)
	{
		pThis = s_pThis;
	}
	assert (pThis != 0);

	switch (Type)
	{
	case ether_event_link:
		pThis->m_StateSpinLock.Acquire ();
		pThis->m_bLinkUp = TRUE;
		pThis->m_StateSpinLock.Release ();
		break;

	case ether_event_disassoc:
		pThis->m_StateSpinLock.Acquire ();
		pThis->m_bLinkUp = FALSE;
		pThis->m_StateSpinLock.Release ();
		break;

	default:
		break;
	}
}

void etheriq (Ether *pEther, Block *pBlock, unsigned nFlag)
{
	assert (pBlock != 0);
	BcmTraceDHCPFrame ("bcm etheriq", pBlock->rp, BLEN (pBlock));
	BcmTraceEAPOLFrame ("bcm etheriq", pBlock->rp, BLEN (pBlock));
	BcmLowEAPOLFrame (BMX_EAPOL_STAGE_BCM_ETHERIQ, pBlock->rp, BLEN (pBlock));
	BcmLowDHCPFrame (BMX_DHCP_STAGE_BCM_ETHERIQ, pBlock->rp, BLEN (pBlock));
	CBcm4343Device::FrameReceived (pBlock->rp, BLEN (pBlock), pBlock->timingus);

	freeb (pBlock);
}

void etherscanresult (Ether *pEther, const void *pBuffer, long nLength)
{
	assert (pBuffer != 0);
	CBcm4343Device::ScanResultReceived (pBuffer, (unsigned) nLength);
}

void addethercard (const char *pName, ether_pnp_t *pEtherPnpHandler)
{
	assert (pEtherPnpHandler != 0);
	s_pEtherPnpHandler = pEtherPnpHandler;
}
