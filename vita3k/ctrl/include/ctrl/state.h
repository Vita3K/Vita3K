// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <algorithm>
#include <atomic>
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

struct VirtualKeyboardState {
    uint32_t buttons = 0;
    uint32_t buttons_ext = 0;
    float axes[4] = {};
};

struct CtrlState {
    std::mutex mutex;
    ControllerList controllers;
    int controllers_num = 0;
    float analog_multiplier = 1.0f;
    bool has_motion_support = false;
    bool free_ports[SCE_CTRL_MAX_WIRELESS_NUM] = { true, true, true, true };
    SceCtrlPadInputMode input_mode = SCE_CTRL_MODE_DIGITAL;
    SceCtrlPadInputMode input_mode_ext = SCE_CTRL_MODE_DIGITAL;

    // last vsync the data was read
    uint64_t last_vcount[5] = {}; // sceCtrl ports.

    VirtualKeyboardState keyboard_state;

    std::atomic<bool> overlay_input_intercepted{ false };
    bool ignore_input = false;

    struct OverlayMouseState {
        std::atomic<float> x{ 0.f };
        std::atomic<float> y{ 0.f };
        std::atomic<bool> pressed{ false };
    };
    OverlayMouseState overlay_mouse;

    void reset_runtime() {
        controllers.clear();
        controllers_num = 0;
        has_motion_support = false;
        std::fill_n(free_ports, SCE_CTRL_MAX_WIRELESS_NUM, true);
        input_mode = SCE_CTRL_MODE_DIGITAL;
        input_mode_ext = SCE_CTRL_MODE_DIGITAL;
        std::fill_n(last_vcount, 5, 0);
        keyboard_state = {};
        overlay_input_intercepted.store(false, std::memory_order_relaxed);
        ignore_input = false;
        overlay_mouse.x.store(0.f, std::memory_order_relaxed);
        overlay_mouse.y.store(0.f, std::memory_order_relaxed);
        overlay_mouse.pressed.store(false, std::memory_order_relaxed);
    }
};
