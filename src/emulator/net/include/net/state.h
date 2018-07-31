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

#include <map>

#ifdef WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Ws2tcpip.h>
#include <winsock2.h>
typedef SOCKET abs_socket;
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int abs_socket;
#endif

#include <mem/mem.h> // Address.

namespace emu {
struct SceNetCtlCallback {
    Address pc;
    Address data;
};
} // namespace emu

typedef std::map<int, abs_socket> sockets;
typedef std::map<int, emu::SceNetCtlCallback> callbacks;

struct NetState {
    bool inited = false;
    int next_id = 0;
    sockets socks;
    callbacks cbs;
    int state = -1;
};
