#ifndef _NET_IF_H_
#define _NET_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

struct if_nameindex {
    unsigned int if_index; // Numeric index of the interface.
    char *if_name;         // Null-terminated name of the interface.
};

#define IF_NAMESIZE 16

char *if_indextoname(unsigned ifindex, char *ifname);
unsigned if_nametoindex(const char *ifname);
struct if_nameindex *if_nameindex(void);
void if_freenameindex(struct if_nameindex *);

#ifdef __cplusplus
}
#endif

#endif
