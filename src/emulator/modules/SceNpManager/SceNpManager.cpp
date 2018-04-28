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

#include "SceNpManager.h"

EXPORT(int, sceNpAuthAbortOAuthRequest) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpAuthCreateOAuthRequest) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpAuthDeleteOAuthRequest) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpAuthGetAuthorizationCode) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpCheckCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpGetServiceState) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpManagerGetAccountRegion) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpManagerGetCachedParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpManagerGetChatRestrictionFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpManagerGetContentRatingFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpManagerGetNpId) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpRegisterServiceStateCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpUnregisterServiceStateCallback) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(sceNpAuthAbortOAuthRequest)
BRIDGE_IMPL(sceNpAuthCreateOAuthRequest)
BRIDGE_IMPL(sceNpAuthDeleteOAuthRequest)
BRIDGE_IMPL(sceNpAuthGetAuthorizationCode)
BRIDGE_IMPL(sceNpCheckCallback)
BRIDGE_IMPL(sceNpGetServiceState)
BRIDGE_IMPL(sceNpInit)
BRIDGE_IMPL(sceNpManagerGetAccountRegion)
BRIDGE_IMPL(sceNpManagerGetCachedParam)
BRIDGE_IMPL(sceNpManagerGetChatRestrictionFlag)
BRIDGE_IMPL(sceNpManagerGetContentRatingFlag)
BRIDGE_IMPL(sceNpManagerGetNpId)
BRIDGE_IMPL(sceNpRegisterServiceStateCallback)
BRIDGE_IMPL(sceNpTerm)
BRIDGE_IMPL(sceNpUnregisterServiceStateCallback)
