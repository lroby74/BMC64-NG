//
// usbfloppydevice.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2024  R. Stange <rsta2@o2online.de>
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

//


//














//



//
#include <circle/usb/usbfloppydevice.h>
#include <circle/usb/usbhostcontroller.h>
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <circle/synchronize.h>
#include <circle/macros.h>
#include <circle/new.h>
#include <assert.h>

#define BLOCK_SIZE		512
#define BLOCK_MASK		(BLOCK_SIZE-1)
#define BLOCK_SHIFT		9

#define MAX_OFFSET		0x1FFFFFFFFFFULL	// 2TB

#define MAX_TRIES		5			// max. attempts



#define TRANSFER_TIMEOUT_MS	8000


#define STATUS_TIMEOUT_MS	1000


#define COMMAND_FAILED		(-1)
#define COMMAND_BROKEN		(-2)

// USB Mass Storage Control/Bulk/Interrupt (CBI) Transport

// UFI Command Set

struct TSCSIInquiry
{
	u8		OperationCode,
#define SCSI_OP_INQUIRY		0x12
			LogicalUnitNumberEVPD,
			PageCode,
			Reserved1,
			AllocationLength,
			Reserved2[7];
}
PACKED;

struct TSCSIInquiryResponse
{
	u8		PeripheralDeviceType	: 5,
#define SCSI_PDT_DIRECT_ACCESS_FLOPPY	0x00			// Direct access device (floppy)
#define SCSI_PDT_NONE			0x1F			// No FDD connected to this unit
			Reserved1		: 3,
			Reserved2		: 7,
			RMB			: 1,		// 1: removable media
			ANSIApprovedVersion	: 3,		// 0
			ECMAVersion		: 3,		// 0
			ISOVersion		: 2,		// 0
			ResponseDataFormat	: 4,		// 1: UFI device
			Reserved3		: 4,
			AdditionalLength,			// 31
			Reserved4[3],
			VendorIdentification[8],
			ProductIdentification[16],
			ProductRevisionLevel[4];
}
PACKED;

struct TSCSITestUnitReady
{
	u8		OperationCode,
#define SCSI_OP_TEST_UNIT_READY		0x00
			Reserved1		: 5,
			LogicalUnitNumber	: 3,
			Reserved2[10];
}
PACKED;

struct TSCSIRequestSense
{
	u8		OperationCode,
#define SCSI_REQUEST_SENSE		0x03
			Reserved1		: 5,
			LogicalUnitNumber	: 3,
			Reserved2[2],
			AllocationLength,
			Reserved3[7];
}
PACKED;

struct TSCSIRequestSenseResponse
{
	u8		ErrorCode		: 7,		// 0x70
			Valid			: 1,
			Reserved1,
			SenseKey		: 4,
#define SCSI_SENSE_KEY_NOT_READY	0x02
#define SCSI_SENSE_KEY_DATA_PROTECT	0x07
			Reserved2		: 4,
			Information[4],				// big endian
			AdditionalSenseLength,			// 10
			Reserved3[4],
			AdditionalSenseCode,
			AdditionalSenseCodeQualifier,
			Reserved4[4];
}
PACKED;


#define ASC_BECOMING_READY	0x04
#define ASC_WRITE_PROTECTED	0x27
#define ASC_MEDIA_CHANGED	0x28
#define ASC_POWER_ON		0x29
#define ASC_NO_MEDIA		0x3A

struct TSCSIReadCapacity
{
	u8		OperationCode,
#define SCSI_OP_READ_CAPACITY		0x25
			RelAdr			: 1,
			Reserved1		: 4,
			LogicalUnitNumber	: 3;
	u32		LogicalBlockAddress;			// set to 0
	u16		Reserved2;
	u8		PartialMediumIndicator	: 1,		// set to 0
			Reserved3		: 7,
			Reserved4[3];
}
PACKED;

struct TSCSIReadCapacityResponse
{
	u32		LastLogicalBlockAddress;		// big endian
	u32		BlockLengthInBytes;			// big endian
}
PACKED;

struct TSCSIRead10
{
	u8		OperationCode,
#define SCSI_OP_READ		0x28
			RelAdr			: 1,
			Reserved1		: 2,
			FUA			: 1,
			DPO			: 1,
			LogicalUnitNumber	: 3;
	u32		LogicalBlockAddress;			// big endian
	u8		Reserved2;
	u16		TransferLength;				// block count, big endian
	u8		Reserved3[3];
}
PACKED;

struct TSCSIWrite10
{
	u8		OperationCode,
#define SCSI_OP_WRITE		0x2A
			RelAdr			: 1,
			Reserved1		: 2,
			FUA			: 1,
			DPO			: 1,
			LogicalUnitNumber	: 3;
	u32		LogicalBlockAddress;			// big endian
	u8		Reserved2;
	u16		TransferLength;				// block count, big endian
	u8		Reserved3[3];
}
PACKED;

struct TSCSISendDiagnostic
{
	u8		OperationCode,
#define SCSI_OP_SEND_DIAGNOSTIC	0x1D
			UnitOfl			: 1,
			DefOfl			: 1,
			SelfTest		: 1,
			Reserved1		: 1,
			PF			: 1,
			LogicalUnitNumber	: 3,
			Reserved2[10];
}
PACKED;

CNumberPool CUSBFloppyDiskDevice::s_DeviceNumberPool (1);

LOGMODULE ("ufd");

static const char DevicePrefix[] = "ufd";

CUSBFloppyDiskDevice::CUSBFloppyDiskDevice (CUSBFunction *pFunction)
:	CUSBFunction (pFunction),
	m_pEndpointIn (0),
	m_pEndpointOut (0),
	m_pEndpointInterrupt (0),
	m_nBlockCount (0),
	m_ullOffset (0),
	m_bMediaPresent (FALSE),
	m_bMediaChanged (FALSE),
	m_ucSenseKey (0),
	m_ucSenseASC (0),
	m_ucSenseASCQ (0),
	m_nDeviceNumber (0)
{
}

CUSBFloppyDiskDevice::~CUSBFloppyDiskDevice (void)
{
	if (m_nDeviceNumber != 0)
	{
		CDeviceNameService::Get ()->RemoveDevice (DevicePrefix, m_nDeviceNumber, TRUE);

		s_DeviceNumberPool.FreeNumber (m_nDeviceNumber);

		m_nDeviceNumber = 0;
	}
	delete m_pEndpointInterrupt;
	m_pEndpointInterrupt = 0;

	delete m_pEndpointOut;
	m_pEndpointOut =  0;

	delete m_pEndpointIn;
	m_pEndpointIn = 0;
}

boolean CUSBFloppyDiskDevice::Configure (void)
{
	if (GetNumEndpoints () < 2)
	{
		ConfigurationError (From);

		return FALSE;
	}

	const TUSBEndpointDescriptor *pEndpointDesc;
	while ((pEndpointDesc = (TUSBEndpointDescriptor *) GetDescriptor (DESCRIPTOR_ENDPOINT)) != 0)
	{
		if ((pEndpointDesc->bmAttributes & 0x3F) == 0x02)		// Bulk
		{
			if ((pEndpointDesc->bEndpointAddress & 0x80) == 0x80)	// Input
			{
				if (m_pEndpointIn != 0)
				{
					ConfigurationError (From);

					return FALSE;
				}

				m_pEndpointIn = new CUSBEndpoint (GetDevice (), pEndpointDesc);
			}
			else							// Output
			{
				if (m_pEndpointOut != 0)
				{
					ConfigurationError (From);

					return FALSE;
				}

				m_pEndpointOut = new CUSBEndpoint (GetDevice (), pEndpointDesc);
			}
		}
		else if ((pEndpointDesc->bmAttributes & 0x3F) == 0x03)		// Interrupt
		{
			if ((pEndpointDesc->bEndpointAddress & 0x80) == 0x80)	// Input
			{
				if (m_pEndpointInterrupt != 0)
				{
					ConfigurationError (From);

					return FALSE;
				}

				m_pEndpointInterrupt = new CUSBEndpoint (GetDevice (),
									 pEndpointDesc);
			}
		}
	}

	if (   m_pEndpointIn == 0
	    || m_pEndpointOut == 0
	    || (   GetInterfaceProtocol () == 0
	        && m_pEndpointInterrupt == 0))
	{
		ConfigurationError (From);

		return FALSE;
	}

	if (!CUSBFunction::Configure ())
	{
		LOGERR ("Cannot set interface");

		return FALSE;
	}

	TSCSIInquiry SCSIInquiry;
	memset (&SCSIInquiry, 0, sizeof SCSIInquiry);
	SCSIInquiry.OperationCode = SCSI_OP_INQUIRY;
	SCSIInquiry.AllocationLength = sizeof (TSCSIInquiryResponse);

	TSCSIInquiryResponse SCSIInquiryResponse;
	if (Command (&SCSIInquiry, sizeof SCSIInquiry,
		     &SCSIInquiryResponse, sizeof SCSIInquiryResponse,
		     TRUE) != (int) sizeof SCSIInquiryResponse)
	{
		LOGERR ("Device does not respond");

		return FALSE;
	}

	if (SCSIInquiryResponse.PeripheralDeviceType != SCSI_PDT_DIRECT_ACCESS_FLOPPY)
	{
		LOGERR ("Unsupported device type: 0x%02X",
			(unsigned) SCSIInquiryResponse.PeripheralDeviceType);

		return FALSE;
	}


	memcpy (m_Vendor, SCSIInquiryResponse.VendorIdentification, 8);
	m_Vendor[8] = '\0';
	memcpy (m_Product, SCSIInquiryResponse.ProductIdentification, 16);
	m_Product[16] = '\0';
	for (int i = 7; i >= 0 && m_Vendor[i] == ' '; i--) m_Vendor[i] = '\0';
	for (int i = 15; i >= 0 && m_Product[i] == ' '; i--) m_Product[i] = '\0';
	LOGNOTE ("%s %s, %s", m_Vendor, m_Product, GetInterfaceProtocol () == 0 ? "CBI" : "CB");




	switch (TestMedia ())
	{
	case FLOPPY_MEDIA_NONE:
		LOGNOTE ("No disk");
		break;

	case FLOPPY_MEDIA_ERROR:
		LOGWARN ("Unit is not ready");
		break;

	default:
		break;
	}

	unsigned nDeviceNumber = s_DeviceNumberPool.AllocateNumber (FALSE);
	if (nDeviceNumber == CNumberPool::Invalid)
	{
		LOGERR ("Too many devices");

		return FALSE;
	}

	assert (m_nDeviceNumber == 0);
	m_nDeviceNumber = nDeviceNumber;

	CDeviceNameService::Get ()->AddDevice (DevicePrefix, m_nDeviceNumber, this, TRUE);

	return TRUE;
}

int CUSBFloppyDiskDevice::Read (void *pBuffer, size_t nCount)
{



	if (TestMedia () != FLOPPY_MEDIA_SAME)
	{
		return -1;
	}

	unsigned nTries = MAX_TRIES;
	int nResult;

	do
	{
		nResult = TryRead (pBuffer, nCount);

		if (nResult != (int) nCount)
		{
			if (   Reset () == COMMAND_BROKEN
			    || TestMedia () != FLOPPY_MEDIA_SAME)
			{
				return -1;
			}
		}
	}
	while (   nResult != (int) nCount
	       && --nTries > 0);

	return nResult;
}

int CUSBFloppyDiskDevice::Write (const void *pBuffer, size_t nCount)
{
	if (TestMedia () != FLOPPY_MEDIA_SAME)
	{
		return -1;
	}

	unsigned nTries = MAX_TRIES;
	int nResult;

	do
	{
		nResult = TryWrite (pBuffer, nCount);

		if (nResult != (int) nCount)
		{

			if (m_ucSenseASC == ASC_WRITE_PROTECTED)
			{
				LOGWARN ("Disk is write protected");

				return -1;
			}

			if (   Reset () == COMMAND_BROKEN
			    || TestMedia () != FLOPPY_MEDIA_SAME)
			{
				return -1;
			}
		}
	}
	while (   nResult != (int) nCount
	       && --nTries > 0);

	return nResult;
}

u64 CUSBFloppyDiskDevice::Seek (u64 ullOffset)
{
	m_ullOffset = ullOffset;

	return m_ullOffset;
}

u64 CUSBFloppyDiskDevice::GetSize (void) const
{
	return (u64) m_nBlockCount << BLOCK_SHIFT;
}

int CUSBFloppyDiskDevice::IOCtl (unsigned long ulCmd, void *pData)
{
	switch (ulCmd)
	{
	case DEVICE_IOCTL_MEDIA_STATE:
		assert (pData != 0);
		*(int *) pData = TestMedia ();
		return 0;

	case DEVICE_IOCTL_MEDIA_TAKE:
		m_bMediaChanged = FALSE;
		return 0;

	default:
		return CUSBFunction::IOCtl (ulCmd, pData);
	}
}


int CUSBFloppyDiskDevice::TestMedia (void)
{
	for (unsigned nTry = 0; nTry < 30; nTry++)
	{
		TSCSITestUnitReady SCSITestUnitReady;
		memset (&SCSITestUnitReady, 0, sizeof SCSITestUnitReady);
		SCSITestUnitReady.OperationCode = SCSI_OP_TEST_UNIT_READY;

		int nResult = Command (&SCSITestUnitReady, sizeof SCSITestUnitReady, 0, 0, FALSE);
		if (nResult == COMMAND_BROKEN)
		{
			return FLOPPY_MEDIA_ERROR;
		}

		if (nResult >= 0)
		{
			if (   !m_bMediaPresent
			    || m_nBlockCount == 0)
			{


				if (ReadCapacity () < 0)
				{
					return FLOPPY_MEDIA_ERROR;
				}

				m_bMediaPresent = TRUE;
				m_bMediaChanged = TRUE;
			}

			return m_bMediaChanged ? FLOPPY_MEDIA_CHANGED : FLOPPY_MEDIA_SAME;
		}

		switch (m_ucSenseASC)
		{
		case ASC_MEDIA_CHANGED:
		case ASC_POWER_ON:

			m_bMediaChanged = TRUE;
			m_nBlockCount = 0;
			break;

		case ASC_NO_MEDIA:
			if (m_bMediaPresent)
			{
				LOGNOTE ("Disk removed");

				m_bMediaChanged = TRUE;
			}
			m_bMediaPresent = FALSE;
			m_nBlockCount = 0;
			return FLOPPY_MEDIA_NONE;

		case ASC_BECOMING_READY:
			CTimer::Get ()->MsDelay (100);
			break;

		default:
			LOGWARN ("Test unit ready: sense %u/0x%02X/0x%02X",
				 (unsigned) m_ucSenseKey, (unsigned) m_ucSenseASC,
				 (unsigned) m_ucSenseASCQ);
			return FLOPPY_MEDIA_ERROR;
		}
	}

	LOGERR ("Unit is not ready");

	return FLOPPY_MEDIA_ERROR;
}

int CUSBFloppyDiskDevice::ReadCapacity (void)
{
	TSCSIReadCapacityResponse SCSIReadCapacityResponse;
	unsigned nTries;
	for (nTries = MAX_TRIES; nTries; nTries--)
	{
		TSCSIReadCapacity SCSIReadCapacity;
		memset (&SCSIReadCapacity, 0, sizeof SCSIReadCapacity);
		SCSIReadCapacity.OperationCode = SCSI_OP_READ_CAPACITY;

		if (Command (&SCSIReadCapacity, sizeof SCSIReadCapacity,
			     &SCSIReadCapacityResponse, sizeof SCSIReadCapacityResponse,
			     TRUE) == (int) sizeof SCSIReadCapacityResponse)
		{
			break;
		}
	}

	if (nTries == 0)
	{
		LOGERR ("Read capacity failed");

		return -1;
	}

	unsigned nBlockSize = le2be32 (SCSIReadCapacityResponse.BlockLengthInBytes);
	if (nBlockSize != BLOCK_SIZE)
	{
		LOGERR ("Unsupported block size: %u", nBlockSize);

		return -1;
	}

	u32 nLastBlock = le2be32 (SCSIReadCapacityResponse.LastLogicalBlockAddress);
	if (nLastBlock == (u32) -1)
	{
		LOGERR ("Unsupported disk size > 2TB");

		return -1;
	}

	m_nBlockCount = nLastBlock + 1;

	LOGNOTE ("Disk: %u KBytes", m_nBlockCount / (0x400 / BLOCK_SIZE));

	return 0;
}


int CUSBFloppyDiskDevice::RequestSense (u8 *pKey, u8 *pASC, u8 *pASCQ)
{
	TSCSIRequestSense SCSIRequestSense;
	memset (&SCSIRequestSense, 0, sizeof SCSIRequestSense);
	SCSIRequestSense.OperationCode = SCSI_REQUEST_SENSE;
	SCSIRequestSense.AllocationLength = sizeof (TSCSIRequestSenseResponse);

	TSCSIRequestSenseResponse SCSIRequestSenseResponse;
	memset (&SCSIRequestSenseResponse, 0, sizeof SCSIRequestSenseResponse);
	int nResult = Command (&SCSIRequestSense, sizeof SCSIRequestSense,
			       &SCSIRequestSenseResponse, sizeof SCSIRequestSenseResponse,
			       TRUE);
	if (nResult < 14)
	{
		LOGERR ("Request sense failed");

		return -1;
	}

	*pKey = SCSIRequestSenseResponse.SenseKey;
	*pASC = SCSIRequestSenseResponse.AdditionalSenseCode;
	*pASCQ = SCSIRequestSenseResponse.AdditionalSenseCodeQualifier;

	return 0;
}

int CUSBFloppyDiskDevice::TryRead (void *pBuffer, size_t nCount)
{
	assert (pBuffer != 0);

	if (   (m_ullOffset & BLOCK_MASK) != 0
	    || m_ullOffset > MAX_OFFSET)
	{
		return -1;
	}
	u32 nBlockAddress = (u32) (m_ullOffset >> BLOCK_SHIFT);

	if ((nCount & BLOCK_MASK) != 0)
	{
		return -1;
	}
	u16 usTransferLength = (u16) (nCount >> BLOCK_SHIFT);

	// LOGDBG ("TryRead %u/0x%lX/%u", nBlockAddress, (uintptr) pBuffer, (unsigned) usTransferLength);

	TSCSIRead10 SCSIRead;
	memset (&SCSIRead, 0, sizeof SCSIRead);
	SCSIRead.OperationCode		= SCSI_OP_READ;
	SCSIRead.LogicalBlockAddress	= le2be32 (nBlockAddress);
	SCSIRead.TransferLength		= le2be16 (usTransferLength);

	if (Command (&SCSIRead, sizeof SCSIRead, pBuffer, nCount, TRUE) != (int) nCount)
	{
		LOGERR ("TryRead failed");

		return -1;
	}

	return nCount;
}

int CUSBFloppyDiskDevice::TryWrite (const void *pBuffer, size_t nCount)
{
	assert (pBuffer != 0);

	if (   (m_ullOffset & BLOCK_MASK) != 0
	    || m_ullOffset > MAX_OFFSET)
	{
		return -1;
	}
	u32 nBlockAddress = (u32) (m_ullOffset >> BLOCK_SHIFT);

	if ((nCount & BLOCK_MASK) != 0)
	{
		return -1;
	}
	u16 usTransferLength = (u16) (nCount >> BLOCK_SHIFT);

	// LOGDBG ("TryWrite %u/0x%lX/%u", nBlockAddress, (uintptr) pBuffer, (unsigned) usTransferLength);

	TSCSIWrite10 SCSIWrite;
	memset (&SCSIWrite, 0, sizeof SCSIWrite);
	SCSIWrite.OperationCode		= SCSI_OP_WRITE;
	SCSIWrite.LogicalBlockAddress	= le2be32 (nBlockAddress);
	SCSIWrite.TransferLength	= le2be16 (usTransferLength);

	if (Command (&SCSIWrite, sizeof SCSIWrite, (void *) pBuffer, nCount, FALSE) != (int) nCount)
	{
		LOGERR ("TryWrite failed");

		return -1;
	}

	return nCount;
}








int CUSBFloppyDiskDevice::Command (void *pCmdBlk, size_t nCmdBlkLen,
				   void *pBuffer, size_t nBufLen, boolean bIn)
{
	assert (pCmdBlk != 0);
	assert (nCmdBlkLen == 12);
	assert (nBufLen == 0 || pBuffer != 0);

	const u8 ucOperation = *(const u8 *) pCmdBlk;
	const boolean bNoStatus =    ucOperation == SCSI_OP_INQUIRY
				  || ucOperation == SCSI_REQUEST_SENSE;

	m_ucSenseKey = 0;
	m_ucSenseASC = 0;
	m_ucSenseASCQ = 0;

	DMA_BUFFER (u8, CmdBuffer, nCmdBlkLen);
	memcpy (CmdBuffer, pCmdBlk, nCmdBlkLen);

	CUSBHostController *pHost = GetHost ();
	assert (pHost != 0);

	boolean bFailed = FALSE;
	int nResult = 0;

	if (pHost->ControlMessage (GetEndpoint0 (),
				   REQUEST_OUT | REQUEST_CLASS | REQUEST_TO_INTERFACE,
				   0, 0, GetInterfaceNumber (), CmdBuffer, nCmdBlkLen) < 0)
	{

		RecoverEndpoint (GetEndpoint0 ());

		bFailed = TRUE;
	}
	else
	{
		if (nBufLen > 0)
		{
			assert (pBuffer != 0);

			u8 *pDMABuffer = 0;
			if (!IS_CACHE_ALIGNED (pBuffer, nBufLen))
			{
				pDMABuffer = new (HEAP_DMA30) u8[nBufLen];
				assert (pDMABuffer != 0);

				if (!bIn)
				{
					memcpy (pDMABuffer, pBuffer, nBufLen);
				}
			}

			CUSBEndpoint *pEndpoint = bIn ? m_pEndpointIn : m_pEndpointOut;
			nResult = pHost->Transfer (pEndpoint,
						   pDMABuffer != 0 ? pDMABuffer : pBuffer, nBufLen,
						   TRANSFER_TIMEOUT_MS);
			if (nResult < 0)
			{

				RecoverEndpoint (pEndpoint);

				bFailed = TRUE;
				nResult = 0;
			}
			else if (   pDMABuffer != 0
				 && bIn)
			{
				memcpy (pBuffer, pDMABuffer, nResult);
			}

			delete [] pDMABuffer;
		}

		if (GetInterfaceProtocol () == 0)
		{
			DMA_BUFFER (u8, Status, 2);

			assert (m_pEndpointInterrupt != 0);






			if (pHost->Transfer (m_pEndpointInterrupt, Status, 2,
					     STATUS_TIMEOUT_MS) != 2)
			{
				LOGERR ("Status transfer failed");

				return COMMAND_BROKEN;
			}


			if (   !bNoStatus
			    && Status[0] != 0)
			{
				bFailed = TRUE;
			}
		}
		else if (   !bFailed
			 && !bNoStatus
			 && !(bIn && nBufLen > 0))
		{

			u8 ucKey, ucASC, ucASCQ;
			if (RequestSense (&ucKey, &ucASC, &ucASCQ) < 0)
			{
				return COMMAND_BROKEN;
			}

			m_ucSenseKey = ucKey;
			m_ucSenseASC = ucASC;
			m_ucSenseASCQ = ucASCQ;

			return ucKey == 0 && ucASC == 0 ? nResult : COMMAND_FAILED;
		}
	}

	if (!bFailed)
	{
		return nResult;
	}

	if (bNoStatus)
	{
		return COMMAND_FAILED;
	}

	u8 ucKey, ucASC, ucASCQ;
	if (RequestSense (&ucKey, &ucASC, &ucASCQ) < 0)
	{
		return COMMAND_BROKEN;
	}

	m_ucSenseKey = ucKey;
	m_ucSenseASC = ucASC;
	m_ucSenseASCQ = ucASCQ;

	return COMMAND_FAILED;
}




void CUSBFloppyDiskDevice::RecoverEndpoint (CUSBEndpoint *pEndpoint)
{
	assert (pEndpoint != 0);

#if RASPPI >= 4

	pEndpoint->GetXHCIEndpoint ()->ResetFromHalted ();

	// TODO: Send CLEAR_TT_BUFFER for EP0 to the hub, this device is connected to.
#endif

	if (pEndpoint != GetEndpoint0 ())
	{
		GetHost ()->ControlMessage (GetEndpoint0 (),
					    REQUEST_OUT | REQUEST_TO_ENDPOINT, CLEAR_FEATURE,
					    ENDPOINT_HALT,
					    pEndpoint->GetNumber () | (pEndpoint->IsDirectionIn () ? 0x80 : 0),
					    0, 0);

		pEndpoint->ResetPID ();
	}
}

int CUSBFloppyDiskDevice::Reset (void)
{
	TSCSISendDiagnostic SCSISendDiagnostic;
	memset (&SCSISendDiagnostic, 0, sizeof SCSISendDiagnostic);
	SCSISendDiagnostic.OperationCode = SCSI_OP_SEND_DIAGNOSTIC;
	SCSISendDiagnostic.SelfTest = 1;

	return Command (&SCSISendDiagnostic, sizeof SCSISendDiagnostic, 0, 0, FALSE);
}
