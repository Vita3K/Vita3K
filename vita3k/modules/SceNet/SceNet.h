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

BRIDGE_DECL(sceNetAccept)
BRIDGE_DECL(sceNetBind)
BRIDGE_DECL(sceNetClearDnsCache)
BRIDGE_DECL(sceNetConnect)
BRIDGE_DECL(sceNetDumpAbort)
BRIDGE_DECL(sceNetDumpCreate)
BRIDGE_DECL(sceNetDumpDestroy)
BRIDGE_DECL(sceNetDumpRead)
BRIDGE_DECL(sceNetEmulationGet)
BRIDGE_DECL(sceNetEmulationSet)
BRIDGE_DECL(sceNetEpollAbort)
BRIDGE_DECL(sceNetEpollControl)
BRIDGE_DECL(sceNetEpollCreate)
BRIDGE_DECL(sceNetEpollDestroy)
BRIDGE_DECL(sceNetEpollWait)
BRIDGE_DECL(sceNetEpollWaitCB)
BRIDGE_DECL(sceNetErrnoLoc)
BRIDGE_DECL(sceNetEtherNtostr)
BRIDGE_DECL(sceNetEtherStrton)
BRIDGE_DECL(sceNetGetMacAddress)
BRIDGE_DECL(sceNetGetSockIdInfo)
BRIDGE_DECL(sceNetGetSockInfo)
BRIDGE_DECL(sceNetGetStatisticsInfo)
BRIDGE_DECL(sceNetGetpeername)
BRIDGE_DECL(sceNetGetsockname)
BRIDGE_DECL(sceNetGetsockopt)
BRIDGE_DECL(sceNetHtonl)
BRIDGE_DECL(sceNetHtonll)
BRIDGE_DECL(sceNetHtons)
BRIDGE_DECL(sceNetInetNtop)
BRIDGE_DECL(sceNetInetPton)
BRIDGE_DECL(sceNetInit)
BRIDGE_DECL(sceNetListen)
BRIDGE_DECL(sceNetNtohl)
BRIDGE_DECL(sceNetNtohll)
BRIDGE_DECL(sceNetNtohs)
BRIDGE_DECL(sceNetRecv)
BRIDGE_DECL(sceNetRecvfrom)
BRIDGE_DECL(sceNetRecvmsg)
BRIDGE_DECL(sceNetResolverAbort)
BRIDGE_DECL(sceNetResolverCreate)
BRIDGE_DECL(sceNetResolverDestroy)
BRIDGE_DECL(sceNetResolverGetError)
BRIDGE_DECL(sceNetResolverStartAton)
BRIDGE_DECL(sceNetResolverStartNtoa)
BRIDGE_DECL(sceNetSend)
BRIDGE_DECL(sceNetSendmsg)
BRIDGE_DECL(sceNetSendto)
BRIDGE_DECL(sceNetSetDnsInfo)
BRIDGE_DECL(sceNetSetsockopt)
BRIDGE_DECL(sceNetShowIfconfig)
BRIDGE_DECL(sceNetShowNetstat)
BRIDGE_DECL(sceNetShowRoute)
BRIDGE_DECL(sceNetShutdown)
BRIDGE_DECL(sceNetSocket)
BRIDGE_DECL(sceNetSocketAbort)
BRIDGE_DECL(sceNetSocketClose)
BRIDGE_DECL(sceNetTerm)
