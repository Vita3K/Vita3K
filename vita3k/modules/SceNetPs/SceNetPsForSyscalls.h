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

#include <module/module.h>

BRIDGE_DECL(sceNetSyscallAccept)
BRIDGE_DECL(sceNetSyscallBind)
BRIDGE_DECL(sceNetSyscallClose)
BRIDGE_DECL(sceNetSyscallConnect)
BRIDGE_DECL(sceNetSyscallControl)
BRIDGE_DECL(sceNetSyscallDescriptorClose)
BRIDGE_DECL(sceNetSyscallDescriptorCreate)
BRIDGE_DECL(sceNetSyscallDescriptorCtl)
BRIDGE_DECL(sceNetSyscallDumpAbort)
BRIDGE_DECL(sceNetSyscallDumpClose)
BRIDGE_DECL(sceNetSyscallDumpCreate)
BRIDGE_DECL(sceNetSyscallDumpRead)
BRIDGE_DECL(sceNetSyscallEpollAbort)
BRIDGE_DECL(sceNetSyscallEpollClose)
BRIDGE_DECL(sceNetSyscallEpollCreate)
BRIDGE_DECL(sceNetSyscallEpollCtl)
BRIDGE_DECL(sceNetSyscallEpollWait)
BRIDGE_DECL(sceNetSyscallGetIfList)
BRIDGE_DECL(sceNetSyscallGetSockinfo)
BRIDGE_DECL(sceNetSyscallGetpeername)
BRIDGE_DECL(sceNetSyscallGetsockname)
BRIDGE_DECL(sceNetSyscallGetsockopt)
BRIDGE_DECL(sceNetSyscallIcmConnect)
BRIDGE_DECL(sceNetSyscallIoctl)
BRIDGE_DECL(sceNetSyscallListen)
BRIDGE_DECL(sceNetSyscallRecvfrom)
BRIDGE_DECL(sceNetSyscallRecvmsg)
BRIDGE_DECL(sceNetSyscallSendmsg)
BRIDGE_DECL(sceNetSyscallSendto)
BRIDGE_DECL(sceNetSyscallSetsockopt)
BRIDGE_DECL(sceNetSyscallShutdown)
BRIDGE_DECL(sceNetSyscallSocket)
BRIDGE_DECL(sceNetSyscallSocketAbort)
BRIDGE_DECL(sceNetSyscallSysctl)
