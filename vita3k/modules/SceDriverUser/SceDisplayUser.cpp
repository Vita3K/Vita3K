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
#include "display_guest.h"
#include <cpu/state.h>
#include <display/guest_frame.h>
#include <display/process_frames.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <module/module.h>
#include <renderer/state.h>

#include <util/tracy.h>

#include <new>

TRACY_MODULE_NAME(SceDisplayUser);

EXPORT(SceInt32, sceDisplayGetFrameBuf, SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync) {
    TRACY_FUNC(sceDisplayGetFrameBuf, pFrameBuf, sync);
    if (!pFrameBuf)
        return SCE_DISPLAY_ERROR_INVALID_ADDR;
    if ((pFrameBuf->size & 0xfffffffb) != 0x18)
        return SCE_DISPLAY_ERROR_INVALID_VALUE;

    uint32_t pFrameBuf_size = pFrameBuf->size;
    return CALL_EXPORT(_sceDisplayGetFrameBuf, pFrameBuf, sync, &pFrameBuf_size);
}

EXPORT(int, sceDisplayGetFrameBufInternal) {
    TRACY_FUNC(sceDisplayGetFrameBufInternal);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplayGetMaximumFrameBufResolution, SceInt32 *width, SceInt32 *height) {
    TRACY_FUNC(sceDisplayGetMaximumFrameBufResolution, width, height);
    return CALL_EXPORT(_sceDisplayGetMaximumFrameBufResolution, width, height);
}

EXPORT(int, sceDisplayGetResolutionInfoInternal) {
    TRACY_FUNC(sceDisplayGetResolutionInfoInternal);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplaySetFrameBuf, Ptr<const SceDisplayFrameBuf> pFrameBuf, SceDisplaySetBufSync sync) {
    TRACY_FUNC(sceDisplaySetFrameBuf, pFrameBuf, sync);
    const auto thread = emuenv.kernel.get_thread(thread_id);
    const auto pid = emuenv.kernel.get_thread_process_id(thread_id);
    if (!thread || !pid)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_ADDR);
    auto &mem = thread->cpu->mem;
    const bool primary = emuenv.kernel.is_primary_address_space(mem);
    if (!pFrameBuf) {
        if (primary) {
            uint32_t size = 0;
            return CALL_EXPORT(_sceDisplaySetFrameBuf, nullptr, sync, &size);
        }
        if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE)
            return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);
        if (display::clear_process_frame(emuenv.display, *pid))
            emuenv.renderer->should_display = true;
        return SCE_DISPLAY_ERROR_OK;
    }

    const auto request = display_guest::read_frame(mem, pFrameBuf);
    if (!request)
        return RET_ERROR(request.error());
    const SceDisplayFrameBuf &frame_buf = *request;
    if (primary) {
        uint32_t size = frame_buf.size;
        return CALL_EXPORT(_sceDisplaySetFrameBuf, &frame_buf, sync, &size);
    }
    if (!frame_buf.base)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_ADDR);
    if (frame_buf.pitch < frame_buf.width)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_PITCH);
    if (frame_buf.pixelformat != SCE_DISPLAY_PIXELFORMAT_A8B8G8R8)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT);
    if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);
    if (frame_buf.width < 480 || frame_buf.height < 272 || frame_buf.pitch < 480
        || frame_buf.width > 1920 || frame_buf.height > 1088)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_RESOLUTION);
    if (sync == SCE_DISPLAY_SETBUF_IMMEDIATE)
        STUBBED("SCE_DISPLAY_SETBUF_IMMEDIATE is not supported");

    DisplayFrameInfo frame{};
    frame.base = frame_buf.base;
    frame.pitch = frame_buf.pitch;
    frame.pixelformat = frame_buf.pixelformat;
    frame.image_size = { static_cast<int32_t>(frame_buf.width), static_cast<int32_t>(frame_buf.height) };
    frame.owner_pid = *pid;
    try {
        if (!display::capture_software_frame(mem, frame))
            return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_ADDR);
        if (display::submit_process_frame(emuenv.display, *pid, std::move(frame)))
            emuenv.renderer->should_display = true;
    } catch (const std::bad_alloc &) {
        return RET_ERROR(SCE_DISPLAY_ERROR_NO_PIXEL_DATA);
    }
    LOG_WARN_ONCE("Secondary display uses software framebuffer snapshots and simplified Shell alpha composition; secondary GXM rendering is not supported");
    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, sceDisplaySetFrameBufForCompat) {
    TRACY_FUNC(sceDisplaySetFrameBufForCompat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplaySetFrameBufInternal, uint32_t maybe_buffer_idx, uint32_t unkn, SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync) {
    TRACY_FUNC(sceDisplaySetFrameBufInternal, maybe_buffer_idx, unkn, pFrameBuf, sync);
    // only render for frame buffer 0 or we'll get double fps
    if (maybe_buffer_idx != 0)
        return 0;

    return CALL_EXPORT(_sceDisplaySetFrameBuf, pFrameBuf, sync, nullptr);
}
