#ifndef _NETINET_IN_H_
#define _NETINET_IN_H_

#include <inttypes.h>
#include <bits/sockettypes.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr
{
    in_addr_t s_addr;
};

/* The sin_port and sin_addr members shall be in network byte order (big-endia). */
struct sockaddr_in
{
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
};

/* Multicast not implemented yet, only for being able to compile currently. */
struct ip_mreq
{
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
};

struct ip_mreqn
{
    struct in_addr imr_multiaddr;
    struct in_addr imr_address;
    int imr_ifindex;
};

struct ip_mreq_source
{
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
    struct in_addr imr_sourceaddr;
};

#define IPPROTO_IP 0        /* Internet protocol. */
#define IPPROTO_ICMP 1      /* Control message protocol. */
#define IPPROTO_TCP 6       /* Transmission control protocol. */
#define IPPROTO_UDP 17      /* User datagram protocol. */
#define IPPROTO_IPV6 41     /* Internet Protocol Version 6. */
#define IPPROTO_ICMPV6 58   /* Control message protocol Version 6. */
#define IPPROTO_RAW 255     /* Raw IP Packets Protocol. */

#define INADDR_ANY ((in_addr_t)0x00000000)       /* IPv4 wildcard address. */
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001)  /* IPv4 local host address. */
#define INADDR_BROADCAST ((in_addr_t)0xffffffff) /* IPv4 broadcast address. */

#define INET_ADDRSTRLEN 16 /* Length of the string form for IP. */

#define IP_ADD_MEMBERSHIP 1
#define IP_ADD_SOURCE_MEMBERSHIP (IP_ADD_MEMBERSHIP + 1)
#define IP_BIND_ADDRESS_NO_PORT (IP_ADD_MEMBERSHIP + 2)
#define IP_BLOCK_SOURCE (IP_ADD_MEMBERSHIP + 3)
#define IP_DROP_MEMBERSHIP (IP_ADD_MEMBERSHIP + 4)
#define IP_DROP_SOURCE_MEMBERSHIP (IP_ADD_MEMBERSHIP + 5)
#define IP_FREEBIND (IP_ADD_MEMBERSHIP + 6)
#define IP_HDRINCL (IP_ADD_MEMBERSHIP + 7)
#define IP_LOCAL_PORT_RANGE (IP_ADD_MEMBERSHIP + 8)
#define IP_MSFILTER (IP_ADD_MEMBERSHIP + 9)
#define IP_MTU (IP_ADD_MEMBERSHIP + 10)
#define IP_MTU_DISCOVER (IP_ADD_MEMBERSHIP + 11)
#define IP_MULTICAST_ALL (IP_ADD_MEMBERSHIP + 12)
#define IP_MULTICAST_IF (IP_ADD_MEMBERSHIP + 13)
#define IP_MULTICAST_LOOP (IP_ADD_MEMBERSHIP + 14)
#define IP_MULTICAST_TTL (IP_ADD_MEMBERSHIP + 15)
#define IP_NODEFRAG (IP_ADD_MEMBERSHIP + 16)
#define IP_OPTIONS (IP_ADD_MEMBERSHIP + 17)
#define IP_PASSSEC (IP_ADD_MEMBERSHIP + 18)
#define IP_PKTINFO (IP_ADD_MEMBERSHIP + 19)
#define IP_RECVERR (IP_ADD_MEMBERSHIP + 20)
#define IP_RECVOPTS (IP_ADD_MEMBERSHIP + 21)
#define IP_RECVORIGDSTADDR (IP_ADD_MEMBERSHIP + 22)
#define IP_RECVTOS (IP_ADD_MEMBERSHIP + 23)
#define IP_RECVTTL (IP_ADD_MEMBERSHIP + 24)
#define IP_RETOPTS (IP_ADD_MEMBERSHIP + 25)
#define IP_ROUTER_ALERT (IP_ADD_MEMBERSHIP + 26)
#define IP_TOS (IP_ADD_MEMBERSHIP + 27)
#define IPIP_TRANSPARENT_TOS (IP_ADD_MEMBERSHIP + 28)
#define IP_TTL (IP_ADD_MEMBERSHIP + 29)
#define IP_UNBLOCK_SOURCE (IP_ADD_MEMBERSHIP + 30)
#define SO_PEERSEC (IP_ADD_MEMBERSHIP + 31)

/* The following IPv6 definitions are not really supported,
   but are necessary to compile Asio: */

struct in6_addr
{
    union
    {
        uint8_t u6_addr8[16];
    }
    in6_u;
#define s6_addr in6_u.u6_addr8
};

struct sockaddr_in6
{
    uint16_t        sin6_family;
    uint16_t        sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

struct ipv6_mreq
{
        struct in6_addr ipv6mr_multiaddr;
        unsigned        ipv6mr_interface;
};

#define IPV6_UNICAST_HOPS (IP_ADD_MEMBERSHIP + 32)
#define IPV6_MULTICAST_IF (IP_ADD_MEMBERSHIP + 33)
#define IPV6_MULTICAST_HOPS (IP_ADD_MEMBERSHIP + 34)
#define IPV6_MULTICAST_LOOP (IP_ADD_MEMBERSHIP + 35)
#define IPV6_JOIN_GROUP (IP_ADD_MEMBERSHIP + 36)
#define IPV6_LEAVE_GROUP (IP_ADD_MEMBERSHIP + 37)

#endif
