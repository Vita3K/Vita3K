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

#include <net/functions.h>

#include <net/state.h>

#include <cstring>

#include <psp2/net/net.h>

static void convertSceSockaddrToPosix(const struct SceNetSockaddr *src, struct sockaddr *dst){
    dst->sa_family = src->sa_family;
    memcpy(&dst->sa_data, &src->sa_data, 14);
}

static void convertPosixSockaddrToSce(struct sockaddr *src, struct SceNetSockaddr *dst){
    dst->sa_family = src->sa_family;
    memcpy(&dst->sa_data, &src->sa_data, 14);
}

bool init(NetState &state) {
    return true;
}

int open_socket(NetState &net, int domain, int type, int protocol) {
    const int id = net.next_id++;
    abs_socket sock = socket(domain, type, protocol);
    net.socks.emplace(id, sock);

    return id;
}

int close_socket(NetState &net, int id){
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

int bind_socket(NetState &net,int id, const SceNetSockaddr *name, unsigned int addrlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
		return bind(socket->second, &addr, addrlen);
    }
    return -1;
}

int send_packet(NetState &net, int id, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (to != NULL){
            struct sockaddr addr;
            convertSceSockaddrToPosix((SceNetSockaddr*)to, &addr);
            return sendto(socket->second, (const char*)msg, len, flags, &addr, tolen);
        }else{
            return send(socket->second, (const char*)msg, len, flags);
        }
    }
    return -1;
}

int recv_packet(NetState &net, int id, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        if (from != NULL){
            struct sockaddr addr;
            int res = recvfrom(socket->second, (char*)buf, len, flags, &addr, (socklen_t*)fromlen);
            convertPosixSockaddrToSce(&addr, from);
            return res;
        }else{
            return recv(socket->second, (char*)buf, len, flags);
        }
    }
    return -1;
}

int get_socket_address(NetState &net, int id, SceNetSockaddr *name, unsigned int *namelen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        int res = getsockname(socket->second, &addr, (socklen_t*)namelen);
        convertPosixSockaddrToSce(&addr, name);
        return res;
    }
    return -1;
}

int set_socket_options(NetState &net, int id, int level, int optname, const void *optval, unsigned int optlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        return setsockopt(socket->second, level, optname, (const char*)optval, optlen);
    }
    return -1;
}

int connect_socket(NetState &net, int id, const SceNetSockaddr *name, unsigned int namelen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        convertSceSockaddrToPosix(name, &addr);
        return connect(socket->second, &addr, namelen);
    }
    return -1;
}

int accept_socket(NetState &net, int id, SceNetSockaddr *name, unsigned int *addrlen){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        struct sockaddr addr;
        abs_socket new_socket = accept(socket->second, &addr, (socklen_t*)addrlen);
#ifdef WIN32
        if (new_socket != INVALID_SOCKET){
            convertPosixSockaddrToSce(&addr, name);
            const int id = net.next_id++;
            net.socks.emplace(id, new_socket);
            return id;
        }else{
            return -1;
        }
#else
        if (new_socket > 0){
            convertPosixSockaddrToSce(&addr, name);
            const int id = net.next_id++;
            net.socks.emplace(id, new_socket);
            return id;
        }else{
            return (int)new_socket;
        }
#endif
    }
    return -1;
}

int listen_socket(NetState &net, int id, int backlog){
    const sockets::const_iterator socket = net.socks.find(id);
    if (socket != net.socks.end()) {
        return listen(socket->second, backlog);
    }
    return -1;
}
