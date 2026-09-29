#ifndef _NETDB_H
#define _NETDB_H

#include <sys/socket.h>

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

#define EAI_BADFLAGS   -1
#define EAI_NONAME     -2
#define EAI_AGAIN      -3
#define EAI_FAIL       -4
#define EAI_FAMILY     -6
#define EAI_SOCKTYPE   -7
#define EAI_SERVICE    -8
#define EAI_MEMORY     -10
#define EAI_SYSTEM     -11
#define EAI_OVERFLOW   -12

#define AI_PASSIVE     0x0001
#define AI_CANONNAME   0x0002
#define AI_NUMERICHOST 0x0004

/* Maxmimum lengths for `getnameinfo' function. */
#define NI_MAXHOST      1025
#define NI_MAXSERV      32

/* Possible values for `flags' for `getnameinfo' function. */
// #define NI_NUMERICHOST  1       /* Don't try to look up hostname. */
#define NI_NUMERICSERV  2       /* Don't convert port number to name. */
// #define NI_NOFQDN       4       /* Only return nodename portion. */
// #define NI_NAMEREQD     8       /* Don't return numeric addresses. */
#define NI_DGRAM        16      /* Look up UDP service rather than TCP. */

/* Possible values left in `h_errno'. */
#define HOST_NOT_FOUND  1       /* Authoritative Answer Host not found. */
#define TRY_AGAIN       2       /* Non-Authoritative Host not found, or SERVERFAIL. */
#define NO_RECOVERY     3       /* Non recoverable errors, FORMERR, REFUSED, NOTIMP. */
#define NO_DATA         4       /* Valid name, no data record of requested type. */

#ifdef __cplusplus
extern "C" {
#endif

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res);

void freeaddrinfo(struct addrinfo *res);

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *node, socklen_t nodelen,
                char *service, socklen_t servicelen,
                int flags);

const char *gai_strerror(int errcode);

#ifdef __cplusplus
}
#endif

#endif /* _NETDB_H */
