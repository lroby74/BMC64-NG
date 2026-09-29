#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif

#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x800
#endif

extern "C"
{
    int fchmodat(int /*dirfd*/, const char * /*pathname*/, mode_t /*mode*/,
                 int /*flags*/)
    {
        errno = ENOSYS;
        return -1;
    }

    ssize_t readlink(const char * /*pathname*/, char * /*buf*/,
                     size_t /*bufsiz*/)
    {
        errno = ENOSYS;
        return -1;
    }

    int openat(int dirfd, const char *pathname, int flags, ...)
    {
        if (dirfd != AT_FDCWD)
        {
            errno = ENOTSUP;
            return -1;
        }
        return open(pathname, flags);
    }

    DIR *fdopendir(int /*fd*/)
    {
        errno = ENOSYS;
        return nullptr;
    }

    int unlinkat(int dirfd, const char *pathname, int flags)
    {
        if (dirfd != AT_FDCWD)
        {
            errno = ENOTSUP;
            return -1;
        }
        if (flags & AT_REMOVEDIR)
        {
            return rmdir(pathname);
        }
        return unlink(pathname);
    }

    int fstatat(int dirfd, const char *pathname, struct stat *buf,
                int /*flags*/)
    {
        if (dirfd != AT_FDCWD)
        {
            errno = ENOTSUP;
            return -1;
        }
        return stat(pathname, buf);
    }

    int utimes(const char * /*filename*/, const struct timeval /*times*/[2])
    {
        errno = ENOSYS;
        return -1;
    }

    int utimensat(int /*dirfd*/, const char * /*pathname*/,
                  const struct timespec /*times*/[2], int /*flags*/)
    {
        errno = ENOSYS;
        return -1;
    }

    int symlink(const char * /*target*/, const char * /*linkpath*/)
    {
        errno = ENOSYS;
        return -1;
    }

    int fchmod(int /*fd*/, mode_t /*mode*/)
    {
        errno = ENOSYS;
        return -1;
    }

    int rmdir(const char *pathname)
    {
        return unlink(pathname);
    }

    char *realpath(const char * /*path*/, char * /*resolved_path*/)
    {
        errno = ENOSYS;
        return nullptr;
    }

    long pathconf(const char * /*path*/, int /*name*/)
    {
        errno = ENOSYS;
        return -1;
    }
}
