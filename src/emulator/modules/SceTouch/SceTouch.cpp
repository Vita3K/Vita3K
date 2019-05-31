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

#include <SDL.h>
#include <SDL_mouse.h>
#include <SDL_video.h>

// TODO Move elsewhere.
static uint64_t timestamp;
static SDL_TouchFingerEvent finger_buffer[8];
static uint8_t finger_count = 0;
static long int finger_id_ref;
static auto touchscreen_port = SCE_TOUCH_PORT_FRONT;

static SceTouchData recover_touch_events() {
    SceTouchData touch_data;
    touch_data.timeStamp = timestamp;

    for (uint8_t i = 0; i < finger_count; i++) {
        touch_data.report[i].id = static_cast<uint8_t>(finger_buffer[i].fingerId - finger_id_ref);
        touch_data.report[i].force = static_cast<uint8_t>(1 + finger_buffer[i].pressure * 127);
        touch_data.report[i].x = static_cast<uint16_t>(finger_buffer[i].x * 1920);

        if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
            touch_data.report[i].y = static_cast<uint16_t>(finger_buffer[i].y * 1088);
        } else {
            touch_data.report[i].y = static_cast<uint16_t>(108 + finger_buffer[i].y * 781);
        }
    }

    touch_data.reportNum = finger_count;

    return touch_data;
}

int handle_touch_event(SDL_TouchFingerEvent &finger) {
    switch (finger.type) {
    case SDL_FINGERDOWN: {
        if (finger_count == 0) {
            finger_id_ref = finger.fingerId;
        }

        finger_buffer[finger_count] = finger;
        finger_count++;

        break;
    }
    case SDL_FINGERUP: {
        int finger_index = -1;
        for (int i = 0; i < finger_count; i++) {
            if (finger.fingerId == finger_buffer[i].fingerId) {
                finger_index = i;
            }
            if (finger_index > -1) {
                finger_buffer[i] = finger_buffer[i + 1];
            }
        }
        finger_count--;
        break;
    }

    case SDL_FINGERMOTION: {
        for (int i = 0; i < finger_count; i++) {
            if (finger.fingerId == finger_buffer[i].fingerId) {
                finger_buffer[i] = finger;
            }
        }
        break;
    }
    }

    return 0;
}

static bool registered_touch() {
    if (finger_count > 0) {
        return true;
    }

    return false;
}

int toggle_touchscreen() {
    if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
        touchscreen_port = SCE_TOUCH_PORT_BACK;
    } else {
        touchscreen_port = SCE_TOUCH_PORT_FRONT;
    }
    return 0;
}

static int peek_touch(HostState &host, SceUInt32 port, SceTouchData *pData, SceUInt32 count) {
    memset(pData, 0, sizeof(*pData));
    pData->timeStamp = timestamp++; // TODO Use the real time and units.

    SceIVector2 touch_pos_window = { 0, 0 };
    const uint32_t buttons = SDL_GetMouseState(&touch_pos_window.x, &touch_pos_window.y);
    const uint32_t mask = (port == SCE_TOUCH_PORT_BACK) ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
    if ((buttons & mask) && host.gui.renderer_focused) {
        SceIVector2 window_size = { 0, 0 };
        SDL_Window *const window = SDL_GetMouseFocus();
        SDL_GetWindowSize(window, &window_size.x, &window_size.y);

        SceFVector2 scale = { 1, 1 };
        if ((window_size.x > 0) && (window_size.y > 0)) {
            scale.x = static_cast<float>(host.drawable_size.x) / window_size.x;
            scale.y = static_cast<float>(host.drawable_size.y) / window_size.y;
        }

        const SceFVector2 touch_pos_drawable = {
            touch_pos_window.x * scale.x,
            touch_pos_window.y * scale.y
        };

        const SceFVector2 touch_pos_viewport = {
            (touch_pos_drawable.x - host.viewport_pos.x) / host.viewport_size.x,
            (touch_pos_drawable.y - host.viewport_pos.y) / host.viewport_size.y
        };

        if ((touch_pos_viewport.x >= 0) && (touch_pos_viewport.y >= 0) && (touch_pos_viewport.x < 1) && (touch_pos_viewport.y < 1)) {
            pData->report[pData->reportNum].x = static_cast<uint16_t>(touch_pos_viewport.x * 1920);
            if (port == SCE_TOUCH_PORT_FRONT) {
                pData->report[pData->reportNum].y = static_cast<uint16_t>(touch_pos_viewport.y * 1088);
            } else {
                pData->report[pData->reportNum].y = static_cast<uint16_t>(108 + touch_pos_viewport.y * 781);
            }
            ++pData->reportNum;
        }

        if (!host.ctrl.touch_mode[port]) {
            pData->reportNum = 0;
        }
    } else if (registered_touch() == true && port == touchscreen_port) {
        pData[0] = recover_touch_events();
    }

    for (uint8_t i = 1; i < count; i++) {
        memcpy(&pData[i], &pData[0], sizeof(SceTouchData));
    }

    return count;
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

EXPORT(int, sceTouchGetPanelInfo, SceUInt32 port, SceTouchPanelInfo *pPanelInfo) {
    switch (port) {
    case SCE_TOUCH_PORT_FRONT:
        // Active Area
        pPanelInfo->minAaX = 0;
        pPanelInfo->minAaY = 0;
        pPanelInfo->maxAaX = 1919;
        pPanelInfo->maxAaY = 1087;

        // Display
        pPanelInfo->minDispX = 0;
        pPanelInfo->minDispY = 0;
        pPanelInfo->maxDispX = 1919;
        pPanelInfo->maxDispY = 1087;

        // Force
        pPanelInfo->minForce = 1;
        pPanelInfo->maxForce = 128;

        return 0;
    case SCE_TOUCH_PORT_BACK:
        // Active Area
        pPanelInfo->minAaX = 0;
        pPanelInfo->minAaY = 108;
        pPanelInfo->maxAaX = 1919;
        pPanelInfo->maxAaY = 889;

        // Display
        pPanelInfo->minDispX = 0;
        pPanelInfo->minDispY = 0;
        pPanelInfo->maxDispX = 1919;
        pPanelInfo->maxDispY = 1087;

        // Force
        pPanelInfo->minForce = 1;
        pPanelInfo->maxForce = 128;

        return 0;
    default:
        return SCE_TOUCH_ERROR_INVALID_ARG;
    }
}

EXPORT(int, sceTouchGetPixelDensity) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetProcessInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetSamplingState, SceUInt32 port, SceTouchSamplingState *pState) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pState == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    *pState = host.ctrl.touch_mode[port];
    return 0;
}

EXPORT(int, sceTouchGetSamplingStateExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeek, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host, port, pData, nBufs);
}

EXPORT(int, sceTouchPeek2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host, port, pData, nBufs);
}

EXPORT(int, sceTouchPeekRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeekRegionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchRead, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host, port, pData, nBufs);
}

EXPORT(int, sceTouchRead2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host, port, pData, nBufs);
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

EXPORT(int, sceTouchSetSamplingState, SceUInt32 port, SceTouchSamplingState state) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (state != SCE_TOUCH_SAMPLING_STATE_STOP && state != SCE_TOUCH_SAMPLING_STATE_START) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    host.ctrl.touch_mode[port] = state;
    return 0;
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
