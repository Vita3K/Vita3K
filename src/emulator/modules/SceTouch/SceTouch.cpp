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

#include "SceTouch.h"

#include <psp2/touch.h>

#include <SDL_mouse.h>
#include <SDL_video.h>

// TODO Move elsewhere.
static uint64_t timestamp;

static int peek_touch(HostState &host, SceUInt32 port, SceTouchData *pData) {
    memset(pData, 0, sizeof(*pData));
    pData->timeStamp = timestamp++; // TODO Use the real time and units.

    int window_x = 0;
    int window_y = 0;
    const uint32_t buttons = SDL_GetMouseState(&window_x, &window_y);

    int window_w = 0;
    int window_h = 0;
    SDL_Window *const window = SDL_GetMouseFocus();
    SDL_GetWindowSize(window, &window_w, &window_h);

    const float normalised_x = window_x / static_cast<float>(window_w);
    const float normalised_y = window_y / static_cast<float>(window_h);

    const uint32_t mask = (port == 1) ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
    if ((buttons & mask) && (host.gui.renderer_focused)) {
        pData->report[pData->reportNum].x = static_cast<uint16_t>(normalised_x * 1920);
        pData->report[pData->reportNum].y = static_cast<uint16_t>(normalised_y * 1088);
        ++pData->reportNum;
    }

    return 0;
}

EXPORT(int, sceTouchActivateRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchClearRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchDisableTouchForce) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchDisableTouchForceExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableIdleTimerCancelSetting) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableTouchForce) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableTouchForceExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetDeviceInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetPanelInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetPixelDensity) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetProcessInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetSamplingState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetSamplingStateExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeek, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    assert(port >= 0);
    assert(port <= 1);
    assert(pData != nullptr);
    assert(nBufs == 1);

    return peek_touch(host, port, pData);
}

EXPORT(int, sceTouchPeek2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    assert(port >= 0);
    assert(port <= 1);
    assert(pData != nullptr);
    assert(nBufs == 1);

    return peek_touch(host, port, pData);
}

EXPORT(int, sceTouchPeekRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeekRegionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchRead, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    assert(port >= 0);
    assert(port <= 1);
    assert(pData != nullptr);
    assert(nBufs == 1);

    return peek_touch(host, port, pData);
}

EXPORT(int, sceTouchRead2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    assert(port >= 0);
    assert(port <= 1);
    assert(pData != nullptr);
    assert(nBufs == 1);

    return peek_touch(host, port, pData);
}

EXPORT(int, sceTouchReadRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchReadRegionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetProcessPrivilege) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetRegionAttr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetSamplingState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetSamplingStateExt) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceTouchActivateRegion)
BRIDGE_IMPL(sceTouchClearRegion)
BRIDGE_IMPL(sceTouchDisableTouchForce)
BRIDGE_IMPL(sceTouchDisableTouchForceExt)
BRIDGE_IMPL(sceTouchEnableIdleTimerCancelSetting)
BRIDGE_IMPL(sceTouchEnableTouchForce)
BRIDGE_IMPL(sceTouchEnableTouchForceExt)
BRIDGE_IMPL(sceTouchGetDeviceInfo)
BRIDGE_IMPL(sceTouchGetPanelInfo)
BRIDGE_IMPL(sceTouchGetPixelDensity)
BRIDGE_IMPL(sceTouchGetProcessInfo)
BRIDGE_IMPL(sceTouchGetSamplingState)
BRIDGE_IMPL(sceTouchGetSamplingStateExt)
BRIDGE_IMPL(sceTouchPeek)
BRIDGE_IMPL(sceTouchPeek2)
BRIDGE_IMPL(sceTouchPeekRegion)
BRIDGE_IMPL(sceTouchPeekRegionExt)
BRIDGE_IMPL(sceTouchRead)
BRIDGE_IMPL(sceTouchRead2)
BRIDGE_IMPL(sceTouchReadRegion)
BRIDGE_IMPL(sceTouchReadRegionExt)
BRIDGE_IMPL(sceTouchSetProcessPrivilege)
BRIDGE_IMPL(sceTouchSetRegion)
BRIDGE_IMPL(sceTouchSetRegionAttr)
BRIDGE_IMPL(sceTouchSetSamplingState)
BRIDGE_IMPL(sceTouchSetSamplingStateExt)
