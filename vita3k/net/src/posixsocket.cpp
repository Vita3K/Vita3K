// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <cstring>
#include <net/socket.h>

// NOTE: This should be SCE_NET_##errname but it causes vitaQuake to softlock in online games
#ifdef WIN32
#define ERROR_CASE(errname) \
    case (WSA##errname):    \
        return SCE_NET_ERROR_##errname;
#else
#define ERROR_CASE(errname) \
    case (errname):         \
        return SCE_NET_ERROR_##errname;
#endif

static int translate_return_value(int retval) {
    if (retval < 0) {
#ifdef WIN32
        switch (WSAGetLastError()) {
#else
        switch (errno) {
#endif
#ifndef WIN32 // These errorcodes don't exist in WinSock
            ERROR_CASE(EPERM)
            ERROR_CASE(ENOENT)
            ERROR_CASE(ESRCH)
            ERROR_CASE(EIO)
            ERROR_CASE(ENXIO)
            ERROR_CASE(E2BIG)
            ERROR_CASE(ENOEXEC)
            ERROR_CASE(EDEADLK)
            ERROR_CASE(ENOMEM)
            ERROR_CASE(ECHILD)
            ERROR_CASE(EBUSY)
            ERROR_CASE(EEXIST)
            ERROR_CASE(EXDEV)
            ERROR_CASE(ENODEV)
            ERROR_CASE(ENOTDIR)
            ERROR_CASE(EISDIR)
            ERROR_CASE(ENFILE)
            ERROR_CASE(ENOTTY)
            ERROR_CASE(ETXTBSY)
            ERROR_CASE(EFBIG)
            ERROR_CASE(ENOSPC)
            ERROR_CASE(ESPIPE)
            ERROR_CASE(EROFS)
            ERROR_CASE(EMLINK)
            ERROR_CASE(EPIPE)
            ERROR_CASE(EDOM)
            ERROR_CASE(ERANGE)
            ERROR_CASE(ENOLCK)
            ERROR_CASE(ENOSYS)
            ERROR_CASE(EIDRM)
            ERROR_CASE(EOVERFLOW)
            ERROR_CASE(EILSEQ)
            ERROR_CASE(ENOTSUP)
            ERROR_CASE(ECANCELED)
            ERROR_CASE(EBADMSG)
            ERROR_CASE(ENODATA)
            ERROR_CASE(ENOSR)
            ERROR_CASE(ENOSTR)
            ERROR_CASE(ETIME)
#endif
            ERROR_CASE(EINTR)
            ERROR_CASE(EBADF)
            ERROR_CASE(EACCES)
            ERROR_CASE(EFAULT)
            ERROR_CASE(EINVAL)
            ERROR_CASE(EMFILE)
            ERROR_CASE(EWOULDBLOCK)
            ERROR_CASE(EINPROGRESS)
            ERROR_CASE(EALREADY)
            ERROR_CASE(ENOTSOCK)
            ERROR_CASE(EDESTADDRREQ)
            ERROR_CASE(EMSGSIZE)
            ERROR_CASE(EPROTOTYPE)
            ERROR_CASE(ENOPROTOOPT)
            ERROR_CASE(EPROTONOSUPPORT)
#if defined(__APPLE__) || defined(WIN32)
            ERROR_CASE(EOPNOTSUPP)
#endif
            ERROR_CASE(EAFNOSUPPORT)
            ERROR_CASE(EADDRINUSE)
            ERROR_CASE(EADDRNOTAVAIL)
            ERROR_CASE(ENETDOWN)
            ERROR_CASE(ENETUNREACH)
            ERROR_CASE(ENETRESET)
            ERROR_CASE(ECONNABORTED)
            ERROR_CASE(ECONNRESET)
            ERROR_CASE(ENOBUFS)
            ERROR_CASE(EISCONN)
            ERROR_CASE(ENOTCONN)
            ERROR_CASE(ETIMEDOUT)
            ERROR_CASE(ECONNREFUSED)
            ERROR_CASE(ELOOP)
            ERROR_CASE(ENAMETOOLONG)
            ERROR_CASE(EHOSTUNREACH)
            ERROR_CASE(ENOTEMPTY)
        }
        return SCE_NET_ERROR_EINTERNAL;
    }

    // zero and positive values do not need to be translated, as they indicate success or bytes sent/received
    return retval;
}

static void convertSceSockaddrToPosix(const SceNetSockaddr *src, sockaddr *dst) {
    if (src == nullptr || dst == nullptr)
        return;
    memset(dst, 0, sizeof(sockaddr));
    const SceNetSockaddrIn *src_in = (const SceNetSockaddrIn *)src;
    sockaddr_in *dst_in = (sockaddr_in *)dst;
    dst_in->sin_family = src_in->sin_family;
    dst_in->sin_port = src_in->sin_port;
    memcpy(&dst_in->sin_addr, &src_in->sin_addr, 4);
}

static void convertPosixSockaddrToSce(sockaddr *src, SceNetSockaddr *dst) {
    if (src == nullptr || dst == nullptr)
        return;
    memset(dst, 0, sizeof(SceNetSockaddr));
    SceNetSockaddrIn *dst_in = (SceNetSockaddrIn *)dst;
    sockaddr_in *src_in = (sockaddr_in *)src;
    dst_in->sin_family = static_cast<unsigned char>(src_in->sin_family);
    dst_in->sin_port = src_in->sin_port;
    memcpy(&dst_in->sin_addr, &src_in->sin_addr, 4);
}

int PosixSocket::connect(const SceNetSockaddr *addr, unsigned int namelen) {
    sockaddr addr2;
    convertSceSockaddrToPosix(addr, &addr2);
    return translate_return_value(::connect(sock, &addr2, sizeof(sockaddr_in)));
}

int PosixSocket::bind(const SceNetSockaddr *addr, unsigned int addrlen) {
    sockaddr addr2;
    convertSceSockaddrToPosix(addr, &addr2);
    return translate_return_value(::bind(sock, &addr2, sizeof(sockaddr_in)));
}

int PosixSocket::listen(int backlog) {
    return translate_return_value(::listen(sock, backlog));
}

int PosixSocket::get_socket_address(SceNetSockaddr *name, unsigned int *namelen) {
    sockaddr addr;
    convertSceSockaddrToPosix(name, &addr);
    if (name != nullptr) {
        *namelen = sizeof(sockaddr_in);
    }
    int res = getsockname(sock, &addr, (socklen_t *)namelen);
    if (res >= 0) {
        convertPosixSockaddrToSce(&addr, name);
        *namelen = sizeof(SceNetSockaddrIn);
    }
    return res;
}

int PosixSocket::close() {
#ifdef WIN32
    auto out = closesocket(sock);
#else
    auto out = ::close(sock);
#endif
    return translate_return_value(out);
}

SocketPtr PosixSocket::accept(SceNetSockaddr *addr, unsigned int *addrlen) {
    sockaddr addr2;
    abs_socket new_socket = ::accept(sock, &addr2, (socklen_t *)addrlen);
#ifdef WIN32
    if (new_socket != INVALID_SOCKET) {
#else
    if (new_socket >= 0) {
#endif
        convertPosixSockaddrToSce(&addr2, addr);
        *addrlen = sizeof(SceNetSockaddrIn);
        return std::make_shared<PosixSocket>(new_socket);
    }
    return nullptr;
}

static int translate_sockopt_level(int level) {
    switch (level) {
    case SCE_NET_SOL_SOCKET: return SOL_SOCKET;
    case SCE_NET_IPPROTO_IP: return IPPROTO_IP;
    case SCE_NET_IPPROTO_TCP: return IPPROTO_TCP;
    }
    return -1;
}

#define CASE_SETSOCKOPT(opt) \
    case SCE_NET_##opt:      \
        return translate_return_value(setsockopt(sock, level, opt, (const char *)optval, optlen))

#define CASE_SETSOCKOPT_VALUE(opt, value) \
    case opt:                             \
        if (optlen != sizeof(*value)) {   \
            return SCE_NET_ERROR_EFAULT;  \
        }                                 \
        memcpy(value, optval, optlen);    \
        return 0

int PosixSocket::set_socket_options(int level, int optname, const void *optval, unsigned int optlen) {
    level = translate_sockopt_level(level);
    if (level == SOL_SOCKET) {
        switch (optname) {
            CASE_SETSOCKOPT(SO_REUSEADDR);
            CASE_SETSOCKOPT(SO_KEEPALIVE);
            CASE_SETSOCKOPT(SO_BROADCAST);
            CASE_SETSOCKOPT(SO_LINGER);
            CASE_SETSOCKOPT(SO_OOBINLINE);
            CASE_SETSOCKOPT(SO_SNDBUF);
            CASE_SETSOCKOPT(SO_RCVBUF);
            CASE_SETSOCKOPT(SO_SNDLOWAT);
            CASE_SETSOCKOPT(SO_RCVLOWAT);
            CASE_SETSOCKOPT(SO_SNDTIMEO);
            CASE_SETSOCKOPT(SO_RCVTIMEO);
            CASE_SETSOCKOPT(SO_ERROR);
            CASE_SETSOCKOPT(SO_TYPE);
            CASE_SETSOCKOPT_VALUE(SCE_NET_SO_REUSEPORT, &sockopt_so_reuseport);
            CASE_SETSOCKOPT_VALUE(SCE_NET_SO_ONESBCAST, &sockopt_so_onesbcast);
            CASE_SETSOCKOPT_VALUE(SCE_NET_SO_USECRYPTO, &sockopt_so_usecrypto);
            CASE_SETSOCKOPT_VALUE(SCE_NET_SO_USESIGNATURE, &sockopt_so_usesignature);
            CASE_SETSOCKOPT_VALUE(SCE_NET_SO_TPPOLICY, &sockopt_so_tppolicy);
        case SCE_NET_SO_NAME:
            return SCE_NET_ERROR_EINVAL; // don't support set for name
        case SCE_NET_SO_NBIO: {
            if (optlen != sizeof(sockopt_so_nbio)) {
                return SCE_NET_ERROR_EFAULT;
            }
            memcpy(&sockopt_so_nbio, optval, optlen);
#ifdef WIN32
            static_assert(sizeof(u_long) == sizeof(sockopt_so_nbio), "type used for ioctlsocket value does not have the expected size");
            return translate_return_value(ioctlsocket(sock, FIONBIO, (u_long *)&sockopt_so_nbio));
#else
            return translate_return_value(ioctl(sock, FIONBIO, &sockopt_so_nbio));
#endif
        }
        }
    } else if (level == IPPROTO_IP) {
        switch (optname) {
            CASE_SETSOCKOPT(IP_HDRINCL);
            CASE_SETSOCKOPT(IP_TOS);
            CASE_SETSOCKOPT(IP_TTL);
            CASE_SETSOCKOPT(IP_MULTICAST_IF);
            CASE_SETSOCKOPT(IP_MULTICAST_TTL);
            CASE_SETSOCKOPT(IP_MULTICAST_LOOP);
            CASE_SETSOCKOPT(IP_ADD_MEMBERSHIP);
            CASE_SETSOCKOPT(IP_DROP_MEMBERSHIP);
            CASE_SETSOCKOPT_VALUE(SCE_NET_IP_TTLCHK, &sockopt_ip_ttlchk);
            CASE_SETSOCKOPT_VALUE(SCE_NET_IP_MAXTTL, &sockopt_ip_maxttl);
        }
    } else if (level == IPPROTO_TCP) {
        switch (optname) {
            CASE_SETSOCKOPT(TCP_NODELAY);
            CASE_SETSOCKOPT(TCP_MAXSEG);
            CASE_SETSOCKOPT_VALUE(SCE_NET_TCP_MSS_TO_ADVERTISE, &sockopt_tcp_mss_to_advertise);
        }
    }

    return SCE_NET_ERROR_EINVAL;
}

#define CASE_GETSOCKOPT(opt)                                                                              \
    case SCE_NET_##opt: {                                                                                 \
        socklen_t optlen_temp = *optlen;                                                                  \
        auto retval = translate_return_value(getsockopt(sock, level, opt, (char *)optval, &optlen_temp)); \
        *optlen = optlen_temp;                                                                            \
        return retval;                                                                                    \
    }
#define CASE_GETSOCKOPT_VALUE(opt, value)   \
    case opt:                               \
        if (*optlen < sizeof(value)) {      \
            *optlen = sizeof(value);        \
            return SCE_NET_ERROR_EFAULT;    \
        }                                   \
        *optlen = sizeof(value);            \
        *(decltype(value) *)optval = value; \
        return 0;

int PosixSocket::get_socket_options(int level, int optname, void *optval, unsigned int *optlen) {
    level = translate_sockopt_level(level);
    if (level == SOL_SOCKET) {
        switch (optname) {
            CASE_GETSOCKOPT(SO_REUSEADDR);
            CASE_GETSOCKOPT(SO_KEEPALIVE);
            CASE_GETSOCKOPT(SO_BROADCAST);
            CASE_GETSOCKOPT(SO_LINGER);
            CASE_GETSOCKOPT(SO_OOBINLINE);
            CASE_GETSOCKOPT(SO_SNDBUF);
            CASE_GETSOCKOPT(SO_RCVBUF);
            CASE_GETSOCKOPT(SO_SNDLOWAT);
            CASE_GETSOCKOPT(SO_RCVLOWAT);
            CASE_GETSOCKOPT(SO_SNDTIMEO);
            CASE_GETSOCKOPT(SO_RCVTIMEO);
            CASE_GETSOCKOPT(SO_ERROR);
            CASE_GETSOCKOPT(SO_TYPE);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_NBIO, sockopt_so_nbio);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_REUSEPORT, sockopt_so_reuseport);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_ONESBCAST, sockopt_so_onesbcast);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_USECRYPTO, sockopt_so_usecrypto);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_USESIGNATURE, sockopt_so_usesignature);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_TPPOLICY, sockopt_so_tppolicy);
            CASE_GETSOCKOPT_VALUE(SCE_NET_SO_NAME, (char)0); // writes an empty string to the output buffer
        }
    } else if (level == IPPROTO_IP) {
        switch (optname) {
            CASE_GETSOCKOPT(IP_HDRINCL);
            CASE_GETSOCKOPT(IP_TOS);
            CASE_GETSOCKOPT(IP_TTL);
            CASE_GETSOCKOPT(IP_MULTICAST_IF);
            CASE_GETSOCKOPT(IP_MULTICAST_TTL);
            CASE_GETSOCKOPT(IP_MULTICAST_LOOP);
            CASE_GETSOCKOPT(IP_ADD_MEMBERSHIP);
            CASE_GETSOCKOPT(IP_DROP_MEMBERSHIP);
            CASE_GETSOCKOPT_VALUE(SCE_NET_IP_TTLCHK, sockopt_ip_ttlchk);
            CASE_GETSOCKOPT_VALUE(SCE_NET_IP_MAXTTL, sockopt_ip_maxttl);
        }
    } else if (level == IPPROTO_TCP) {
        switch (optname) {
            CASE_GETSOCKOPT(TCP_NODELAY);
            CASE_GETSOCKOPT(TCP_MAXSEG);
            CASE_GETSOCKOPT_VALUE(SCE_NET_TCP_MSS_TO_ADVERTISE, sockopt_tcp_mss_to_advertise);
        }
    }
    return SCE_NET_ERROR_EINVAL;
}

int PosixSocket::recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    if (from != nullptr) {
        sockaddr addr;
        int res = recvfrom(sock, (char *)buf, len, flags, &addr, (socklen_t *)fromlen);
        convertPosixSockaddrToSce(&addr, from);
        *fromlen = sizeof(SceNetSockaddrIn);

        return translate_return_value(res);
    } else {
        return translate_return_value(recv(sock, (char *)buf, len, flags));
    }
}

int PosixSocket::send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    if (to != nullptr) {
        sockaddr addr;
        convertSceSockaddrToPosix(to, &addr);
        return translate_return_value(sendto(sock, (const char *)msg, len, flags, &addr, sizeof(sockaddr_in)));
    } else {
        return translate_return_value(send(sock, (const char *)msg, len, flags));
    }
}
