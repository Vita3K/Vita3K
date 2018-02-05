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

struct NetState;
struct SceNetSockaddr;

bool init(NetState &state);
int open_socket(NetState &net, int domain, int type, int protocol);
int close_socket(NetState &net, int id);
int bind_socket(NetState &net,int s, const SceNetSockaddr *name, unsigned int addrlen);
int send_packet(NetState &net, int s, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen);
int recv_packet(NetState &net, int s, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen);
int get_socket_address(NetState &net, int s, SceNetSockaddr *name, unsigned int *namelen);
int set_socket_options(NetState &net, int s, int level, int optname, const void *optval, unsigned int optlen);
int connect_socket(NetState &net, int s, const SceNetSockaddr *name, unsigned int namelen);
int accept_socket(NetState &net, int s, SceNetSockaddr *addr, unsigned int *addrlen);
int listen_socket(NetState &net, int s, int backlog);
