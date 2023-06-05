// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <SDL_gamecontroller.h>
#include <SDL_haptic.h>
#include <SDL_joystick.h>

#include <array>
#include <map>
#include <memory>

struct _SDL_GameController;

typedef std::shared_ptr<_SDL_GameController> GameControllerPtr;
typedef std::shared_ptr<_SDL_Haptic> HapticPtr;

struct Controller {
    GameControllerPtr controller;
    int port;
};

struct ControllerBinding {
    SDL_GameControllerButton controller;
    uint32_t button;
};

constexpr std::array<ControllerBinding, 13> controller_bindings = { {
    { SDL_CONTROLLER_BUTTON_BACK, SCE_CTRL_SELECT },
    { SDL_CONTROLLER_BUTTON_START, SCE_CTRL_START },
    { SDL_CONTROLLER_BUTTON_DPAD_UP, SCE_CTRL_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SCE_CTRL_RIGHT },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, SCE_CTRL_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, SCE_CTRL_LEFT },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SCE_CTRL_LTRIGGER },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SCE_CTRL_RTRIGGER },
    { SDL_CONTROLLER_BUTTON_Y, SCE_CTRL_TRIANGLE },
    { SDL_CONTROLLER_BUTTON_B, SCE_CTRL_CIRCLE },
    { SDL_CONTROLLER_BUTTON_A, SCE_CTRL_CROSS },
    { SDL_CONTROLLER_BUTTON_X, SCE_CTRL_SQUARE },
    { SDL_CONTROLLER_BUTTON_GUIDE, SCE_CTRL_PSBUTTON },
} };

constexpr std::array<ControllerBinding, 15> controller_bindings_ext = { {
    { SDL_CONTROLLER_BUTTON_BACK, SCE_CTRL_SELECT },
    { SDL_CONTROLLER_BUTTON_START, SCE_CTRL_START },
    { SDL_CONTROLLER_BUTTON_DPAD_UP, SCE_CTRL_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SCE_CTRL_RIGHT },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, SCE_CTRL_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, SCE_CTRL_LEFT },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SCE_CTRL_L1 },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SCE_CTRL_R1 },
    { SDL_CONTROLLER_BUTTON_LEFTSTICK, SCE_CTRL_L3 },
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK, SCE_CTRL_R3 },
    { SDL_CONTROLLER_BUTTON_Y, SCE_CTRL_TRIANGLE },
    { SDL_CONTROLLER_BUTTON_B, SCE_CTRL_CIRCLE },
    { SDL_CONTROLLER_BUTTON_A, SCE_CTRL_CROSS },
    { SDL_CONTROLLER_BUTTON_X, SCE_CTRL_SQUARE },
    { SDL_CONTROLLER_BUTTON_GUIDE, SCE_CTRL_PSBUTTON },
} };

typedef std::map<SDL_JoystickGUID, Controller> ControllerList;

struct CtrlState {
    ControllerList controllers;
    int controllers_num = 0;
    const char *controllers_name[SCE_CTRL_MAX_WIRELESS_NUM];
    bool free_ports[SCE_CTRL_MAX_WIRELESS_NUM] = { true, true, true, true };
    SceCtrlPadInputMode input_mode = SCE_CTRL_MODE_DIGITAL;
    SceCtrlPadInputMode input_mode_ext = SCE_CTRL_MODE_DIGITAL;

    // last vsync the data was read
    uint64_t last_vcount[5] = {};
};
