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

#pragma once

#include <module/module.h>

BRIDGE_DECL(sceNetAdhocGetPdpStat)
BRIDGE_DECL(sceNetAdhocGetPtpStat)
BRIDGE_DECL(sceNetAdhocGetSocketAlert)
BRIDGE_DECL(sceNetAdhocInit)
BRIDGE_DECL(sceNetAdhocPdpCreate)
BRIDGE_DECL(sceNetAdhocPdpDelete)
BRIDGE_DECL(sceNetAdhocPdpRecv)
BRIDGE_DECL(sceNetAdhocPdpSend)
BRIDGE_DECL(sceNetAdhocPollSocket)
BRIDGE_DECL(sceNetAdhocPtpAccept)
BRIDGE_DECL(sceNetAdhocPtpClose)
BRIDGE_DECL(sceNetAdhocPtpConnect)
BRIDGE_DECL(sceNetAdhocPtpFlush)
BRIDGE_DECL(sceNetAdhocPtpListen)
BRIDGE_DECL(sceNetAdhocPtpOpen)
BRIDGE_DECL(sceNetAdhocPtpRecv)
BRIDGE_DECL(sceNetAdhocPtpSend)
BRIDGE_DECL(sceNetAdhocSetSocketAlert)
BRIDGE_DECL(sceNetAdhocTerm)
BRIDGE_DECL(sceNetAdhocctlGetAddrByName)
BRIDGE_DECL(sceNetAdhocctlGetAdhocId)
BRIDGE_DECL(sceNetAdhocctlGetEtherAddr)
BRIDGE_DECL(sceNetAdhocctlGetNameByAddr)
BRIDGE_DECL(sceNetAdhocctlGetParameter)
BRIDGE_DECL(sceNetAdhocctlGetPeerInfo)
BRIDGE_DECL(sceNetAdhocctlGetPeerList)
BRIDGE_DECL(sceNetAdhocctlInit)
BRIDGE_DECL(sceNetAdhocctlTerm)
