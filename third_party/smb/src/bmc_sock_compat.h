#ifndef BMC_SOCK_COMPAT_H
#define BMC_SOCK_COMPAT_H

#include "bmc_sock.h"

#define readv bmc_sock_readv
#define writev bmc_sock_writev
#define close bmc_sock_close

#endif
