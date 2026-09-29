/**
 * @file select.h
 * @author Stephan Mühlstrasser
 * @brief select implementation for Circle
 * 
 * @copyright Copyright (c) 2022
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#include <sys/_sigset.h>
#include <sys/_timeval.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_SIGSET_T_DECLARED)
#define	_SIGSET_T_DECLARED
typedef	__sigset_t	sigset_t;
#endif

typedef struct {
    int _x;
} fd_set;

int pselect(int nfds, fd_set *readfds,
            fd_set *writefds, fd_set *errorfds,
            const struct timespec *timeout,
            const sigset_t *sigmask);
int select(int nfds, fd_set *readfds,
           fd_set *writefds, fd_set *errorfds,
           struct timeval *timeout);

void FD_CLR(int fd, fd_set *fdset);
int FD_ISSET(int fd, fd_set *fdset);
void FD_SET(int fd, fd_set *fdset);
void FD_ZERO(fd_set *fdset);

#define FD_SETSIZE 32

#ifdef __cplusplus
}
#endif

#endif
