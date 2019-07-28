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

#define SCE_NP_ERROR_ALREADY_INITIALIZED 0x80550001
#define SCE_NP_ERROR_NOT_INITIALIZED 0x80550002
#define SCE_NP_ERROR_INVALID_ARGUMENT 0x80550003
#define SCE_NP_ERROR_UNKNOWN_PLATFORM_TYPE 0x80550004
#define SCE_NP_MANAGER_ERROR_ABORTED 0x80550507
#define SCE_NP_MANAGER_ERROR_ALREADY_INITIALIZED 0x80550501
#define SCE_NP_MANAGER_ERROR_OUT_OF_MEMORY 0x80550504
#define SCE_NP_MANAGER_ERROR_NOT_INITIALIZED 0x80550502
#define SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT 0x80550503
#define SCE_NP_MANAGER_ERROR_INVALID_STATE 0x80550506
#define SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL 0x80550509

BRIDGE_DECL(sceNpAuthAbortOAuthRequest)
BRIDGE_DECL(sceNpAuthCreateOAuthRequest)
BRIDGE_DECL(sceNpAuthDeleteOAuthRequest)
BRIDGE_DECL(sceNpAuthGetAuthorizationCode)
BRIDGE_DECL(sceNpCheckCallback)
BRIDGE_DECL(sceNpGetServiceState)
BRIDGE_DECL(sceNpInit)
BRIDGE_DECL(sceNpManagerGetAccountRegion)
BRIDGE_DECL(sceNpManagerGetCachedParam)
BRIDGE_DECL(sceNpManagerGetChatRestrictionFlag)
BRIDGE_DECL(sceNpManagerGetContentRatingFlag)
BRIDGE_DECL(sceNpManagerGetNpId)
BRIDGE_DECL(sceNpRegisterServiceStateCallback)
BRIDGE_DECL(sceNpTerm)
BRIDGE_DECL(sceNpUnregisterServiceStateCallback)
