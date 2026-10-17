// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "../SceDisplay/SceDisplay.h"
#include <module/module.h>

#include "SceAppMgr.h"

#include <display/state.h>
#include <gxm/types.h>

#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

#include <util/tracy.h>

#include <atomic>

TRACY_MODULE_NAME(SceSharedFB);

typedef struct SceSharedFbInfo { // size is 0x58
    Ptr<void> base1; // cdram base
    int memsize;
    Ptr<void> base2; // cdram base
    int unk_0C;
    Ptr<void> unk_10;
    int unk_14;
    int unk_18;
    int unk_1C;
    int unk_20;
    int pitch; // 960
    int width; // 960
    int height; // 544
    SceGxmColorFormat color_format; // SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR
    int curbuf;
    int unk_38;
    int unk_3C;
    int unk_40;
    int unk_44;
    int vsync;
    int unk_4C;
    int unk_50;
    int unk_54;
} SceSharedFbInfo;

struct SharedFbState {
    SceSharedFbInfo info;
    std::atomic<bool> primary_registered{ false };
    std::atomic<bool> primary_vsync{ false };
};

LIBRARY_INIT(SceSharedFb) {
    emuenv.kernel.obj_store.create<SharedFbState>();
}

DECL_EXPORT(int, sceSharedFbCreate, int smth);

EXPORT(int, _sceSharedFbOpen, int smth) {
    TRACY_FUNC(_sceSharedFbOpen, smth);
    STUBBED("sceSharedFbCreate");
    return CALL_EXPORT(sceSharedFbCreate, smth);
}

EXPORT(int, sceSharedFbBegin, int id, SceSharedFbInfo *info) {
    TRACY_FUNC(sceSharedFbBegin, id, info);
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    state->primary_vsync.store(info->vsync != 0);
    state->info.curbuf = 1;
    *info = state->info;
    return 0;
}

EXPORT(int, sceSharedFbClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbCreate, int smth) {
    TRACY_FUNC(sceSharedFbCreate, smth);
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    if (state->info.memsize == 0) {
        // enough memory for 2 956x544 buffers
        constexpr uint32_t alloc_size = 4 * 1024 * 512 * 2;
        Ptr<uint8_t> data = Ptr<uint8_t>(alloc(emuenv.mem, alloc_size, "sharedFB"));
        state->info = SceSharedFbInfo{
            .base1 = data,
            .memsize = alloc_size,
            .base2 = data + alloc_size / 2,
            .pitch = 960,
            .width = 960,
            .height = 544,
            .color_format = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            .curbuf = 1,
        };
        state->primary_registered.store(true);
    }
    return 1;
}

EXPORT(int, sceSharedFbDelete) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbEnd) {
    TRACY_FUNC(sceSharedFbEnd);
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    Ptr<void> data = state->info.base2;
    // tell the display a new buffer is ready
    SceDisplayFrameBuf frame_buf{
        .size = sizeof(SceDisplayFrameBuf),
        .base = data,
        .pitch = 960,
        .width = 960,
        .height = 544
    };
    return CALL_EXPORT(_sceDisplaySetFrameBuf, &frame_buf, SCE_DISPLAY_SETBUF_NEXTFRAME, nullptr);
}

EXPORT(int, sceSharedFbGetInfo, int id, SceSharedFbInfo *info) {
    TRACY_FUNC(sceSharedFbGetInfo);
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    *info = state->info;
    return 0;
}

EXPORT(int, sceSharedFbGetRenderingInfo, SceUID pid) {
    TRACY_FUNC(sceSharedFbGetRenderingInfo, pid);
    constexpr SceInt32 permission_denied = static_cast<SceInt32>(0x80010058U);
    constexpr SceInt32 not_registered = static_cast<SceInt32>(0x80200004U);
    constexpr SceInt32 not_rendering = static_cast<SceInt32>(0x80200005U);
    const auto caller = emuenv.kernel.get_thread(thread_id);
    if (!caller)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    const auto caller_identity = emuenv.kernel.get_process_identity(caller->cpu->mem);
    if (!caller_identity || !caller_identity->is_firmware_app())
        return permission_denied;

    const auto primary = emuenv.kernel.get_process_identity(emuenv.mem);
    if (primary && primary->pid == pid) {
        const auto *state = emuenv.kernel.obj_store.get<SharedFbState>();
        if (!state || !state->primary_registered.load())
            return not_registered;
        return state->primary_vsync.load() ? 0 : not_rendering;
    }
    if (!is_secondary_process_id(emuenv, pid))
        return not_registered;

    // Secondary SharedFb is not implemented; a published display frame is the available readiness signal.
    LOG_WARN_ONCE("Secondary SharedFb rendering readiness is approximated using published display frames");
    const std::lock_guard lock(emuenv.display.display_info_mutex);
    const auto frame = emuenv.display.process_frames.find(pid);
    if (frame == emuenv.display.process_frames.end() || !frame->second.software_pixels) {
        LOG_INFO_ONCE("SharedFb is waiting for the first display frame of secondary process {}", pid);
        return not_rendering;
    }
    LOG_INFO_ONCE("SharedFb detected a display frame for secondary process {}", pid);
    return 0;
}

EXPORT(int, sceSharedFbGetShellRenderPort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbUpdateProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbUpdateProcessBegin) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbUpdateProcessEnd) {
    return UNIMPLEMENTED();
}
