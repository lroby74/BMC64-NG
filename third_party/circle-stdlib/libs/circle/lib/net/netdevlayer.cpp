//
// netdevlayer.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015-2025  R. Stange <rsta2@gmx.net>
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
#include <circle/net/netdevlayer.h>
#include <circle/net/phytask.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/synchronize.h>
#include <circle/macros.h>
#include <circle/util.h>
#include <assert.h>

const char FromNetDev[] = "netdev";

#ifdef BMC64_WLAN_TRACE
static const unsigned TraceETHHeaderLen = 14;
static const unsigned TraceIPHeaderMinLen = 20;
static const unsigned TraceUDPHeaderLen = 8;
static const u16 TraceETHProtIP = 0x0800;
static const u8 TraceIPProtoUDP = 17;
static const u16 TraceDHCPPortServer = 67;
static const u16 TraceDHCPPortClient = 68;

static u16 NetDevTraceBE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static u32 NetDevTraceBE32 (const u8 *p)
{
	return   (u32) p[0] << 24
	       | (u32) p[1] << 16
	       | (u32) p[2] << 8
	       | (u32) p[3];
}

static const char *NetDevTraceDHCPTypeName (unsigned nType)
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

static unsigned NetDevTraceDHCPType (const u8 *pPayload, unsigned nPayloadLen)
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

static boolean NetDevTraceIsDHCPPort (u16 nSourcePort, u16 nDestPort)
{
	return    nSourcePort == TraceDHCPPortServer
	       || nSourcePort == TraceDHCPPortClient
	       || nDestPort == TraceDHCPPortServer
	       || nDestPort == TraceDHCPPortClient;
}

static void NetDevTraceDHCPFrame (const char *pStage, const void *pBuffer, unsigned nLength)
{
	const u8 *pFrame = (const u8 *) pBuffer;
	if (pFrame == 0 || nLength < TraceETHHeaderLen + TraceIPHeaderMinLen + TraceUDPHeaderLen)
	{
		return;
	}

	u16 nEtherType = NetDevTraceBE16 (pFrame+12);
	if (nEtherType != TraceETHProtIP)
	{
		return;
	}

	const u8 *pIP = pFrame + TraceETHHeaderLen;
	unsigned nIPHeaderLen = (pIP[0] & 0x0F) * 4;
	if (   (pIP[0] >> 4) != 4
	    || nIPHeaderLen < TraceIPHeaderMinLen
	    || nLength < TraceETHHeaderLen + nIPHeaderLen + TraceUDPHeaderLen
	    || pIP[9] != TraceIPProtoUDP)
	{
		return;
	}

	unsigned nIPTotalLen = NetDevTraceBE16 (pIP+2);
	if (nIPTotalLen < nIPHeaderLen + TraceUDPHeaderLen)
	{
		return;
	}

	const u8 *pUDP = pIP + nIPHeaderLen;
	u16 nSourcePort = NetDevTraceBE16 (pUDP);
	u16 nDestPort = NetDevTraceBE16 (pUDP+2);
	if (!NetDevTraceIsDHCPPort (nSourcePort, nDestPort))
	{
		return;
	}

	unsigned nUDPLen = NetDevTraceBE16 (pUDP+4);
	if (   nUDPLen < TraceUDPHeaderLen
	    || nLength < TraceETHHeaderLen + nIPHeaderLen + nUDPLen)
	{
		CLogger::Get ()->Write ("bmx-dhcp-frame", LogNotice,
					"%s truncated len=%u iplen=%u udplen=%u ports=%u>%u",
					pStage, nLength, nIPTotalLen, nUDPLen,
					nSourcePort, nDestPort);
		return;
	}

	const u8 *pDHCP = pUDP + TraceUDPHeaderLen;
	unsigned nDHCPPayloadLen = nUDPLen - TraceUDPHeaderLen;
	unsigned nDHCPType = NetDevTraceDHCPType (pDHCP, nDHCPPayloadLen);
	u32 nXID = nDHCPPayloadLen >= 8 ? NetDevTraceBE32 (pDHCP+4) : 0;
	u16 nFlags = nDHCPPayloadLen >= 12 ? NetDevTraceBE16 (pDHCP+10) : 0;

	CLogger::Get ()->Write ("bmx-dhcp-frame", LogNotice,
				"%s len=%u eth=%02X:%02X:%02X:%02X:%02X:%02X>%02X:%02X:%02X:%02X:%02X:%02X ip=%u.%u.%u.%u>%u.%u.%u.%u udp=%u>%u dhcp=%s(%u) xid=%08X flags=%04X chaddr=%02X:%02X:%02X:%02X:%02X:%02X",
				pStage, nLength,
				pFrame[6], pFrame[7], pFrame[8], pFrame[9], pFrame[10], pFrame[11],
				pFrame[0], pFrame[1], pFrame[2], pFrame[3], pFrame[4], pFrame[5],
				pIP[12], pIP[13], pIP[14], pIP[15],
				pIP[16], pIP[17], pIP[18], pIP[19],
				nSourcePort, nDestPort,
				NetDevTraceDHCPTypeName (nDHCPType), nDHCPType,
				nXID, nFlags,
				nDHCPPayloadLen >= 34 ? pDHCP[28] : 0,
				nDHCPPayloadLen >= 34 ? pDHCP[29] : 0,
				nDHCPPayloadLen >= 34 ? pDHCP[30] : 0,
				nDHCPPayloadLen >= 34 ? pDHCP[31] : 0,
				nDHCPPayloadLen >= 34 ? pDHCP[32] : 0,
				nDHCPPayloadLen >= 34 ? pDHCP[33] : 0);
}
#else
#define NetDevTraceDHCPFrame(_stage, _buffer, _length)
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_DHCP_STAGE_NETDEV_TX = 5,
	BMX_DHCP_STAGE_NETDEV_TX_FAILED = 6,
	BMX_DHCP_STAGE_NETDEV_TX_DONE = 7,
	BMX_DHCP_STAGE_NETDEV_RX = 8
};

extern "C" void bmx_dhcp_low_frame (unsigned nStage, const unsigned char *pFrame,
				    unsigned nLength, int nTXSeq, int nTXWindow,
				    unsigned nFCMask, unsigned nQueueLength);

#define NetDevLowDHCPFrame(_stage, _buffer, _length) \
	bmx_dhcp_low_frame ((_stage), (const unsigned char *) (_buffer), (_length), -1, -1, 0, 0)
#else
#define NetDevLowDHCPFrame(_stage, _buffer, _length)
#endif

CNetDeviceLayer::CNetDeviceLayer (CNetConfig *pNetConfig, TNetDeviceType DeviceType)
:	m_DeviceType (DeviceType),
	m_pNetConfig (pNetConfig),
	m_pDevice (0),
	m_TxQueue (TRUE),
	m_RxQueue (TRUE),
	m_pRxBuffer (new CNetBuffer (CNetBuffer::Receive, FRAME_BUFFER_SIZE))
{
}

CNetDeviceLayer::~CNetDeviceLayer (void)
{
	delete m_pRxBuffer;
	m_pRxBuffer = 0;

	m_pDevice = 0;
	m_pNetConfig = 0;
}

boolean CNetDeviceLayer::Initialize (boolean bWaitForActivate)
{
#if RASPPI == 4
	if (!m_Bcm54213.Initialize ())
	{
		return FALSE;
	}
#elif RASPPI >= 5
	if (!m_MACB.Initialize ())
	{
		return FALSE;
	}
#endif

	if (!bWaitForActivate)
	{
		return TRUE;
	}

	assert (m_pDevice == 0);
	m_pDevice = CNetDevice::GetNetDevice (m_DeviceType);
	if (m_pDevice == 0)
	{
		CLogger::Get ()->Write (FromNetDev, LogError, "Net device not available");

		return FALSE;
	}

	new CPHYTask (m_pDevice);

	// wait for Ethernet PHY to come up
	unsigned nStartTicks = CTimer::Get ()->GetTicks ();
	do
	{
		if (CTimer::Get ()->GetTicks () - nStartTicks >= 4*HZ)
		{
			CLogger::Get ()->Write (FromNetDev, LogWarning, "Link is down");

			return TRUE;
		}
	}
	while (!m_pDevice->IsLinkUp ());

	TNetDeviceSpeed Speed = m_pDevice->GetLinkSpeed ();
	if (Speed != NetDeviceSpeedUnknown)
	{
		CLogger::Get ()->Write (FromNetDev, LogNotice, "Link is %s",
					CNetDevice::GetSpeedString (Speed));
	}

	return TRUE;
}

void CNetDeviceLayer::Process (void)
{
	if (m_pDevice == 0)
	{
		m_pDevice = CNetDevice::GetNetDevice (m_DeviceType);
		if (m_pDevice == 0)
		{
			return;
		}

		new CPHYTask (m_pDevice);
	}

	DMA_BUFFER (u8, Buffer, FRAME_BUFFER_SIZE);
	unsigned nLength;
	CNetBuffer *pTxBuffer;
	while (   m_pDevice->IsSendFrameAdvisable ()
	       && (pTxBuffer = m_TxQueue.Dequeue ()) != 0)
	{
		void *pBuffer = pTxBuffer->GetPtr ();
		nLength = pTxBuffer->GetLength ();
		assert (pBuffer != 0);
		assert (nLength != 0);

		NetDevTraceDHCPFrame ("netdev tx", pBuffer, nLength);
		NetDevLowDHCPFrame (BMX_DHCP_STAGE_NETDEV_TX, pBuffer, nLength);

		if (unlikely (!IS_CACHE_ALIGNED (pBuffer, 0)))
		{
			memcpy (Buffer, pBuffer, nLength);
			pBuffer = Buffer;

			static boolean bShowOnce = FALSE;
			if (!bShowOnce)
			{
				CLogger::Get ()->Write (FromNetDev, LogWarning,
							"Buffer is not cache aligned");

				pTxBuffer->Dump (FromNetDev);

				bShowOnce = TRUE;
			}
		}

		if (!m_pDevice->SendFrame (pBuffer, nLength))
		{
			NetDevTraceDHCPFrame ("netdev tx-failed", pBuffer, nLength);
			NetDevLowDHCPFrame (BMX_DHCP_STAGE_NETDEV_TX_FAILED, pBuffer, nLength);
			CLogger::Get ()->Write (FromNetDev, LogWarning, "Frame dropped");

			delete pTxBuffer;

			break;
		}

		NetDevTraceDHCPFrame ("netdev tx-done", pBuffer, nLength);
		NetDevLowDHCPFrame (BMX_DHCP_STAGE_NETDEV_TX_DONE, pBuffer, nLength);

		delete pTxBuffer;
	}

	assert (m_pRxBuffer != 0);
	while (m_pDevice->ReceiveFrame (m_pRxBuffer->GetPtr (), &nLength))
	{
		assert (nLength < FRAME_BUFFER_SIZE);
		NetDevTraceDHCPFrame ("netdev rx", m_pRxBuffer->GetPtr (), nLength);
		NetDevLowDHCPFrame (BMX_DHCP_STAGE_NETDEV_RX, m_pRxBuffer->GetPtr (), nLength);
		m_pRxBuffer->RemoveTrailer (FRAME_BUFFER_SIZE - nLength);

		m_RxQueue.Enqueue (m_pRxBuffer);

		m_pRxBuffer = new CNetBuffer (CNetBuffer::Receive, FRAME_BUFFER_SIZE);
		assert (m_pRxBuffer != 0);
	}
}

const CMACAddress *CNetDeviceLayer::GetMACAddress (void) const
{
	if (m_pDevice == 0)
	{
		return 0;
	}

	return m_pDevice->GetMACAddress ();
}

void CNetDeviceLayer::Send (CNetBuffer *pNetBuffer)
{
	m_TxQueue.Enqueue (pNetBuffer);
}

CNetBuffer *CNetDeviceLayer::Receive (void)
{
	return m_RxQueue.Dequeue ();
}

void CNetDeviceLayer::FlushReceiveQueue (void)
{
	m_RxQueue.Flush ();
}

boolean CNetDeviceLayer::IsRunning (void) const
{
	return m_pDevice != 0 && m_pDevice->IsLinkUp ();
}

boolean CNetDeviceLayer::SetMulticastFilter (const u8 Groups[][MAC_ADDRESS_SIZE])
{
	assert (m_pDevice != 0);
	return m_pDevice->SetMulticastFilter (Groups);
}
