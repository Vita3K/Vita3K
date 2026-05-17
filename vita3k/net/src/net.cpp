// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <net/state.h>

#ifdef _WIN32
#include <WinSock2.h>
#endif

void NetState::abort_all() {
    for (auto &[id, sock] : socks)
        sock->abort(SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION | SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION);
}

void NetState::deinit() {
    for (auto &[id, sock] : socks) {
        sock->close();
    }
    socks.clear();
    epolls.clear();

#ifdef _WIN32
    if (inited)
        WSACleanup();
#endif

    inited = false;
    next_id = 0;
    next_epoll_id = 0;
    state = -1;
    resolver_id = 0;
    current_addr_index = 0;
    broadcastAddr = 0xFFFFFFFF;
    netAddr = 0xFFFFFFFF;
}
