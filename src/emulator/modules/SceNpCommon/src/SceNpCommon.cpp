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

#include <SceNpCommon/exports.h>

EXPORT(int, sceNpAuthAbortRequest) {
    return unimplemented("sceNpAuthAbortRequest");
}

EXPORT(int, sceNpAuthCreateStartRequest) {
    return unimplemented("sceNpAuthCreateStartRequest");
}

EXPORT(int, sceNpAuthDestroyRequest) {
    return unimplemented("sceNpAuthDestroyRequest");
}

EXPORT(int, sceNpAuthGetEntitlementById) {
    return unimplemented("sceNpAuthGetEntitlementById");
}

EXPORT(int, sceNpAuthGetEntitlementByIdPrefix) {
    return unimplemented("sceNpAuthGetEntitlementByIdPrefix");
}

EXPORT(int, sceNpAuthGetEntitlementIdList) {
    return unimplemented("sceNpAuthGetEntitlementIdList");
}

EXPORT(int, sceNpAuthGetTicket) {
    return unimplemented("sceNpAuthGetTicket");
}

EXPORT(int, sceNpAuthGetTicketParam) {
    return unimplemented("sceNpAuthGetTicketParam");
}

EXPORT(int, sceNpAuthInit) {
    return unimplemented("sceNpAuthInit");
}

EXPORT(int, sceNpAuthTerm) {
    return unimplemented("sceNpAuthTerm");
}

EXPORT(int, sceNpCmpNpId) {
    return unimplemented("sceNpCmpNpId");
}

EXPORT(int, sceNpCmpNpIdInOrder) {
    return unimplemented("sceNpCmpNpIdInOrder");
}

EXPORT(int, sceNpCmpOnlineId) {
    return unimplemented("sceNpCmpOnlineId");
}

EXPORT(int, sceNpGetPlatformType) {
    return unimplemented("sceNpGetPlatformType");
}

EXPORT(int, sceNpSetPlatformType) {
    return unimplemented("sceNpSetPlatformType");
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
