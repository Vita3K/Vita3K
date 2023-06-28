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

#include "SceNpCommon.h"

#include <io/state.h>
#include <kernel/state.h>
#include <np/common.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNpCommon);

EXPORT(int, sceNpAuthAbortRequest) {
    TRACY_FUNC(sceNpAuthAbortRequest);
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
    TRACY_FUNC(sceNpAuthCreateStartRequest, param);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    // todo: this callback function should be called from sceNpCheckCallback
    STUBBED("Immediately call ticket callback");
    thread->run_callback(param->ticketCb.address(), { 1, 1, param->cbArg.address() });
    return 1;
}

EXPORT(int, sceNpAuthDestroyRequest) {
    TRACY_FUNC(sceNpAuthDestroyRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetEntitlementById) {
    TRACY_FUNC(sceNpAuthGetEntitlementById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetEntitlementByIdPrefix) {
    TRACY_FUNC(sceNpAuthGetEntitlementByIdPrefix);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetEntitlementIdList) {
    TRACY_FUNC(sceNpAuthGetEntitlementIdList);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetTicket) {
    TRACY_FUNC(sceNpAuthGetTicket);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetTicketParam) {
    TRACY_FUNC(sceNpAuthGetTicketParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthInit) {
    TRACY_FUNC(sceNpAuthInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthTerm) {
    TRACY_FUNC(sceNpAuthTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCmpNpId, np::SceNpId *npid1, np::SceNpId *npid2) {
    TRACY_FUNC(sceNpCmpNpId, npid1, npid2);
    STUBBED("assume single user");
    if (std::string(npid1->handle.data) == emuenv.io.user_name && std::string(npid2->handle.data) == emuenv.io.user_name) {
        return 0;
    }
    return 0x80550605; // INVALID NP ID
}

EXPORT(int, sceNpCmpNpIdInOrder) {
    TRACY_FUNC(sceNpCmpNpIdInOrder);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCmpOnlineId) {
    TRACY_FUNC(sceNpCmpOnlineId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonBase64Encode) {
    TRACY_FUNC(sceNpCommonBase64Encode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonFreeNpServerName) {
    TRACY_FUNC(sceNpCommonFreeNpServerName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonGetNpEnviroment) {
    TRACY_FUNC(sceNpCommonGetNpEnviroment);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonGetSystemSwVersion) {
    TRACY_FUNC(sceNpCommonGetSystemSwVersion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCommonMallocNpServerName) {
    TRACY_FUNC(sceNpCommonMallocNpServerName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpGetPlatformType) {
    TRACY_FUNC(sceNpGetPlatformType);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSetPlatformType) {
    TRACY_FUNC(sceNpSetPlatformType);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNpCommon_650D111B) {
    TRACY_FUNC(SceNpCommon_650D111B);
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
BRIDGE_IMPL(SceNpCommon_650D111B)
