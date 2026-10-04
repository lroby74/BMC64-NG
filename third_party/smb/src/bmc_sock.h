#ifndef BMC_SOCK_H
#define BMC_SOCK_H

#include <stddef.h>
#include <sys/types.h>

#ifdef BMC_SOCK_IOVEC
#ifndef BMC_SOCK_IOVEC_DEFINED
#define BMC_SOCK_IOVEC_DEFINED
struct iovec {
	void *iov_base;
	size_t iov_len;
};
#endif
#else
#include <sys/uio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

ssize_t bmc_sock_readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t bmc_sock_writev(int fd, const struct iovec *iov, int iovcnt);
int bmc_sock_close(int fd);
void bmc_sock_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif
