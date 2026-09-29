//

//


//




//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
#ifndef _circle_usb_usbxum1541_h
#define _circle_usb_usbxum1541_h

#include <circle/usb/usbfunction.h>
#include <circle/usb/usbendpoint.h>
#include <circle/usb/usbdevicefactory.h>
#include <circle/numberpool.h>
#include <circle/types.h>

class CUSBXum1541Device : public CUSBFunction
{
public:
	CUSBXum1541Device (CUSBFunction *pFunction);
	~CUSBXum1541Device (void);

	boolean Configure (void);



	int ControlIn (u8 ucRequest, void *pBuffer, u16 usLength);
	int ControlOut (u8 ucRequest);





	void ClearHalts (void);





	int BulkOut (const void *pBuffer, unsigned nCount, unsigned nTimeoutMs);
	int BulkIn (void *pBuffer, unsigned nCount, unsigned nTimeoutMs);




	void SetIrqDelay (unsigned nIMODI);
	unsigned GetIrqDelay (void);

	static const TUSBDeviceID *GetDeviceIDTable (void);

private:
	static void SetPatience (CUSBEndpoint *pEndpoint);

private:
	CUSBEndpoint *m_pEndpointIn;
	CUSBEndpoint *m_pEndpointOut;

	unsigned m_nDeviceNumber;
	static CNumberPool s_DeviceNumberPool;
};

#endif
