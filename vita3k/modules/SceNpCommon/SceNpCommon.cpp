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

#include "SceNpCommon.h"

#include <io/state.h>
#include <kernel/state.h>
#include <np/common.h>

EXPORT(int, sceNpAuthAbortRequest) {
    return UNIMPLEMENTED();
}

struct SceNpAuthRequestParameter {
    SceSize size;
    uint32_t version;
    Ptr<const char> serviceId;
    Ptr<const void> cookie;
    SceSize cookieSize;
    Ptr<const char> entitlementId;
    SceUInt32 consumedCount;
    Ptr<void> ticketCb; // int (*ticketCb)(SceNpAuthRequestId, int, void *);
    Ptr<void> cbArg;
};

EXPORT(int, sceNpAuthCreateStartRequest, const SceNpAuthRequestParameter *param) {
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    // todo: this callback function should be called from sceNpCheckCallback
    STUBBED("Immediately call ticket callback");
    thread->request_callback(param->ticketCb.address(), { 1, 1, param->cbArg.address() });
    return 1;
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
    if (std::string(npid1->online_id.name) == emuenv.io.user_name && std::string(npid2->online_id.name) == emuenv.io.user_name) {
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

EXPORT(int, sceNpCommonBase64Encode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonFreeNpServerName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonGetNpEnviroment) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonGetSystemSwVersion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonMallocNpServerName) {
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
BRIDGE_IMPL(sceNpCommonBase64Encode)
BRIDGE_IMPL(sceNpCommonFreeNpServerName)
BRIDGE_IMPL(sceNpCommonGetNpEnviroment)
BRIDGE_IMPL(sceNpCommonGetSystemSwVersion)
BRIDGE_IMPL(sceNpCommonMallocNpServerName)
BRIDGE_IMPL(sceNpGetPlatformType)
BRIDGE_IMPL(sceNpSetPlatformType)
