#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

extern "C"
const char *inet_ntop (int af, const void *src, char *dst, socklen_t size)
{
	if (af != AF_INET)
	{
		errno = EAFNOSUPPORT;

		return nullptr;
	}

	if (!src || !dst || !size)
	{
		errno = EINVAL;

		return nullptr;
	}

	const unsigned char *p = static_cast<const unsigned char *> (src);
	int res = snprintf (dst, size, "%hhu.%hhu.%hhu.%hhu", p[0], p[1], p[2], p[3]);

	if (   res < 0
	    || (socklen_t) res >= size)
	{
		errno = ENOSPC;

		return nullptr;
	}

	return dst;
}

extern "C"
int inet_pton (int af, const char *src, void *dst)
{
	if (af != AF_INET)
	{
		errno = EAFNOSUPPORT;

		return -1;
	}

	if (!src || !dst)
	{
		errno = EINVAL;

		return -1;
	}

	unsigned char *p = static_cast<unsigned char *> (dst);

	for (unsigned i = 0; i < 4; i++)
	{
		char *end = nullptr;
		unsigned long num = strtoul (src, &end, 10);

		if (i < 3)
		{
			if (!end || *end != '.')
			{
				return 0;
			}
		}
		else
		{
			if (end && *end != '\0')
			{
				return 0;
			}
		}

		if (num > 255)
		{
			return 0;
		}

		p[i] = (unsigned char) num;

		assert (end != 0);
		src = end + 1;
	}

	return 1;
}
