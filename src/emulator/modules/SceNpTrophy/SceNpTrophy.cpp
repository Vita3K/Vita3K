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
            return SCE_NP_TROPHY_ERROR_TRP_FILE_NOT_FOUND;
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

static int copy_file_data_from_trophy_file(emu::np::trophy::Context *context, const char *filename, void *buffer,
    SceSize *size) {
    // Search for ICON0.PNG in the trophy file
    const std::uint32_t file_index = context->trophy_file.search_file(filename);

    if (file_index == static_cast<std::uint32_t>(-1)) {
        return SCE_NP_TROPHY_ERROR_ICON_FILE_NOT_FOUND;
    }

    if (!buffer) {
        // Set the needed size for the target buffer
        *size = static_cast<SceSize>(context->trophy_file.entries[file_index].size);
        return 0;
    }

    // Do buffer copy
    *size = std::min<SceSize>(static_cast<SceSize>(context->trophy_file.entries[file_index].size),
        *size);

    std::uint32_t size_left = *size;

    context->trophy_file.get_entry_data(file_index, [&](void *source, std::uint32_t source_size) -> bool {
        if (size_left == 0) {
            return false;
        }

        source_size = std::min<std::uint32_t>(size_left, source_size);
        std::copy(reinterpret_cast<std::uint8_t*>(source), reinterpret_cast<std::uint8_t*>(source) + source_size,
            reinterpret_cast<std::uint8_t*>(buffer));

        buffer = reinterpret_cast<std::uint8_t*>(buffer) + source_size;
        return true;
    });

    return 0;
}

#define NP_TROPHY_GET_FUNCTION_STARTUP(context_handle)                                              \
    if (!host.np.trophy_state.inited) {                                                             \
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;                                                 \
    }                                                                                               \
    if (!size) {                                                                                    \
        return SCE_NP_TROPHY_ERROR_INVALID_ARGUMENT;                                                \
    }                                                                                               \
    emu::np::trophy::Context *context = get_trophy_context(host.np.trophy_state, context_handle);   \
    if (!context) {                                                                                 \
        return SCE_NP_TROPHY_ERROR_INVALID_CONTEXT;                                                 \
    }

EXPORT(int, sceNpTrophyGetGameIcon, emu::np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle,
    void *buffer, SceSize *size) {
    NP_TROPHY_GET_FUNCTION_STARTUP(context_handle)
    return copy_file_data_from_trophy_file(context, "ICON0.PNG", buffer, size);
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

EXPORT(int, sceNpTrophyGetTrophyIcon, emu::np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle,
    SceNpTrophyID trophy_id, void *buffer, SceSize *size) {
    NP_TROPHY_GET_FUNCTION_STARTUP(context_handle)

    // Trophy should only be in this region
    if (trophy_id < 0 || trophy_id >= NP_MAX_TROPHIES) {
        return SCE_NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    }

    // Make filename
    const std::string trophy_icon_filename = fmt::format("TROP{:0>3d}.PNG", trophy_id);
    return copy_file_data_from_trophy_file(context, trophy_icon_filename.c_str(), buffer, size);
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
