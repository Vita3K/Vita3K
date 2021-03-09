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

#include <np/functions.h>
#include <np/trophy/context.h>
#include <util/log.h>

#include "SceNpTrophy.h"

EXPORT(int, sceNpTrophyAbortHandle) {
    STUBBED("Stubbed with SCE_OK");
    return 0;
}

EXPORT(int, sceNpTrophyCreateContext, np::trophy::ContextHandle *context, const np::CommunicationID *comm_id,
    void *comm_sign, const std::uint64_t options) {
    if (!host.np.trophy_state.inited) {
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;
    }

    if (comm_id && comm_id->num > 99) {
        return SCE_NP_TROPHY_ERROR_INVALID_NPCOMMID;
    }

    np::NpTrophyError err = np::NpTrophyError::TROPHY_ERROR_NONE;
    *context = create_trophy_context(host.np, &host.io, host.pref_path, comm_id, static_cast<std::uint32_t>(host.cfg.sys_lang),
        &err);

    if (*context == np::trophy::INVALID_CONTEXT_HANDLE) {
        switch (err) {
        case np::NpTrophyError::TROPHY_CONTEXT_EXIST: {
            return SCE_NP_TROPHY_ERROR_CONTEXT_ALREADY_EXISTS;
        }

        case np::NpTrophyError::TROPHY_CONTEXT_FILE_NON_EXIST: {
            return SCE_NP_TROPHY_ERROR_TRP_FILE_NOT_FOUND;
        }

        default:
            break;
        }
    }

    np::trophy::Context *ctx_ptr = get_trophy_context(host.np.trophy_state, *context);
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

EXPORT(int, sceNpTrophyDestroyContext, np::trophy::ContextHandle handle) {
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

#define NP_TROPHY_GET_FUNCTION_STARTUP(context_handle)                                       \
    if (!host.np.trophy_state.inited) {                                                      \
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;                                          \
    }                                                                                        \
    if (!size) {                                                                             \
        return SCE_NP_TROPHY_ERROR_INVALID_ARGUMENT;                                         \
    }                                                                                        \
    np::trophy::Context *context = get_trophy_context(host.np.trophy_state, context_handle); \
    if (!context) {                                                                          \
        return SCE_NP_TROPHY_ERROR_INVALID_CONTEXT;                                          \
    }

EXPORT(int, sceNpTrophyGetGameIcon, np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle,
    void *buffer, SceSize *size) {
    NP_TROPHY_GET_FUNCTION_STARTUP(context_handle)
    return context->copy_file_data_from_trophy_file("ICON0.PNG", buffer, size);
}

EXPORT(int, sceNpTrophyGetGameInfo, np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle, SceNpTrophyGameDetails *details, SceNpTrophyGameData *data) {
    if (!host.np.trophy_state.inited) {
        return RET_ERROR(SCE_NP_TROPHY_ERROR_NOT_INITIALIZED);
    }

    if ((context_handle == np::trophy::INVALID_CONTEXT_HANDLE)
        || (api_handle == -1)
        || (!details && !data)
        || (details && (details->size != sizeof(SceNpTrophyGameDetails)))
        || (data && (data->size != sizeof(SceNpTrophyGameData)))) {
        return RET_ERROR(SCE_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    }

    np::trophy::Context *context = get_trophy_context(host.np.trophy_state, context_handle);
    if (!context) {
        return RET_ERROR(SCE_NP_TROPHY_ERROR_INVALID_CONTEXT);
    }

    if (details) {
        details->numGroups = context->group_count;
        details->numTrophies = context->trophy_count;

        std::string name, detail;
        if (!context->get_trophy_set(name, detail))
            return RET_ERROR(SCE_NP_TROPHY_ERROR_UNSUPPORTED_TROPHY_CONF);

        memcpy((char *)details->title, name.c_str(), name.size() + 1);
        memcpy((char *)details->description, detail.c_str(), detail.size() + 1);

        for (std::uint32_t i = 0; i < context->trophy_count; i++) {
            switch (context->trophy_kinds[i]) {
            case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
                details->numPlatinum++;
                break;
            case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
                details->numGold++;
                break;
            case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
                details->numSilver++;
                break;
            case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
                details->numBronze++;
                break;
            default:
                break;
            }
        }
    }

    if (data) {
        data->unlockedTrophies = context->total_trophy_unlocked();

        for (std::uint32_t i = 0; i < context->trophy_count; i++) {
            if (context->is_trophy_unlocked(i)) {
                switch (context->trophy_kinds[i]) {
                case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
                    data->unlockedPlatinum++;
                    break;
                case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
                    data->unlockedGold++;
                    break;
                case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
                    data->unlockedSilver++;
                    break;
                case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
                    data->unlockedBronze++;
                    break;
                default:
                    break;
                }
            }
        }

        if (context->trophy_count > 0)
            data->progressPercentage = data->unlockedTrophies * 100 / context->trophy_count;
    }

    return 0;
}

EXPORT(int, sceNpTrophyGetGroupIcon) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetGroupInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophyGetTrophyIcon, np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle,
    SceNpTrophyID trophy_id, void *buffer, SceSize *size) {
    NP_TROPHY_GET_FUNCTION_STARTUP(context_handle)

    // Trophy should only be in this region
    if (trophy_id < 0 || trophy_id >= np::trophy::MAX_TROPHIES) {
        return SCE_NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    }

    // Make filename
    const std::string trophy_icon_filename = fmt::format("TROP{:0>3d}.PNG", trophy_id);
    return context->copy_file_data_from_trophy_file(trophy_icon_filename.c_str(), buffer, size);
}

EXPORT(int, sceNpTrophyGetTrophyInfo, np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle, SceNpTrophyID trophy_id, SceNpTrophyDetails *details, SceNpTrophyData *data) {
    if (!host.np.trophy_state.inited) {
        return RET_ERROR(SCE_NP_TROPHY_ERROR_NOT_INITIALIZED);
    }

    if ((context_handle == np::trophy::INVALID_CONTEXT_HANDLE)
        || (api_handle == -1)
        || ((trophy_id < 0) || (trophy_id >= np::trophy::MAX_TROPHIES))
        || (!details && !data)
        || (details && (details->size != sizeof(SceNpTrophyDetails)))
        || (data && (data->size != sizeof(SceNpTrophyData)))) {
        return RET_ERROR(SCE_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    }

    np::trophy::Context *context = get_trophy_context(host.np.trophy_state, context_handle);
    if (!context) {
        return RET_ERROR(SCE_NP_TROPHY_ERROR_INVALID_CONTEXT);
    }

    if (details) {
        details->trophyId = trophy_id;
        details->trophyGrade = context->trophy_kinds[trophy_id];
        details->hidden = context->is_trophy_hidden(trophy_id);
        int32_t id = 0;
        for (uint32_t gid = 0; gid < context->group_count + 1; gid++) {
            for (auto count = 0; count < context->trophy_count_by_group[gid]; count++, id++) {
                if (trophy_id == id) {
                    details->groupId = gid;
                    break;
                }
            }
        }

        std::string name, detail;
        if (!context->get_trophy_details(trophy_id, name, detail))
            return RET_ERROR(SCE_NP_TROPHY_ERROR_UNSUPPORTED_TROPHY_CONF);

        memcpy((char *)details->name, name.c_str(), name.size() + 1);
        memcpy((char *)details->description, detail.c_str(), detail.size() + 1);
    }

    if (data) {
        data->trophyId = trophy_id;
        data->unlocked = context->is_trophy_unlocked(trophy_id);
        data->timestamp.tick = context->unlock_timestamps[trophy_id];
    }

    return 0;
}

EXPORT(int, sceNpTrophyGetTrophyUnlockState, np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle,
    np::trophy::TrophyFlagArray *flag_array, std::uint32_t *count) {
    if (!host.np.trophy_state.inited) {
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;
    }

    if (!flag_array || !count) {
        return SCE_NP_TROPHY_ERROR_INVALID_ARGUMENT;
    }

    // Get context
    np::trophy::Context *context = get_trophy_context(host.np.trophy_state, context_handle);
    if (!context) {
        return SCE_NP_TROPHY_ERROR_INVALID_CONTEXT;
    }

    *count = context->trophy_count;
    std::memcpy(flag_array, context->trophy_progress, sizeof(np::trophy::TrophyFlagArray));

    return 0;
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

static int do_trophy_callback(HostState &host, np::trophy::Context *context, SceNpTrophyID trophy_id) {
    NpTrophyUnlockCallbackData callback_data;

    if (context->trophy_kinds[trophy_id] == np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_UNKNOWN) {
        // Yes this ID is not present. So return INVALID_ID
        return SCE_NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    }

    callback_data.np_com_id = fmt::format("{}_{:0>2d}", context->comm_id.data, context->comm_id.num);
    callback_data.trophy_id = fmt::format("{:0>3d}", trophy_id);
    callback_data.trophy_kind = context->trophy_kinds[trophy_id];
    if (!context->get_trophy_details(trophy_id, callback_data.trophy_name, callback_data.trophy_detail)) {
        return SCE_NP_TROPHY_ERROR_UNSUPPORTED_TROPHY_CONF;
    }

    LOG_INFO("Trophy unlocked, name: {}, detail: {}, id = {}", callback_data.trophy_name, callback_data.trophy_detail, trophy_id);

    // Call this async.
    if (host.np.trophy_state.trophy_unlock_callback) {
        std::uint32_t buf_size = 0;

        // Make filename
        const std::string trophy_icon_filename = fmt::format("TROP{:0>3d}.PNG", trophy_id);
        context->copy_file_data_from_trophy_file(trophy_icon_filename.c_str(), nullptr, &buf_size);

        callback_data.icon_buf.resize(buf_size);
        context->copy_file_data_from_trophy_file(trophy_icon_filename.c_str(), &callback_data.icon_buf[0], &buf_size);

        host.np.trophy_state.trophy_unlock_callback(callback_data);
    }

    return 0;
}

EXPORT(int, sceNpTrophyUnlockTrophy, np::trophy::ContextHandle context_handle, SceNpTrophyHandle api_handle,
    SceNpTrophyID trophy_id, SceNpTrophyID *platinum_id) {
    if (!host.np.trophy_state.inited) {
        return SCE_NP_TROPHY_ERROR_NOT_INITIALIZED;
    }

    // Trophy should only be in this region
    if (trophy_id < 0 || trophy_id >= np::trophy::MAX_TROPHIES) {
        return SCE_NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    }

    // Get context
    np::trophy::Context *context = get_trophy_context(host.np.trophy_state, context_handle);
    if (!context) {
        return SCE_NP_TROPHY_ERROR_INVALID_CONTEXT;
    }

    np::NpTrophyError error;
    if (!context->unlock_trophy(trophy_id, &error)) {
        switch (error) {
        case np::NpTrophyError::TROPHY_PLATINUM_IS_UNBREAKABLE: {
            return SCE_NP_TROPHY_ERROR_PLATINUM_CANNOT_UNLOCK;
        }

        case np::NpTrophyError::TROPHY_ALREADY_UNLOCKED: {
            return SCE_NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED;
        }

        case np::NpTrophyError::TROPHY_ID_INVALID: {
            return SCE_NP_TROPHY_ERROR_INVALID_TROPHY_ID;
        }

        default:
            return SCE_NP_TROPHY_ERROR_ABORT;
        }
    }

    *platinum_id = -1; // SCE_NP_TROPHY_INVALID_TROPHY_ID

    if ((context->platinum_trophy_id >= 0) && (context->total_trophy_unlocked() == context->trophy_count)) {
        // Force unlock platinum trophy
        context->unlock_trophy(context->platinum_trophy_id, &error, true);
        *platinum_id = context->platinum_trophy_id;
    }

    const int err = do_trophy_callback(host, context, trophy_id);

    if (err < 0) {
        return err;
    }

    if (*platinum_id != -1) {
        // Do trophy callback for platinum too! But this time, ignore the error
        do_trophy_callback(host, context, context->platinum_trophy_id);
    }

    return 0;
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
