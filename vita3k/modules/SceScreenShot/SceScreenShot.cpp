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

EXPORT(int, sceScreenShotCapture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotDisable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotDisableNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotEnableNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotGetParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotSetOverlayImage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotSetParam) {
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
