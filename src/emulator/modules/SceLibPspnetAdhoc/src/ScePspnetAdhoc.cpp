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

#include <SceLibPspnetAdhoc/exports.h>

EXPORT(int, sceNetAdhocGetPdpStat) {
    return unimplemented("sceNetAdhocGetPdpStat");
}

EXPORT(int, sceNetAdhocGetPtpStat) {
    return unimplemented("sceNetAdhocGetPtpStat");
}

EXPORT(int, sceNetAdhocGetSocketAlert) {
    return unimplemented("sceNetAdhocGetSocketAlert");
}

EXPORT(int, sceNetAdhocInit) {
    return unimplemented("sceNetAdhocInit");
}

EXPORT(int, sceNetAdhocPdpCreate) {
    return unimplemented("sceNetAdhocPdpCreate");
}

EXPORT(int, sceNetAdhocPdpDelete) {
    return unimplemented("sceNetAdhocPdpDelete");
}

EXPORT(int, sceNetAdhocPdpRecv) {
    return unimplemented("sceNetAdhocPdpRecv");
}

EXPORT(int, sceNetAdhocPdpSend) {
    return unimplemented("sceNetAdhocPdpSend");
}

EXPORT(int, sceNetAdhocPollSocket) {
    return unimplemented("sceNetAdhocPollSocket");
}

EXPORT(int, sceNetAdhocPtpAccept) {
    return unimplemented("sceNetAdhocPtpAccept");
}

EXPORT(int, sceNetAdhocPtpClose) {
    return unimplemented("sceNetAdhocPtpClose");
}

EXPORT(int, sceNetAdhocPtpConnect) {
    return unimplemented("sceNetAdhocPtpConnect");
}

EXPORT(int, sceNetAdhocPtpFlush) {
    return unimplemented("sceNetAdhocPtpFlush");
}

EXPORT(int, sceNetAdhocPtpListen) {
    return unimplemented("sceNetAdhocPtpListen");
}

EXPORT(int, sceNetAdhocPtpOpen) {
    return unimplemented("sceNetAdhocPtpOpen");
}

EXPORT(int, sceNetAdhocPtpRecv) {
    return unimplemented("sceNetAdhocPtpRecv");
}

EXPORT(int, sceNetAdhocPtpSend) {
    return unimplemented("sceNetAdhocPtpSend");
}

EXPORT(int, sceNetAdhocSetSocketAlert) {
    return unimplemented("sceNetAdhocSetSocketAlert");
}

EXPORT(int, sceNetAdhocTerm) {
    return unimplemented("sceNetAdhocTerm");
}

EXPORT(int, sceNetAdhocctlGetAddrByName) {
    return unimplemented("sceNetAdhocctlGetAddrByName");
}

EXPORT(int, sceNetAdhocctlGetAdhocId) {
    return unimplemented("sceNetAdhocctlGetAdhocId");
}

EXPORT(int, sceNetAdhocctlGetEtherAddr) {
    return unimplemented("sceNetAdhocctlGetEtherAddr");
}

EXPORT(int, sceNetAdhocctlGetNameByAddr) {
    return unimplemented("sceNetAdhocctlGetNameByAddr");
}

EXPORT(int, sceNetAdhocctlGetParameter) {
    return unimplemented("sceNetAdhocctlGetParameter");
}

EXPORT(int, sceNetAdhocctlGetPeerInfo) {
    return unimplemented("sceNetAdhocctlGetPeerInfo");
}

EXPORT(int, sceNetAdhocctlGetPeerList) {
    return unimplemented("sceNetAdhocctlGetPeerList");
}

EXPORT(int, sceNetAdhocctlInit) {
    return unimplemented("sceNetAdhocctlInit");
}

EXPORT(int, sceNetAdhocctlTerm) {
    return unimplemented("sceNetAdhocctlTerm");
}

BRIDGE_IMPL(sceNetAdhocGetPdpStat)
BRIDGE_IMPL(sceNetAdhocGetPtpStat)
BRIDGE_IMPL(sceNetAdhocGetSocketAlert)
BRIDGE_IMPL(sceNetAdhocInit)
BRIDGE_IMPL(sceNetAdhocPdpCreate)
BRIDGE_IMPL(sceNetAdhocPdpDelete)
BRIDGE_IMPL(sceNetAdhocPdpRecv)
BRIDGE_IMPL(sceNetAdhocPdpSend)
BRIDGE_IMPL(sceNetAdhocPollSocket)
BRIDGE_IMPL(sceNetAdhocPtpAccept)
BRIDGE_IMPL(sceNetAdhocPtpClose)
BRIDGE_IMPL(sceNetAdhocPtpConnect)
BRIDGE_IMPL(sceNetAdhocPtpFlush)
BRIDGE_IMPL(sceNetAdhocPtpListen)
BRIDGE_IMPL(sceNetAdhocPtpOpen)
BRIDGE_IMPL(sceNetAdhocPtpRecv)
BRIDGE_IMPL(sceNetAdhocPtpSend)
BRIDGE_IMPL(sceNetAdhocSetSocketAlert)
BRIDGE_IMPL(sceNetAdhocTerm)
BRIDGE_IMPL(sceNetAdhocctlGetAddrByName)
BRIDGE_IMPL(sceNetAdhocctlGetAdhocId)
BRIDGE_IMPL(sceNetAdhocctlGetEtherAddr)
BRIDGE_IMPL(sceNetAdhocctlGetNameByAddr)
BRIDGE_IMPL(sceNetAdhocctlGetParameter)
BRIDGE_IMPL(sceNetAdhocctlGetPeerInfo)
BRIDGE_IMPL(sceNetAdhocctlGetPeerList)
BRIDGE_IMPL(sceNetAdhocctlInit)
BRIDGE_IMPL(sceNetAdhocctlTerm)
