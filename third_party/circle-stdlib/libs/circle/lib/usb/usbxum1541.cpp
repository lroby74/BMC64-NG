//

//

//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
#include <circle/usb/usbxum1541.h>
#include <circle/usb/usbhostcontroller.h>
#if RASPPI >= 4
#include <circle/usb/xhciendpoint.h>
#endif
#include <circle/timer.h>
#include <circle/usb/usbrequest.h>
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <assert.h>

CNumberPool CUSBXum1541Device::s_DeviceNumberPool (1);




#define XUM1541_PATIENCE_TICKS	(15 * HZ)

static const char FromXum1541[] = "uxum";
static const char DevicePrefix[] = "uxum";


static const TUSBDeviceID s_DeviceIDTable[] =
{
	{ USB_DEVICE (0x16d0, 0x0504) },
	{ 0, 0 }
};

CUSBXum1541Device::CUSBXum1541Device (CUSBFunction *pFunction)
:	CUSBFunction (pFunction),
	m_pEndpointIn (0),
	m_pEndpointOut (0),
	m_nDeviceNumber (0)
{
}

CUSBXum1541Device::~CUSBXum1541Device (void)
{
	if (m_nDeviceNumber != 0)
	{
		CDeviceNameService::Get ()->RemoveDevice (DevicePrefix, m_nDeviceNumber, FALSE);

		s_DeviceNumberPool.FreeNumber (m_nDeviceNumber);
	}

	delete m_pEndpointOut;
	m_pEndpointOut = 0;

	delete m_pEndpointIn;
	m_pEndpointIn = 0;
}

boolean CUSBXum1541Device::Configure (void)
{
	if (GetNumEndpoints () < 2)
	{
		ConfigurationError (FromXum1541);

		return FALSE;
	}

	const TUSBEndpointDescriptor *pEndpointDesc;
	while ((pEndpointDesc = (TUSBEndpointDescriptor *) GetDescriptor (DESCRIPTOR_ENDPOINT)) != 0)
	{
		if ((pEndpointDesc->bmAttributes & 0x3F) != 0x02)
		{
			continue;
		}

		if ((pEndpointDesc->bEndpointAddress & 0x80) == 0x80)
		{
			if (m_pEndpointIn == 0)
			{
				m_pEndpointIn = new CUSBEndpoint (GetDevice (), pEndpointDesc);
			}
		}
		else
		{
			if (m_pEndpointOut == 0)
			{
				m_pEndpointOut = new CUSBEndpoint (GetDevice (), pEndpointDesc);
			}
		}
	}

	if (   m_pEndpointIn == 0
	    || m_pEndpointOut == 0)
	{
		ConfigurationError (FromXum1541);

		return FALSE;
	}

	if (!CUSBFunction::Configure ())
	{
		CLogger::Get ()->Write (FromXum1541, LogError, "Cannot set interface");

		return FALSE;
	}

	assert (m_nDeviceNumber == 0);
	m_nDeviceNumber = s_DeviceNumberPool.AllocateNumber (TRUE, FromXum1541);

	CDeviceNameService::Get ()->AddDevice (DevicePrefix, m_nDeviceNumber, this, FALSE);








	SetPatience (m_pEndpointIn);
	SetPatience (m_pEndpointOut);

	CLogger::Get ()->Write (FromXum1541, LogNotice, "xum1541 (ZoomFloppy) ready");

	return TRUE;
}

void CUSBXum1541Device::SetPatience (CUSBEndpoint *pEndpoint)
{
#if RASPPI >= 4
	if (pEndpoint != 0)
	{
		CXHCIEndpoint *pXHCI = pEndpoint->GetXHCIEndpoint ();
		if (pXHCI != 0)
		{
			pXHCI->SetTimeoutTicks (XUM1541_PATIENCE_TICKS);
		}
	}
#else
	(void) pEndpoint;
#endif
}

int CUSBXum1541Device::ControlIn (u8 ucRequest, void *pBuffer, u16 usLength)
{
	CUSBHostController *pHost = GetHost ();
	assert (pHost != 0);


	return pHost->ControlMessage (GetEndpoint0 (),
				      REQUEST_IN | REQUEST_CLASS,
				      ucRequest, 0, 0, pBuffer, usLength);
}

int CUSBXum1541Device::ControlOut (u8 ucRequest)
{
	CUSBHostController *pHost = GetHost ();
	assert (pHost != 0);


	return pHost->ControlMessage (GetEndpoint0 (),
				      REQUEST_OUT | REQUEST_CLASS,
				      ucRequest, 0, 0, 0, 0);
}

void CUSBXum1541Device::ClearHalts (void)
{
	CUSBHostController *pHost = GetHost ();
	assert (pHost != 0);




	static const u8 ClearFeature = 1;
	static const u16 EndpointHalt = 0;

	if (m_pEndpointOut != 0)
	{
		pHost->ControlMessage (GetEndpoint0 (), 0x02, ClearFeature,
				       EndpointHalt,
				       m_pEndpointOut->GetNumber (), 0, 0);
		m_pEndpointOut->ResetPID ();
	}

	if (m_pEndpointIn != 0)
	{
		pHost->ControlMessage (GetEndpoint0 (), 0x02, ClearFeature,
				       EndpointHalt,
				       m_pEndpointIn->GetNumber () | 0x80, 0, 0);
		m_pEndpointIn->ResetPID ();
	}
}



//

//



//







int CUSBXum1541Device::BulkOut (const void *pBuffer, unsigned nCount,
				unsigned nTimeoutMs)
{
	CUSBHostController *pHost = GetHost ();
	assert (pHost != 0);

	if (   m_pEndpointOut == 0
	    || pBuffer == 0
	    || nCount == 0)
	{
		return -1;
	}

	(void) nTimeoutMs;

	return pHost->Transfer (m_pEndpointOut, (void *) pBuffer, nCount,
				USB_TIMEOUT_NONE);
}

int CUSBXum1541Device::BulkIn (void *pBuffer, unsigned nCount, unsigned nTimeoutMs)
{
	CUSBHostController *pHost = GetHost ();
	assert (pHost != 0);

	if (   m_pEndpointIn == 0
	    || pBuffer == 0
	    || nCount == 0)
	{
		return -1;
	}

	(void) nTimeoutMs;

	return pHost->Transfer (m_pEndpointIn, pBuffer, nCount,
				USB_TIMEOUT_NONE);
}

void CUSBXum1541Device::SetIrqDelay (unsigned nIMODI)
{
#if RASPPI >= 4
	if (m_pEndpointIn != 0)
	{
		CXHCIEndpoint *pXHCI = m_pEndpointIn->GetXHCIEndpoint ();
		if (pXHCI != 0)
		{
			pXHCI->SetInterruptModeration (nIMODI);
		}
	}
#else
	(void) nIMODI;
#endif
}

unsigned CUSBXum1541Device::GetIrqDelay (void)
{
#if RASPPI >= 4
	if (m_pEndpointIn != 0)
	{
		CXHCIEndpoint *pXHCI = m_pEndpointIn->GetXHCIEndpoint ();
		if (pXHCI != 0)
		{
			return pXHCI->GetInterruptModeration ();
		}
	}
#endif
	return 0;
}

const TUSBDeviceID *CUSBXum1541Device::GetDeviceIDTable (void)
{
	return s_DeviceIDTable;
}
