#include "config.h"

#include "wrap_fatfs.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>

extern "C"
char *getcwd(char *buf, size_t size)
{
    if (size == 0)
    {
        errno = EINVAL;
        return nullptr;
    }

    auto const f_result = f_getcwd(buf, size);

    char *result;
    if (f_result == FR_OK)
    {
        result = buf;

        // Remove the drive prefix before the first '/'
        char * const slash = strchr(result, '/');
        if (slash != nullptr)
        {
            memmove(result, slash, strlen(slash) + 1);
        }
    }
    else
    {
        result = nullptr;

        switch (f_result)
        {
        case FR_NOT_ENOUGH_CORE:
            errno = ERANGE;
            break;

        default:
            errno = EACCES;
            break;
        }
    }

	return result;
}
