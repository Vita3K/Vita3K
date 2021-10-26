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

#include <ctrl/ctrl.h>

#include <SDL_gamecontroller.h>
#include <SDL_haptic.h>
#include <SDL_joystick.h>

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

typedef std::map<SDL_JoystickGUID, Controller> ControllerList;

struct CtrlState {
    ControllerList controllers;
    int controllers_num = 0;
    const char *controllers_name[SCE_CTRL_MAX_WIRELESS_NUM];
    bool free_ports[SCE_CTRL_MAX_WIRELESS_NUM] = { true, true, true, true };
    SceCtrlPadInputMode input_mode = SCE_CTRL_MODE_DIGITAL;
    SceCtrlPadInputMode input_mode_ext = SCE_CTRL_MODE_DIGITAL;
};
