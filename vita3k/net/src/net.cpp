// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <psp2/net/net.h>

#include <net/functions.h>

#include <net/state.h>

#include <cstring>

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

bool init(NetState &state) {
    return true;
}

int open_socket(NetState &net, int domain, int type, int protocol) {
    abs_socket sock = socket(domain, type, protocol);
    if (sock >= 0) {
        if (protocol == SCE_NET_IPPROTO_UDP) {
            bool _true = true;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&_true, 1);
        }
        const int id = net.next_id++;
        net.socks.emplace(id, sock);
        return id;
    }
    return -1;
}

int close_socket(NetState &net, int id) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
#ifdef WIN32
        closesocket(socket->second);
#else
        close(socket->second);
#endif
        net.socks.erase(id);
        return 0;
    }
    return -1;
}

int bind_socket(NetState &net, int id, const SceNetSockaddr *name, unsigned int addrlen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        return bind(socket->second, &addr, sizeof(struct sockaddr_in));
    }
    return -1;
}

int send_packet(NetState &net, int id, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (to != nullptr) {
            struct sockaddr addr;
            convertSceSockaddrToPosix((SceNetSockaddr *)to, &addr);
            return sendto(socket->second, (const char *)msg, len, flags, &addr, sizeof(struct sockaddr_in));
        } else {
            return send(socket->second, (const char *)msg, len, flags);
        }
    }
    return -1;
}

int recv_packet(NetState &net, int id, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (from != nullptr) {
            struct sockaddr addr;
            int res = recvfrom(socket->second, (char *)buf, len, flags, &addr, (socklen_t *)fromlen);
            if (res >= 0) {
                convertPosixSockaddrToSce(&addr, from);
                *fromlen = sizeof(SceNetSockaddrIn);
            }
            return res;
        } else {
            return recv(socket->second, (char *)buf, len, flags);
        }
    }
    return -1;
}

int get_socket_address(NetState &net, int id, SceNetSockaddr *name, unsigned int *namelen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        if (name != nullptr) {
            *namelen = sizeof(sockaddr_in);
        }
        int res = getsockname(socket->second, &addr, (socklen_t *)namelen);
        if (res >= 0) {
            convertPosixSockaddrToSce(&addr, name);
            *namelen = sizeof(SceNetSockaddrIn);
        }
        return res;
    }
    return -1;
}

int set_socket_options(NetState &net, int id, int level, int optname, const void *optval, unsigned int optlen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (optname == SCE_NET_SO_NBIO) {
#ifdef WIN32
            u_long mode;
            memcpy(&mode, optval, optlen);
            return ioctlsocket(socket->second, FIONBIO, &mode);
#else
            int mode;
            memcpy(&mode, optval, optlen);
            return ioctl(socket->second, FIONBIO, &mode);
#endif
        }
        return setsockopt(socket->second, level, optname, (const char *)optval, optlen);
    }
    return -1;
}

int connect_socket(NetState &net, int id, const SceNetSockaddr *name, unsigned int namelen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        return connect(socket->second, &addr, sizeof(struct sockaddr_in));
    }
    return -1;
}

int accept_socket(NetState &net, int id, SceNetSockaddr *name, unsigned int *addrlen) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        abs_socket new_socket = accept(socket->second, &addr, (socklen_t *)addrlen);
        if (new_socket >= 0) {
            convertPosixSockaddrToSce(&addr, name);
            *addrlen = sizeof(SceNetSockaddrIn);
            const int id = net.next_id++;
            net.socks.emplace(id, new_socket);
            return id;
        }
    }
    return -1;
}

int listen_socket(NetState &net, int id, int backlog) {
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        return listen(socket->second, backlog);
    }
    return -1;
}
