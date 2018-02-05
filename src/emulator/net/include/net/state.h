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
#include <memory>
#include <vector>

#ifdef WIN32
# include <winsock2.h>
# include <Ws2tcpip.h>
typedef SOCKET abs_socket;
typedef int socklen_t;
#else
# include <unistd.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
typedef uint32_t abs_socket;
#endif

typedef std::map<int, abs_socket> sockets;

struct NetState {
    bool inited = false;
    int next_id = 0;
    sockets socks;
};
