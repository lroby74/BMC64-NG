#ifndef _CIRCLE_STDLIB_SYS_POLL_H_
#define _CIRCLE_STDLIB_SYS_POLL_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An unsigned integer type used for the number of file descriptors.
 */
typedef unsigned int nfds_t;

/**
 * Structure describing a polling request.
 */
struct pollfd
{
    int fd;        ///< The descriptor being polled.
    short events;  ///< The input event flags (see below).
    short revents; ///< The output event flags (see below).
};

/**
 * Symbolic constants for the events and revents members of struct pollfd.
 * 
 * Zero or more of the following symbolic constants may be OR'ed together
 * to form the events or revents members in the pollfd structure:
 */
#define POLLIN (1 << 0)     ///< Data other than high-priority data may be read without blocking.
#define POLLRDNORM (1 << 1) ///< Normal data may be read without blocking.
#define POLLRDBAND (1 << 2) ///< Priority data may be read without blocking.
#define POLLPRI (1 << 3)    ///< High priority data may be read without blocking.
#define POLLOUT (1 << 4)    ///< Normal data may be written without blocking.
#define POLLWRNORM (1 << 5) ///< Equivalent to POLLOUT.
#define POLLWRBAND (1 << 6) ///< Priority data may be written.
#define POLLERR (1 << 7)    ///< An error has occurred (revents only).
#define POLLHUP (1 << 8)    ///< Device has been disconnected (revents only).
#define POLLNVAL (1 << 9)   ///< Invalid fd member (revents only).

int poll(struct pollfd[], nfds_t, int);

#ifdef __cplusplus
}
#endif

#endif