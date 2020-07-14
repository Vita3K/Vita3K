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

#include "SceNpCommon.h"

EXPORT(int, sceNpAuthAbortRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthCreateStartRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthDestroyRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetEntitlementById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetEntitlementByIdPrefix) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetEntitlementIdList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetTicket) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetTicketParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCmpNpId, np::NpId *npid1, np::NpId *npid2) {
    STUBBED("assume single user");
    auto username = host.cfg.online_id[host.cfg.user_id];
    if (std::string(npid1->online_id.name) == username && std::string(npid2->online_id.name) == username) {
        return 0;
    }
    return 0x80550605; // INVALID NP ID
}

EXPORT(int, sceNpCmpNpIdInOrder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCmpOnlineId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpGetPlatformType) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSetPlatformType) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceNpAuthAbortRequest)
BRIDGE_IMPL(sceNpAuthCreateStartRequest)
BRIDGE_IMPL(sceNpAuthDestroyRequest)
BRIDGE_IMPL(sceNpAuthGetEntitlementById)
BRIDGE_IMPL(sceNpAuthGetEntitlementByIdPrefix)
BRIDGE_IMPL(sceNpAuthGetEntitlementIdList)
BRIDGE_IMPL(sceNpAuthGetTicket)
BRIDGE_IMPL(sceNpAuthGetTicketParam)
BRIDGE_IMPL(sceNpAuthInit)
BRIDGE_IMPL(sceNpAuthTerm)
BRIDGE_IMPL(sceNpCmpNpId)
BRIDGE_IMPL(sceNpCmpNpIdInOrder)
BRIDGE_IMPL(sceNpCmpOnlineId)
BRIDGE_IMPL(sceNpGetPlatformType)
BRIDGE_IMPL(sceNpSetPlatformType)
