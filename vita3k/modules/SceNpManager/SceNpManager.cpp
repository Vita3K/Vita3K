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

#include "SceNpManager.h"

#include "util/types.h"

#include <io/state.h>
#include <kernel/state.h>
#include <util/log.h>

#include <np/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNpManager);

enum SceNpErrorCode : uint32_t {
    SCE_NP_ERROR_ALREADY_INITIALIZED = 0x80550001,
    SCE_NP_ERROR_NOT_INITIALIZED = 0x80550002,
    SCE_NP_ERROR_INVALID_ARGUMENT = 0x80550003,
    SCE_NP_ERROR_UNKNOWN_PLATFORM_TYPE = 0x80550004
};

enum SceNpManagerErrorCode : uint32_t {
    SCE_NP_MANAGER_ERROR_ABORTED = 0x80550507,
    SCE_NP_MANAGER_ERROR_ALREADY_INITIALIZED = 0x80550501,
    SCE_NP_MANAGER_ERROR_OUT_OF_MEMORY = 0x80550504,
    SCE_NP_MANAGER_ERROR_NOT_INITIALIZED = 0x80550502,
    SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT = 0x80550503,
    SCE_NP_MANAGER_ERROR_INVALID_STATE = 0x80550506,
    SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL = 0x80550509
};

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

    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    const SceNpServiceState state = emuenv.cfg.current_config.psn_signed_in ? SCE_NP_SERVICE_STATE_SIGNED_IN : SCE_NP_SERVICE_STATE_SIGNED_OUT;
    for (auto &[_, np_callback] : emuenv.np.cbs) {
        thread->run_callback(np_callback.pc, { static_cast<uint32_t>(state), 0, np_callback.data });
    }

    return STUBBED("Stub");
}

EXPORT(int, sceNpGetServiceState, SceNpServiceState *state) {
    TRACY_FUNC(sceNpGetServiceState, state);
    *state = emuenv.cfg.current_config.psn_signed_in ? SCE_NP_SERVICE_STATE_SIGNED_IN : SCE_NP_SERVICE_STATE_SIGNED_OUT;

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

#define SCE_NP_MANAGER_ERROR_NEED_CALL_NETCHECK_DIALOG 0x8055050b

EXPORT(int, sceNpManagerGetNpId, np::SceNpId *id) {
    TRACY_FUNC(sceNpManagerGetNpId, id);
    if (emuenv.io.user_name.length() > SCE_NP_ONLINEID_MAX_LENGTH) {
        LOG_ERROR("Your online ID has over 16 characters, try again with shorter name");
        return SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL;
    }
    // Fill the unused stuffs to 0 (prevent some weird things happen)
    memset(id, 0, sizeof(np::SceNpId));
    strcpy(id->handle.data, emuenv.io.user_name.c_str());
    id->isIdValid = true;
    id->opt.platformType[0] = 'p';
    id->opt.platformType[1] = 's';
    id->opt.platformType[2] = 'p';
    id->opt.platformType[3] = '2';
    return 0;
}

EXPORT(int, sceNpRegisterServiceStateCallback, Ptr<void> callback, Ptr<void> data) {
    TRACY_FUNC(sceNpRegisterServiceStateCallback, callback, data);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    SceUID cid = emuenv.kernel.get_next_uid();
    SceNpServiceStateCallback sceNpServiceStateCallback{
        .pc = callback.address(),
        .data = data.address()
    };
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
