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

#pragma once

#include <net/types.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Ws2tcpip.h>
#include <iphlpapi.h>
#include <winsock2.h>
#undef s_addr
typedef SOCKET abs_socket;
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int abs_socket;
#endif

struct Socket;

typedef std::shared_ptr<Socket> SocketPtr;

struct Socket {
    explicit Socket(int domain, int type, int protocol) {}

    virtual ~Socket() = default;

    virtual int close() = 0;
    virtual int bind(const SceNetSockaddr *addr, unsigned int addrlen) = 0;
    virtual int send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) = 0;
    virtual int recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) = 0;
    virtual int set_socket_options(int level, int optname, const void *optval, unsigned int optlen) = 0;
    virtual int get_socket_options(int level, int optname, void *optval, unsigned int *optlen) = 0;
    virtual int connect(const SceNetSockaddr *addr, unsigned int namelen) = 0;
    virtual SocketPtr accept(SceNetSockaddr *addr, unsigned int *addrlen) = 0;
    virtual int listen(int backlog) = 0;
    virtual int get_socket_address(SceNetSockaddr *name, unsigned int *namelen) = 0;
};

// udp, tcp
struct PosixSocket : public Socket {
    abs_socket sock;

    int sockopt_so_reuseport = 0;
    int sockopt_so_onesbcast = 0;
    int sockopt_so_usecrypto = 0;
    int sockopt_so_usesignature = 0;
    int sockopt_so_tppolicy = 0;
    int sockopt_so_nbio = 0;
    int sockopt_ip_ttlchk = 0;
    int sockopt_ip_maxttl = 0;
    int sockopt_tcp_mss_to_advertise = 0;

    explicit PosixSocket(int domain, int type, int protocol)
        : Socket(domain, type, protocol)
        , sock(socket(domain, type, protocol)) {}

    explicit PosixSocket(abs_socket sock)
        : Socket(0, 0, 0)
        , sock(sock) {}

    int close() override;
    int bind(const SceNetSockaddr *addr, unsigned int addrlen) override;
    int send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) override;
    int recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) override;
    int set_socket_options(int level, int optname, const void *optval, unsigned int optlen) override;
    int get_socket_options(int level, int optname, void *optval, unsigned int *optlen) override;
    int connect(const SceNetSockaddr *addr, unsigned int namelen) override;
    SocketPtr accept(SceNetSockaddr *addr, unsigned int *addrlen) override;
    int listen(int backlog) override;
    int get_socket_address(SceNetSockaddr *name, unsigned int *namelen) override;
};

struct P2PSocket : public Socket {
    explicit P2PSocket(int domain, int type, int protocol)
        : Socket(domain, type, protocol) {}

    int close() override;
    int bind(const SceNetSockaddr *addr, unsigned int addrlen) override;
    int send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) override;
    int recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) override;
    int set_socket_options(int level, int optname, const void *optval, unsigned int optlen) override;
    int get_socket_options(int level, int optname, void *optval, unsigned int *optlen) override;
    int connect(const SceNetSockaddr *addr, unsigned int namelen) override;
    SocketPtr accept(SceNetSockaddr *addr, unsigned int *addrlen) override;
    int listen(int backlog) override;
    int get_socket_address(SceNetSockaddr *name, unsigned int *namelen) override;
};
