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

#include <util/types.h>

#define SCE_TOUCH_MAX_REPORT 8
#define MAX_TOUCH_BUFFER_SAVED 64

struct SDL_GamepadTouchpadEvent;
struct SDL_TouchFingerEvent;

enum SceTouchSamplingState {
    SCE_TOUCH_SAMPLING_STATE_STOP,
    SCE_TOUCH_SAMPLING_STATE_START
};

enum SceTouchErrorCode : uint32_t {
    SCE_TOUCH_OK = 0x0,
    SCE_TOUCH_ERROR_INVALID_ARG = 0x80350001,
    SCE_TOUCH_ERROR_PRIV_REQUIRED = 0x80350002,
    SCE_TOUCH_ERROR_FATAL = 0x803500FF
};

enum SceTouchPortType {
    SCE_TOUCH_PORT_FRONT = 0,
    SCE_TOUCH_PORT_BACK = 1,
    SCE_TOUCH_PORT_MAX_NUM = 2
};

struct SceTouchReport {
    SceUInt8 id;
    SceUInt8 force;
    SceInt16 x;
    SceInt16 y;
    SceUInt8 reserved[8];
    SceUInt16 info;
};
static_assert(sizeof(SceTouchReport) == 0x10, "SceTouchReport is an invalid size");

struct SceTouchPanelInfo {
    SceInt16 minAaX;
    SceInt16 minAaY;
    SceInt16 maxAaX;
    SceInt16 maxAaY;
    SceInt16 minDispX;
    SceInt16 minDispY;
    SceInt16 maxDispX;
    SceInt16 maxDispY;
    SceUInt8 minForce;
    SceUInt8 maxForce;
    SceUInt8 reserved[30];
};
static_assert(sizeof(SceTouchPanelInfo) == 0x30, "SceTouchPanelInfo is an invalid size");

struct SceTouchData {
    SceUInt64 timeStamp;
    SceUInt32 status;
    SceUInt32 reportNum;
    SceTouchReport report[SCE_TOUCH_MAX_REPORT];
};
static_assert(sizeof(SceTouchData) == 0x90, "SceTouchData is an invalid size");
