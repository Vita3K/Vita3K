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

#include <util/types.h>

struct NetState;
struct SceNetSockaddr;

bool init(NetState &state);
s32 open_socket(NetState &net, s32 domain, s32 type, s32 protocol);
s32 close_socket(NetState &net, s32 id);
s32 bind_socket(NetState &net,s32 s, const SceNetSockaddr *name, u32 addrlen);
s32 send_packet(NetState &net, s32 s, const void *msg, u32 len, s32 flags, const SceNetSockaddr *to, u32 tolen);
s32 recv_packet(NetState &net, s32 s, void *buf, u32 len, s32 flags, SceNetSockaddr *from, u32 *fromlen);
s32 get_socket_address(NetState &net, s32 s, SceNetSockaddr *name, u32 *namelen);
s32 set_socket_options(NetState &net, s32 s, s32 level, s32 optname, const void *optval, u32 optlen);
s32 connect_socket(NetState &net, s32 s, const SceNetSockaddr *name, u32 namelen);
s32 accept_socket(NetState &net, s32 s, SceNetSockaddr *addr, u32 *addrlen);
s32 listen_socket(NetState &net, s32 s, s32 backlog);
