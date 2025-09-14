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

#include <net/socket.h>
#include <util/bit_cast.h>

static SceNetSockaddr convertP2PToPosix(const SceNetSockaddr *addr) {
    if (!addr) {
        return SceNetSockaddr{};
    }
    SceNetSockaddrIn result = *reinterpret_cast<const SceNetSockaddrIn *>(addr);
    // Convert ports from network to host order
    const uint16_t port = ntohs(result.sin_port);
    const uint16_t vport = ntohs(result.sin_vport);

    // Combine the two ports in host order, then convert back to network order
    result.sin_port = htons(port + vport);
    result.sin_vport = 0; // Clear virtual port since it's not used in Posix
    return std::bit_cast<SceNetSockaddr>(result);
}

static SceNetSockaddr convertPosixToP2P(const SceNetSockaddr *addr) {
    if (!addr) {
        return SceNetSockaddr{};
    }
    SceNetSockaddrIn result = *reinterpret_cast<const SceNetSockaddrIn *>(addr);
    // Attempt to recover original port and virtual port if port was previously combined
    const uint16_t port = ntohs(result.sin_port);
    if (port > SCE_NET_ADHOC_PORT) {
        const uint16_t vport = port - SCE_NET_ADHOC_PORT;
        result.sin_port = htons(SCE_NET_ADHOC_PORT);
        result.sin_vport = htons(vport);
    } else {
        result.sin_port = htons(port);
        result.sin_vport = 0;
    }
    return std::bit_cast<SceNetSockaddr>(result);
}

static int p2pSocketTypeToPosixSocketType(int type) {
    int hostSockType;
    switch (type) {
    case SCE_NET_SOCK_DGRAM_P2P:
        hostSockType = SOCK_DGRAM;
        break;
    case SCE_NET_SOCK_STREAM_P2P:
        hostSockType = SOCK_STREAM;
        break;
    default:
        hostSockType = -1;
    }
    return hostSockType;
}

P2PSocket::P2PSocket(int domain, int type, int protocol)
    : PosixSocket(domain, p2pSocketTypeToPosixSocketType(type), protocol){};

SocketPtr P2PSocket::accept(SceNetSockaddr *addr, unsigned int *addrlen, int &err) {
    const auto res = PosixSocket::accept(addr, addrlen, err);
    *addr = convertPosixToP2P(addr);
    return res;
}

int P2PSocket::connect(const SceNetSockaddr *addr, unsigned int namelen) {
    const auto p2p_addr = convertP2PToPosix(addr);
    return PosixSocket::connect(&p2p_addr, namelen);
}

int P2PSocket::recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    const auto res = PosixSocket::recv_packet(buf, len, flags, from, fromlen);
    if ((res > 0) && from)
        *from = convertPosixToP2P(from);

    return res;
}

int P2PSocket::send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    const auto p2p_to = convertP2PToPosix(to);
    return PosixSocket::send_packet(msg, len, flags, &p2p_to, tolen);
}

int P2PSocket::bind(const SceNetSockaddr *addr, unsigned int addrlen) {
    const auto p2p_addr = convertP2PToPosix(addr);
    return PosixSocket::bind(&p2p_addr, addrlen);
}

int P2PSocket::get_socket_address(SceNetSockaddr *name, unsigned int *namelen) {
    const auto res = PosixSocket::get_socket_address(name, namelen);
    *name = convertPosixToP2P(name);
    return res;
}
