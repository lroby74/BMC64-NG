




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#undef HAVE_DCERPC_FULL

#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#define LIBSMB2_DCERPC_RESTORE_CONFIG_H 1
#endif
#include "libsmb2-dcerpc-prefix.h"
#include "../libdcerpc/dcerpc.c"
#ifdef LIBSMB2_DCERPC_RESTORE_CONFIG_H
#define HAVE_CONFIG_H 1
#endif
