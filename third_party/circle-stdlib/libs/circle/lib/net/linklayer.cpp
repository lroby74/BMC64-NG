//
// linklayer.cpp
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
#include <circle/net/linklayer.h>
#include <circle/net/networklayer.h>
#include <circle/net/netbuffer.h>
#ifdef BMC64_WLAN_TRACE
#include <circle/logger.h>
#endif
#include <circle/util.h>
#include <assert.h>

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_EAPOL_STAGE_LINK_RX = 18,
	BMX_EAPOL_STAGE_LINK_DROP_MAC,
	BMX_EAPOL_STAGE_LINK_DROP_PROTO,
	BMX_EAPOL_STAGE_LINK_RAW_ENQUEUE,
	BMX_EAPOL_STAGE_LINK_RAW_DEQUEUE
};

extern "C" void bmx_eapol_low_frame (unsigned nStage, const unsigned char *pFrame,
				     unsigned nLength, int nTXSeq, int nTXWindow,
				     unsigned nFCMask, unsigned nQueueLength);
extern "C" void bmx_eapol_low_payload (unsigned nStage, const unsigned char *pPayload,
				       unsigned nLength);

#define LinkLowEAPOLFrame(_stage, _frame, _len) \
	bmx_eapol_low_frame ((_stage), (const unsigned char *) (_frame), (_len), -1, -1, 0, 0)
#define LinkLowEAPOLPayload(_stage, _payload, _len) \
	bmx_eapol_low_payload ((_stage), (const unsigned char *) (_payload), (_len))
#else
#define LinkLowEAPOLFrame(_stage, _frame, _len)
#define LinkLowEAPOLPayload(_stage, _payload, _len)
#endif

#ifdef BMC64_WLAN_TRACE
static const unsigned LinkTraceIPHeaderMinLen = 20;
static const unsigned LinkTraceUDPHeaderLen = 8;
static const u8 LinkTraceIPProtoUDP = 17;
static const u16 LinkTraceDHCPPortServer = 67;
static const u16 LinkTraceDHCPPortClient = 68;

static u16 LinkTraceBE16 (const u8 *p)
{
	return (u16) p[0] << 8 | p[1];
}

static u32 LinkTraceBE32 (const u8 *p)
{
	return   (u32) p[0] << 24
	       | (u32) p[1] << 16
	       | (u32) p[2] << 8
	       | (u32) p[3];
}

static boolean LinkTraceIsDHCPPort (u16 nSourcePort, u16 nDestPort)
{
	return    nSourcePort == LinkTraceDHCPPortServer
	       || nSourcePort == LinkTraceDHCPPortClient
	       || nDestPort == LinkTraceDHCPPortServer
	       || nDestPort == LinkTraceDHCPPortClient;
}

static unsigned LinkTraceDHCPType (const u8 *pPayload, unsigned nPayloadLen)
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

static const char *LinkTraceDHCPTypeName (unsigned nType)
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

static void LinkTraceDHCPFrame (const char *pStage, const CNetBuffer *pNetBuffer)
{
	if (pNetBuffer == 0)
	{
		return;
	}

	unsigned nLength = pNetBuffer->GetLength ();
	const u8 *pFrame = (const u8 *) pNetBuffer->GetPtr ();
	if (pFrame == 0 || nLength < sizeof (TEthernetHeader) + LinkTraceIPHeaderMinLen + LinkTraceUDPHeaderLen)
	{
		return;
	}

	const TEthernetHeader *pEthernet = (const TEthernetHeader *) pFrame;
	if (pEthernet->nProtocolType != BE (ETH_PROT_IP))
	{
		return;
	}

	const u8 *pIP = pFrame + sizeof (TEthernetHeader);
	unsigned nIPHeaderLen = (pIP[0] & 0x0F) * 4;
	if (   (pIP[0] >> 4) != 4
	    || nIPHeaderLen < LinkTraceIPHeaderMinLen
	    || nLength < sizeof (TEthernetHeader) + nIPHeaderLen + LinkTraceUDPHeaderLen
	    || pIP[9] != LinkTraceIPProtoUDP)
	{
		return;
	}

	const u8 *pUDP = pIP + nIPHeaderLen;
	u16 nSourcePort = LinkTraceBE16 (pUDP);
	u16 nDestPort = LinkTraceBE16 (pUDP+2);
	if (!LinkTraceIsDHCPPort (nSourcePort, nDestPort))
	{
		return;
	}

	unsigned nUDPLen = LinkTraceBE16 (pUDP+4);
	if (   nUDPLen < LinkTraceUDPHeaderLen
	    || nLength < sizeof (TEthernetHeader) + nIPHeaderLen + nUDPLen)
	{
		CLogger::Get ()->Write ("bmx-link", LogNotice,
					"%s dhcp truncated len=%u udplen=%u ports=%u>%u",
					pStage, nLength, nUDPLen, nSourcePort, nDestPort);
		return;
	}

	const u8 *pDHCP = pUDP + LinkTraceUDPHeaderLen;
	unsigned nDHCPPayloadLen = nUDPLen - LinkTraceUDPHeaderLen;
	unsigned nDHCPType = LinkTraceDHCPType (pDHCP, nDHCPPayloadLen);
	u32 nXID = nDHCPPayloadLen >= 8 ? LinkTraceBE32 (pDHCP+4) : 0;

	CLogger::Get ()->Write ("bmx-link", LogNotice,
				"%s dhcp eth=%02X:%02X:%02X:%02X:%02X:%02X>%02X:%02X:%02X:%02X:%02X:%02X ip=%u.%u.%u.%u>%u.%u.%u.%u udp=%u>%u type=%s(%u) xid=%08X",
				pStage,
				pFrame[6], pFrame[7], pFrame[8], pFrame[9], pFrame[10], pFrame[11],
				pFrame[0], pFrame[1], pFrame[2], pFrame[3], pFrame[4], pFrame[5],
				pIP[12], pIP[13], pIP[14], pIP[15],
				pIP[16], pIP[17], pIP[18], pIP[19],
				nSourcePort, nDestPort,
				LinkTraceDHCPTypeName (nDHCPType), nDHCPType, nXID);
}
#else
#define LinkTraceDHCPFrame(_stage, _buffer)
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_DHCP_STAGE_LINK_RX = 9,
	BMX_DHCP_STAGE_LINK_DROP_MAC = 10,
	BMX_DHCP_STAGE_LINK_IP = 11,
	BMX_DHCP_STAGE_LINK_DROP_PROTO = 12
};

extern "C" void bmx_dhcp_low_frame (unsigned nStage, const unsigned char *pFrame,
				    unsigned nLength, int nTXSeq, int nTXWindow,
				    unsigned nFCMask, unsigned nQueueLength);

#define LinkLowDHCPFrame(_stage, _buffer, _length) \
	bmx_dhcp_low_frame ((_stage), (const unsigned char *) (_buffer), (_length), -1, -1, 0, 0)
#else
#define LinkLowDHCPFrame(_stage, _buffer, _length)
#endif

CLinkLayer::CLinkLayer (CNetConfig *pNetConfig, CNetDeviceLayer *pNetDevLayer)
:	m_pNetConfig (pNetConfig),
	m_pNetDevLayer (pNetDevLayer),
	m_pNetworkLayer (0),
	m_pARPHandler (0),
	m_ARPRxQueue (TRUE),
	m_IPRxQueue (TRUE),
	m_RawRxQueue (TRUE),
	m_nRawProtocolType (0)
{
	assert (m_pNetConfig != 0);
	assert (m_pNetDevLayer != 0);

	for (unsigned i = 0; i < MaxGroups; i++)
	{
		m_nMulticastUseCounter[i] = 0;
	}
}

CLinkLayer::~CLinkLayer (void)
{
	delete m_pARPHandler;
	m_pARPHandler = 0;

	m_pNetworkLayer = 0;
	m_pNetDevLayer = 0;
	m_pNetConfig = 0;
}

boolean CLinkLayer::Initialize (void)
{
	assert (m_pNetConfig != 0);
	m_pARPHandler = new CARPHandler (m_pNetConfig, m_pNetDevLayer, this, &m_ARPRxQueue);
	assert (m_pARPHandler != 0);

	return TRUE;
}

void CLinkLayer::AttachLayer (CNetworkLayer *pNetworkLayer)
{
	assert (m_pNetworkLayer == 0);
	m_pNetworkLayer = pNetworkLayer;
	assert (m_pNetworkLayer != 0);
}

void CLinkLayer::Process (void)
{
	assert (m_pNetDevLayer != 0);
	const CMACAddress *pOwnMACAddress = m_pNetDevLayer->GetMACAddress ();
	if (pOwnMACAddress == 0)
	{
		return;
	}

	assert (m_pNetDevLayer != 0);
	CNetBuffer *pNetBuffer;
	while ((pNetBuffer = m_pNetDevLayer->Receive ()) != 0)
	{
		size_t nLength = pNetBuffer->GetLength ();
		assert (nLength <= FRAME_BUFFER_SIZE);
		if (nLength <= sizeof (TEthernetHeader))
		{
			delete pNetBuffer;

			continue;
		}

		TEthernetHeader *pHeader = (TEthernetHeader *) pNetBuffer->GetPtr ();
		assert (pHeader != 0);
		LinkLowDHCPFrame (BMX_DHCP_STAGE_LINK_RX, pHeader, nLength);
		LinkLowEAPOLFrame (BMX_EAPOL_STAGE_LINK_RX, pHeader, nLength);

		CMACAddress MACAddressReceiver (pHeader->MACReceiver);
		if (    MACAddressReceiver != *pOwnMACAddress
		    && !MACAddressReceiver.IsBroadcast ())
		{
			if (!MACAddressReceiver.IsMulticast ())
			{
				LinkLowDHCPFrame (BMX_DHCP_STAGE_LINK_DROP_MAC, pHeader, nLength);
				LinkLowEAPOLFrame (BMX_EAPOL_STAGE_LINK_DROP_MAC, pHeader, nLength);
				LinkTraceDHCPFrame ("rx drop-mac", pNetBuffer);
				delete pNetBuffer;

				continue;
			}

			unsigned i;
			for (i = 0; i < MaxGroups; i++)
			{
				if (   m_nMulticastUseCounter[i] > 0
				    && m_MulticastGroup[i] == MACAddressReceiver)
				{
					break;
				}
			}

			if (i == MaxGroups)
			{
				LinkLowDHCPFrame (BMX_DHCP_STAGE_LINK_DROP_MAC, pHeader, nLength);
				LinkLowEAPOLFrame (BMX_EAPOL_STAGE_LINK_DROP_MAC, pHeader, nLength);
				LinkTraceDHCPFrame ("rx drop-multicast", pNetBuffer);
				delete pNetBuffer;

				continue;
			}
		}

		pNetBuffer->RemoveHeader (sizeof (TEthernetHeader));

		switch (pHeader->nProtocolType)
		{
		case BE (ETH_PROT_IP):
			LinkLowDHCPFrame (BMX_DHCP_STAGE_LINK_IP, pHeader, nLength);
			LinkTraceDHCPFrame ("rx accept-ip", pNetBuffer);
			m_IPRxQueue.Enqueue (pNetBuffer);
			break;

		case BE (ETH_PROT_ARP):
			m_ARPRxQueue.Enqueue (pNetBuffer);
			break;

		default:
			if (pHeader->nProtocolType == m_nRawProtocolType)
			{
				LinkLowEAPOLFrame (BMX_EAPOL_STAGE_LINK_RAW_ENQUEUE,
						   pHeader, nLength);
				pNetBuffer->SetPrivateData (pHeader->MACSender, MAC_ADDRESS_SIZE);

				m_RawRxQueue.Enqueue (pNetBuffer);
			}
			else
			{
				LinkLowDHCPFrame (BMX_DHCP_STAGE_LINK_DROP_PROTO, pHeader, nLength);
				LinkLowEAPOLFrame (BMX_EAPOL_STAGE_LINK_DROP_PROTO, pHeader, nLength);
				delete pNetBuffer;
			}
			break;
		}
	}

	assert (m_pARPHandler != 0);
	m_pARPHandler->Process ();
}

boolean CLinkLayer::Send (const CIPAddress &rReceiver, CNetBuffer *pNetBuffer)
{
	assert (pNetBuffer != 0);
	unsigned nFrameLength = sizeof (TEthernetHeader) + pNetBuffer->GetLength ();	// may wrap
	if (   nFrameLength <= sizeof (TEthernetHeader)
	    || nFrameLength > FRAME_BUFFER_SIZE)
	{
		return FALSE;
	}

	assert (m_pNetConfig != 0);
	if (   !rReceiver.IsNull ()
	    && rReceiver == *m_pNetConfig->GetIPAddress ())
	{
		m_IPRxQueue.Enqueue (pNetBuffer);	// loop back to own address

		return TRUE;
	}

	TEthernetHeader *pHeader =
		(TEthernetHeader *) pNetBuffer->AddHeader (sizeof (TEthernetHeader));

	assert (m_pNetDevLayer != 0);
	const CMACAddress *pOwnMACAddress = m_pNetDevLayer->GetMACAddress ();
	assert (pOwnMACAddress != 0);
	pOwnMACAddress->CopyTo (pHeader->MACSender);

	pHeader->nProtocolType = BE (ETH_PROT_IP);

	if (nFrameLength < ETH_MIN_LEN)
	{
		pNetBuffer->AddPadding (ETH_MIN_LEN - nFrameLength);
	}

	assert (m_pARPHandler != 0);
	CMACAddress MACAddressReceiver;
	if (   rReceiver.IsBroadcast ()
	    || rReceiver == *m_pNetConfig->GetBroadcastAddress ())
	{
		MACAddressReceiver.SetBroadcast ();
	}
	else if (rReceiver.IsMulticast ())
	{
		MACAddressReceiver.SetMulticast (rReceiver.Get ());
	}
	else if (!m_pARPHandler->Resolve (rReceiver, &MACAddressReceiver, pNetBuffer))
	{
		return TRUE;		// packet will be retransmitted by ARP handler
	}

	MACAddressReceiver.CopyTo (pHeader->MACReceiver);

	LinkTraceDHCPFrame ("tx enqueue", pNetBuffer);
	m_pNetDevLayer->Send (pNetBuffer);

	return TRUE;
}

CNetBuffer *CLinkLayer::Receive (void)
{
	return m_IPRxQueue.Dequeue ();
}

boolean CLinkLayer::SendRaw (const void *pFrame, unsigned nLength)
{
	assert (pFrame != 0);
	assert (nLength > 0);
	CNetBuffer *pNetBuffer = new CNetBuffer (CNetBuffer::LLRawSend, nLength, pFrame);
	assert (pNetBuffer != 0);

	assert (m_pNetDevLayer != 0);
	m_pNetDevLayer->Send (pNetBuffer);

	return TRUE;
}

boolean CLinkLayer::ReceiveRaw (void *pBuffer, unsigned *pResultLength, CMACAddress *pSender)
{
	CNetBuffer *pNetBuffer = m_RawRxQueue.Dequeue ();
	if (pNetBuffer == 0)
	{
		return FALSE;
	}

	assert (pResultLength != 0);
	*pResultLength = pNetBuffer->GetLength ();

	if (pSender != 0)
	{
		pSender->Set ((const u8 *) pNetBuffer->GetPrivateData ());
	}

	assert (pBuffer != 0);
	LinkLowEAPOLPayload (BMX_EAPOL_STAGE_LINK_RAW_DEQUEUE,
			     (const unsigned char *) pNetBuffer->GetPtr (),
			     pNetBuffer->GetLength ());
	memcpy (pBuffer, pNetBuffer->GetPtr (), pNetBuffer->GetLength ());

	delete pNetBuffer;

	return TRUE;
}

void CLinkLayer::FlushRawReceiveQueue (void)
{
	m_RawRxQueue.Flush ();
}

boolean CLinkLayer::EnableReceiveRaw (u16 nProtocolType)
{
	if (m_nRawProtocolType != 0)
	{
		return FALSE;
	}

	assert (nProtocolType != 0);
	m_nRawProtocolType = le2be16 (nProtocolType);

	return TRUE;
}

boolean CLinkLayer::IsRunning (void) const
{
	assert (m_pNetDevLayer != 0);
	return m_pNetDevLayer->IsRunning ();
}

boolean CLinkLayer::JoinLocalGroup (const CIPAddress &rGroupAddress)
{
	CMACAddress Group;
	Group.SetMulticast (rGroupAddress.Get ());

	unsigned j = MaxGroups;
	for (unsigned i = 0; i < MaxGroups; i++)
	{
		if (m_nMulticastUseCounter[i] == 0)
		{
			if (j == MaxGroups)
			{
				j = i;
			}

			continue;
		}

		if (m_MulticastGroup[i] == Group)
		{
			m_nMulticastUseCounter[i]++;

			return TRUE;
		}
	}

	if (j == MaxGroups)
	{
		return FALSE;
	}

	m_MulticastGroup[j].Set (Group.Get ());
	m_nMulticastUseCounter[j]++;

	return UpdateMulticastFilter ();
}

boolean CLinkLayer::LeaveLocalGroup (const CIPAddress &rGroupAddress)
{
	CMACAddress Group;
	Group.SetMulticast (rGroupAddress.Get ());

	for (unsigned i = 0; i < MaxGroups; i++)
	{
		if (   m_nMulticastUseCounter[i] > 0
		    && m_MulticastGroup[i] == Group)
		{
			if (--m_nMulticastUseCounter[i] > 0)
			{
				return TRUE;
			}

			return UpdateMulticastFilter ();
		}
	}

	return FALSE;
}

boolean CLinkLayer::UpdateMulticastFilter (void)
{
	u8 Groups[MaxGroups+1][MAC_ADDRESS_SIZE];
	memset (Groups, 0, sizeof Groups);

	for (unsigned i = 0; i < MaxGroups; i++)
	{
		if (m_nMulticastUseCounter[i] > 0)
		{
			m_MulticastGroup[i].CopyTo (Groups[i]);
		}
	}

	assert (m_pNetDevLayer != 0);
	return m_pNetDevLayer->SetMulticastFilter (Groups);
}

void CLinkLayer::ResolveFailed (CNetBuffer *pReturnedFrame)
{
	assert (pReturnedFrame != 0);
	pReturnedFrame->RemoveHeader (sizeof (TEthernetHeader));

	assert (m_pNetworkLayer != 0);
	m_pNetworkLayer->SendFailed (ICMP_CODE_DEST_HOST_UNREACH, pReturnedFrame);
}
