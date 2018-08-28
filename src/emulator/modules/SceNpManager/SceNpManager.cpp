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
        run_on_current(*thread, Ptr<void>(callback.second.pc), host.np.state, argp);
    }

    return STUBBED("Stub");
}

EXPORT(int, sceNpGetServiceState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpInit) {
    return UNIMPLEMENTED();
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

EXPORT(int, sceNpManagerGetNpId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpRegisterServiceStateCallback, Ptr<void> callback, Ptr<void> data) {
    const std::lock_guard<std::mutex> lock(host.kernel.mutex);
    uint32_t cid = host.kernel.get_next_uid();
    emu::SceNpServiceStateCallback sceNpServiceStateCallback;
    sceNpServiceStateCallback.pc = callback.address();
    sceNpServiceStateCallback.data = data.address();
    host.np.cbs.emplace(cid, sceNpServiceStateCallback);
    return 0;
}

EXPORT(int, sceNpTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpUnregisterServiceStateCallback) {
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
