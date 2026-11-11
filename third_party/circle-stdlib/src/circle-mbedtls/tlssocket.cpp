//
// tlssocket.cpp
//
// Copyright (C) 2018  R. Stange <rsta2@o2online.de>
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
#include <circle-mbedtls/tlssocket.h>
#include <circle/net/dnsclient.h>
#include <circle/net/ipaddress.h>
#include <circle/net/in.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <mbedtls/net_sockets.h>
#include <assert.h>

using namespace CircleMbedTLS;

CTLSSocket::CTLSSocket (CNetSubSystem *pNetSubSystem, int nProtocol, boolean bExternalBIO)
:	CNetSocket (pNetSubSystem),
	m_pNetSubSystem (pNetSubSystem),
	m_pSocket (bExternalBIO ? 0 : new CAdaptedSocket (pNetSubSystem, nProtocol)),
	m_bConnected (FALSE),
	m_bExternalBIO (bExternalBIO),
	m_bBIOReady (FALSE),
	m_nReceiveTimeoutMs (DefaultReceiveTimeoutMs),
	m_nHandshakeTimeoutMs (DefaultHandshakeTimeoutMs),
	m_nOperationStartTicks (0),
	m_nOperationTimeoutTicks (0),
	m_bOperationBlocking (TRUE),
	m_bOperationTimeoutActive (FALSE)
{
	mbedtls_ssl_init (&m_Context);
}

CTLSSocket::~CTLSSocket (void)
{
	if (m_bConnected && !m_bExternalBIO)
	{
		mbedtls_ssl_close_notify (&m_Context);
	}

	mbedtls_ssl_free (&m_Context);

	delete m_pSocket;
	m_pSocket = 0;

	m_pNetSubSystem = 0;
}

int CTLSSocket::SetTimeouts (unsigned nReceiveTimeoutMs, unsigned nHandshakeTimeoutMs)
{
	if (   m_bConnected
	    || nReceiveTimeoutMs == 0
	    || nReceiveTimeoutMs > MaximumTimeoutMs
	    || nHandshakeTimeoutMs == 0
	    || nHandshakeTimeoutMs > MaximumTimeoutMs)
	{
		return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
	}

	m_nReceiveTimeoutMs = nReceiveTimeoutMs;
	m_nHandshakeTimeoutMs = nHandshakeTimeoutMs;

	return 0;
}

int CTLSSocket::Setup (const CTLSConfig &rConfig)
{
	assert (!m_bConnected);
	return mbedtls_ssl_setup (&m_Context, rConfig.Get ());
}

int CTLSSocket::SetHostname (const char *pHostname)
{
	assert (!m_bConnected);
	assert (pHostname != 0);
	return mbedtls_ssl_set_hostname (&m_Context, pHostname);
}

int CTLSSocket::Connect (const CIPAddress &rForeignIP, u16 nForeignPort)
{
	if (m_pSocket == 0)
	{
		return MBEDTLS_ERR_SSL_ALLOC_FAILED;
	}

	assert (!m_bConnected);

	int nResult = m_pSocket->Connect (rForeignIP, nForeignPort);
	if (nResult != 0)
	{
		return nResult;
	}

	mbedtls_ssl_set_bio (&m_Context, this, SendCallback, ReceiveCallback,
			     ReceiveTimeoutCallback);

	BeginOperationTimeout (m_nHandshakeTimeoutMs);
	while ((nResult = mbedtls_ssl_handshake (&m_Context)) != 0)
	{
		if (   nResult != MBEDTLS_ERR_SSL_WANT_READ
		    && nResult != MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			EndOperationTimeout ();
			return nResult;
		}

		if (OperationTimedOut ())
		{
			EndOperationTimeout ();
			return MBEDTLS_ERR_SSL_TIMEOUT;
		}
	}
	if (OperationTimedOut ())
	{
		EndOperationTimeout ();
		return MBEDTLS_ERR_SSL_TIMEOUT;
	}
	EndOperationTimeout ();

	m_bConnected = TRUE;

	u32 nFlags = mbedtls_ssl_get_verify_result (&m_Context);
	if (nFlags != 0)
	{
		char Info[512];
		mbedtls_x509_crt_verify_info (Info, sizeof Info, "x509: ", nFlags);

		CLogger::Get ()->Write ("tlssocket", LogWarning, Info);

		return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
	}

	return 0;
}

int CTLSSocket::Connect (const char *pHost, const char *pPort)
{
	assert (pHost != 0);
	assert (pPort != 0);

	char *pEnd = 0;
	unsigned long ulPort = strtoul (pPort, &pEnd, 10);
	if (   (pEnd != 0 && *pEnd != '\0')
	    || (ulPort == 0 || ulPort > 0xFFFF))
	{
		return MBEDTLS_ERR_NET_CONNECT_FAILED;
	}

	assert (m_pNetSubSystem != 0);
	CIPAddress IPAddress;
	CDNSClient DNSClient (m_pNetSubSystem);
	if (!DNSClient.Resolve (pHost, &IPAddress))
	{
		return MBEDTLS_ERR_NET_UNKNOWN_HOST;
	}

	return Connect (IPAddress, static_cast<u16> (ulPort));
}

int CTLSSocket::Receive (void *pBuffer, unsigned nLength, int nFlags)
{
	return ReceiveWithTimeout (pBuffer, nLength, nFlags, m_nReceiveTimeoutMs);
}

int CTLSSocket::ReceiveWithTimeout (void *pBuffer, unsigned nLength, int nFlags,
				    unsigned nTimeoutMs)
{
	assert (m_pSocket != 0);
	boolean bBlocking = nFlags != MSG_DONTWAIT ? TRUE : FALSE;
	if (   bBlocking
	    && (nTimeoutMs == 0 || nTimeoutMs > MaximumTimeoutMs))
	{
		return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
	}
	m_pSocket->SetOptionBlocking (bBlocking);
	m_bOperationBlocking = bBlocking;
	if (bBlocking)
	{
		BeginOperationTimeout (nTimeoutMs);
	}

	assert (m_bConnected);
	int nResult = mbedtls_ssl_read (&m_Context, reinterpret_cast<u8 *> (pBuffer), nLength);
	boolean bTimedOut = bBlocking && OperationTimedOut ();
	if (bBlocking)
	{
		EndOperationTimeout ();
	}
	if (bTimedOut)
	{
		return MBEDTLS_ERR_SSL_TIMEOUT;
	}
	switch (nResult)
	{
	case MBEDTLS_ERR_SSL_WANT_READ:
	case MBEDTLS_ERR_SSL_WANT_WRITE:
		assert (nFlags == MSG_DONTWAIT);
		nResult = 0;
		break;

	case 0:			// EOF
		nResult = -1;
		break;
	}

	return nResult;
}

int CTLSSocket::Send (const void *pBuffer, unsigned nLength, int nFlags)
{
	assert (m_bConnected);
	return mbedtls_ssl_write (&m_Context, reinterpret_cast<const u8 *> (pBuffer), nLength);
}

const u8 *CTLSSocket::GetForeignIP (void) const
{
	if (m_pSocket == 0)
	{
		return 0;
	}

	return m_pSocket->GetForeignIP ();
}

void CTLSSocket::BeginOperationTimeout (unsigned nTimeoutMs)
{
	assert (nTimeoutMs > 0);
	assert (nTimeoutMs <= MaximumTimeoutMs);

	m_nOperationStartTicks = CTimer::GetClockTicks64 ();
	m_nOperationTimeoutTicks = static_cast<u64> (nTimeoutMs) * CLOCKHZ / 1000;
	m_bOperationTimeoutActive = TRUE;
}

void CTLSSocket::EndOperationTimeout (void)
{
	m_bOperationTimeoutActive = FALSE;
	m_nOperationStartTicks = 0;
	m_nOperationTimeoutTicks = 0;
}

boolean CTLSSocket::OperationTimedOut (void) const
{
	if (!m_bOperationTimeoutActive)
	{
		return TRUE;
	}

	u64 nElapsedTicks = CTimer::GetClockTicks64 ()-m_nOperationStartTicks;
	return nElapsedTicks >= m_nOperationTimeoutTicks ? TRUE : FALSE;
}

unsigned CTLSSocket::GetRemainingOperationTimeoutMs (void) const
{
	if (OperationTimedOut ())
	{
		return 0;
	}

	u64 nElapsedTicks = CTimer::GetClockTicks64 ()-m_nOperationStartTicks;
	u64 nRemainingTicks = m_nOperationTimeoutTicks-nElapsedTicks;
	u64 nTicksPerMs = CLOCKHZ / 1000;
	u64 nRemainingMs = (nRemainingTicks+nTicksPerMs-1)/nTicksPerMs;
	assert (nRemainingMs > 0);
	assert (nRemainingMs <= MaximumTimeoutMs);

	return static_cast<unsigned> (nRemainingMs);
}

int CTLSSocket::ReceiveTransport (u8 *pBuffer, size_t nLength, uint32_t nTimeoutMs)
{
	assert (m_pSocket != 0);
	if (!m_bOperationBlocking)
	{
		return m_pSocket->Receive (pBuffer, nLength);
	}

	unsigned nRemainingMs = GetRemainingOperationTimeoutMs ();
	if (nRemainingMs == 0)
	{
		return MBEDTLS_ERR_SSL_TIMEOUT;
	}
	if (nTimeoutMs != 0 && nTimeoutMs < nRemainingMs)
	{
		nRemainingMs = nTimeoutMs;
	}

	return m_pSocket->ReceiveTimeout (pBuffer, nLength, nRemainingMs);
}

int CTLSSocket::ReceiveCallback (void *pParam, u8 *pBuffer, size_t nLength)
{
	CTLSSocket *pSocket = reinterpret_cast<CTLSSocket *> (pParam);
	assert (pSocket != 0);
	return pSocket->ReceiveTransport (pBuffer, nLength, 0);
}

int CTLSSocket::ReceiveTimeoutCallback (void *pParam, u8 *pBuffer, size_t nLength,
					uint32_t nTimeoutMs)
{
	CTLSSocket *pSocket = reinterpret_cast<CTLSSocket *> (pParam);
	assert (pSocket != 0);
	return pSocket->ReceiveTransport (pBuffer, nLength, nTimeoutMs);
}

int CTLSSocket::SendCallback (void *pParam, const u8 *pBuffer, size_t nLength)
{
	CTLSSocket *pTLSSocket = reinterpret_cast<CTLSSocket *> (pParam);
	assert (pTLSSocket != 0);
	CAdaptedSocket *pSocket = pTLSSocket->m_pSocket;
	assert (pSocket != 0);
	return pSocket->Send (pBuffer, nLength);
}

int CTLSSocket::SetExternalBIO (void *pContext, mbedtls_ssl_send_t *pSend,
			       mbedtls_ssl_recv_t *pReceive)
{
	if (!m_bExternalBIO || m_bConnected || m_bBIOReady || !pSend || !pReceive)
		return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
	mbedtls_ssl_set_bio (&m_Context, pContext, pSend, pReceive, 0);
	m_bBIOReady = TRUE;
	return 0;
}

int CTLSSocket::HandshakeSome (void)
{
	if (!m_bExternalBIO || !m_bBIOReady) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
	if (m_bConnected) return 0;
	int nResult = mbedtls_ssl_handshake (&m_Context);
	if (nResult != 0) return nResult;
	if (mbedtls_ssl_get_verify_result (&m_Context) != 0)
		return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
	m_bConnected = TRUE;
	return 0;
}

int CTLSSocket::ReadSome (void *pBuffer, size_t nLength)
{
	if (!m_bExternalBIO || !m_bConnected) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
	return mbedtls_ssl_read (&m_Context, static_cast<u8 *> (pBuffer), nLength);
}

int CTLSSocket::WriteSome (const void *pBuffer, size_t nLength)
{
	if (!m_bExternalBIO || !m_bConnected) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
	return mbedtls_ssl_write (&m_Context, static_cast<const u8 *> (pBuffer), nLength);
}
