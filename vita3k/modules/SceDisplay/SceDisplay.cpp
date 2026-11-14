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

#include "SceDisplay.h"

#include <display/functions.h>
#include <display/state.h>
#include <io/state.h>
#include <kernel/state.h>
#include <packages/functions.h>
#include <renderer/state.h>
#include <util/types.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceDisplay);

static SceInt32 display_wait(EmuEnvState &emuenv, SceUID thread_id, int vcount, const bool is_since_setbuf, const bool is_cb) {
    const ThreadStateRef thread = emuenv.kernel.get_thread(thread_id);

    const SceInt32 result = is_since_setbuf
        ? wait_until_vblank(emuenv, *thread, emuenv.display.last_setframe_vblank_count + vcount, is_cb)
        : wait_vblank_start(emuenv, *thread, vcount, is_cb);
    if (result < 0)
        return result;

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

    // ignore value of sync in GetFrameBuf
    DisplayFrameInfo *info = &emuenv.display.sce_frame;

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

EXPORT(SceInt32, _sceDisplayGetMaximumFrameBufResolution, SceInt32 *width, SceInt32 *height) {
    TRACY_FUNC(_sceDisplayGetMaximumFrameBufResolution, width, height);
    if (!width || !height)
        return 0;
    if (emuenv.cfg.pstv_mode) {
        *width = 1920;
        *height = 1088;
    } else {
        // PSVita does this exact same check
        auto &title_id = emuenv.io.title_id;
        bool cond = (title_id == "PCSG80001")
            || (title_id == "PCSG80007")
            || (title_id == "PCSG00318")
            || (title_id == "PCSG00319")
            || (title_id == "PCSG00320")
            || (title_id == "PCSG00321")
            || (title_id == "PCSH00059");
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

    DisplayFrameInfo &info = emuenv.display.sce_frame;

    info.base = pFrameBuf->base;
    info.pitch = pFrameBuf->pitch;
    info.pixelformat = pFrameBuf->pixelformat;
    info.image_size.x = pFrameBuf->width;
    info.image_size.y = pFrameBuf->height;
    update_prediction(emuenv, info);

    emuenv.display.last_setframe_vblank_count = emuenv.display.vblank_count.load();
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
    return static_cast<SceInt32>(emuenv.display.vcount_60hz.load()) & 0xFFFF;
}

EXPORT(int, sceDisplayGetVcountInternal) {
    TRACY_FUNC(sceDisplayGetVcountInternal);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplayRegisterVblankStartCallback, SceUID uid) {
    TRACY_FUNC(sceDisplayRegisterVblankStartCallback, uid);

    CallbackRef callback = emuenv.kernel.objects.find<Callback>(uid);
    if (!callback)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    const SimpleEventRef event = emuenv.kernel.kernel_objects.find<SimpleEvent>(emuenv.display.vblank_event);
    if (!event)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);
    return event->register_callback(emuenv.kernel, std::move(callback), SCE_UID_INVALID_UID);
}

EXPORT(SceInt32, sceDisplayUnregisterVblankStartCallback, SceUID uid) {
    TRACY_FUNC(sceDisplayUnregisterVblankStartCallback, uid);
    // A UID of any class passes, one that names no object does not
    if (!emuenv.kernel.objects.find(uid))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    const SimpleEventRef event = emuenv.kernel.kernel_objects.find<SimpleEvent>(emuenv.display.vblank_event);
    if (!event)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);
    if (const SceInt32 error = event->unregister_callback(uid))
        return RET_ERROR(error);
    return SCE_KERNEL_OK;
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
