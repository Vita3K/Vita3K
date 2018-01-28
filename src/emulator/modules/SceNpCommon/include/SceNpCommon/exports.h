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

// SceNpCommon
BRIDGE_DECL(sceNpAuthAbortRequest)
BRIDGE_DECL(sceNpAuthCreateStartRequest)
BRIDGE_DECL(sceNpAuthDestroyRequest)
BRIDGE_DECL(sceNpAuthGetEntitlementById)
BRIDGE_DECL(sceNpAuthGetEntitlementByIdPrefix)
BRIDGE_DECL(sceNpAuthGetEntitlementIdList)
BRIDGE_DECL(sceNpAuthGetTicket)
BRIDGE_DECL(sceNpAuthGetTicketParam)
BRIDGE_DECL(sceNpAuthInit)
BRIDGE_DECL(sceNpAuthTerm)
BRIDGE_DECL(sceNpCmpNpId)
BRIDGE_DECL(sceNpCmpNpIdInOrder)
BRIDGE_DECL(sceNpCmpOnlineId)
BRIDGE_DECL(sceNpGetPlatformType)
BRIDGE_DECL(sceNpSetPlatformType)
