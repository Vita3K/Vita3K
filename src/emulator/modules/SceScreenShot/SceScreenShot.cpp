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

#include "SceScreenShot.h"

#include <psp2/screenshot.h>

EXPORT(int, sceScreenShotCapture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotDisable) {
    LOG_INFO("sceScreenShotDisable Stubbed for testing.");
    return 0;
}

EXPORT(int, sceScreenShotDisableNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotEnable) {
    LOG_INFO("sceScreenShotEnable Stubbed for testing.");
    return 0;
}

EXPORT(int, sceScreenShotEnableNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotGetParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotSetOverlayImage, const char *filepath, int offsetX, int offsetY) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotSetParam, const SceScreenShotParam *param) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceScreenShotCapture)
BRIDGE_IMPL(sceScreenShotDisable)
BRIDGE_IMPL(sceScreenShotDisableNotification)
BRIDGE_IMPL(sceScreenShotEnable)
BRIDGE_IMPL(sceScreenShotEnableNotification)
BRIDGE_IMPL(sceScreenShotGetParam)
BRIDGE_IMPL(sceScreenShotSetOverlayImage)
BRIDGE_IMPL(sceScreenShotSetParam)
