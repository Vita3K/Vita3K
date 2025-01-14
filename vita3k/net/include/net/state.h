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

#include <net/epoll.h>
#include <net/socket.h>
#include <net/types.h>

#include <array>
#include <map>
#include <mutex>

typedef std::map<int, SocketPtr> NetSockets;
typedef std::map<int, EpollPtr> NetEpolls;

struct NetState {
    bool inited = false;
    int next_id = 0;
    NetSockets socks;
    int next_epoll_id = 0;
    NetEpolls epolls;
    int state = -1;
    int resolver_id = 0;
};

struct NetCtlState {
    std::array<SceNetCtlCallback, 8> adhocCallbacks;
    std::array<SceNetCtlCallback, 8> callbacks;
    bool inited = false;
    std::mutex mutex;
};
