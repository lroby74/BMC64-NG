//
// usbmouse.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2026  R. Stange <rsta2@gmx.net>
//
// USB mouse wheel support including HID report parser:
// Copyright (C) 2020  H. Kocevar <hinxx@protonmail.com>
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
#include <circle/usb/usbmouse.h>
#include <circle/usb/usbhid.h>
#include <circle/logger.h>
#include <assert.h>

// HID Report Items from HID 1.11 Section 6.2.2
#define HID_USAGE_PAGE      0x04
#define HID_USAGE           0x08
#define HID_COLLECTION      0xA0
#define HID_END_COLLECTION  0xC0
#define HID_REPORT_COUNT    0x94
#define HID_REPORT_SIZE     0x74
#define HID_INPUT           0x80
#define HID_REPORT_ID       0x84

// HID Report Usage Pages from HID Usage Tables 1.12 Section 3, Table 1
#define HID_USAGE_PAGE_BUTTONS         0x09

// HID Report Usages from HID Usage Tables 1.12 Section 4, Table 6
#define HID_USAGE_POINTER   0x01
#define HID_USAGE_MOUSE     0x02
#define HID_USAGE_X         0x30
#define HID_USAGE_Y         0x31
#define HID_USAGE_WHEEL     0x38





extern "C" {
volatile unsigned bmc_mouse_presi = 0;
volatile unsigned bmc_mouse_scartati = 0;
volatile unsigned bmc_mouse_lunghezza = 0;
volatile unsigned char bmc_mouse_ultimo[8];
volatile int bmc_mouse_rotella = 0;
}








static unsigned MaxInputReportBytes (const u8 *pDesc, unsigned nLength)
{
	unsigned nIDs[16] = {0};
	unsigned nBits[16] = {0};
	unsigned nUsed = 1;
	unsigned nCurrent = 0;
	unsigned nSize = 0, nCount = 0;

	while (nLength > 0)
	{
		u8 ucItem = *pDesc++;
		nLength--;
		unsigned nArgLen = (ucItem & 0x03) == 3 ? 4 : (ucItem & 0x03);
		if (nArgLen > nLength)
		{
			break;
		}
		unsigned nArg = 0;
		for (unsigned i = 0; i < nArgLen; i++)
		{
			nArg |= (unsigned) pDesc[i] << (8 * i);
		}
		pDesc += nArgLen;
		nLength -= nArgLen;

		switch (ucItem & 0xFC)
		{
		case HID_REPORT_SIZE:
			nSize = nArg;
			break;
		case HID_REPORT_COUNT:
			nCount = nArg;
			break;
		case HID_REPORT_ID:
			nCurrent = 0;
			for (unsigned i = 1; i < nUsed; i++)
			{
				if (nIDs[i] == nArg)
				{
					nCurrent = i;
				}
			}
			if (nCurrent == 0 && nUsed < 16)
			{
				nCurrent = nUsed++;
				nIDs[nCurrent] = nArg;
			}
			break;
		case HID_INPUT:
			nBits[nCurrent] += nSize * nCount;
			break;
		}
	}

	unsigned nMax = 0;
	for (unsigned i = 0; i < nUsed; i++)
	{
		unsigned nBytes = (nBits[i] + 7) / 8 + (i > 0 ? 1 : 0);
		if (nBits[i] != 0 && nBytes > nMax)
		{
			nMax = nBytes;
		}
	}

	return nMax;
}

static const char FromUSBMouse[] = "umouse";

CUSBMouseDevice::CUSBMouseDevice (CUSBFunction *pFunction)
:	CUSBHIDDevice (pFunction),
	m_pMouseDevice (0),
	m_pHIDReportDescriptor (0)
{
}

CUSBMouseDevice::~CUSBMouseDevice (void)
{
	delete m_pMouseDevice;
	m_pMouseDevice = 0;

	delete [] m_pHIDReportDescriptor;
	m_pHIDReportDescriptor = 0;
}

boolean CUSBMouseDevice::Configure (void)
{
	TUSBHIDDescriptor *pHIDDesc = (TUSBHIDDescriptor *) GetDescriptor (DESCRIPTOR_HID);
	if (   pHIDDesc == 0
	    || pHIDDesc->wReportDescriptorLength == 0)
	{
		ConfigurationError (FromUSBMouse);

		return FALSE;
	}

	m_usReportDescriptorLength = pHIDDesc->wReportDescriptorLength;
	m_pHIDReportDescriptor = new u8[m_usReportDescriptorLength];
	assert (m_pHIDReportDescriptor != 0);

	if (   GetHost ()->GetDescriptor (GetEndpoint0 (),
					  pHIDDesc->bReportDescriptorType, DESCRIPTOR_INDEX_DEFAULT,
					  m_pHIDReportDescriptor, m_usReportDescriptorLength,
					  REQUEST_IN | REQUEST_TO_INTERFACE, GetInterfaceNumber ())
	    != m_usReportDescriptorLength)
	{
		CLogger::Get ()->Write (FromUSBMouse, LogError, "Cannot get HID report descriptor");

		return FALSE;
	}

	DecodeReport ();



	unsigned nMaxReport = MaxInputReportBytes (m_pHIDReportDescriptor,
						    m_usReportDescriptorLength);
	if (nMaxReport < m_MouseReport.byteSize)
	{
		nMaxReport = m_MouseReport.byteSize;
	}
	CLogger::Get ()->Write (FromUSBMouse, LogNotice,
		"int%u-%u-%u: rapporto id %u, %u byte (il piu' lungo %u); "
		"tasti %u@%u x %u@%u y %u@%u rotella %u@%u",
		GetInterfaceClass (), GetInterfaceSubClass (), GetInterfaceProtocol (),
		m_MouseReport.id, m_MouseReport.byteSize, nMaxReport,
		m_MouseReport.items[MouseItemButtons].bitSize,
		m_MouseReport.items[MouseItemButtons].bitOffset,
		m_MouseReport.items[MouseItemXAxis].bitSize,
		m_MouseReport.items[MouseItemXAxis].bitOffset,
		m_MouseReport.items[MouseItemYAxis].bitSize,
		m_MouseReport.items[MouseItemYAxis].bitOffset,
		m_MouseReport.items[MouseItemWheel].bitSize,
		m_MouseReport.items[MouseItemWheel].bitOffset);
	for (unsigned nPos = 0; nPos < m_usReportDescriptorLength && nPos < 240; nPos += 40)
	{
		char Hex[40 * 3 + 1];
		unsigned nOut = 0;
		for (unsigned i = nPos; i < m_usReportDescriptorLength && i < nPos + 40; i++)
		{
			static const char Digits[] = "0123456789abcdef";
			Hex[nOut++] = Digits[m_pHIDReportDescriptor[i] >> 4];
			Hex[nOut++] = Digits[m_pHIDReportDescriptor[i] & 15];
			Hex[nOut++] = ' ';
		}
		Hex[nOut] = '\0';
		CLogger::Get ()->Write (FromUSBMouse, LogNotice, "descrittore %u: %s",
					nPos, Hex);
	}

	// ignoring unsupported HID interface
	if (m_MouseReport.byteSize == 0)
	{
		return FALSE;
	}

	if (!CUSBHIDDevice::ConfigureHID (nMaxReport))
	{
		CLogger::Get ()->Write (FromUSBMouse, LogError, "Cannot configure HID device");

		return FALSE;
	}

	
	m_pMouseDevice = new CMouseDevice(m_MouseReport.items[MouseItemButtons].bitSize,
					  m_MouseReport.items[MouseItemWheel].bitSize != 0, this);
	assert (m_pMouseDevice != 0);

	return StartRequest ();
}

void CUSBMouseDevice::ReportHandler (const u8 *pReport, unsigned nReportSize)
{



	if (pReport != 0)
	{
		bmc_mouse_lunghezza = nReportSize;
		for (unsigned i = 0; i < 8; i++)
		{
			bmc_mouse_ultimo[i] = i < nReportSize ? pReport[i] : 0;
		}
	}
	if (   pReport != 0
	    && nReportSize >= m_MouseReport.byteSize
	    && (m_MouseReport.id == 0 || pReport[0] == m_MouseReport.id))
	{
		bmc_mouse_presi++;
		if (m_pMouseDevice != 0)
		{
			u32 ucHIDButtons = 0;
			s32 xMove = 0;
			s32 yMove = 0;
			s32 wheelMove = 0;
			TMouseReportItem *item = &m_MouseReport.items[MouseItemButtons];
			ucHIDButtons = ExtractUnsigned(pReport, item->bitOffset, item->bitSize);
			item = &m_MouseReport.items[MouseItemXAxis];
			xMove = ExtractSigned(pReport, item->bitOffset, item->bitSize);
			if (xMove > 127)
				xMove = 127;
			if (xMove < -127)
				xMove = -127;
			item = &m_MouseReport.items[MouseItemYAxis];
			yMove = ExtractSigned(pReport, item->bitOffset, item->bitSize);
			if (yMove > 127)
				yMove = 127;
			if (yMove < -127)
				yMove = -127;
			item = &m_MouseReport.items[MouseItemWheel];
			wheelMove = ExtractSigned(pReport, item->bitOffset, item->bitSize);

			u32 nButtons = 0;
			if (ucHIDButtons & USBHID_BUTTON1)
			{
				nButtons |= MOUSE_BUTTON_LEFT;
			}
			if (ucHIDButtons & USBHID_BUTTON2)
			{
				nButtons |= MOUSE_BUTTON_RIGHT;
			}
			if (ucHIDButtons & USBHID_BUTTON3)
			{
				nButtons |= MOUSE_BUTTON_MIDDLE;
			}
			if (ucHIDButtons & USBHID_BUTTON4)
			{
				nButtons |= MOUSE_BUTTON_SIDE1;
			}
			if (ucHIDButtons & USBHID_BUTTON5)
			{
				nButtons |= MOUSE_BUTTON_SIDE2;
			}




			nButtons |= ucHIDButtons & ~0x1Fu;

			m_pMouseDevice->ReportHandler (nButtons, xMove, yMove, wheelMove);
			bmc_mouse_rotella += wheelMove;
		}
	}
	else if (pReport != 0)
	{
		bmc_mouse_scartati++;
	}
}

u32 CUSBMouseDevice::ExtractUnsigned(const void *buffer, u32 offset, u32 length)
{
	assert(buffer != 0);
	assert(length <= 32);

	if (length == 0)
	{
		return 0;
	}

	u8 *bits = (u8 *)buffer;
	unsigned shift = offset % 8;
	offset = offset / 8;
	bits = bits + offset;
	unsigned number = bits[0];
	if (length > 8) number |= (u16) bits[1] << 8;
	if (length > 16) number |= (u32) bits[2] << 16;
	offset = shift;

	unsigned result = 0;
	if (length > 24)
	{
		result = (((1 << 24) - 1) & (number >> offset));
		bits = bits + 3;
		length = length - 24;
		number = bits[0];
		if (length > 8) number |= (u16) bits[1] << 8;
		if (length > 16) number |= (u32) bits[2] << 16;
		unsigned result2 = (((1 << length) - 1) & (number >> offset));
		result = (result2 << 24) | result;
	}
	else
	{
		result = (((1 << length) - 1) & (number >> offset));
	}

	return result;
}

s32 CUSBMouseDevice::ExtractSigned(const void *buffer, u32 offset, u32 length)
{
	assert(buffer != 0);
	assert(length <= 32);

	unsigned result = ExtractUnsigned(buffer, offset, length);
	if ((length == 0) || (length == 32))
	{
		return result;
	}

	if (result & (1 << (length - 1)))
	{
		result |= 0xffffffff - ((1 << length) - 1);
	}

	return result;
}

void CUSBMouseDevice::DecodeReport (void)
{
	s32 item, arg;
	u32 offset = 0, size = 0, count = 0;
	u32 id = 0;
	u32 nCollections = 0;
	u32 itemIndex = 0;
	boolean parse = FALSE;
	boolean foundMouseUsage = FALSE;
	u32 itemIndexes[MouseItemCount] = {0};

	assert (m_pHIDReportDescriptor != 0);
	s8 *pHIDReportDescriptor = (s8 *) m_pHIDReportDescriptor;

	for (u16 usReportDescriptorLength = m_usReportDescriptorLength; usReportDescriptorLength > 0; )
	{
		item = *pHIDReportDescriptor++;
		usReportDescriptorLength--;

		switch(item & 0x03)
		{
		case 0:
			arg = 0;
			break;
		case 1:
			arg = *pHIDReportDescriptor++;
			usReportDescriptorLength--;
			break;
		case 2:
			arg = *pHIDReportDescriptor++ & 0xFF;
			arg = arg | (*pHIDReportDescriptor++ << 8);
			usReportDescriptorLength -= 2;
			break;
		default:
			arg = *pHIDReportDescriptor++;
			arg = arg | (*pHIDReportDescriptor++ << 8);
			arg = arg | (*pHIDReportDescriptor++ << 16);
			arg = arg | (*pHIDReportDescriptor++ << 24);
			usReportDescriptorLength -= 4;
			break;
		}

		switch(item & 0xFC)
		{
		case HID_COLLECTION:
			nCollections++;
			break;
		case HID_END_COLLECTION:
			nCollections--;
			if (nCollections == 0)
				parse = FALSE;
			break;
		case HID_USAGE:
			if (arg == HID_USAGE_MOUSE)
			{
				foundMouseUsage = TRUE;
			}
			else if (arg == HID_USAGE_POINTER)
			{
				if (id == 0)
				{
					parse = true;
				}
			}
			break;
		case HID_REPORT_ID:
			if (foundMouseUsage)
			{
				if (id == 0)
				{
					id = arg;
					offset += 8;
					parse = TRUE;
				}
				else
				{
					if ((u32)arg != id)
					{
						parse = FALSE;
					}
					else
					{
						parse = TRUE;
					}
				}
			}
			break;
		}

		if (! parse)
			continue;

		switch(item & 0xFC)
		{
		case HID_USAGE_PAGE:
			switch(arg)
			{
			case HID_USAGE_PAGE_BUTTONS:
				itemIndexes[itemIndex] = MouseItemButtons;
				itemIndex++;
				break;
			}
			break;
		case HID_USAGE:
			switch(arg)
			{
			case HID_USAGE_X:
				itemIndexes[itemIndex] = MouseItemXAxis;
				itemIndex++;
				break;
			case HID_USAGE_Y:
				itemIndexes[itemIndex] = MouseItemYAxis;
				itemIndex++;
				break;
			case HID_USAGE_WHEEL:
				itemIndexes[itemIndex] = MouseItemWheel;
				itemIndex++;
				break;
			}
			break;
		case HID_REPORT_SIZE:
			size = arg;
			break;
		case HID_REPORT_COUNT:
			count = arg;
			break;
		case HID_INPUT:
			if ((arg & 0x03) == 0x02)
			{
				u32 bitOffset = offset;
				u32 bitSize = 0;
				u32 index = 0;
				u32 item = 0;
				assert(itemIndex < MouseItemCount);
				while (index < itemIndex)
				{
					item = itemIndexes[index];
					assert(item < MouseItemCount);
					bitSize = size;
					if (item == MouseItemButtons)
					{
						bitSize = count;
					}
					m_MouseReport.items[item].bitSize = bitSize;
					m_MouseReport.items[item].bitOffset = bitOffset;
					bitOffset += bitSize;
					index++;
				}
				itemIndex = 0;
			}

			offset += count * size;
			break;
		}
	}

	m_MouseReport.id = id;
	m_MouseReport.byteSize = (offset + 7) / 8;
}
