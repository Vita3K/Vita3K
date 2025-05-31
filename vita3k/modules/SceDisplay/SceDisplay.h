// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#pragma once

#include <display/state.h>
#include <module/module.h>
#include <util/tracy.h>

enum SceDisplaySetBufSync {
    SCE_DISPLAY_SETBUF_IMMEDIATE = 0,
    SCE_DISPLAY_SETBUF_NEXTFRAME = 1
};

template <>
inline std::string to_debug_str<SceDisplaySetBufSync>(const MemState &mem, SceDisplaySetBufSync type) {
    switch (type) {
    case SCE_DISPLAY_SETBUF_IMMEDIATE:
        return "SCE_DISPLAY_SETBUF_IMMEDIATE";
    case SCE_DISPLAY_SETBUF_NEXTFRAME:
        return "SCE_DISPLAY_SETBUF_NEXTFRAME";
    }
    return std::to_string(type);
}

enum SceDisplayErrorCode : uint32_t {
    SCE_DISPLAY_ERROR_OK = 0,
    SCE_DISPLAY_ERROR_INVALID_HEAD = 0x80290000,
    SCE_DISPLAY_ERROR_INVALID_VALUE = 0x80290001,
    SCE_DISPLAY_ERROR_INVALID_ADDR = 0x80290002,
    SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT = 0x80290003,
    SCE_DISPLAY_ERROR_INVALID_PITCH = 0x80290004,
    SCE_DISPLAY_ERROR_INVALID_RESOLUTION = 0x80290005,
    SCE_DISPLAY_ERROR_INVALID_UPDATETIMING = 0x80290006,
    SCE_DISPLAY_ERROR_NO_FRAME_BUFFER = 0x80290007,
    SCE_DISPLAY_ERROR_NO_PIXEL_DATA = 0x80290008,
    SCE_DISPLAY_ERROR_NO_OUTPUT_SIGNAL = 0x80290009
};

struct SceDisplayFrameBuf {
    SceUInt32 size = 0;
    Ptr<const void> base;
    SceUInt32 pitch = 0;
    SceUInt32 pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    SceUInt32 width = 0;
    SceUInt32 height = 0;
};

// Only BigSky uses this version of the structure
struct SceDisplayFrameBuf2 : public SceDisplayFrameBuf {
    // BigSky uses this field and sets it to a value between 0 and 5, in practice always 1
    SceUInt32 unkn = 0;
};

DECL_EXPORT(SceInt32, _sceDisplayGetFrameBuf, SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync, uint32_t *pFrameBuf_size);
DECL_EXPORT(SceInt32, _sceDisplayGetMaximumFrameBufResolution, SceInt32 *width, SceInt32 *height);
DECL_EXPORT(SceInt32, _sceDisplaySetFrameBuf, const SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync, uint32_t *pFrameBuf_size);
DECL_EXPORT(SceInt32, sceDisplayRegisterVblankStartCallback, SceUID uid);
