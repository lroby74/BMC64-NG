#include "circle_macros.h"

#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/net/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "cglueio.h"
#include "filetable.h"
#include "circlenetmap.h"

#include <circle/logger.h>

namespace _CircleStdlib
{
    static_assert(sizeof(struct sockaddr_in) <= sizeof(struct sockaddr_storage),
                  "sockaddr_storage must be large enough for sockaddr_in");

    CNetSubSystem *pCNet = nullptr;

    /**
     * Map Circle network error codes to errno values.
     */
    static int MapCircleNetErrorToErrno(int circleError)
    {
        switch (-circleError)
        {
        case NET_ERROR_WOULD_BLOCK:
            return EWOULDBLOCK;

        case NET_ERROR_PERMISSION_DENIED:
            return EACCES;

        case NET_ERROR_INVALID_VALUE:
            return EINVAL;

        case NET_ERROR_PROTOCOL_ERROR:
            return EPROTO;

        case NET_ERROR_PROTOCOL_NOT_SUPPORTED:
            return EPROTONOSUPPORT;

        case NET_ERROR_OPERATION_NOT_SUPPORTED:
            return EOPNOTSUPP;

        case NET_ERROR_CONNECTION_RESET:
            return ECONNRESET;

        case NET_ERROR_IS_CONNECTED:
            return EISCONN;

        case NET_ERROR_NOT_CONNECTED:
            return ENOTCONN;

        case NET_ERROR_CONNECTION_TIMED_OUT:
            return ETIMEDOUT;

        case NET_ERROR_CONNECTION_REFUSED:
            return ECONNREFUSED;

        case NET_ERROR_DESTINATION_UNREACHABLE:
            return EHOSTUNREACH;

        default:
            return EIO;
        }
    }

    /**
     * Posix sockets
     */
    struct CGlueIoSocket : public CGlueIO
    {
        enum socket_state
        {
            socket_state_new,
            socket_state_bound,
            socket_state_listening,
            socket_state_connected
        };

        CGlueIoSocket(int nProtocol)
            : mSocket(new CSocket(pCNet, nProtocol)),
              mState(socket_state_new)
        {
        }

        CGlueIoSocket(CSocket *acceptedSocket)
            : mSocket(acceptedSocket),
              mState(socket_state_connected)
        {
            assert(acceptedSocket);
        }

        ~CGlueIoSocket()
        {
            delete mSocket;
        }

        int
        LSeek(int ptr, int dir)
        {
            return -1;
        }

        int
        Close(void)
        {
            CScheduler::Get()->Yield();

            // Nothing to do here. The Circle socket object will be
            // implicitly closed and destroyed when the CGlueIO object
            // is destroyed.
            return 0;
        }

        int
        FStat(struct stat *buf)
        {
            assert(buf);
            memset(buf, 0, sizeof(*buf));

            // Just some arbitrary but fixed values.
            buf->st_dev = _CircleStdlib::CGlueIO::DeviceIdSocket;
            buf->st_ino = 2000;
            buf->st_nlink = 1;

            // The important flag is S_IFCHR. This is needed by newlib
            // internally to recognize that this is a TTY.
            buf->st_mode = S_IRUSR | S_IWUSR | S_IFSOCK;

            return 0;
        }

        int
        IsATty(void)
        {
            errno = ENOTTY;
            return -1;
        }

        int
        Bind(const struct sockaddr *sa,
             socklen_t len)
        {
            if (sa->sa_family != AF_INET)
            {
                errno = EOPNOTSUPP;
                return -1;
            }

            if (len != sizeof(struct sockaddr_in) || mState != socket_state_new)
            {
                errno = EINVAL;
                return -1;
            }

            const struct sockaddr_in *const sa_in = reinterpret_cast<const struct sockaddr_in *>(sa);

#if 0
            // Circle has no means of binding to a specific interface.
            // For compatibility reasons the s_addr member is ignored for the time being.
            if (sa_in->sin_addr.s_addr != htonl(INADDR_ANY))
            {
                errno = EADDRNOTAVAIL;
                return -1;
            }
#endif

            /*
             * Circle expects the port in little-endian representation, e.g. in host byte order.
             * The socket interface requires the port in network byte order. Therefore we
             * have to convert to host byte order here.
             */
            int bind_result = mSocket->Bind(ntohs(sa_in->sin_port));

            if (bind_result < 0)
            {
                errno = MapCircleNetErrorToErrno(bind_result);
                bind_result = -1;
            }
            else
            {
                mState = socket_state_bound;
                bind_result = 0;
            }

            CScheduler::Get()->Yield();

            return bind_result;
        }

        int
        Listen(int backlog)
        {
            if (mState == socket_state_connected)
            {
                errno = EINVAL;
                return -1;
            }

            // The listen() function is documented with the following behaviors:
            //
            // "The implementation may have an upper limit on the length of the
            // listen queue-either global or per accepting socket. If backlog
            // exceeds this limit, the length of the listen queue is set to the limit.
            //
            // If listen() is called with a backlog argument value that is less
            // than 0, the function behaves as if it had been called with a backlog
            // argument value of 0.
            //
            // A backlog argument of 0 may allow the socket to accept connections,
            // in which case the length of the listen queue may be set to an
            // implementation-defined minimum value."
            //
            // Circle's Listen() fails with a backlog of 0, so we use 1 as the
            // implementation-defined minimum value.
            unsigned int const ubacklog =
                backlog < 1
                    ? 1
                    : (backlog > SOMAXCONN
                           ? SOMAXCONN
                           : backlog);

            int listen_result = mSocket->Listen(ubacklog);
            if (listen_result < 0)
            {
                errno = MapCircleNetErrorToErrno(listen_result);
                listen_result = -1;
            }
            else
            {
                mState = socket_state_listening;
                listen_result = 0;
            }

            CScheduler::Get()->Yield();

            return listen_result;
        }

        int Accept(struct sockaddr *address, socklen_t *address_len)
        {
            if (mState != socket_state_listening)
            {
                errno = EINVAL;
                return -1;
            }

            CIPAddress ForeignIP;
            u16 nForeignPort;
            CSocket *const pConnection = mSocket->Accept(&ForeignIP, &nForeignPort);

            if (!pConnection)
            {
                errno = EINVAL;
                return -1;
            }

            _CircleStdlib::CircleFile *circleFile = nullptr;
            int const slot = _CircleStdlib::FileTable::FindFreeFileSlot(circleFile);

            if (slot != -1)
            {
                assert(circleFile != nullptr);

                auto const new_socket = new _CircleStdlib::CGlueIoSocket(pConnection);
                circleFile->AssignGlueIO(*new_socket);

                if (address && address_len && *address_len > 0)
                {
                    struct sockaddr_in sockaddr;
                    socklen_t out_len = sizeof(struct sockaddr_in);
                    socklen_t const in_len = *address_len;

                    *address_len = out_len;
                    if (out_len > in_len)
                    {
                        out_len = in_len;
                    }

                    sockaddr.sin_family = AF_INET;
                    sockaddr.sin_addr.s_addr = (u32)ForeignIP;
                    sockaddr.sin_port = nForeignPort;

                    memcpy(address, &sockaddr, out_len);
                }
            }
            else
            {
                delete pConnection;
                errno = ENFILE;
            }

            CScheduler::Get()->Yield();

            return slot;
        }

        int
        Connect(const struct sockaddr *address, socklen_t address_len)
        {
            if (address->sa_family != AF_INET || mState == socket_state_listening)
            {
                errno = EOPNOTSUPP;
                return -1;
            }

            if (address_len != sizeof(struct sockaddr_in))
            {
                errno = EINVAL;
                return -1;
            }

            if (mState == socket_state_connected)
            {
                errno = EISCONN;
                return -1;
            }

            const struct sockaddr_in *const in_addr = reinterpret_cast<const struct sockaddr_in *>(address);

            CIPAddress circle_address{in_addr->sin_addr.s_addr};
            u16 const circle_port = ntohs(in_addr->sin_port);

            int result = mSocket->Connect(circle_address, circle_port);
            if (result < 0)
            {
                errno = MapCircleNetErrorToErrno(result);
                result = -1;
            }
            else
            {
                mState = socket_state_connected;
                result = 0;
            }

            CScheduler::Get()->Yield();

            return result;
        }

        int
        Read(void *pBuffer, int nCount)
        {
            assert(mSocket);

            if (mState != socket_state_connected)
            {
                errno = ENOTCONN;
                return -1;
            }

            int result = mSocket->Receive(pBuffer, nCount, 0);

            if (result < 0)
            {
                errno = MapCircleNetErrorToErrno(result);
                result = -1;
            }

            CScheduler::Get()->Yield();

            return result;
        }

        int
        Write(const void *pBuffer, int nCount)
        {
            assert(mSocket);

            if (mState != socket_state_connected)
            {
                errno = ECONNRESET;
                return -1;
            }

            int result = mSocket->Send(pBuffer, nCount, 0);

            if (result < 0)
            {
                errno = MapCircleNetErrorToErrno(result);
                result = -1;
            }

            CScheduler::Get()->Yield();

            return result;
        }

        TStatus GetSelectStatus(void) const
        {
            assert(mSocket);

            CSocket::TStatus status = mSocket->GetStatus();
            return {status.bConnected, status.bRxReady, status.bTxReady, status.bException};
        }

        CSocket *mSocket;
        socket_state mState;
    };
}

void CGlueNetworkInit(CNetSubSystem &rNetwork)
{
    assert(!_CircleStdlib::pCNet);
    _CircleStdlib::pCNet = &rNetwork;
}

namespace
{
    /**
     * A wrapper for checking that the socket is valid.
     */
    template <typename Func>
    int ValidateAndExecute(int socket, Func operation)
    {
        _CircleStdlib::FileTable::FileTableLock fileTabLock;

        _CircleStdlib::CircleFile *const socket_file = _CircleStdlib::FileTable::GetFile(socket);

        if (!socket_file || !socket_file->IsOpen())
        {
            errno = EBADF;
            return -1;
        }

        _CircleStdlib::CGlueIO *const glueIO = socket_file->GetGlueIO();
        assert(glueIO);
        _CircleStdlib::CGlueIoSocket *const socketGlueIO = dynamic_cast<_CircleStdlib::CGlueIoSocket *>(glueIO);
        if (!socketGlueIO)
        {
            errno = ENOTSOCK;
            return -1;
        }

        return operation(socketGlueIO);
    }

    /**
     * Warn about untested socket functions.
     */
    [[maybe_unused]]
    void WarnUntestedSocketFunction(const char *functionName)
    {
        CLogger::Get()->Write("circle-stdlib socket", LogWarning,
                              "Socket function %s is untested!", functionName);
    }

    /**
     * Warn about unimplemented socket functions.
     */
    void WarnUnimplementedSocketFunction(const char *functionName)
    {
        CLogger::Get()->Write("circle-stdlib socket", LogWarning,
                              "Socket function %s is unimplemented!", functionName);
    }

    /**
     * Warn about unsupported flags.
     */
    void WarnUnsupportedSocketFlags(const char *functionName, int flags, int supportedFlags)
    {
        int const unimplementedFlags = flags & ~supportedFlags;
        if (unimplementedFlags)
        {
            CLogger::Get()->Write("circle-stdlib socket", LogWarning,
                                  "Socket function %s called with unimplemented flags 0x%X!", functionName, flags & ~supportedFlags);
        }
    }

}

extern "C" int accept(int socket, struct sockaddr *address,
                      socklen_t *address_len)
{
    return ValidateAndExecute(socket, [address, address_len](_CircleStdlib::CGlueIoSocket *glueIO)
                              { return glueIO->Accept(address, address_len); });
}

extern "C" int bind(int socket, const struct sockaddr *address,
                    socklen_t address_len)
{
    return ValidateAndExecute(socket, [address, address_len](_CircleStdlib::CGlueIoSocket *glueIO)
                              { return glueIO->Bind(address, address_len); });
}

extern "C" int connect(int socket, const struct sockaddr *address,
                       socklen_t address_len)
{
    return ValidateAndExecute(socket, [address, address_len](_CircleStdlib::CGlueIoSocket *glueIO)
                              { return glueIO->Connect(address, address_len); });
}

extern "C" int getpeername(int socket, struct sockaddr *address,
                           socklen_t *address_len)
{
    WarnUnimplementedSocketFunction(__func__);
    errno = ENOSYS;
    return -1;
}

extern "C" int getsockname(int socket, struct sockaddr *address,
                           socklen_t *address_len)
{
    return ValidateAndExecute(socket, [address, address_len](_CircleStdlib::CGlueIoSocket *glueIO)
                              {
        /*
        * From the Opengroup documentation about getsockname():
        *
        * The getsockname() function shall retrieve the locally-bound name
        * of the specified socket, store this address in the sockaddr structure
        * pointed to by the address argument, and store the length of this
        * address in the object pointed to by the address_len argument.
        *
        * If the actual length of the address is greater than the length of
        * the supplied sockaddr structure, the stored address shall be truncated.
        *
        * The Linux documentation makes it clear that on entry *address_len
        * shall contain the length of the amount of space pointed to by address.
        */
        CIPAddress const * const p_circle_ip_addr{_CircleStdlib::pCNet->GetConfig()->GetIPAddress()};
        u32 const circle_ip_addr{*p_circle_ip_addr};
        in_addr const socket_ip_addr{circle_ip_addr};
        sockaddr_in const in_addr{
            AF_INET,
            htons(glueIO->mSocket->GetOwnPort()),
            socket_ip_addr
        };

        socklen_t const dest_len = *address_len;
        *address_len = sizeof(sockaddr_in);

        size_t const copy_len = sizeof(sockaddr_in) > dest_len ?
                                    dest_len : sizeof(sockaddr_in);

        memcpy(address, &in_addr, copy_len);

        return 0; });
}

extern "C" int getsockopt(int socket, int level, int option_name,
                          void *option_value, socklen_t *option_len)
{
    WarnUnimplementedSocketFunction(__func__);
    errno = ENOSYS;
    return -1;
}

extern "C" int listen(int socket, int backlog)
{
    return ValidateAndExecute(socket, [backlog](_CircleStdlib::CGlueIoSocket *glueIO)
                              { return glueIO->Listen(backlog); });
}

namespace _CircleStdlib
{
    ssize_t DoRecvFrom(const char *func, int socket, void *buffer, size_t length,
                       int flags, struct sockaddr *address, socklen_t *address_len)
    {
        if (flags & MSG_OOB)
        {
            // According to Posix EINVAL is a valid behavior:
            // "The MSG_OOB flag is set and no out-of-band data is available."
            errno = EINVAL;
            return -1;
        }

        constexpr int supported_flags = MSG_DONTWAIT;

        WarnUnsupportedSocketFlags(func, flags, supported_flags);

        auto const recv_from = [buffer, length, flags, address, address_len](_CircleStdlib::CGlueIoSocket *glueIO)
        {
            int circle_flags = 0;
            if (flags & MSG_DONTWAIT)
            {
                circle_flags |= _CircleStdlib::CircleNetMap::C_MSG_DONTWAIT;
            }

            CIPAddress ForeignIP;
            u16 usForeignPort;
            CIPAddress *const pForeignIP = address ? &ForeignIP : nullptr;
            u16 *const pUsForeignPort = address ? &usForeignPort : nullptr;
            int result = glueIO->mSocket->ReceiveFrom(buffer, length, circle_flags, pForeignIP, pUsForeignPort);
            if (result >= 0 && address && address_len && *address_len > 0)
            {
                struct sockaddr_in sockaddr;
                socklen_t out_len = sizeof(struct sockaddr_in);
                socklen_t const in_len = *address_len;

                *address_len = out_len;
                if (out_len > in_len)
                {
                    out_len = in_len;
                }

                sockaddr.sin_family = AF_INET;
                sockaddr.sin_addr.s_addr = static_cast<in_addr_t>(ForeignIP);
                sockaddr.sin_port = htons(usForeignPort);

                memcpy(address, &sockaddr, out_len);
            }

            if (result == -NET_ERROR_CONNECTION_RESET)
            {
                // According to POSIX, recv() and recvfrom() shall return 0
                // when the connection has been closed by the peer.
                result = 0;
            }
            else if (result < 0)
            {
                errno = MapCircleNetErrorToErrno(result);
                result = -1;
            }
            else if ((flags & MSG_DONTWAIT) && result == 0)
            {
                errno = EWOULDBLOCK;
                result = -1;
            }

            return result;
        };

        return static_cast<ssize_t>(ValidateAndExecute(socket, recv_from));
    }
}

extern "C" ssize_t recv(int socket, void *buffer, size_t length, int flags)
{
    return _CircleStdlib::DoRecvFrom(__func__, socket, buffer, length, flags, nullptr, nullptr);
}

extern "C" ssize_t recvfrom(int socket, void *buffer, size_t length,
                            int flags, struct sockaddr *address, socklen_t *address_len)
{
    return _CircleStdlib::DoRecvFrom(__func__, socket, buffer, length, flags, address, address_len);
}

extern "C" ssize_t recvmsg(int socket, struct msghdr *message, int flags)
{
    WarnUnimplementedSocketFunction(__func__);
    errno = ENOSYS;
    return -1;
}

extern "C" ssize_t send(int socket, const void *message, size_t length, int flags)
{
    constexpr int supported_flags = MSG_DONTWAIT;

    WarnUnsupportedSocketFlags(__func__, flags, supported_flags);

    int circle_flags = 0;
    if (flags & MSG_DONTWAIT)
    {
        circle_flags |= _CircleStdlib::CircleNetMap::C_MSG_DONTWAIT;
    }

    return ValidateAndExecute(socket, [message, length, circle_flags](_CircleStdlib::CGlueIoSocket *glueIO)
                              {
        int result = glueIO->mSocket->Send(message, static_cast<unsigned int>(length), circle_flags);
        if (result < 0)
        {
            errno = _CircleStdlib::MapCircleNetErrorToErrno(result);
            result = -1;
        }
        return result; });
}

extern "C" ssize_t sendmsg(int socket, const struct msghdr *message, int flags)
{
    WarnUnimplementedSocketFunction(__func__);
    errno = ENOSYS;
    return -1;
}

extern "C" ssize_t sendto(int socket, const void *message, size_t length, int flags,
                          const struct sockaddr *dest_addr, socklen_t dest_len)
{
    // Circle supports MSG_DONTWAIT, but this is not documented for sendto().
    constexpr int supported_flags = 0;
    WarnUnsupportedSocketFlags(__func__, flags, supported_flags);

    CIPAddress circle_address;
    u16 circle_port = 0;

    if (dest_addr)
    {
        const struct sockaddr_in *const in_addr = reinterpret_cast<const struct sockaddr_in *>(dest_addr);

        if (dest_addr->sa_family != AF_INET)
        {
            errno = EAFNOSUPPORT;
            return -1;
        }

        if (dest_len != sizeof(struct sockaddr_in))
        {
            errno = EINVAL;
            return -1;
        }

        circle_address.Set(in_addr->sin_addr.s_addr);
        circle_port = ntohs(in_addr->sin_port);
    }

    return ValidateAndExecute(socket, [&](_CircleStdlib::CGlueIoSocket *glueIO)
                              {
        int circle_result = glueIO->mSocket->SendTo(message, static_cast<unsigned int>(length), 0,
                                                   circle_address, circle_port);
        if (circle_result < 0)
        {
            errno = _CircleStdlib::MapCircleNetErrorToErrno(circle_result);
            circle_result = -1;
        }
    
        return circle_result; });
}

extern "C" int setsockopt(int socket, int level, int option_name,
                          const void *option_value, socklen_t option_len)
{
    return ValidateAndExecute(socket, [level, option_name, option_value, option_len](_CircleStdlib::CGlueIoSocket *glueIO)
                              {
        // TODO incomplete implementation
        int result = 0;
        CIPAddress ip_address;
        switch (level)
        {
        case SOL_SOCKET:
            switch (option_name)
            {
            case SO_REUSEADDR:
                // Circle sockets always reuse addresses.
                return 0;

            case SO_BROADCAST:
                if (option_len != sizeof(int))
                {
                    errno = EINVAL;
                    return -1;
                }
                result = glueIO->mSocket->SetOptionBroadcast(
                                            !!*static_cast<const int *>(option_value));
                if (result < 0)
                {
                    errno = _CircleStdlib::MapCircleNetErrorToErrno(result);
                    result = -1;
                }
                return result;

            default:
                WarnUnimplementedSocketFunction(__func__);
                break;
            }
            break;
            
        case IPPROTO_IP:
            switch (option_name)
            {
            case IP_ADD_MEMBERSHIP:
            case IP_DROP_MEMBERSHIP:
                // Both structs are possible parameters
                if (   option_len != sizeof(struct ip_mreqn)
                    && option_len != sizeof(struct ip_mreq))
                {
                    errno = EINVAL;
                    return -1;
                }
                // TODO: Access option_value->imr_multiaddr using struct definitions
                ip_address.Set(static_cast<const u8 *>(option_value));
                if (option_name == IP_ADD_MEMBERSHIP)
                {
                    result = glueIO->mSocket->SetOptionAddMembership(ip_address);
                }
                else
                {
                    result = glueIO->mSocket->SetOptionDropMembership(ip_address);
                }
                if (result < 0)
                {
                    errno = _CircleStdlib::MapCircleNetErrorToErrno(result);
                    result = -1;
                }
                return result;

            case IP_MULTICAST_LOOP:
                if (option_len != sizeof(int))
                {
                    errno = EINVAL;
                    return -1;
                }
                // Cannot be enabled in Circle
                if (*static_cast<const int *>(option_value))
                {
                    break;
                }
                return 0;

            case IP_MULTICAST_IF:
                return 0;

            default:
                WarnUnimplementedSocketFunction(__func__);
                break;
            }
            break;
        default:
            break;
        }

        CLogger::Get()->Write("circle-stdlib socket", LogDebug,
                              "setsockopt(%d, %d) is unimplemented", level, option_name);

        errno = ENOPROTOOPT;
        return -1; });
}

extern "C" int shutdown(int /* socket */, int /* how */)
{
    // Warning disabled because it clutters the output too much.
    // WarnUnimplementedSocketFunction(__func__);

    // For the time being this function returns 0 although it does nothing
    // for compatibility reasons.

    return 0;
}

extern "C" int socket(int domain, int type, int protocol)
{
    if (domain != AF_INET)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }

    int circle_socket_protocol = 0;
    switch (type)
    {
    case SOCK_STREAM:
        switch (protocol)
        {
        case IPPROTO_TCP:
        case 0:
            circle_socket_protocol = _CircleStdlib::CircleNetMap::C_IPPROTO_TCP;
            break;

        default:
            errno = EPROTONOSUPPORT;
            return -1;
        }
        break;

    case SOCK_DGRAM:
        switch (protocol)
        {
        case IPPROTO_UDP:
        case 0:
            circle_socket_protocol = _CircleStdlib::CircleNetMap::C_IPPROTO_UDP;
            break;

        default:
            errno = EPROTONOSUPPORT;
            return -1;
        }
        break;

    default:
        errno = EPROTOTYPE;
        return -1;
    }

    _CircleStdlib::FileTable::FileTableLock fileTabLock;

    _CircleStdlib::CircleFile *circleFile = nullptr;
    int const slot = _CircleStdlib::FileTable::FindFreeFileSlot(circleFile);

    if (slot != -1)
    {
        assert(circleFile != nullptr);

        auto const new_socket = new _CircleStdlib::CGlueIoSocket(circle_socket_protocol);
        circleFile->AssignGlueIO(*new_socket);
    }
    else
    {
        errno = ENFILE;
    }

    return slot;
}

extern "C" int socketpair(int domain, int type, int protocol,
                          int socket_vector[2])
{
    WarnUnimplementedSocketFunction(__func__);
    errno = ENOSYS;
    return -1;
}

extern "C" uint32_t htonl(uint32_t hostlong)
{
    return
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        __builtin_bswap32(hostlong);
#else
        hostlong
#endif
    ;
}

extern "C" uint16_t htons(uint16_t hostshort)
{
    return
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        __builtin_bswap16(hostshort)
#else
        hostshort
#endif
    ;
}

extern "C" uint32_t ntohl(uint32_t netlong)
{
    return
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        __builtin_bswap32(netlong);
#else
        netlong
#endif
    ;
}

extern "C" uint16_t ntohs(uint16_t netshort)
{
    return
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        __builtin_bswap16(netshort)
#else
        netshort
#endif
    ;
}
