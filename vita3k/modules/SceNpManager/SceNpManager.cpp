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

#include "SceNpManager.h"
#include "util/types.h"

#include <io/state.h>
#include <kernel/state.h>
#include <np/state.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <np/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNpManager);

EXPORT(int, sceNpAuthAbortOAuthRequest) {
    TRACY_FUNC(sceNpAuthAbortOAuthRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthCreateOAuthRequest) {
    TRACY_FUNC(sceNpAuthCreateOAuthRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthDeleteOAuthRequest) {
    TRACY_FUNC(sceNpAuthDeleteOAuthRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetAuthorizationCode) {
    TRACY_FUNC(sceNpAuthGetAuthorizationCode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCheckCallback) {
    TRACY_FUNC(sceNpCheckCallback);
    if (emuenv.np.state == 0)
        return 0;

    emuenv.np.state = emuenv.cfg.current_config.psn_status;

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    for (auto &callback : emuenv.np.cbs) {
        thread->run_callback(callback.second.pc, { (uint32_t)emuenv.np.state, 0, callback.second.data });
    }

    return STUBBED("Stub");
}

EXPORT(int, sceNpGetServiceState, SceNpServiceState *state) {
    TRACY_FUNC(sceNpGetServiceState, state);
    *state = static_cast<SceNpServiceState>(emuenv.cfg.current_config.psn_status);

    return STUBBED("Stub");
}

EXPORT(int, sceNpInit, np::CommunicationConfig *comm_config, void *dontcare) {
    TRACY_FUNC(sceNpInit, comm_config, dontcare);
    if (emuenv.np.inited) {
        return SCE_NP_ERROR_ALREADY_INITIALIZED;
    }

    const np::CommunicationID *comm_id = (comm_config) ? comm_config->comm_id.get(emuenv.mem) : nullptr;

    if (!init(emuenv.np, comm_id)) {
        return SCE_NP_ERROR_NOT_INITIALIZED;
    }

    return 0;
}

EXPORT(int, sceNpManagerGetAccountRegion) {
    TRACY_FUNC(sceNpManagerGetAccountRegion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpManagerGetCachedParam) {
    TRACY_FUNC(sceNpManagerGetCachedParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpManagerGetChatRestrictionFlag, SceInt *isRestricted) {
    TRACY_FUNC(sceNpManagerGetChatRestrictionFlag, isRestricted);
    *isRestricted = 0; // User is never restricted
    return 0;
}

EXPORT(int, sceNpManagerGetContentRatingFlag, SceInt *isRestricted, SceInt *age) {
    TRACY_FUNC(sceNpManagerGetContentRatingFlag, isRestricted, age);
    *isRestricted = 0; // User is never restricted
    *age = 21; // Assume user is 21 years old
    return STUBBED("isRestricted = 0; age = 21; return 0;");
}

EXPORT(int, sceNpManagerGetNpId, np::SceNpId *id) {
    TRACY_FUNC(sceNpManagerGetNpId, id);
    if (emuenv.io.user_name.length() > SCE_NP_ONLINEID_MAX_LENGTH) {
        LOG_ERROR("Your online ID has over 16 characters, try again with shorter name");
        return SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL;
    }
    strcpy(id->handle.data, emuenv.io.user_name.c_str());
    id->handle.term = '\0';
    std::fill(id->handle.dummy, id->handle.dummy + 3, 0);

    // Fill the unused stuffs to 0 (prevent some weird things happen)
    std::fill(id->opt, id->opt + 8, 0);
    std::fill(id->reserved, id->reserved + 8, 0);

    return 0;
}

EXPORT(int, sceNpRegisterServiceStateCallback, Ptr<void> callback, Ptr<void> data) {
    TRACY_FUNC(sceNpRegisterServiceStateCallback, callback, data);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    uint32_t cid = emuenv.kernel.get_next_uid();
    SceNpServiceStateCallback sceNpServiceStateCallback;
    sceNpServiceStateCallback.pc = callback.address();
    sceNpServiceStateCallback.data = data.address();
    emuenv.np.cbs.emplace(cid, sceNpServiceStateCallback);
    emuenv.np.state_cb_id = cid;
    return 0;
}

EXPORT(void, sceNpTerm) {
    TRACY_FUNC(sceNpTerm);
    if (!emuenv.np.inited) {
        LOG_WARN("NP library not initialized but termination got called");
        return;
    }
    deinit(emuenv.np);
}

EXPORT(int, sceNpUnregisterServiceStateCallback) {
    TRACY_FUNC(sceNpUnregisterServiceStateCallback);
    if (emuenv.np.state_cb_id) {
        emuenv.np.cbs.erase(emuenv.np.state_cb_id);
    }
    return 0;
}

EXPORT(int, SceNpManager_B24FC028) {
    TRACY_FUNC(SceNpManager_B24FC028);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNpManager_CFAEB050) {
    TRACY_FUNC(SceNpManager_CFAEB050);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNpManager_E45E4003) {
    TRACY_FUNC(SceNpManager_E45E4003);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNpCommon_A385C8CD) {
    TRACY_FUNC(SceNpCommon_A385C8CD);
    return UNIMPLEMENTED();
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
BRIDGE_IMPL(SceNpManager_B24FC028)
BRIDGE_IMPL(SceNpManager_CFAEB050)
BRIDGE_IMPL(SceNpManager_E45E4003)
BRIDGE_IMPL(SceNpCommon_A385C8CD)
