#include "config.h"

#include "wrap_fatfs.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>

extern "C"
int
chdir (const char *path)
{
    if (path == nullptr || strlen(path) == 0)
    {
        errno = ENOENT;
        return -1;
    }

    auto const fatfs_result = f_chdir (path);
    int const result = fatfs_result == FR_OK ? 0 : -1;

    if (result == -1)
	{
        errno = ENOENT;
	}

	return result;
}
