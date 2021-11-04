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

#include "SceMotion.h"

EXPORT(int, sceMotionGetAngleThreshold) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetBasicOrientation, SceFVector3 *basicOrientation) {
    if (!basicOrientation)
        return RET_ERROR(SCE_MOTION_ERROR_NULL_PARAMETER);

    STUBBED("set default value");
    basicOrientation->x = 0;
    basicOrientation->y = 0;
    basicOrientation->z = 1;

    return 0;
}

EXPORT(int, sceMotionGetDeadband) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetDeadbandExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetDeviceLocation) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetGyroBiasCorrection) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetMagnetometerState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetSensorState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetStateExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetStateInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetTiltCorrection) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetTiltCorrectionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionInitLibraryExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionMagnetometerOff) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionMagnetometerOn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionResetExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionRotateYaw) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetAngleThreshold) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetDeadband) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetDeadbandExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetGyroBiasCorrection) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetTiltCorrection) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetTiltCorrectionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionStartSampling) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionStartSamplingExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionStopSampling) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionStopSamplingExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionTermLibraryExt) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceMotionGetAngleThreshold)
BRIDGE_IMPL(sceMotionGetBasicOrientation)
BRIDGE_IMPL(sceMotionGetDeadband)
BRIDGE_IMPL(sceMotionGetDeadbandExt)
BRIDGE_IMPL(sceMotionGetDeviceLocation)
BRIDGE_IMPL(sceMotionGetGyroBiasCorrection)
BRIDGE_IMPL(sceMotionGetMagnetometerState)
BRIDGE_IMPL(sceMotionGetSensorState)
BRIDGE_IMPL(sceMotionGetState)
BRIDGE_IMPL(sceMotionGetStateExt)
BRIDGE_IMPL(sceMotionGetStateInternal)
BRIDGE_IMPL(sceMotionGetTiltCorrection)
BRIDGE_IMPL(sceMotionGetTiltCorrectionExt)
BRIDGE_IMPL(sceMotionInitLibraryExt)
BRIDGE_IMPL(sceMotionMagnetometerOff)
BRIDGE_IMPL(sceMotionMagnetometerOn)
BRIDGE_IMPL(sceMotionReset)
BRIDGE_IMPL(sceMotionResetExt)
BRIDGE_IMPL(sceMotionRotateYaw)
BRIDGE_IMPL(sceMotionSetAngleThreshold)
BRIDGE_IMPL(sceMotionSetDeadband)
BRIDGE_IMPL(sceMotionSetDeadbandExt)
BRIDGE_IMPL(sceMotionSetGyroBiasCorrection)
BRIDGE_IMPL(sceMotionSetTiltCorrection)
BRIDGE_IMPL(sceMotionSetTiltCorrectionExt)
BRIDGE_IMPL(sceMotionStartSampling)
BRIDGE_IMPL(sceMotionStartSamplingExt)
BRIDGE_IMPL(sceMotionStopSampling)
BRIDGE_IMPL(sceMotionStopSamplingExt)
BRIDGE_IMPL(sceMotionTermLibraryExt)
