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

#pragma once

#ifdef USE_GDBSTUB

#include <functional>
#include <memory>
#include <thread>

#ifdef _WIN32
typedef SOCKET socket_t;
constexpr socket_t BAD_SOCK = (socket_t)~0;
#else
typedef int32_t socket_t;
constexpr socket_t BAD_SOCK = -1;
#endif

constexpr uint32_t GDB_SERVER_PORT = 2159;

struct GDBState {
#ifdef _WIN32
    WSADATA wsaData;
#endif

    socket_t listen_socket = BAD_SOCK;
    socket_t client_socket = BAD_SOCK;

    std::shared_ptr<std::thread> server_thread = nullptr;
    bool server_die = false;

    std::string last_reply = "";

    SceUID current_thread = 0;

    std::map<Address, uint32_t> breakpoints;
};

#endif
