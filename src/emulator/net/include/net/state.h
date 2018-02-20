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
# include <winsock2.h>
# include <Ws2tcpip.h>
typedef SOCKET abs_socket;
typedef s32 socklen_t;
#else
# include <unistd.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
typedef s32 abs_socket;
#endif

typedef std::map<s32, abs_socket> sockets;

struct NetState {
    bool inited = false;
    s32 next_id = 0;
    sockets socks;
};
