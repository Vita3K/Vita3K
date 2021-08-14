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

BRIDGE_DECL(sceNpPartyCheckCallback)
BRIDGE_DECL(sceNpPartyGetGameSessionReadyState)
BRIDGE_DECL(sceNpPartyGetId)
BRIDGE_DECL(sceNpPartyGetMemberInfo)
BRIDGE_DECL(sceNpPartyGetMemberSessionInfo)
BRIDGE_DECL(sceNpPartyGetMemberVoiceInfo)
BRIDGE_DECL(sceNpPartyGetMembers)
BRIDGE_DECL(sceNpPartyGetState)
BRIDGE_DECL(sceNpPartyInit)
BRIDGE_DECL(sceNpPartyRegisterHandler)
BRIDGE_DECL(sceNpPartyTerm)
