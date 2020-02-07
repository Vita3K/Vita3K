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

#include "SceDisplay.h"

#include <host/functions.h>

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

static int display_wait(HostState &host) {
    std::unique_lock<std::mutex> lock(host.display.mutex);
    host.display.condvar.wait(lock);

    if (host.display.abort.load())
        return SCE_DISPLAY_ERROR_NO_PIXEL_DATA;

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, _sceDisplayGetFrameBuf) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceDisplaySetFrameBuf) {
    return UNIMPLEMENTED();
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

EXPORT(int, sceDisplayGetRefreshRate, float *pFps) {
    *pFps = 59.94005f;
    return 0;
}

EXPORT(int, sceDisplayGetVcount) {
    return static_cast<int>(host.frame_count);
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

EXPORT(int, sceDisplayWaitSetFrameBuf) {
    STUBBED("move after setframebuf");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitSetFrameBufCB) {
    STUBBED("move after setframebuf");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitSetFrameBufMulti) {
    STUBBED("move after setframebuf");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitSetFrameBufMultiCB) {
    STUBBED("move after setframebuf");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitVblankStart) {
    STUBBED("wait for vblank");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitVblankStartCB) {
    STUBBED("wait for vblank");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitVblankStartMulti) {
    STUBBED("wait for vblank");

    return display_wait(host);
}

EXPORT(int, sceDisplayWaitVblankStartMultiCB) {
    STUBBED("wait for vblank");

    return display_wait(host);
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
