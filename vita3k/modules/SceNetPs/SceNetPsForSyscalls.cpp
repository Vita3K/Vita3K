// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceNetPsForSyscalls.h"

EXPORT(int, sceNetSyscallAccept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallBind) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallConnect) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallControl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDescriptorClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDescriptorCreate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDescriptorCtl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDumpAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDumpClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDumpCreate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallDumpRead) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallEpollAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallEpollClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallEpollCreate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallEpollCtl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallEpollWait) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallGetIfList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallGetSockinfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallGetpeername) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallGetsockname) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallGetsockopt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallIcmConnect) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallIoctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallListen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallRecvfrom) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallRecvmsg) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallSendmsg) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallSendto) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallSetsockopt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallShutdown) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallSocket) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallSocketAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSyscallSysctl) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceNetSyscallAccept)
BRIDGE_IMPL(sceNetSyscallBind)
BRIDGE_IMPL(sceNetSyscallClose)
BRIDGE_IMPL(sceNetSyscallConnect)
BRIDGE_IMPL(sceNetSyscallControl)
BRIDGE_IMPL(sceNetSyscallDescriptorClose)
BRIDGE_IMPL(sceNetSyscallDescriptorCreate)
BRIDGE_IMPL(sceNetSyscallDescriptorCtl)
BRIDGE_IMPL(sceNetSyscallDumpAbort)
BRIDGE_IMPL(sceNetSyscallDumpClose)
BRIDGE_IMPL(sceNetSyscallDumpCreate)
BRIDGE_IMPL(sceNetSyscallDumpRead)
BRIDGE_IMPL(sceNetSyscallEpollAbort)
BRIDGE_IMPL(sceNetSyscallEpollClose)
BRIDGE_IMPL(sceNetSyscallEpollCreate)
BRIDGE_IMPL(sceNetSyscallEpollCtl)
BRIDGE_IMPL(sceNetSyscallEpollWait)
BRIDGE_IMPL(sceNetSyscallGetIfList)
BRIDGE_IMPL(sceNetSyscallGetSockinfo)
BRIDGE_IMPL(sceNetSyscallGetpeername)
BRIDGE_IMPL(sceNetSyscallGetsockname)
BRIDGE_IMPL(sceNetSyscallGetsockopt)
BRIDGE_IMPL(sceNetSyscallIcmConnect)
BRIDGE_IMPL(sceNetSyscallIoctl)
BRIDGE_IMPL(sceNetSyscallListen)
BRIDGE_IMPL(sceNetSyscallRecvfrom)
BRIDGE_IMPL(sceNetSyscallRecvmsg)
BRIDGE_IMPL(sceNetSyscallSendmsg)
BRIDGE_IMPL(sceNetSyscallSendto)
BRIDGE_IMPL(sceNetSyscallSetsockopt)
BRIDGE_IMPL(sceNetSyscallShutdown)
BRIDGE_IMPL(sceNetSyscallSocket)
BRIDGE_IMPL(sceNetSyscallSocketAbort)
BRIDGE_IMPL(sceNetSyscallSysctl)
