#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS
int getentropy(void *, size_t);
__END_DECLS

#endif /* _SYS_RANDOM_H */
