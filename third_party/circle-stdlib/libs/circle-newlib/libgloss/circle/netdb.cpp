#include "circle_macros.h"

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <circle/net/dnsclient.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/ipaddress.h>
#include <circle/types.h>
#include <arpa/inet.h>

namespace _CircleStdlib
{
    extern CNetSubSystem *pCNet;
}

extern "C" int getaddrinfo(char const *node, char const *service,
                           struct addrinfo const *hints,
                           struct addrinfo **res)
{
    assert(_CircleStdlib::pCNet);

    if (node == nullptr && service == nullptr)
    {
        return EAI_NONAME;
    }

    struct addrinfo * const ai = static_cast<struct addrinfo *>(malloc(sizeof(struct addrinfo)));
    if (!ai) return EAI_MEMORY;
    memset(ai, 0, sizeof(struct addrinfo));
    ai->ai_addr = nullptr;
    ai->ai_canonname = nullptr;
    ai->ai_next = nullptr;

    struct sockaddr_in * const sa = static_cast<struct sockaddr_in *>(malloc(sizeof(struct sockaddr_in)));
    if (!sa)
    {
        free(ai);
        return EAI_MEMORY;
    }
    memset(sa, 0, sizeof(struct sockaddr_in));

    sa->sin_family = AF_INET;
    ai->ai_family = AF_INET;
    
    if (hints)
    {
        ai->ai_socktype = hints->ai_socktype;
        ai->ai_protocol = hints->ai_protocol;
    }
    else
    {
        ai->ai_socktype = SOCK_STREAM;
        ai->ai_protocol = 0;
    }
    
    ai->ai_addr = reinterpret_cast<struct sockaddr *>(sa);
    ai->ai_addrlen = sizeof(struct sockaddr_in);

    // 1. Resolve Service (port)
    if (service != nullptr)
    {
        // Simple numeric port string evaluation.
        sa->sin_port = htons(atoi(service));
    }
    else
    {
        sa->sin_port = 0;
    }

    // 2. Resolve Node (Hostname or IP)
    if (node != nullptr)
    {
        // CDNSClient handles hostname to IP resolution
        CDNSClient dns(_CircleStdlib::pCNet);
        CIPAddress ip;
        
        // dns.Resolve gracefully handles either a string IP like "192.168.0.1"
        // or a hostname like "google.com" implicitly querying the DNS server.
        if (dns.Resolve(node, &ip))
        {
            sa->sin_addr.s_addr = static_cast<in_addr_t>(ip);
        }
        else
        {
            free(sa);
            free(ai);
            return EAI_FAIL;
        }
    }
    else
    {
        in_addr_t const addr = (hints && (hints->ai_flags & AI_PASSIVE)) ? INADDR_ANY : INADDR_LOOPBACK;
        sa->sin_addr.s_addr = htonl(addr);
    }

    *res = ai;
    return 0;
}

extern "C" void freeaddrinfo(struct addrinfo *res)
{
    while (res != nullptr)
    {
        struct addrinfo * const next = res->ai_next;
        if (res->ai_addr)
        {
            free(res->ai_addr);
        }
        if (res->ai_canonname)
        {
            free(res->ai_canonname);
        }
        free(res);
        res = next;
    }
}

extern "C" char const *gai_strerror(int errcode)
{
    switch (errcode)
    {
    case EAI_BADFLAGS:   return "Invalid value for ai_flags";
    case EAI_NONAME:     return "Name or service not known";
    case EAI_AGAIN:      return "Temporary failure in name resolution";
    case EAI_FAIL:       return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:     return "ai_family not supported";
    case EAI_SOCKTYPE:   return "ai_socktype not supported";
    case EAI_SERVICE:    return "Servname not supported for ai_socktype";
    case EAI_MEMORY:     return "Memory allocation failure";
    case EAI_SYSTEM:     return "System error";
    case EAI_OVERFLOW:   return "Argument buffer overflow";
    default:             return "Unknown error";
    }
}
