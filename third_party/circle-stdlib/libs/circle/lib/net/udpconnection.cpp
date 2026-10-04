//
// udpconnection.cpp
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
#include <circle/net/udpconnection.h>
#include <circle/net/error.h>
#include <circle/net/sizes.h>
#include <circle/net/in.h>
#ifdef BMC64_WLAN_TRACE
#include <circle/logger.h>
#include <circle/string.h>
#endif
#include <circle/macros.h>
#include <circle/util.h>
#include <assert.h>

struct TUDPHeader
{
	u16 	nSourcePort;
	u16 	nDestPort;
	u16 	nLength;
	u16 	nChecksum;
#define UDP_CHECKSUM_NONE	0
}
PACKED;

ASSERT_STATIC (sizeof (TUDPHeader) == UDP_HEADER_LEN);

struct TUDPPrivateData
{
	u8	SourceAddress[IP_ADDRESS_SIZE];
	u16	nSourcePort;
};

#ifdef BMC64_WLAN_TRACE
static boolean UDPTraceIsDHCPPort (u16 nSourcePort, u16 nDestPort)
{
	return    nSourcePort == 67
	       || nSourcePort == 68
	       || nDestPort == 67
	       || nDestPort == 68;
}

static void UDPTraceDHCP (const char *pStage, const CIPAddress &rSenderIP,
			  const CIPAddress &rReceiverIP, u16 nSourcePort,
			  u16 nDestPort, size_t nLength, u16 nOwnPort,
			  u16 nForeignPort, boolean bActiveOpen)
{
	if (!UDPTraceIsDHCPPort (nSourcePort, nDestPort))
	{
		return;
	}

	CString SenderString;
	CString ReceiverString;
	rSenderIP.Format (&SenderString);
	rReceiverIP.Format (&ReceiverString);
	CLogger::Get ()->Write ("bmx-udp", LogNotice,
				"%s dhcp src=%s:%u dst=%s:%u len=%u own=%u foreign=%u active=%u",
				pStage, (const char *) SenderString, nSourcePort,
				(const char *) ReceiverString, nDestPort,
				(unsigned) nLength, nOwnPort, nForeignPort,
				bActiveOpen ? 1 : 0);
}
#else
#define UDPTraceDHCP(_stage, _sender, _receiver, _source_port, _dest_port, _length, _own_port, _foreign_port, _active)
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum
{
	BMX_DHCP_STAGE_UDP_ENTER = 21,
	BMX_DHCP_STAGE_UDP_DROP_OWNPORT = 22,
	BMX_DHCP_STAGE_UDP_DROP_FOREIGNPORT = 23,
	BMX_DHCP_STAGE_UDP_DROP_FOREIGNIP = 24,
	BMX_DHCP_STAGE_UDP_DROP_LENGTH = 25,
	BMX_DHCP_STAGE_UDP_DROP_CHECKSUM = 26,
	BMX_DHCP_STAGE_UDP_DROP_BCAST = 27,
	BMX_DHCP_STAGE_UDP_ENQUEUE = 28,
	BMX_DHCP_STAGE_UDP_DEQUEUE = 29
};

enum
{
	BMX_DHCP_REASON_NONE = 0,
	BMX_DHCP_REASON_SHORT = 1
};

extern "C" void bmx_dhcp_low_udp (unsigned nStage, const unsigned char *pUDP,
				  unsigned nLength, const unsigned char *pSourceIP,
				  const unsigned char *pDestIP, unsigned nReason);
extern "C" void bmx_dhcp_low_payload (unsigned nStage, const unsigned char *pPayload,
				      unsigned nLength, const unsigned char *pSourceIP,
				      unsigned nSourcePort, unsigned nDestPort,
				      unsigned nReason);

#define UDPLowDHCPPacket(_stage, _packet, _length, _sender, _receiver, _reason) \
	bmx_dhcp_low_udp ((_stage), (const unsigned char *) (_packet), (_length), \
			  (_sender).Get (), (_receiver).Get (), (_reason))
#define UDPLowDHCPPayload(_stage, _payload, _length, _source, _sport, _dport, _reason) \
	bmx_dhcp_low_payload ((_stage), (const unsigned char *) (_payload), (_length), \
			      (_source), (_sport), (_dport), (_reason))
#else
#define UDPLowDHCPPacket(_stage, _packet, _length, _sender, _receiver, _reason)
#define UDPLowDHCPPayload(_stage, _payload, _length, _source, _sport, _dport, _reason)
#endif

#define UDP_CONFIG_MSS	(ETH_MAX_LEN - ETH_HEADER_LEN - IP_HEADER_LEN - UDP_HEADER_LEN)

CUDPConnection::CUDPConnection (CNetConfig	*pNetConfig,
				CNetworkLayer	*pNetworkLayer,
				const CIPAddress &rForeignIP,
				u16		 nForeignPort,
				u16		 nOwnPort)
:	CNetConnection (pNetConfig, pNetworkLayer, rForeignIP, nForeignPort, nOwnPort, IPPROTO_UDP),
	m_bOpen (TRUE),
	m_bActiveOpen (TRUE),
	m_RxQueue (TRUE),
	m_nReceiveTimeout (0),
	m_bBroadcastsAllowed (FALSE),
	m_pHostGroup (0),
	m_nErrno (0)
{
	m_nMSS = UDP_CONFIG_MSS;
}

CUDPConnection::CUDPConnection (CNetConfig	*pNetConfig,
				CNetworkLayer	*pNetworkLayer,
				u16		 nOwnPort)
:	CNetConnection (pNetConfig, pNetworkLayer, nOwnPort, IPPROTO_UDP),
	m_bOpen (TRUE),
	m_bActiveOpen (FALSE),
	m_RxQueue (TRUE),
	m_nReceiveTimeout (0),
	m_bBroadcastsAllowed (FALSE),
	m_pHostGroup (0),
	m_nErrno (0)
{
	m_nMSS = UDP_CONFIG_MSS;
}

CUDPConnection::~CUDPConnection (void)
{
	assert (!m_bOpen);
	assert (!m_pHostGroup);
}

int CUDPConnection::Connect (void)
{
	assert (m_bOpen);

	return 0;
}

int CUDPConnection::Accept (CIPAddress *pForeignIP, u16 *pForeignPort)
{
	return -NET_ERROR_OPERATION_NOT_SUPPORTED;
}

int CUDPConnection::Close (void)
{
	if (!m_bOpen)
	{
		return -NET_ERROR_NOT_CONNECTED;
	}

	if (m_pHostGroup != 0)
	{
		assert (m_pNetworkLayer != 0);
		m_pNetworkLayer->LeaveHostGroup (*m_pHostGroup);

		delete m_pHostGroup;
		m_pHostGroup = 0;
	}

	m_bOpen = FALSE;

	return 0;
}

int CUDPConnection::Send (CNetBuffer *pNetBuffer, int nFlags)
{
	if (m_nErrno < 0)
	{
		int nErrno = m_nErrno;
		m_nErrno = 0;

		return nErrno;
	}

	if (!m_bActiveOpen)
	{
		return -NET_ERROR_OPERATION_NOT_SUPPORTED;
	}

	nFlags &= ~MSG_MORE;
	if (   nFlags != 0
	    && nFlags != MSG_DONTWAIT)
	{
		return -NET_ERROR_INVALID_VALUE;
	}

	assert (pNetBuffer != 0);
	int nLength = pNetBuffer->GetLength ();
	unsigned nPacketLength = sizeof (TUDPHeader) + nLength;		// may wrap
	if (   nPacketLength <= sizeof (TUDPHeader)
	    || nPacketLength > FRAME_BUFFER_SIZE)
	{
		return -NET_ERROR_INVALID_VALUE;
	}

	assert (m_pNetConfig != 0);
	if (   !m_bBroadcastsAllowed
	    && (   m_ForeignIP.IsBroadcast ()
	        || m_ForeignIP == *m_pNetConfig->GetBroadcastAddress ()))
	{
		return -NET_ERROR_PERMISSION_DENIED;
	}

	TUDPHeader *pHeader = (TUDPHeader *) pNetBuffer->AddHeader (sizeof (TUDPHeader));
	assert (pHeader != 0);

	pHeader->nSourcePort = le2be16 (m_nOwnPort);
	pHeader->nDestPort   = le2be16 (m_nForeignPort);
	pHeader->nLength     = le2be16 (nPacketLength);
	pHeader->nChecksum   = 0;
	
	m_Checksum.SetSourceAddress (*m_pNetConfig->GetIPAddress ());
	m_Checksum.SetDestinationAddress (m_ForeignIP);
	pHeader->nChecksum = m_Checksum.Calculate (pHeader, nPacketLength);

	assert (m_pNetworkLayer != 0);
	boolean bOK = m_pNetworkLayer->Send (m_ForeignIP, pNetBuffer, IPPROTO_UDP);
	
	return bOK ? nLength : -NET_ERROR_IO;
}

int CUDPConnection::Receive (CNetBuffer **ppNetBuffer, int nFlags)
{
	nFlags &= ~MSG_MORE;

	assert (ppNetBuffer != 0);
	do
	{
		if (m_nErrno < 0)
		{
			int nErrno = m_nErrno;
			m_nErrno = 0;

			return nErrno;
		}

		*ppNetBuffer = m_RxQueue.Dequeue ();
		if (*ppNetBuffer == 0)
		{
			if (nFlags == MSG_DONTWAIT)
			{
				return 0;
			}

			m_Event.Clear ();

			if (m_nReceiveTimeout == 0)
			{
				m_Event.Wait ();
			}
			else
			{
				if (m_Event.WaitWithTimeout (m_nReceiveTimeout))
				{
					m_nErrno = -1;
				}
			}

			if (m_nErrno < 0)
			{
				int nErrno = m_nErrno;
				m_nErrno = 0;

				return nErrno;
			}
		}
	}
	while (*ppNetBuffer == 0);

	size_t nLength = (*ppNetBuffer)->GetLength ();
	assert (nLength <= FRAME_BUFFER_SIZE);
	TUDPPrivateData *pData = (TUDPPrivateData *) (*ppNetBuffer)->GetPrivateData ();
	if (pData != 0)
	{
		UDPLowDHCPPayload (BMX_DHCP_STAGE_UDP_DEQUEUE, (*ppNetBuffer)->GetPtr (),
				   nLength, pData->SourceAddress, pData->nSourcePort,
				   m_nOwnPort, BMX_DHCP_REASON_NONE);
	}

	return nLength;
}

int CUDPConnection::SendTo (CNetBuffer *pNetBuffer, int nFlags,
			    const CIPAddress &rForeignIP, u16 nForeignPort)
{
	if (m_nErrno < 0)
	{
		int nErrno = m_nErrno;
		m_nErrno = 0;

		return nErrno;
	}

	if (m_bActiveOpen)
	{
		// ignore rForeignIP and nForeignPort
		return Send (pNetBuffer, nFlags);
	}

	nFlags &= ~MSG_MORE;
	if (   nFlags != 0
	    && nFlags != MSG_DONTWAIT)
	{
		return -NET_ERROR_INVALID_VALUE;
	}

	assert (pNetBuffer != 0);
	int nLength = pNetBuffer->GetLength ();
	unsigned nPacketLength = sizeof (TUDPHeader) + nLength;		// may wrap
	if (   nPacketLength <= sizeof (TUDPHeader)
	    || nPacketLength > FRAME_BUFFER_SIZE)
	{
		return -NET_ERROR_INVALID_VALUE;
	}

	assert (m_pNetConfig != 0);
	if (   !m_bBroadcastsAllowed
	    && (   rForeignIP.IsBroadcast ()
	        || rForeignIP == *m_pNetConfig->GetBroadcastAddress ()))
	{
		return -NET_ERROR_PERMISSION_DENIED;
	}

	TUDPHeader *pHeader = (TUDPHeader *) pNetBuffer->AddHeader (sizeof (TUDPHeader));
	assert (pHeader != 0);

	pHeader->nSourcePort = le2be16 (m_nOwnPort);
	pHeader->nDestPort   = le2be16 (nForeignPort);
	pHeader->nLength     = le2be16 (nPacketLength);
	pHeader->nChecksum   = 0;
	
	m_Checksum.SetSourceAddress (*m_pNetConfig->GetIPAddress ());
	m_Checksum.SetDestinationAddress (rForeignIP);
	pHeader->nChecksum = m_Checksum.Calculate (pHeader, nPacketLength);

	assert (m_pNetworkLayer != 0);
	boolean bOK = m_pNetworkLayer->Send (rForeignIP, pNetBuffer, IPPROTO_UDP);
	
	return bOK ? nLength : -NET_ERROR_IO;
}

int CUDPConnection::ReceiveFrom (CNetBuffer **ppNetBuffer, int nFlags,
				 CIPAddress *pForeignIP, u16 *pForeignPort)
{
	nFlags &= ~MSG_MORE;

	assert (ppNetBuffer != 0);
	do
	{
		if (m_nErrno < 0)
		{
			int nErrno = m_nErrno;
			m_nErrno = 0;

			return nErrno;
		}

		*ppNetBuffer = m_RxQueue.Dequeue ();
		if (*ppNetBuffer == 0)
		{
			if (nFlags == MSG_DONTWAIT)
			{
				return 0;
			}

			m_Event.Clear ();

			if (m_nReceiveTimeout == 0)
			{
				m_Event.Wait ();
			}
			else
			{
				if (m_Event.WaitWithTimeout (m_nReceiveTimeout))
				{
					m_nErrno = -1;
				}
			}

			if (m_nErrno < 0)
			{
				int nErrno = m_nErrno;
				m_nErrno = 0;

				return nErrno;
			}
		}
	}
	while (*ppNetBuffer == 0);

	size_t nLength = (*ppNetBuffer)->GetLength ();
	assert (nLength <= FRAME_BUFFER_SIZE);

	TUDPPrivateData *pData = (TUDPPrivateData *) (*ppNetBuffer)->GetPrivateData ();
	assert (pData != 0);
	if (   pForeignIP != 0
	    && pForeignPort != 0)
	{
		pForeignIP->Set (pData->SourceAddress);
		*pForeignPort = pData->nSourcePort;
	}

	return nLength;
}

int CUDPConnection::SetOptionReceiveTimeout (unsigned nMicroSeconds)
{
	m_nReceiveTimeout = nMicroSeconds;

	return 0;
}

int CUDPConnection::SetOptionSendTimeout (unsigned nMicroSeconds)
{
	return 0;
}

int CUDPConnection::SetOptionBroadcast (boolean bAllowed)
{
	m_bBroadcastsAllowed = bAllowed;

	return 0;
}

int CUDPConnection::SetOptionAddMembership (const CIPAddress &rGroupAddress)
{
	if (m_pHostGroup != 0)
	{
		return -NET_ERROR_IS_CONNECTED;
	}

	if (!rGroupAddress.IsMulticast ())
	{
		return -NET_ERROR_INVALID_VALUE;
	}

	assert (m_pNetworkLayer != 0);
	if (!m_pNetworkLayer->JoinHostGroup (rGroupAddress))
	{
		return -NET_ERROR_IO;
	}

	m_pHostGroup = new CIPAddress (rGroupAddress);
	assert (m_pHostGroup != 0);

	return 0;
}

int CUDPConnection::SetOptionDropMembership (const CIPAddress &rGroupAddress)
{
	if (m_pHostGroup == 0)
	{
		return -NET_ERROR_NOT_CONNECTED;
	}

	if (*m_pHostGroup != rGroupAddress)
	{
		return -NET_ERROR_INVALID_VALUE;
	}

	delete m_pHostGroup;
	m_pHostGroup = 0;

	assert (m_pNetworkLayer != 0);
#ifndef NDEBUG
	boolean bOK =
#endif
		m_pNetworkLayer->LeaveHostGroup (rGroupAddress);
	assert (bOK);

	return 0;
}

boolean CUDPConnection::IsConnected (void) const
{
	return FALSE;
}

boolean CUDPConnection::IsTerminated (void) const
{
	return !m_bOpen;
}
	
void CUDPConnection::Process (void)
{
}

int CUDPConnection::PacketReceived (CNetBuffer *pPacket,
				    CIPAddress &rSenderIP, CIPAddress &rReceiverIP, int nProtocol)
{
	if (nProtocol != IPPROTO_UDP)
	{
		return 0;
	}

	assert (pPacket != 0);
	size_t nLength = pPacket->GetLength ();
	if (nLength <= sizeof (TUDPHeader))
	{
		delete pPacket;

		return -1;
	}
	TUDPHeader *pHeader = (TUDPHeader *) pPacket->GetPtr ();
	assert (pHeader != 0);

	u16 nSourcePort = be2le16 (pHeader->nSourcePort);
	u16 nDestPort = be2le16 (pHeader->nDestPort);
	UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_ENTER, pHeader, nLength, rSenderIP,
			  rReceiverIP, BMX_DHCP_REASON_NONE);

	if (m_nOwnPort != nDestPort)
	{
		UDPTraceDHCP ("skip-port", rSenderIP, rReceiverIP, nSourcePort, nDestPort,
			      nLength, m_nOwnPort, m_nForeignPort, m_bActiveOpen);
		UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_DROP_OWNPORT, pHeader, nLength,
				  rSenderIP, rReceiverIP, BMX_DHCP_REASON_NONE);
		return 0;
	}

	if (rSenderIP.IsMulticast ())
	{
		UDPTraceDHCP ("drop-multicast-source", rSenderIP, rReceiverIP,
			      nSourcePort, nDestPort, nLength, m_nOwnPort,
			      m_nForeignPort, m_bActiveOpen);
		delete pPacket;

		return -1;
	}

	assert (m_pNetConfig != 0);

	if (m_bActiveOpen)
	{
		if (m_nForeignPort != nSourcePort)
		{
			UDPTraceDHCP ("skip-foreign-port", rSenderIP, rReceiverIP,
				      nSourcePort, nDestPort, nLength, m_nOwnPort,
				      m_nForeignPort, m_bActiveOpen);
			UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_DROP_FOREIGNPORT, pHeader,
					  nLength, rSenderIP, rReceiverIP,
					  BMX_DHCP_REASON_NONE);
			return 0;
		}

		if (   m_ForeignIP != rSenderIP
		    && !m_ForeignIP.IsMulticast ()
		    && !m_ForeignIP.IsBroadcast ()
		    && m_ForeignIP != *m_pNetConfig->GetBroadcastAddress ())
		{
			UDPTraceDHCP ("skip-foreign-ip", rSenderIP, rReceiverIP,
				      nSourcePort, nDestPort, nLength, m_nOwnPort,
				      m_nForeignPort, m_bActiveOpen);
			UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_DROP_FOREIGNIP, pHeader,
					  nLength, rSenderIP, rReceiverIP,
					  BMX_DHCP_REASON_NONE);
			return 0;
		}
	}

	if (nLength < be2le16 (pHeader->nLength))
	{
		UDPTraceDHCP ("drop-truncated", rSenderIP, rReceiverIP,
			      nSourcePort, nDestPort, nLength, m_nOwnPort,
			      m_nForeignPort, m_bActiveOpen);
		UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_DROP_LENGTH, pHeader, nLength,
				  rSenderIP, rReceiverIP, BMX_DHCP_REASON_SHORT);
		delete pPacket;

		return -1;
	}
	
	if (pHeader->nChecksum != UDP_CHECKSUM_NONE)
	{
		m_Checksum.SetSourceAddress (rSenderIP);
		m_Checksum.SetDestinationAddress (rReceiverIP);

		if (m_Checksum.Calculate (pHeader, nLength) != CHECKSUM_OK)
		{
			UDPTraceDHCP ("drop-checksum", rSenderIP, rReceiverIP,
				      nSourcePort, nDestPort, nLength, m_nOwnPort,
				      m_nForeignPort, m_bActiveOpen);
			UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_DROP_CHECKSUM, pHeader,
					  nLength, rSenderIP, rReceiverIP,
					  BMX_DHCP_REASON_NONE);
			delete pPacket;

			return -1;
		}
	}

	if (   !m_bBroadcastsAllowed
	    && (   rReceiverIP.IsBroadcast ()
	        || rReceiverIP == *m_pNetConfig->GetBroadcastAddress ()))
	{
		UDPTraceDHCP ("drop-broadcast-disabled", rSenderIP, rReceiverIP,
			      nSourcePort, nDestPort, nLength, m_nOwnPort,
			      m_nForeignPort, m_bActiveOpen);
		UDPLowDHCPPacket (BMX_DHCP_STAGE_UDP_DROP_BCAST, pHeader, nLength,
				  rSenderIP, rReceiverIP, BMX_DHCP_REASON_NONE);
		delete pPacket;

		return 1;
	}

	if (   rReceiverIP.IsMulticast ()
	    && (   m_pHostGroup == 0
	        || *m_pHostGroup != rReceiverIP))
	{
		UDPTraceDHCP ("skip-multicast-group", rSenderIP, rReceiverIP,
			      nSourcePort, nDestPort, nLength, m_nOwnPort,
			      m_nForeignPort, m_bActiveOpen);
		return 0;
	}

	UDPTraceDHCP ("accept", rSenderIP, rReceiverIP, nSourcePort, nDestPort,
		      nLength, m_nOwnPort, m_nForeignPort, m_bActiveOpen);

	pPacket->RemoveHeader (sizeof (TUDPHeader));

	TUDPPrivateData Data;
	rSenderIP.CopyTo (Data.SourceAddress);
	Data.nSourcePort = nSourcePort;
	pPacket->SetPrivateData (&Data, sizeof Data);
	UDPLowDHCPPayload (BMX_DHCP_STAGE_UDP_ENQUEUE, pPacket->GetPtr (),
			   pPacket->GetLength (), Data.SourceAddress,
			   nSourcePort, m_nOwnPort, BMX_DHCP_REASON_NONE);

	m_RxQueue.Enqueue (pPacket);

	m_Event.Set ();

	return 1;
}

int CUDPConnection::NotificationReceived (TICMPNotificationType  Type,
					  CIPAddress		&rSenderIP,
					  CIPAddress		&rReceiverIP,
					  u16			 nSendPort,
					  u16			 nReceivePort,
					  int			 nProtocol)
{
	if (nProtocol != IPPROTO_UDP)
	{
		return 0;
	}

	if (m_nOwnPort != nReceivePort)
	{
		return 0;
	}

	assert (m_pNetConfig != 0);
	if (rReceiverIP != *m_pNetConfig->GetIPAddress ())
	{
		return 0;
	}

	if (m_bActiveOpen)
	{
		if (m_nForeignPort != nSendPort)
		{
			return 0;
		}

		if (m_ForeignIP != rSenderIP)
		{
			return 0;
		}
	}

	m_nErrno =   Type == ICMPNotificationDestUnreach
		   ? -NET_ERROR_DESTINATION_UNREACHABLE
		   : -NET_ERROR_PROTOCOL_ERROR;

	m_Event.Set ();

	return 1;
}

CNetConnection::TStatus CUDPConnection::GetStatus (void) const
{
	TStatus Status = {FALSE, FALSE, FALSE, FALSE};

	if (!m_bOpen)
	{
		return Status;
	}

	Status.bConnected = TRUE;

	if (   m_nErrno < 0
	    || !m_RxQueue.IsEmpty ())
	{
		Status.bRxReady = TRUE;
	}

	Status.bTxReady = TRUE;

	return Status;
}
