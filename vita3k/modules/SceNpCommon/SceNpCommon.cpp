// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

#include <io/state.h>
#include <kernel/state.h>
#include <np/common.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNpCommon);

enum SceNpUtilErrorCode {
    SCE_NP_UTIL_Ok = 0x0,
    SCE_NP_UTIL_ERROR_INVALID_ARGUMENT = 0x80550601,
    SCE_NP_UTIL_ERROR_INVALID_NP_ID = 0x80550605,
    SCE_NP_UTIL_ERROR_NOT_MATCH = 0x80550609,
};

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

    if (npid1 == nullptr || npid2 == nullptr)
        return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (!npid1->isIdValid || !npid2->isIdValid)
        return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
    if (std::strncmp(npid1->handle.data, npid2->handle.data, SCE_NP_ONLINEID_MAX_LENGTH) != 0)
        return SCE_NP_UTIL_ERROR_NOT_MATCH;

    if (std::memcmp(npid1->opt.unknown, npid2->opt.unknown, 4) != 0)
        return SCE_NP_UTIL_ERROR_NOT_MATCH;

    if (npid1->opt.platformType[0] == 0 && npid2->opt.platformType[0] == 0)
        return SCE_NP_UTIL_Ok;
    if (std::memcmp(&npid1->opt.platformType, &npid2->opt.platformType, 4) != 0)
        return SCE_NP_UTIL_ERROR_NOT_MATCH;

    return SCE_NP_UTIL_Ok;
}

EXPORT(int, sceNpCmpNpIdInOrder) {
    TRACY_FUNC(sceNpCmpNpIdInOrder);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCmpOnlineId, const np::SceNpId *npid1, const np::SceNpId *npid2) {
    TRACY_FUNC(sceNpCmpOnlineId);
    if (!npid1 || !npid2)
        return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;

    if (std::strncmp(npid1->handle.data, npid2->handle.data, SCE_NP_ONLINEID_MAX_LENGTH) != 0)
        return SCE_NP_UTIL_ERROR_NOT_MATCH;

    return SCE_NP_UTIL_Ok;
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
