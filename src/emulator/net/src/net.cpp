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
#include <util/types.h>

#include <cstring>

static void convertSceSockaddrToPosix(const struct SceNetSockaddr *src, struct sockaddr *dst){
    if (src == nullptr || dst == nullptr) return;
    memset(dst, 0, sizeof(struct sockaddr));
    SceNetSockaddrIn* src_in = (SceNetSockaddrIn*)src;
    sockaddr_in* dst_in = (sockaddr_in*)dst;
    dst_in->sin_family = src_in->sin_family;
    dst_in->sin_port = src_in->sin_port;
    memcpy(&dst_in->sin_addr, &src_in->sin_addr, 4);
}

static void convertPosixSockaddrToSce(struct sockaddr *src, struct SceNetSockaddr *dst){
    if (src == nullptr || dst == nullptr) return;
    memset(dst, 0, sizeof(struct SceNetSockaddr));
    SceNetSockaddrIn* dst_in = (SceNetSockaddrIn*)dst;
    sockaddr_in* src_in = (sockaddr_in*)src;
    dst_in->sin_family = src_in->sin_family;
    dst_in->sin_port = src_in->sin_port;
    memcpy(&dst_in->sin_addr, &src_in->sin_addr, 4);
}

bool init(NetState &state) {
    return true;
}

s32 open_socket(NetState &net, s32 domain, s32 type, s32 protocol) {
    abs_socket sock = socket(domain, type, protocol);
    if (sock >= 0){
        if (protocol == SCE_NET_IPPROTO_UDP){
            bool _true = true;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&_true, 1);
        }
        const s32 id = net.next_id++;
        net.socks.emplace(id, sock);
        return id;
    }
    return -1;
}

s32 close_socket(NetState &net, s32 id){
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

s32 bind_socket(NetState &net,s32 id, const SceNetSockaddr *name, u32 addrlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
		return bind(socket->second, &addr, sizeof(struct sockaddr_in));
    }
    return -1;
}

s32 send_packet(NetState &net, s32 id, const void *msg, u32 len, s32 flags, const SceNetSockaddr *to, u32 tolen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (to != nullptr){
            struct sockaddr addr;
            convertSceSockaddrToPosix((SceNetSockaddr*)to, &addr);
            return sendto(socket->second, (const char*)msg, len, flags, &addr, sizeof(struct sockaddr_in));
        }else{
            return send(socket->second, (const char*)msg, len, flags);
        }
    }
    return -1;
}

s32 recv_packet(NetState &net, s32 id, void *buf, u32 len, s32 flags, SceNetSockaddr *from, u32 *fromlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (from != nullptr){
            struct sockaddr addr;
            s32 res = recvfrom(socket->second, (char *)buf, len, flags, &addr, (socklen_t*)fromlen);
            if (res >= 0){
                convertPosixSockaddrToSce(&addr, from);
                *fromlen = sizeof(SceNetSockaddrIn);
            }
            return res;
        }else{
            return recv(socket->second, (char*)buf, len, flags);
        }
    }
    return -1;
}

s32 get_socket_address(NetState &net, s32 id, SceNetSockaddr *name, u32 *namelen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        if (name != nullptr){
            *namelen = sizeof(sockaddr_in);
        }
        s32 res = getsockname(socket->second, &addr, (socklen_t*)namelen);
        if (res >= 0){
            convertPosixSockaddrToSce(&addr, name);
            *namelen = sizeof(SceNetSockaddrIn);
        }
        return res;
    }
    return -1;
}

s32 set_socket_options(NetState &net, s32 id, s32 level, s32 optname, const void *optval, u32 optlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (optname == SCE_NET_SO_NBIO){
#ifdef WIN32
            u_long mode;
            memcpy(&mode, optval, optlen);
            return ioctlsocket(socket->second, FIONBIO, &mode);
#else
            s32 mode;
            memcpy(&mode, optval, optlen);
            return ioctl(socket->second, FIONBIO, &mode);
#endif
        }
        return setsockopt(socket->second, level, optname, (const char*)optval, optlen);
    }
    return -1;
}

s32 connect_socket(NetState &net, s32 id, const SceNetSockaddr *name, u32 namelen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        return connect(socket->second, &addr, sizeof(struct sockaddr_in));
    }
    return -1;
}

s32 accept_socket(NetState &net, s32 id, SceNetSockaddr *name, u32 *addrlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        abs_socket new_socket = accept(socket->second, &addr, (socklen_t*)addrlen);
        if (new_socket >= 0){
            convertPosixSockaddrToSce(&addr, name);
            *addrlen = sizeof(SceNetSockaddrIn);
            const s32 id = net.next_id++;
            net.socks.emplace(id, new_socket);
            return id;
        }
    }
    return -1;
}

s32 listen_socket(NetState &net, s32 id, s32 backlog){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        return listen(socket->second, backlog);
    }
    return -1;
}
