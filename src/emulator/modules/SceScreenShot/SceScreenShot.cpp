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

#include <io/functions.h>
#include <io/types.h>
#include <psp2/screenshot.h>
EXPORT(int, sceScreenShotCapture) {
    
    if (host.screenshot.enabled) {
        // get base FB

        // get image size
        // do magic
        write_file();
        return 0;
    } else {
        LOG_INFO("The app has requested to disable Screenshot Access.");
        return 0;
    }
    return 0;
}

EXPORT(int, sceScreenShotDisable) {
    if (host.screenshot.enabled) {
        host.screenshot.enabled = false;
    } else {
        LOG_INFO("Already disabled. App called Disabled Again.");
    }
    return 0;
}

EXPORT(int, sceScreenShotDisableNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotEnable) {
    if (!host.screenshot.enabled) {
        host.screenshot.enabled = true;
    } else {
        LOG_INFO("Already disabled. App called Disabled Again.");
    }
    return 0;
}

EXPORT(int, sceScreenShotEnableNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotGetParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceScreenShotSetOverlayImage, const char *filepath, int offsetX, int offsetY) {
    host.screenshot.overlayfilePath = std::string(filepath);
    host.screenshot.overlayoffsetX = offsetX;
    host.screenshot.overlayoffsetY = offsetY;
    return 0;
}

EXPORT(int, sceScreenShotSetParam, const SceScreenShotParam *param) {
    //TODO: Find a way to set WChar stuff.
    //host.screenshot.gameTitle = std::wstring(param->gameTitle);
    //host.screenshot.photoTitle = param->photoTitle;
    //host.screenshot.gameTitle = param->gameTitle;
    return 0;
}

BRIDGE_IMPL(sceScreenShotCapture)
BRIDGE_IMPL(sceScreenShotDisable)
BRIDGE_IMPL(sceScreenShotDisableNotification)
BRIDGE_IMPL(sceScreenShotEnable)
BRIDGE_IMPL(sceScreenShotEnableNotification)
BRIDGE_IMPL(sceScreenShotGetParam)
BRIDGE_IMPL(sceScreenShotSetOverlayImage)
BRIDGE_IMPL(sceScreenShotSetParam)
