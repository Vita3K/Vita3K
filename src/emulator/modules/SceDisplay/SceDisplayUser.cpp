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

#include "SceDisplayUser.h"

#include <host/version.h>

#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <psp2/display.h>

#include <sstream>

namespace emu {
    struct SceDisplayFrameBuf {
        uint32_t size = 0;
        Ptr<const void> base;
        uint32_t pitch = 0;
        uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        uint32_t width = 0;
        uint32_t height = 0;
    };
}

using namespace emu;

EXPORT(int, sceDisplayGetFrameBuf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayGetFrameBufInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayGetMaximumFrameBufResolution) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplayGetResolutionInfoInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplaySetFrameBuf, const emu::SceDisplayFrameBuf *pParam, SceDisplaySetBufSync sync) {
    assert(pParam != nullptr); // Todo: pParam can be NULL, in that case black screen is shown
    if (pParam->size != sizeof(emu::SceDisplayFrameBuf)) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_VALUE);
    }
    if (!pParam->base) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_ADDR);
    }
    if (pParam->pitch < pParam->width) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_PITCH);
    }
    if (pParam->pixelformat != SCE_DISPLAY_PIXELFORMAT_A8B8G8R8) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT);
    }
    if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE) {
        return RET_ERROR(SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);
    }

    host.display.base = pParam->base;
    host.display.pitch = pParam->pitch;
    host.display.pixelformat = pParam->pixelformat;
    host.display.image_size.width = pParam->width;
    host.display.image_size.height = pParam->height;
    ++host.frame_count;

    MicroProfileFlip(nullptr);

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, sceDisplaySetFrameBufForCompat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDisplaySetFrameBufInternal) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceDisplayGetFrameBuf)
BRIDGE_IMPL(sceDisplayGetFrameBufInternal)
BRIDGE_IMPL(sceDisplayGetMaximumFrameBufResolution)
BRIDGE_IMPL(sceDisplayGetResolutionInfoInternal)
BRIDGE_IMPL(sceDisplaySetFrameBuf)
BRIDGE_IMPL(sceDisplaySetFrameBufForCompat)
BRIDGE_IMPL(sceDisplaySetFrameBufInternal)
