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

#include <ctrl/ctrl.h>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_haptic.h>

#include <cstring>
#include <map>
#include <memory>
#include <mutex>

typedef std::shared_ptr<SDL_Gamepad> GamepadPtr;
typedef std::shared_ptr<SDL_Haptic> HapticPtr;

struct Controller {
    GamepadPtr controller;
    int port; // SDL_Gamepad index
    bool has_accel;
    bool has_gyro;
    bool has_led;
    const char *name;
};

struct ControllerBinding {
    SDL_GamepadButton controller;
    uint32_t button;
};

struct SDL_GUIDComparator {
    bool operator()(const SDL_GUID &a, const SDL_GUID &b) const {
        return memcmp(&a, &b, sizeof(a)) < 0;
    }
};

typedef std::map<SDL_GUID, Controller, SDL_GUIDComparator> ControllerList;

struct CtrlState {
    std::mutex mutex;
    ControllerList controllers;
    int controllers_num = 0;
    bool has_motion_support = false;
    bool free_ports[SCE_CTRL_MAX_WIRELESS_NUM] = { true, true, true, true };
    SceCtrlPadInputMode input_mode = SCE_CTRL_MODE_DIGITAL;
    SceCtrlPadInputMode input_mode_ext = SCE_CTRL_MODE_DIGITAL;

    // last vsync the data was read
    uint64_t last_vcount[5] = {}; // sceCtrl ports.
};
