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

#include "SceDisplay.h"

#include <display/functions.h>
#include <display/state.h>
#include <kernel/state.h>
#include <packages/functions.h>
#include <renderer/state.h>
#include <util/lock_and_find.h>
#include <util/types.h>

#include "io/state.h"

#include <util/tracy.h>
TRACY_MODULE_NAME(SceDisplay);

static int display_wait(EmuEnvState &emuenv, SceUID thread_id, int vcount, const bool is_since_setbuf, const bool is_cb) {
    const auto &thread = emuenv.kernel.get_thread(thread_id);

    uint64_t target_vcount;
    if (is_since_setbuf) {
        target_vcount = emuenv.display.last_setframe_vblank_count + vcount;
    } else {
        // the wait is considered starting from the last time the thread resumed
        // from a vblank wait (sceDisplayWait...) and not from the time this function was called
        // but we still need to wait at least for one vblank
        const uint64_t next_vsync = emuenv.display.vblank_count + 1;
        const uint64_t min_vsync = thread->last_vblank_waited + vcount;
        thread->last_vblank_waited = std::max(next_vsync, min_vsync);
        target_vcount = thread->last_vblank_waited;
    }

    wait_vblank(emuenv.display, emuenv.kernel, thread, target_vcount, is_cb);

    if (emuenv.display.abort.load())
        return SCE_DISPLAY_ERROR_NO_PIXEL_DATA;

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(SceInt32, _sceDisplayGetFrameBuf, SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync, uint32_t *pFrameBuf_size) {
    TRACY_FUNC(_sceDisplayGetFrameBuf, pFrameBuf, sync, pFrameBuf_size);
    if (pFrameBuf->size != sizeof(SceDisplayFrameBuf) && pFrameBuf->size != sizeof(SceDisplayFrameBuf2))
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);
    else if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);

    const std::lock_guard<std::mutex> guard(emuenv.display.display_info_mutex);

    DisplayFrameInfo *info;
    // ignore value of sync in GetFrameBuf
    if (emuenv.display.has_next_frame) {
        info = &emuenv.display.next_frame;
    } else {
        info = &emuenv.display.frame;
    }

    pFrameBuf->base = info->base;
    pFrameBuf->pitch = info->pitch;
    pFrameBuf->pixelformat = info->pixelformat;
    pFrameBuf->width = info->image_size.x;
    pFrameBuf->height = info->image_size.y;

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, _sceDisplayGetFrameBufInternal) {
    TRACY_FUNC(_sceDisplayGetFrameBufInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceDisplayGetMaximumFrameBufResolution, int *width, int *height) {
    TRACY_FUNC(_sceDisplayGetMaximumFrameBufResolution, width, height);
    if (!width || !height)
        return 0;
    if (emuenv.cfg.pstv_mode) {
        *width = 1920;
        *height = 1088;
    } else {
        auto &title_id = emuenv.io.title_id;
        bool cond = title_id == "PCSG80001"
            || title_id == "PCSG80007"
            || title_id == "PCSG00318"
            || title_id == "PCSG00319"
            || title_id == "PCSG00320"
            || title_id == "PCSG00321"
            || title_id == "PCSH00059";
        if (cond) {
            *width = 960;
            *height = 544;

        } else {
            *width = 1280;
            *height = 725;
        }
    }
    return 0;
}

EXPORT(int, _sceDisplayGetResolutionInfoInternal) {
    TRACY_FUNC(_sceDisplayGetResolutionInfoInternal);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceDisplaySetFrameBuf, const SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync, uint32_t *pFrameBuf_size) {
    TRACY_FUNC(_sceDisplaySetFrameBuf, pFrameBuf, sync, pFrameBuf_size);
    if (!pFrameBuf)
        return SCE_DISPLAY_ERROR_OK;
    if (pFrameBuf->size != sizeof(SceDisplayFrameBuf) && pFrameBuf->size != sizeof(SceDisplayFrameBuf2)) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);
    }
    if (!pFrameBuf->base) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_ADDR);
    }
    if (pFrameBuf->pitch < pFrameBuf->width) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_PITCH);
    }
    if (pFrameBuf->pixelformat != SCE_DISPLAY_PIXELFORMAT_A8B8G8R8) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT);
    }
    if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);
    }
    if ((pFrameBuf->width < 480) || (pFrameBuf->height < 272) || (pFrameBuf->pitch < 480))
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_RESOLUTION);

    if (sync == SCE_DISPLAY_SETBUF_IMMEDIATE) {
        // we are supposed to swap the displayed buffer in the middle of the frame
        // which we do not support
        STUBBED("SCE_DISPLAY_SETBUF_IMMEDIATE is not supported");
    }

    {
        const std::lock_guard<std::mutex> guard(emuenv.display.display_info_mutex);

        emuenv.display.has_next_frame = true;
        DisplayFrameInfo &info = emuenv.display.next_frame;

        info.base = pFrameBuf->base;
        info.pitch = pFrameBuf->pitch;
        info.pixelformat = pFrameBuf->pixelformat;
        info.image_size.x = pFrameBuf->width;
        info.image_size.y = pFrameBuf->height;
        emuenv.display.last_setframe_vblank_count = emuenv.display.vblank_count.load();

        // hack (kind of)
        // we can assume the framebuffer is already fully rendered
        // (always the case when using gxm, and should also be the case when it is not used)
        // so set this buffer as ready to be displayed
        // this should decrease the latency
        emuenv.display.frame = emuenv.display.next_frame;
        emuenv.renderer->should_display = true;
    }

    emuenv.frame_count++;

#ifdef TRACY_ENABLE
    FrameMarkNamed("SCE frame buffer"); // Tracy - Secondary frame end mark for the emulated frame buffer
#endif

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, _sceDisplaySetFrameBufForCompat) {
    TRACY_FUNC(_sceDisplaySetFrameBufForCompat);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceDisplaySetFrameBufInternal) {
    TRACY_FUNC(_sceDisplaySetFrameBufInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayGetPrimaryHead) {
    TRACY_FUNC(sceDisplayGetPrimaryHead);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplayGetRefreshRate, float *pFps) {
    TRACY_FUNC(sceDisplayGetRefreshRate, pFps);
    *pFps = 59.94005f;
    return 0;
}

EXPORT(SceInt32, sceDisplayGetVcount) {
    TRACY_FUNC(sceDisplayGetVcount);
    return static_cast<SceInt32>(emuenv.display.vblank_count.load()) & 0xFFFF;
}

EXPORT(int, sceDisplayGetVcountInternal) {
    TRACY_FUNC(sceDisplayGetVcountInternal);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplayRegisterVblankStartCallback, SceUID uid) {
    TRACY_FUNC(sceDisplayRegisterVblankStartCallback, uid);
    const auto cb = lock_and_find(uid, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);

    emuenv.display.vblank_callbacks[uid] = cb;

    return 0;
}

EXPORT(SceInt32, sceDisplayUnregisterVblankStartCallback, SceUID uid) {
    TRACY_FUNC(sceDisplayUnregisterVblankStartCallback, uid);
    if (emuenv.display.vblank_callbacks.find(uid) == emuenv.display.vblank_callbacks.end())
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);

    emuenv.display.vblank_callbacks.erase(uid);

    return 0;
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBuf) {
    TRACY_FUNC(sceDisplayWaitSetFrameBuf);
    return display_wait(emuenv, thread_id, 1, true, false);
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBufCB) {
    TRACY_FUNC(sceDisplayWaitSetFrameBufCB);
    return display_wait(emuenv, thread_id, 1, true, true);
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBufMulti, SceUInt vcount) {
    TRACY_FUNC(sceDisplayWaitSetFrameBufMulti, vcount);
    return display_wait(emuenv, thread_id, static_cast<int>(vcount), true, false);
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBufMultiCB, SceUInt vcount) {
    TRACY_FUNC(sceDisplayWaitSetFrameBufMultiCB, vcount);
    return display_wait(emuenv, thread_id, static_cast<int>(vcount), true, true);
}

EXPORT(SceInt32, sceDisplayWaitVblankStart) {
    TRACY_FUNC(sceDisplayWaitVblankStart);
    return display_wait(emuenv, thread_id, 1, false, false);
}

EXPORT(SceInt32, sceDisplayWaitVblankStartCB) {
    TRACY_FUNC(sceDisplayWaitVblankStartCB);
    return display_wait(emuenv, thread_id, 1, false, true);
}

EXPORT(SceInt32, sceDisplayWaitVblankStartMulti, SceUInt vcount) {
    TRACY_FUNC(sceDisplayWaitVblankStartMulti, vcount);
    return display_wait(emuenv, thread_id, static_cast<int>(vcount), false, false);
}

EXPORT(SceInt32, sceDisplayWaitVblankStartMultiCB, SceUInt vcount) {
    TRACY_FUNC(sceDisplayWaitVblankStartMultiCB, vcount);
    return display_wait(emuenv, thread_id, static_cast<int>(vcount), false, true);
}

BRIDGE_IMPL(_sceDisplayGetFrameBuf)
BRIDGE_IMPL(_sceDisplayGetFrameBufInternal)
BRIDGE_IMPL(_sceDisplayGetMaximumFrameBufResolution)
BRIDGE_IMPL(_sceDisplayGetResolutionInfoInternal)
BRIDGE_IMPL(_sceDisplaySetFrameBuf)
BRIDGE_IMPL(_sceDisplaySetFrameBufForCompat)
BRIDGE_IMPL(_sceDisplaySetFrameBufInternal)
BRIDGE_IMPL(sceDisplayGetPrimaryHead)
BRIDGE_IMPL(sceDisplayGetRefreshRate)
BRIDGE_IMPL(sceDisplayGetVcount)
BRIDGE_IMPL(sceDisplayGetVcountInternal)
BRIDGE_IMPL(sceDisplayRegisterVblankStartCallback)
BRIDGE_IMPL(sceDisplayUnregisterVblankStartCallback)
BRIDGE_IMPL(sceDisplayWaitSetFrameBuf)
BRIDGE_IMPL(sceDisplayWaitSetFrameBufCB)
BRIDGE_IMPL(sceDisplayWaitSetFrameBufMulti)
BRIDGE_IMPL(sceDisplayWaitSetFrameBufMultiCB)
BRIDGE_IMPL(sceDisplayWaitVblankStart)
BRIDGE_IMPL(sceDisplayWaitVblankStartCB)
BRIDGE_IMPL(sceDisplayWaitVblankStartMulti)
BRIDGE_IMPL(sceDisplayWaitVblankStartMultiCB)
