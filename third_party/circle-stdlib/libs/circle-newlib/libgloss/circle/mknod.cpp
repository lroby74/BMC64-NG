#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

extern "C" int
mknod (char const * const pathname, mode_t const /* mode */, dev_t const /* dev */)
{
	if (pathname == nullptr || pathname[0] == '\0')
	{
		errno = ENOENT;
		return -1;
	}

	struct stat st;
	if (stat (pathname, &st) == 0)
	{
		errno = EEXIST;
		return -1;
	}

	if (errno == ENOTDIR)
	{
		errno = ENOENT;
		return -1;
	}

	int const fd = open (pathname, O_CREAT | O_WRONLY, 0666);
	if (fd >= 0)
	{
		close (fd);
		return 0;
	}

	errno = EACCES;
	return -1;
}