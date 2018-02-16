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

#include "SceMotion.h"

EXPORT(int, sceMotionGetAngleThreshold) {
    return unimplemented("sceMotionGetAngleThreshold");
}

EXPORT(int, sceMotionGetBasicOrientation) {
    return unimplemented("sceMotionGetBasicOrientation");
}

EXPORT(int, sceMotionGetDeadband) {
    return unimplemented("sceMotionGetDeadband");
}

EXPORT(int, sceMotionGetDeviceLocation) {
    return unimplemented("sceMotionGetDeviceLocation");
}

EXPORT(int, sceMotionGetGyroBiasCorrection) {
    return unimplemented("sceMotionGetGyroBiasCorrection");
}

EXPORT(int, sceMotionGetMagnetometerState) {
    return unimplemented("sceMotionGetMagnetometerState");
}

EXPORT(int, sceMotionGetSensorState) {
    return unimplemented("sceMotionGetSensorState");
}

EXPORT(int, sceMotionGetState) {
    return unimplemented("sceMotionGetState");
}

EXPORT(int, sceMotionGetTiltCorrection) {
    return unimplemented("sceMotionGetTiltCorrection");
}

EXPORT(int, sceMotionMagnetometerOff) {
    return unimplemented("sceMotionMagnetometerOff");
}

EXPORT(int, sceMotionMagnetometerOn) {
    return unimplemented("sceMotionMagnetometerOn");
}

EXPORT(int, sceMotionReset) {
    return unimplemented("sceMotionReset");
}

EXPORT(int, sceMotionRotateYaw) {
    return unimplemented("sceMotionRotateYaw");
}

EXPORT(int, sceMotionSetAngleThreshold) {
    return unimplemented("sceMotionSetAngleThreshold");
}

EXPORT(int, sceMotionSetDeadband) {
    return unimplemented("sceMotionSetDeadband");
}

EXPORT(int, sceMotionSetGyroBiasCorrection) {
    return unimplemented("sceMotionSetGyroBiasCorrection");
}

EXPORT(int, sceMotionSetTiltCorrection) {
    return unimplemented("sceMotionSetTiltCorrection");
}

EXPORT(int, sceMotionStartSampling) {
    return unimplemented("sceMotionStartSampling");
}

EXPORT(int, sceMotionStopSampling) {
    return unimplemented("sceMotionStopSampling");
}

BRIDGE_IMPL(sceMotionGetAngleThreshold)
BRIDGE_IMPL(sceMotionGetBasicOrientation)
BRIDGE_IMPL(sceMotionGetDeadband)
BRIDGE_IMPL(sceMotionGetDeviceLocation)
BRIDGE_IMPL(sceMotionGetGyroBiasCorrection)
BRIDGE_IMPL(sceMotionGetMagnetometerState)
BRIDGE_IMPL(sceMotionGetSensorState)
BRIDGE_IMPL(sceMotionGetState)
BRIDGE_IMPL(sceMotionGetTiltCorrection)
BRIDGE_IMPL(sceMotionMagnetometerOff)
BRIDGE_IMPL(sceMotionMagnetometerOn)
BRIDGE_IMPL(sceMotionReset)
BRIDGE_IMPL(sceMotionRotateYaw)
BRIDGE_IMPL(sceMotionSetAngleThreshold)
BRIDGE_IMPL(sceMotionSetDeadband)
BRIDGE_IMPL(sceMotionSetGyroBiasCorrection)
BRIDGE_IMPL(sceMotionSetTiltCorrection)
BRIDGE_IMPL(sceMotionStartSampling)
BRIDGE_IMPL(sceMotionStopSampling)
