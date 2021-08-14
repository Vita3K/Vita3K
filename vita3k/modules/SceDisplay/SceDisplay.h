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

#pragma once

#include <module/module.h>

enum SceDisplaySetBufSync {
    SCE_DISPLAY_SETBUF_IMMEDIATE = 0,
    SCE_DISPLAY_SETBUF_NEXTFRAME = 1
};

enum SceDisplayErrorCode {
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

EXPORT(SceInt32, _sceDisplaySetFrameBuf, const SceDisplayFrameBuf *pFrameBuf, SceDisplaySetBufSync sync);

SceInt32 _sceDisplaySetFrameBuf(const SceDisplayFrameBuf *pParam, SceDisplaySetBufSync sync);

BRIDGE_DECL(_sceDisplayGetFrameBuf)
BRIDGE_DECL(_sceDisplayGetFrameBufInternal)
BRIDGE_DECL(_sceDisplayGetMaximumFrameBufResolution)
BRIDGE_DECL(_sceDisplayGetResolutionInfoInternal)
BRIDGE_DECL(_sceDisplaySetFrameBuf)
BRIDGE_DECL(_sceDisplaySetFrameBufForCompat)
BRIDGE_DECL(_sceDisplaySetFrameBufInternal)
BRIDGE_DECL(sceDisplayGetPrimaryHead)
BRIDGE_DECL(sceDisplayGetRefreshRate)
BRIDGE_DECL(sceDisplayGetVcount)
BRIDGE_DECL(sceDisplayGetVcountInternal)
BRIDGE_DECL(sceDisplayRegisterVblankStartCallback)
BRIDGE_DECL(sceDisplayUnregisterVblankStartCallback)
BRIDGE_DECL(sceDisplayWaitSetFrameBuf)
BRIDGE_DECL(sceDisplayWaitSetFrameBufCB)
BRIDGE_DECL(sceDisplayWaitSetFrameBufMulti)
BRIDGE_DECL(sceDisplayWaitSetFrameBufMultiCB)
BRIDGE_DECL(sceDisplayWaitVblankStart)
BRIDGE_DECL(sceDisplayWaitVblankStartCB)
BRIDGE_DECL(sceDisplayWaitVblankStartMulti)
BRIDGE_DECL(sceDisplayWaitVblankStartMultiCB)
