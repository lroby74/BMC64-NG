/*
 * Stub version of stat.
 */

#include "config.h"

#include "wrap_fatfs.h"

#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "circle_macros.h"
#include "circle_glue.h"
#include "cglueio.h"

extern "C" int
_stat(const char *file, struct stat *statbuf)
{
	FILINFO finfo;

	if (!file || strlen (file) == 0)
	{
		errno = ENOENT;
		return -1;
	}

	FRESULT const fr = f_stat (file, &finfo);
	if (fr != FR_OK)
	{
		switch (fr)
		{
		case FR_NO_FILE:
		case FR_INVALID_NAME:
			errno = ENOENT;
			break;

		case FR_NO_PATH:
			errno = ENOTDIR;
			break;

		default:
			errno = EIO;
			break;
		}
		
		return -1;
	}

	assert (statbuf);
	memset (statbuf, 0, sizeof(*statbuf));

	// Reading, write and executing allowed for all, corrected
	// below if readonly flag is set
	statbuf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;

	if (finfo.fattrib & AM_RDO)
	{
		// Readonly, remove write permissions for all
		statbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	}

	if (finfo.fattrib & AM_DIR)
	{
		statbuf->st_mode |= S_IFDIR;
	}
	else
	{
		statbuf->st_mode |= S_IFREG;
	}

	statbuf->st_dev = _CircleStdlib::CGlueIO::DeviceIdFatFs;
	statbuf->st_ino = 1000;
	statbuf->st_nlink = 1;
	statbuf->st_uid = 0;
	statbuf->st_gid = 0;
	statbuf->st_size = finfo.fsize;
	statbuf->st_blksize = 512;
	statbuf->st_blocks = (finfo.fsize + 511) / 512;

	return 0;
}

extern "C" int
lstat (char const * const file, struct stat * const statbuf)
{
	return _stat (file, statbuf);
}
