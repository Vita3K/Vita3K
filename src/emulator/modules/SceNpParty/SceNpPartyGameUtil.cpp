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

#include "SceNpPartyGameUtil.h"

EXPORT(int, sceNpPartyCheckCallback) {
    return unimplemented("sceNpPartyCheckCallback");
}

EXPORT(int, sceNpPartyGetGameSessionReadyState) {
    return unimplemented("sceNpPartyGetGameSessionReadyState");
}

EXPORT(int, sceNpPartyGetId) {
    return unimplemented("sceNpPartyGetId");
}

EXPORT(int, sceNpPartyGetMemberInfo) {
    return unimplemented("sceNpPartyGetMemberInfo");
}

EXPORT(int, sceNpPartyGetMemberSessionInfo) {
    return unimplemented("sceNpPartyGetMemberSessionInfo");
}

EXPORT(int, sceNpPartyGetMemberVoiceInfo) {
    return unimplemented("sceNpPartyGetMemberVoiceInfo");
}

EXPORT(int, sceNpPartyGetMembers) {
    return unimplemented("sceNpPartyGetMembers");
}

EXPORT(int, sceNpPartyGetState) {
    return unimplemented("sceNpPartyGetState");
}

EXPORT(int, sceNpPartyInit) {
    return unimplemented("sceNpPartyInit");
}

EXPORT(int, sceNpPartyRegisterHandler) {
    return unimplemented("sceNpPartyRegisterHandler");
}

EXPORT(int, sceNpPartyTerm) {
    return unimplemented("sceNpPartyTerm");
}

BRIDGE_IMPL(sceNpPartyCheckCallback)
BRIDGE_IMPL(sceNpPartyGetGameSessionReadyState)
BRIDGE_IMPL(sceNpPartyGetId)
BRIDGE_IMPL(sceNpPartyGetMemberInfo)
BRIDGE_IMPL(sceNpPartyGetMemberSessionInfo)
BRIDGE_IMPL(sceNpPartyGetMemberVoiceInfo)
BRIDGE_IMPL(sceNpPartyGetMembers)
BRIDGE_IMPL(sceNpPartyGetState)
BRIDGE_IMPL(sceNpPartyInit)
BRIDGE_IMPL(sceNpPartyRegisterHandler)
BRIDGE_IMPL(sceNpPartyTerm)
