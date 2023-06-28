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

#include "SceSharedFb.h"

#include "../SceDisplay/SceDisplay.h"

#include <gxm/types.h>
#include <kernel/state.h>

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
};

LIBRARY_INIT_IMPL(SceSharedFb) {
    emuenv.kernel.obj_store.create<SharedFbState>();
}
LIBRARY_INIT_REGISTER(SceSharedFb)

EXPORT(int, sceSharedFbCreate, int smth);

EXPORT(int, _sceSharedFbOpen, int smth) {
    STUBBED("sceSharedFbCreate");
    return CALL_EXPORT(sceSharedFbCreate, smth);
}

EXPORT(int, sceSharedFbBegin, int id, SceSharedFbInfo *info) {
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    state->info.curbuf = 1;
    *info = state->info;
    return 0;
}

EXPORT(int, sceSharedFbClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbCreate, int smth) {
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
    }
    return 1;
}

EXPORT(int, sceSharedFbDelete) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSharedFbEnd) {
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    Ptr<void> data = (state->info.curbuf == 0) ? state->info.base2 : state->info.base1;
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
    SharedFbState *state = emuenv.kernel.obj_store.get<SharedFbState>();
    *info = state->info;
    return 0;
}

EXPORT(int, sceSharedFbGetRenderingInfo) {
    return UNIMPLEMENTED();
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

BRIDGE_IMPL(_sceSharedFbOpen)
BRIDGE_IMPL(sceSharedFbBegin)
BRIDGE_IMPL(sceSharedFbClose)
BRIDGE_IMPL(sceSharedFbCreate)
BRIDGE_IMPL(sceSharedFbDelete)
BRIDGE_IMPL(sceSharedFbEnd)
BRIDGE_IMPL(sceSharedFbGetInfo)
BRIDGE_IMPL(sceSharedFbGetRenderingInfo)
BRIDGE_IMPL(sceSharedFbGetShellRenderPort)
BRIDGE_IMPL(sceSharedFbUpdateProcess)
BRIDGE_IMPL(sceSharedFbUpdateProcessBegin)
BRIDGE_IMPL(sceSharedFbUpdateProcessEnd)
