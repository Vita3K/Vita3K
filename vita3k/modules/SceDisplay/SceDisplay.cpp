// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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
#include <host/functions.h>
#include <util/types.h>

static int display_wait(HostState &host, SceUID current_thread, const std::int32_t vcount, const bool is_since_setbuf) {
    wait_vblank(host.display, host.kernel, current_thread, vcount, is_since_setbuf);

    if (host.display.abort.load())
        return SCE_DISPLAY_ERROR_NO_PIXEL_DATA;

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(SceInt32, _sceDisplayGetFrameBuf, SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync) {
    if (pFrameBuf->size != sizeof(SceDisplayFrameBuf))
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);
    else if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE)
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);

    pFrameBuf->base = host.display.base;
    pFrameBuf->pitch = host.display.pitch;
    pFrameBuf->pixelformat = host.display.pixelformat;
    pFrameBuf->width = host.display.image_size.x;
    pFrameBuf->height = host.display.image_size.y;

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, _sceDisplayGetFrameBufInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceDisplayGetMaximumFrameBufResolution) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceDisplayGetResolutionInfoInternal) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceDisplaySetFrameBuf, const SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync) {
    if (!pFrameBuf)
        return SCE_DISPLAY_ERROR_OK;
    if (pFrameBuf->size != sizeof(SceDisplayFrameBuf)) {
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

    {
        const std::lock_guard<std::mutex> guard(host.display.display_info_mutex);

        host.display.base = pFrameBuf->base;
        host.display.pitch = pFrameBuf->pitch;
        host.display.pixelformat = pFrameBuf->pixelformat;
        host.display.image_size.x = pFrameBuf->width;
        host.display.image_size.y = pFrameBuf->height;
        host.display.last_setframe_vblank_count = host.display.vblank_count.load();
    }

    host.frame_count++;

    MicroProfileFlip(nullptr);

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, _sceDisplaySetFrameBufForCompat) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceDisplaySetFrameBufInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayGetPrimaryHead) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplayGetRefreshRate, float *pFps) {
    *pFps = 59.94005f;
    return 0;
}

EXPORT(SceInt32, sceDisplayGetVcount) {
    return static_cast<int>(host.display.vblank_count.load());
}

EXPORT(int, sceDisplayGetVcountInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayRegisterVblankStartCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayUnregisterVblankStartCallback) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBuf) {
    return display_wait(host, thread_id, 1, true);
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBufCB) {
    STUBBED("NO CB");

    return display_wait(host, thread_id, 1, true);
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBufMulti, SceUInt vcount) {
    return display_wait(host, thread_id, static_cast<std::int32_t>(vcount), true);
}

EXPORT(SceInt32, sceDisplayWaitSetFrameBufMultiCB, SceUInt vcount) {
    STUBBED("NO CB");

    return display_wait(host, thread_id, static_cast<std::int32_t>(vcount), true);
}

EXPORT(SceInt32, sceDisplayWaitVblankStart) {
    return display_wait(host, thread_id, 1, false);
}

EXPORT(SceInt32, sceDisplayWaitVblankStartCB) {
    STUBBED("NO CB");

    return display_wait(host, thread_id, 1, false);
}

EXPORT(SceInt32, sceDisplayWaitVblankStartMulti, SceUInt vcount) {
    return display_wait(host, thread_id, static_cast<std::int32_t>(vcount), false);
}

EXPORT(SceInt32, sceDisplayWaitVblankStartMultiCB, SceUInt vcount) {
    STUBBED("NO CB");

    return display_wait(host, thread_id, static_cast<std::int32_t>(vcount), false);
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
