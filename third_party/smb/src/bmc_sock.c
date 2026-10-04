#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "bmc_sock.h"

#ifdef BMC_SOCK_CIRCLE



int bmc_sock_chiudi(int fd);
#define CHIUDI_PRESA(fd) bmc_sock_chiudi(fd)
#else
#define CHIUDI_PRESA(fd) close(fd)
#endif

#define BS_STAGE 1600
#define BS_FDS 64
#define BS_GATHER 8192

struct bs_stage {
	size_t pos;
	size_t len;
	unsigned char buf[BS_STAGE];
};

static struct bs_stage bs_st[BS_FDS];
static unsigned char bs_gather[BS_GATHER];

static struct bs_stage *bs_get(int fd)
{
	if (fd < 0 || fd >= BS_FDS) {
		return NULL;
	}
	return &bs_st[fd];
}

ssize_t bmc_sock_readv(int fd, const struct iovec *iov, int iovcnt)
{
	struct bs_stage *st = bs_get(fd);
	ssize_t total = 0;
	int i;

	if (st == NULL) {
		errno = EBADF;
		return -1;
	}
	for (i = 0; i < iovcnt; i++) {
		size_t off = 0;

		while (off < iov[i].iov_len) {
			size_t n;

			if (st->pos == st->len) {
				ssize_t got = recv(fd, st->buf, BS_STAGE, MSG_DONTWAIT);

				if (got <= 0) {
					st->pos = 0;
					st->len = 0;
					return total > 0 ? total : got;
				}
				st->pos = 0;
				st->len = (size_t)got;
			}
			n = st->len - st->pos;
			if (n > iov[i].iov_len - off) {
				n = iov[i].iov_len - off;
			}
			memcpy((unsigned char *)iov[i].iov_base + off, st->buf + st->pos, n);
			st->pos += n;
			off += n;
			total += (ssize_t)n;
		}
	}
	return total;
}

static int bs_send_all(int fd, const unsigned char *p, size_t len)
{
	while (len > 0) {
		ssize_t n = send(fd, p, len, 0);

		if (n <= 0) {
			return -1;
		}
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

ssize_t bmc_sock_writev(int fd, const struct iovec *iov, int iovcnt)
{
	size_t used = 0;
	ssize_t total = 0;
	int i;

	for (i = 0; i < iovcnt; i++) {
		const unsigned char *p = iov[i].iov_base;
		size_t len = iov[i].iov_len;

		if (len >= BS_GATHER) {
			if (used > 0) {
				if (bs_send_all(fd, bs_gather, used) != 0) {
					return total > 0 ? total : -1;
				}
				total += (ssize_t)used;
				used = 0;
			}
			if (bs_send_all(fd, p, len) != 0) {
				return total > 0 ? total : -1;
			}
			total += (ssize_t)len;
			continue;
		}
		while (len > 0) {
			size_t n = BS_GATHER - used;

			if (n > len) {
				n = len;
			}
			memcpy(bs_gather + used, p, n);
			used += n;
			p += n;
			len -= n;
			if (used == BS_GATHER) {
				if (bs_send_all(fd, bs_gather, used) != 0) {
					return total > 0 ? total : -1;
				}
				total += (ssize_t)used;
				used = 0;
			}
		}
	}
	if (used > 0) {
		if (bs_send_all(fd, bs_gather, used) != 0) {
			return total > 0 ? total : -1;
		}
		total += (ssize_t)used;
	}
	return total;
}

int bmc_sock_close(int fd)
{
	struct bs_stage *st = bs_get(fd);

	if (st != NULL) {
		st->pos = 0;
		st->len = 0;
	}
	return CHIUDI_PRESA(fd);
}

void bmc_sock_reset_all(void)
{
	memset(bs_st, 0, sizeof bs_st);
}
