// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

BRIDGE_DECL(_ZN4IPMI6Client10disconnectEv)
BRIDGE_DECL(_ZN4IPMI6Client11getUserDataEv)
BRIDGE_DECL(_ZN4IPMI6Client12tryGetResultEjPiPvPmm)
BRIDGE_DECL(_ZN4IPMI6Client12tryGetResultEjjPiPNS_10BufferInfoEj)
BRIDGE_DECL(_ZN4IPMI6Client13pollEventFlagEjjjPj)
BRIDGE_DECL(_ZN4IPMI6Client13waitEventFlagEjjjPjS1_)
BRIDGE_DECL(_ZN4IPMI6Client16invokeSyncMethodEjPKNS_8DataInfoEjPiPNS_10BufferInfoEj)
BRIDGE_DECL(_ZN4IPMI6Client16invokeSyncMethodEjPKvjPiPvPjj)
BRIDGE_DECL(_ZN4IPMI6Client17invokeAsyncMethodEjPKNS_8DataInfoEjPjPKNS0_12EventNotifeeE)
BRIDGE_DECL(_ZN4IPMI6Client17invokeAsyncMethodEjPKvjPiPKNS0_12EventNotifeeE)
BRIDGE_DECL(_ZN4IPMI6Client19terminateConnectionEv)
BRIDGE_DECL(_ZN4IPMI6Client6Config24estimateClientMemorySizeEv)
BRIDGE_DECL(_ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_)
BRIDGE_DECL(_ZN4IPMI6Client6getMsgEjPvPjjS2_)
BRIDGE_DECL(_ZN4IPMI6Client7connectEPKvjPi)
BRIDGE_DECL(_ZN4IPMI6Client7destroyEv)
BRIDGE_DECL(_ZN4IPMI6Client9tryGetMsgEjPvPmm)
BRIDGE_DECL(_ZN4IPMI6ClientD1Ev)
