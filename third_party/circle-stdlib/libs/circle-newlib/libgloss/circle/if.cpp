#include <net/if.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static const char s_ifname[] = "eth0";

extern "C"
char *if_indextoname (unsigned ifindex, char *ifname)
{
	if (ifindex != 1)
	{
		errno = ENXIO;

		return nullptr;
	}

	assert (ifname);
	return strncpy (ifname, s_ifname, IF_NAMESIZE);
}

extern "C"
unsigned if_nametoindex (const char *ifname)
{
	assert (ifname);
	if (strcmp (ifname, s_ifname) != 0)
	{
		return 0;
	}

	return 1;
}


extern "C"
struct if_nameindex *if_nameindex(void)
{
	struct if_nameindex * const ifni = static_cast<struct if_nameindex *> (malloc (2 * sizeof (struct if_nameindex)));
	if (ifni == nullptr)
	{
		errno = ENOBUFS;
		return nullptr;
	}

	char * const name = static_cast<char *> (malloc (sizeof (s_ifname)));
	if (name == nullptr)
	{
		free (ifni);
		errno = ENOBUFS;
		return nullptr;
	}

	strncpy (name, s_ifname, sizeof (s_ifname));

	ifni[0].if_index = 1;
	ifni[0].if_name = name;

	ifni[1].if_index = 0;
	ifni[1].if_name = nullptr;

	return ifni;
}

extern "C"
void if_freenameindex(struct if_nameindex *ptr)
{
	if (ptr == nullptr)
	{
		return;
	}

	while (ptr->if_index != 0 || ptr->if_name != nullptr)
	{
		if (ptr->if_name != nullptr)
		{
			free (ptr->if_name);
		}
		ptr += 1;
	}

	free (ptr);
}