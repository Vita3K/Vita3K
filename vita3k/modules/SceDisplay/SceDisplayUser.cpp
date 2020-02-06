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

enum SceDisplaySetBufSync {
    SCE_DISPLAY_SETBUF_IMMEDIATE = 0,
    SCE_DISPLAY_SETBUF_NEXTFRAME = 1
};

enum SceDisplayErrorCode {
    SCE_DISPLAY_ERROR_OK                    = 0,
    SCE_DISPLAY_ERROR_INVALID_HEAD          = 0x80290000,
    SCE_DISPLAY_ERROR_INVALID_VALUE         = 0x80290001,
    SCE_DISPLAY_ERROR_INVALID_ADDR          = 0x80290002,
    SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT   = 0x80290003,
    SCE_DISPLAY_ERROR_INVALID_PITCH         = 0x80290004,
    SCE_DISPLAY_ERROR_INVALID_RESOLUTION    = 0x80290005,
    SCE_DISPLAY_ERROR_INVALID_UPDATETIMING  = 0x80290006,
    SCE_DISPLAY_ERROR_NO_FRAME_BUFFER       = 0x80290007,
    SCE_DISPLAY_ERROR_NO_PIXEL_DATA         = 0x80290008,
    SCE_DISPLAY_ERROR_NO_OUTPUT_SIGNAL      = 0x80290009
};

struct SceDisplayFrameBuf {
    uint32_t size = 0;
    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    uint32_t width = 0;
    uint32_t height = 0;
};

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

EXPORT(int, sceDisplaySetFrameBuf, const SceDisplayFrameBuf *pParam, SceDisplaySetBufSync sync) {
    assert(pParam != nullptr); // Todo: pParam can be NULL, in that case black screen is shown
    if (pParam->size != sizeof(SceDisplayFrameBuf)) {
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
    host.display.image_size.x = pParam->width;
    host.display.image_size.y = pParam->height;
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
