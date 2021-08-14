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

enum SceMotionErrorCode {
    SCE_MOTION_ERROR_DATA_INVALID = 0x80360200,
    SCE_MOTION_ERROR_READING = 0x80360201,
    SCE_MOTION_ERROR_NON_INIT_ERR = 0x80360202,
    SCE_MOTION_ERROR_STATE_INVALID = 0x80360203,
    SCE_MOTION_ERROR_CALIB_READ_FAIL = 0x80360204,
    SCE_MOTION_ERROR_OUT_OF_BOUNDS = 0x80360205,
    SCE_MOTION_ERROR_NOT_SAMPLING = 0x80360206,
    SCE_MOTION_ERROR_ALREADY_SAMPLING = 0x80360207,
    SCE_MOTION_ERROR_MEM_IN_USE = 0x80360208,
    SCE_MOTION_ERROR_NULL_PARAMETER = 0x80360208,
    SCE_MOTION_ERROR_ANGLE_OUT_OF_RANGE = 0x8036020A
};

BRIDGE_DECL(sceMotionGetAngleThreshold)
BRIDGE_DECL(sceMotionGetBasicOrientation)
BRIDGE_DECL(sceMotionGetDeadband)
BRIDGE_DECL(sceMotionGetDeviceLocation)
BRIDGE_DECL(sceMotionGetGyroBiasCorrection)
BRIDGE_DECL(sceMotionGetMagnetometerState)
BRIDGE_DECL(sceMotionGetSensorState)
BRIDGE_DECL(sceMotionGetState)
BRIDGE_DECL(sceMotionGetTiltCorrection)
BRIDGE_DECL(sceMotionMagnetometerOff)
BRIDGE_DECL(sceMotionMagnetometerOn)
BRIDGE_DECL(sceMotionReset)
BRIDGE_DECL(sceMotionRotateYaw)
BRIDGE_DECL(sceMotionSetAngleThreshold)
BRIDGE_DECL(sceMotionSetDeadband)
BRIDGE_DECL(sceMotionSetGyroBiasCorrection)
BRIDGE_DECL(sceMotionSetTiltCorrection)
BRIDGE_DECL(sceMotionStartSampling)
BRIDGE_DECL(sceMotionStopSampling)
