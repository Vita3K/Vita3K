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

#include <module/module.h>

#include <net/types.h>

DECL_EXPORT(int, sceNetBind, int sid, const SceNetSockaddr *addr, unsigned int addrlen);
DECL_EXPORT(int, sceNetInetPton, int af, const char *src, void *dst);
DECL_EXPORT(int, sceNetSetsockopt, int sid, SceNetProtocol level, SceNetSocketOption optname, const void *optval, unsigned int optlen);
DECL_EXPORT(int, sceNetShutdown, int sid, int how);
DECL_EXPORT(int, sceNetSocket, const char *name, int domain, SceNetSocketType type, SceNetProtocol protocol);
DECL_EXPORT(int, sceNetSocketAbort, int sid, int flags);
DECL_EXPORT(int, sceNetSocketClose, int sid);
