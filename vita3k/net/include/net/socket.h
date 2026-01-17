// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#ifdef _WIN32
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
#include <net/if.h>
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
    int sockopt_so_onesbcast = 0;
    bool is_bound = false;
    int sce_type;

    uint32_t bound_ctx_id = 0;
    const char *name = nullptr;

    explicit Socket(int domain, int type, int protocol)
        : sce_type(type) {}

    virtual ~Socket() = default;

    virtual int abort(int flags) = 0;
    virtual int close() = 0;
    virtual int shutdown_socket(int how) = 0;
    virtual int bind(const SceNetSockaddr *addr, unsigned int addrlen) = 0;
    virtual int send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) = 0;
    virtual int recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) = 0;
    virtual int set_socket_options(int level, int optname, const void *optval, unsigned int optlen) = 0;
    virtual int get_socket_options(int level, int optname, void *optval, unsigned int *optlen) = 0;
    virtual int connect(const SceNetSockaddr *addr, unsigned int addrlen) = 0;
    virtual SocketPtr accept(SceNetSockaddr *addr, unsigned int *addrlen, int &err) = 0;
    virtual int listen(int backlog) = 0;
    virtual int get_peer_address(SceNetSockaddr *addr, unsigned int *addrlen) = 0;
    virtual int get_socket_address(SceNetSockaddr *addr, unsigned int *addrlen) = 0;
};

// udp, tcp
struct PosixSocket : public Socket {
    abs_socket sock;

    int sockopt_so_reuseport = 0;
    int sockopt_so_usecrypto = 0;
    int sockopt_so_usesignature = 0;
    int sockopt_so_tppolicy = 0;
    int sockopt_so_nbio = 0;
    int sockopt_ip_ttlchk = 0;
    int sockopt_ip_maxttl = 0;
    int sockopt_tcp_mss_to_advertise = 0;

    int abort_flags = 0;
    bool is_aborted = false;

    bool is_listening = false;

    explicit PosixSocket(int domain, int type, int protocol)
        : Socket(domain, type, protocol)
        , sock(socket(domain, type, protocol)) {}

    explicit PosixSocket(abs_socket sock, int type)
        : Socket(0, type, 0)
        , sock(sock) {}

    static int translate_return_value(int retval);

    int abort(int flags) override;
    int close() override;
    int shutdown_socket(int how) override;
    int bind(const SceNetSockaddr *addr, unsigned int addrlen) override;
    int send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) override;
    int recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) override;
    int set_socket_options(int level, int optname, const void *optval, unsigned int optlen) override;
    int get_socket_options(int level, int optname, void *optval, unsigned int *optlen) override;
    int connect(const SceNetSockaddr *addr, unsigned int addrlen) override;
    SocketPtr accept(SceNetSockaddr *addr, unsigned int *addrlen, int &err) override;
    int listen(int backlog) override;
    int get_peer_address(SceNetSockaddr *addr, unsigned int *addrlen) override;
    int get_socket_address(SceNetSockaddr *addr, unsigned int *addrlen) override;
};

struct P2PSocket : public PosixSocket {
    explicit P2PSocket(int domain, int type, int protocol);
    explicit P2PSocket(abs_socket sock, int type)
        : PosixSocket(sock, type) {}

    int bind(const SceNetSockaddr *addr, unsigned int addrlen) override;
    int send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) override;
    int recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) override;
    int connect(const SceNetSockaddr *addr, unsigned int addrlen) override;
    SocketPtr accept(SceNetSockaddr *addr, unsigned int *addrlen, int &err) override;
    int get_peer_address(SceNetSockaddr *addr, unsigned int *addrlen) override;
    int get_socket_address(SceNetSockaddr *addr, unsigned int *addrlen) override;
};
