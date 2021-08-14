// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

static int translate_errorcode(int error) {
    if (error < 0) {
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
    return 0;
}

static void convertSceSockaddrToPosix(const struct SceNetSockaddr *src, struct sockaddr *dst) {
    if (src == nullptr || dst == nullptr)
        return;
    memset(dst, 0, sizeof(struct sockaddr));
    SceNetSockaddrIn *src_in = (SceNetSockaddrIn *)src;
    sockaddr_in *dst_in = (sockaddr_in *)dst;
    dst_in->sin_family = src_in->sin_family;
    dst_in->sin_port = src_in->sin_port;
    memcpy(&dst_in->sin_addr, &src_in->sin_addr, 4);
}

static void convertPosixSockaddrToSce(struct sockaddr *src, struct SceNetSockaddr *dst) {
    if (src == nullptr || dst == nullptr)
        return;
    memset(dst, 0, sizeof(struct SceNetSockaddr));
    SceNetSockaddrIn *dst_in = (SceNetSockaddrIn *)dst;
    sockaddr_in *src_in = (sockaddr_in *)src;
    dst_in->sin_family = static_cast<unsigned char>(src_in->sin_family);
    dst_in->sin_port = src_in->sin_port;
    memcpy(&dst_in->sin_addr, &src_in->sin_addr, 4);
}

int PosixSocket::connect(const SceNetSockaddr *addr, unsigned int namelen) {
    struct sockaddr addr2;
    convertSceSockaddrToPosix(addr, &addr2);
    return translate_errorcode(::connect(sock, &addr2, sizeof(struct sockaddr_in)));
}

int PosixSocket::bind(const SceNetSockaddr *addr, unsigned int addrlen) {
    struct sockaddr addr2;
    convertSceSockaddrToPosix(addr, &addr2);
    return translate_errorcode(::bind(sock, &addr2, sizeof(struct sockaddr_in)));
}

int PosixSocket::listen(int backlog) {
    return translate_errorcode(::listen(sock, backlog) < 0);
}

int PosixSocket::get_socket_address(SceNetSockaddr *name, unsigned int *namelen) {
    struct sockaddr addr;
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
    return translate_errorcode(out);
}

SocketPtr PosixSocket::accept(SceNetSockaddr *addr, unsigned int *addrlen) {
    struct sockaddr addr2;
    abs_socket new_socket = ::accept(sock, &addr2, (socklen_t *)addrlen);
    if (new_socket >= 0) {
        convertPosixSockaddrToSce(&addr2, addr);
        *addrlen = sizeof(SceNetSockaddrIn);
        return std::make_shared<PosixSocket>(new_socket);
    }
    return nullptr;
}

int PosixSocket::set_socket_options(int level, int optname, const void *optval, unsigned int optlen) {
    if (optname == SCE_NET_SO_NBIO) {
#ifdef WIN32
        u_long mode;
        memcpy(&mode, optval, optlen);
        return translate_errorcode(ioctlsocket(sock, FIONBIO, &mode));
#else
        int mode;
        memcpy(&mode, optval, optlen);
        return translate_errorcode(ioctl(sock, FIONBIO, &mode));
#endif
    }
    return translate_errorcode(setsockopt(sock, level, optname, (const char *)optval, optlen));
}

int PosixSocket::recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    if (from != nullptr) {
        struct sockaddr addr;
        int res = recvfrom(sock, (char *)buf, len, flags, &addr, (socklen_t *)fromlen);
        convertPosixSockaddrToSce(&addr, from);
        *fromlen = sizeof(SceNetSockaddrIn);

        return translate_errorcode(res);
    } else {
        return translate_errorcode(recv(sock, (char *)buf, len, flags));
    }
}

int PosixSocket::send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    if (to != nullptr) {
        struct sockaddr addr;
        convertSceSockaddrToPosix((SceNetSockaddr *)to, &addr);
        return translate_errorcode(sendto(sock, (const char *)msg, len, flags, &addr, sizeof(struct sockaddr_in)));
    } else {
        return translate_errorcode(send(sock, (const char *)msg, len, flags));
    }
}
