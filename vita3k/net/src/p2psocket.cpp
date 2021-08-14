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

#include <net/socket.h>

int P2PSocket::close() {
    return 0;
}

int P2PSocket::listen(int backlog) {
    return 0;
}

SocketPtr P2PSocket::accept(SceNetSockaddr *addr, unsigned int *addrlen) {
    return nullptr;
}

int P2PSocket::connect(const SceNetSockaddr *addr, unsigned int namelen) {
    return 0;
}

int P2PSocket::set_socket_options(int level, int optname, const void *optval, unsigned int optlen) {
    return 0;
}

int P2PSocket::recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    return SCE_NET_ERROR_EAGAIN;
}

int P2PSocket::send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    return 0;
}

int P2PSocket::bind(const SceNetSockaddr *addr, unsigned int addrlen) {
    return 0;
}

int P2PSocket::get_socket_address(SceNetSockaddr *name, unsigned int *namelen) {
    return 0;
}
