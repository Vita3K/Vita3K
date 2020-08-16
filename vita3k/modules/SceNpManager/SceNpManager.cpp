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

#include <kernel/thread/thread_functions.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <np/functions.h>

EXPORT(int, sceNpAuthAbortOAuthRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthCreateOAuthRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthDeleteOAuthRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpAuthGetAuthorizationCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpCheckCallback) {
    if (host.np.state == 0)
        return 0;

    host.np.state = 0;

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    for (auto &callback : host.np.cbs) {
        Ptr<void> argp = Ptr<void>(callback.second.data);
        run_guest_function(*thread, callback.second.pc, host.np.state, argp);
    }

    return STUBBED("Stub");
}

EXPORT(int, sceNpGetServiceState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpInit, np::CommunicationConfig *comm_config, void *dontcare) {
    if (host.np.inited) {
        return SCE_NP_ERROR_ALREADY_INITIALIZED;
    }

    const np::CommunicationID *comm_id = (comm_config) ? comm_config->comm_id.get(host.mem) : nullptr;

    if (!init(host.np, comm_id)) {
        return SCE_NP_ERROR_NOT_INITIALIZED;
    }

    return 0;
}

EXPORT(int, sceNpManagerGetAccountRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpManagerGetCachedParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpManagerGetChatRestrictionFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpManagerGetContentRatingFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpManagerGetNpId, np::NpId *id) {
    auto str = host.cfg.online_id[host.cfg.user_id];
    if (str.length() > 16) {
        LOG_ERROR("Your online ID has over 16 characters, try again with shorter name");
        return SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL;
    }
    strcpy(id->online_id.name, str.c_str());
    id->online_id.term = '\0';
    std::fill(id->online_id.dummy, id->online_id.dummy + 3, 0);

    // Fill the unused stuffs to 0 (prevent some weird things happen)
    std::fill(id->opt, id->opt + 8, 0);
    std::fill(id->unk0, id->unk0 + 8, 0);

    return 0;
}

EXPORT(int, sceNpRegisterServiceStateCallback, Ptr<void> callback, Ptr<void> data) {
    const std::lock_guard<std::mutex> lock(host.kernel.mutex);
    uint32_t cid = host.kernel.get_next_uid();
    SceNpServiceStateCallback sceNpServiceStateCallback;
    sceNpServiceStateCallback.pc = callback.address();
    sceNpServiceStateCallback.data = data.address();
    host.np.cbs.emplace(cid, sceNpServiceStateCallback);
    host.np.state_cb_id = cid;
    return 0;
}

EXPORT(void, sceNpTerm) {
    if (!host.np.inited) {
        LOG_WARN("NP library not initialized but termination got called");
        return;
    }
    deinit(host.np);
}

EXPORT(int, sceNpUnregisterServiceStateCallback) {
    if (host.np.state_cb_id) {
        host.np.cbs.erase(host.np.state_cb_id);
    }
    return 0;
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
