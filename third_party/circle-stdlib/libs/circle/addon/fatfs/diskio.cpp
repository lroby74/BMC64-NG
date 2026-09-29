/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2019        */
/* Implementation for Circle by R. Stange <rsta2@o2online.de>            */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include <circle/device.h>
#include <circle/devicenameservice.h>
#include <circle/util.h>
#include <circle/types.h>
#include <circle/timer.h>
#include <circle/usb/usbfloppydevice.h>
#include <assert.h>

#if FF_MIN_SS != FF_MAX_SS
	#error FF_MIN_SS != FF_MAX_SS is not supported!
#endif
#define SECTOR_SIZE		FF_MIN_SS

/*-----------------------------------------------------------------------*/
/* Static Data                                                           */
/*-----------------------------------------------------------------------*/

static const char *s_pVolumeName[FF_VOLUMES] =
{
	"emmc1",
	"emmc2",
	"umsd1",
	"umsd2",
	"umsd3",
	"ufd1",
	"ufd2",
	"nvme1"
};

static CDevice *s_pVolume[FF_VOLUMES] = {0};

static u8 *s_pBuffer = 0;
static unsigned s_nBufferSize = 0;



/*-----------------------------------------------------------------------*/
/* Callbacks                                                             */
/*-----------------------------------------------------------------------*/

static void disk_removed (
	CDevice *pDevice,	/* device removed */
	void *pContext
)
{
	*((CDevice **) pContext) = 0;
}



/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/





















extern "C" int circle_usb_presta (void);
extern "C" void circle_usb_restituisci (void);










extern "C" volatile unsigned long bmc_scheda_settori;


static inline int bmc64_drive_e_usb (BYTE pdrv)
{
	return pdrv >= 2 && pdrv <= 6;
}















static inline int bmc64_drive_e_floppy (BYTE pdrv)
{
	return pdrv == 5 || pdrv == 6;
}
























#define DISCHETTO_SETTORI	16

static u8 s_disco_dati[2][DISCHETTO_SETTORI * SECTOR_SIZE]
	__attribute__ ((aligned (16)));
static LBA_t s_disco_primo[2];
static UINT s_disco_quanti[2];

static inline int floppy_posto (BYTE pdrv)
{
	return pdrv == 5 ? 0 : 1;
}

static void floppy_cache_butta (BYTE pdrv)
{
	if (bmc64_drive_e_floppy (pdrv))
	{
		s_disco_quanti[floppy_posto (pdrv)] = 0;
	}
}

#define FLOPPY_FIDUCIA_US	500000

static unsigned s_nFloppyVisto[FF_VOLUMES];
static int s_bFloppyBuono[FF_VOLUMES];

static void floppy_segna (BYTE pdrv, int bBuono)
{
	s_nFloppyVisto[pdrv] = CTimer::GetClockTicks ();
	s_bFloppyBuono[pdrv] = bBuono;
}


static int floppy_stato (BYTE pdrv)
{
	int nStato = FLOPPY_MEDIA_ERROR;
	if (s_pVolume[pdrv]->IOCtl (DEVICE_IOCTL_MEDIA_STATE, &nStato) < 0)
	{
		nStato = FLOPPY_MEDIA_ERROR;
	}
	floppy_segna (pdrv, nStato == FLOPPY_MEDIA_SAME);
	return nStato;
}

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	if (   pdrv < FF_VOLUMES
	    && s_pVolume[pdrv] != 0)
	{
		if (   bmc64_drive_e_floppy (pdrv)
		    && !(   s_bFloppyBuono[pdrv]
			 && CTimer::GetClockTicks () - s_nFloppyVisto[pdrv] < FLOPPY_FIDUCIA_US))
		{
			if (!circle_usb_presta ())
			{
				return STA_NOINIT;
			}
			int nStato = floppy_stato (pdrv);
			circle_usb_restituisci ();

			if (nStato == FLOPPY_MEDIA_NONE)
			{
				return STA_NOINIT | STA_NODISK;
			}
			if (nStato != FLOPPY_MEDIA_SAME)
			{
				return STA_NOINIT;
			}
		}

		return 0;
	}

	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	if (pdrv >= FF_VOLUMES)
	{
		return STA_NOINIT;
	}


	floppy_cache_butta (pdrv);

	if (bmc64_drive_e_usb (pdrv) && !circle_usb_presta ())
	{
		return STA_NOINIT;
	}

	CDevice *pDevice = CDeviceNameService::Get ()->GetDevice (s_pVolumeName[pdrv], TRUE);
	if (pDevice != 0)
	{


		if (pDevice != s_pVolume[pdrv])
		{
			pDevice->RegisterRemovedHandler (disk_removed, &s_pVolume[pdrv]);
		}
		s_pVolume[pdrv] = pDevice;

		if (bmc64_drive_e_floppy (pdrv))
		{
			int nStato = floppy_stato (pdrv);
			if (   nStato != FLOPPY_MEDIA_SAME
			    && nStato != FLOPPY_MEDIA_CHANGED)
			{
				s_bFloppyBuono[pdrv] = 0;
				circle_usb_restituisci ();

				return nStato == FLOPPY_MEDIA_NONE ? STA_NOINIT | STA_NODISK
								   : STA_NOINIT;
			}


			pDevice->IOCtl (DEVICE_IOCTL_MEDIA_TAKE, 0);
			floppy_segna (pdrv, 1);
		}

		if (bmc64_drive_e_usb (pdrv))
		{
			circle_usb_restituisci ();
		}

		return 0;
	}

	if (bmc64_drive_e_usb (pdrv))
	{
		circle_usb_restituisci ();
	}

	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/






static DRESULT floppy_leggi (BYTE pdrv, CDevice *pDevice, BYTE *buff,
			     LBA_t sector, UINT count)
{
	int v = floppy_posto (pdrv);

	if (   s_disco_quanti[v] > 0
	    && sector >= s_disco_primo[v]
	    && sector + count <= s_disco_primo[v] + s_disco_quanti[v])
	{
		memcpy (buff,
			s_disco_dati[v]
				+ (size_t) (sector - s_disco_primo[v]) * SECTOR_SIZE,
			(size_t) count * SECTOR_SIZE);

		return RES_OK;
	}





	UINT quanti = DISCHETTO_SETTORI;
	u64 ullSize = pDevice->GetSize ();
	if (ullSize != (u64) -1 && ullSize > 0)
	{
		LBA_t totali = (LBA_t) (ullSize / SECTOR_SIZE);

		if (sector >= totali)
		{
			quanti = count;
		}
		else if (sector + quanti > totali)
		{
			quanti = (UINT) (totali - sector);
		}
	}
	if (quanti < count)
	{
		quanti = count;
	}

	s_disco_quanti[v] = 0;

	if (!circle_usb_presta ())
	{
		return RES_NOTRDY;
	}

	QWORD offset = sector;
	offset *= SECTOR_SIZE;
	pDevice->Seek (offset);

	int bAndato = pDevice->Read (s_disco_dati[v], quanti * SECTOR_SIZE) < 0
			? 0 : 1;

	circle_usb_restituisci ();

	floppy_segna (pdrv, bAndato);

	if (!bAndato)
	{
		return RES_ERROR;
	}

	bmc_scheda_settori += quanti;

	s_disco_primo[v] = sector;
	s_disco_quanti[v] = quanti;

	memcpy (buff, s_disco_dati[v], (size_t) count * SECTOR_SIZE);

	return RES_OK;
}

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv >= FF_VOLUMES)
	{
		return RES_PARERR;
	}

	CDevice *pDevice = s_pVolume[pdrv];
	if (pDevice == 0)
	{
		return RES_NOTRDY;
	}


	if (   bmc64_drive_e_floppy (pdrv)
	    && count <= DISCHETTO_SETTORI)
	{
		return floppy_leggi (pdrv, pDevice, buff, sector, count);
	}

	/* Ensure that the transfer buffer is word aligned */
	BYTE *pBuffer = buff;
	unsigned nSize = count * SECTOR_SIZE;
	if (((uintptr) pBuffer & 3) != 0)
	{
		if (s_nBufferSize < nSize)
		{
			delete [] s_pBuffer;

			s_nBufferSize = nSize;

			s_pBuffer = new u8[s_nBufferSize];
			assert (s_pBuffer != 0);
		}

		pBuffer = s_pBuffer;
	}

	QWORD offset = sector;
	offset *= SECTOR_SIZE;

	if (bmc64_drive_e_usb (pdrv))
	{
		int bAndato;

		if (!circle_usb_presta ())
		{
			return RES_NOTRDY;
		}

		pDevice->Seek (offset);
		bAndato = pDevice->Read (pBuffer, nSize) < 0 ? 0 : 1;

		circle_usb_restituisci ();

		if (bmc64_drive_e_floppy (pdrv))
		{
			floppy_segna (pdrv, bAndato);
		}

		if (!bAndato)
		{
			return RES_ERROR;
		}
	}
	else
	{
		pDevice->Seek (offset);

		if (pDevice->Read (pBuffer, nSize) < 0)
		{
			return RES_ERROR;
		}
	}

	if (pBuffer != buff)
	{
		memcpy (buff, pBuffer, nSize);
	}

	bmc_scheda_settori += count;

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv >= FF_VOLUMES)
	{
		return RES_PARERR;
	}

	CDevice *pDevice = s_pVolume[pdrv];
	if (pDevice == 0)
	{
		return RES_NOTRDY;
	}


	floppy_cache_butta (pdrv);

	/* Ensure that the transfer buffer is word aligned */
	const BYTE *pBuffer = buff;
	unsigned nSize = count * SECTOR_SIZE;
	if (((uintptr) pBuffer & 3) != 0)
	{
		if (s_nBufferSize < nSize)
		{
			delete [] s_pBuffer;

			s_nBufferSize = nSize;

			s_pBuffer = new u8[s_nBufferSize];
			assert (s_pBuffer != 0);
		}

		memcpy (s_pBuffer, buff, nSize);

		pBuffer = s_pBuffer;
	}

	QWORD offset = sector;
	offset *= SECTOR_SIZE;

	if (bmc64_drive_e_usb (pdrv))
	{
		int bAndato;

		if (!circle_usb_presta ())
		{
			return RES_NOTRDY;
		}

		pDevice->Seek (offset);
		bAndato = pDevice->Write (pBuffer, nSize) < 0 ? 0 : 1;

		circle_usb_restituisci ();

		if (bmc64_drive_e_floppy (pdrv))
		{
			floppy_segna (pdrv, bAndato);
		}

		if (!bAndato)
		{
			return RES_ERROR;
		}

		bmc_scheda_settori += count;

		return RES_OK;
	}

	pDevice->Seek (offset);

	if (pDevice->Write (pBuffer, nSize) < 0)
	{
		return RES_ERROR;
	}

	bmc_scheda_settori += count;

	return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	switch (cmd)
	{
	case GET_SECTOR_COUNT:
		{
			if (pdrv >= FF_VOLUMES)
			{
				return RES_PARERR;
			}

			CDevice *pDevice =
				CDeviceNameService::Get ()->GetDevice (s_pVolumeName[pdrv], TRUE);
			if (pDevice != 0)
			{
				u64 ullSize = pDevice->GetSize ();
				if (ullSize == (u64) -1)
				{
					return RES_PARERR;
				}
				if (ullSize == 0)
				{
					return RES_NOTRDY;
				}

				*(LBA_t *) buff = (LBA_t) (ullSize / SECTOR_SIZE);
			}
			else
			{
				return RES_NOTRDY;
			}
		}
		return RES_OK;

	case CTRL_SYNC:
		{
			if (pdrv >= FF_VOLUMES)
			{
				return RES_PARERR;
			}

			CDevice *pDevice =
				CDeviceNameService::Get ()->GetDevice (s_pVolumeName[pdrv], TRUE);
			if (pDevice != 0)
			{
				if (bmc64_drive_e_usb (pdrv) && !circle_usb_presta ())
				{
					return RES_NOTRDY;
				}

				/* This fails, if unsupported, so ignore eventual errors. */
				pDevice->IOCtl (DEVICE_IOCTL_SYNC, 0);

				if (bmc64_drive_e_usb (pdrv))
				{
					circle_usb_restituisci ();
				}
			}
			else
			{
				return RES_NOTRDY;
			}
		}
		return RES_OK;

	case GET_SECTOR_SIZE:
		assert (buff != 0);
		*(WORD *) buff = SECTOR_SIZE;
		return RES_OK;

	case CTRL_EJECT:
		if (pdrv >= FF_VOLUMES)
		{
			return RES_PARERR;
		}

		if (s_pVolume[pdrv] == 0)
		{
			s_pVolume[pdrv] = CDeviceNameService::Get ()->GetDevice (s_pVolumeName[pdrv], TRUE);
			if (s_pVolume[pdrv] == 0)
			{
				return RES_NOTRDY;
			}
		}

		if (!s_pVolume[pdrv]->RemoveDevice ())
		{
			return RES_ERROR;
		}

		s_pVolume[pdrv] = 0;

		return RES_OK;
	}

	return RES_PARERR;
}

