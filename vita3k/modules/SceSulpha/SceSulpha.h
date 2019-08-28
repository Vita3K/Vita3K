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

BRIDGE_DECL(sceSulphaAgentsGetNeededMemory)
BRIDGE_DECL(sceSulphaAgentsRegister)
BRIDGE_DECL(sceSulphaAgentsUnregister)
BRIDGE_DECL(sceSulphaEventAdd)
BRIDGE_DECL(sceSulphaEventDelete)
BRIDGE_DECL(sceSulphaEventNew)
BRIDGE_DECL(sceSulphaEventReport)
BRIDGE_DECL(sceSulphaFileConnect)
BRIDGE_DECL(sceSulphaFileDisconnect)
BRIDGE_DECL(sceSulphaGetAgent)
BRIDGE_DECL(sceSulphaGetDefaultConfig)
BRIDGE_DECL(sceSulphaGetNeededMemory)
BRIDGE_DECL(sceSulphaGetTimestamp)
BRIDGE_DECL(sceSulphaInit)
BRIDGE_DECL(sceSulphaLogHandler)
BRIDGE_DECL(sceSulphaLogSetLevel)
BRIDGE_DECL(sceSulphaNetworkInit)
BRIDGE_DECL(sceSulphaNetworkShutdown)
BRIDGE_DECL(sceSulphaNodeDelete)
BRIDGE_DECL(sceSulphaNodeNew)
BRIDGE_DECL(sceSulphaSetBookmark)
BRIDGE_DECL(sceSulphaShutdown)
BRIDGE_DECL(sceSulphaUpdate)
