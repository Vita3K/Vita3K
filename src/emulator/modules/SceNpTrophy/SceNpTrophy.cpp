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

#include <np/trophy/context.h>
#include <np/functions.h>
#include <util/log.h>

#include "SceNpTrophy.h"

EXPORT(int, sceNpTrophyAbortHandle) {
    STUBBED("Stubbed with SCE_OK");
    return 0;
}

EXPORT(int, sceNpTrophyCreateContext, emu::np::trophy::ContextHandle *context, const emu::np::CommunicationID *comm_id,
    void *comm_sign, const std::uint64_t options) {
    if (!host.np.trophy_state.inited) {
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;
    }

    if (comm_id && comm_id->num > 99) {
        return SCE_NP_TROPHY_ERROR_INVALID_NPCOMMID;
    }

    NpTrophyError err = NpTrophyError::TROPHY_ERROR_NONE;
    *context = create_trophy_context(host.np, host.io, host.pref_path, comm_id, &err);

    if (*context == emu::np::trophy::INVALID_CONTEXT_HANDLE) {
        switch (err) {
        case NpTrophyError::TROPHY_CONTEXT_EXIST: {
            return SCE_NP_TROPHY_ERROR_CONTEXT_ALREADY_EXISTS;
        }

        case NpTrophyError::TROPHY_CONTEXT_FILE_NON_EXIST: {
            return SCE_NP_TROPHY_ERROR_INVALID_NPCOMMID;
        }

        default:
            break;
        }
    }

    emu::np::trophy::Context *ctx_ptr = get_trophy_context(host.np.trophy_state, *context);
    LOG_TRACE("Trophy context for {}_{:0>2d} create successfuly!", ctx_ptr->comm_id.data, ctx_ptr->comm_id.num);

    return 0;
}

EXPORT(int, sceNpTrophyCreateHandle, SceNpTrophyHandle *handle) {
    // We don't handle "handle" for now. It's just a mechanic for async and its abortion.
    // Everything emulated here is sync :)
    STUBBED("Stubbed handle with 1");
    *handle = 1;
    return 0;
}

EXPORT(int, sceNpTrophyDestroyContext, emu::np::trophy::ContextHandle handle) {
    const bool result = destroy_trophy_context(host.np.trophy_state, handle);

    if (!result) {
        return SCE_NP_TROPHY_ERROR_INVALID_CONTEXT;
    }

    return 0;
}

EXPORT(int, sceNpTrophyDestroyHandle, SceNpTrophyHandle handle) {
    STUBBED("Stubbed with SCE_OK");
    return 0;
}

EXPORT(int, sceNpTrophyGetGameIcon) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetGameInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetGroupIcon) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetGroupInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetTrophyIcon) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetTrophyInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetTrophyUnlockState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyInit, void *opt) {
    if (host.np.trophy_state.inited) {
        return SCE_NP_TROPHY_ERROR_ALREADY_INITIALIZED;
    }

    if (!init(host.np.trophy_state)) {
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;
    }

    return 0;
}

EXPORT(int, sceNpTrophyTerm) {
    if (!host.np.trophy_state.inited) {
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;
    }

    if (!deinit(host.np.trophy_state)) {
        return SCE_NP_TROPHY_ERROR_ABORT;
    }

    return 0;
}

EXPORT(int, sceNpTrophyUnlockTrophy) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceNpTrophyAbortHandle)
BRIDGE_IMPL(sceNpTrophyCreateContext)
BRIDGE_IMPL(sceNpTrophyCreateHandle)
BRIDGE_IMPL(sceNpTrophyDestroyContext)
BRIDGE_IMPL(sceNpTrophyDestroyHandle)
BRIDGE_IMPL(sceNpTrophyGetGameIcon)
BRIDGE_IMPL(sceNpTrophyGetGameInfo)
BRIDGE_IMPL(sceNpTrophyGetGroupIcon)
BRIDGE_IMPL(sceNpTrophyGetGroupInfo)
BRIDGE_IMPL(sceNpTrophyGetTrophyIcon)
BRIDGE_IMPL(sceNpTrophyGetTrophyInfo)
BRIDGE_IMPL(sceNpTrophyGetTrophyUnlockState)
BRIDGE_IMPL(sceNpTrophyInit)
BRIDGE_IMPL(sceNpTrophyTerm)
BRIDGE_IMPL(sceNpTrophyUnlockTrophy)
