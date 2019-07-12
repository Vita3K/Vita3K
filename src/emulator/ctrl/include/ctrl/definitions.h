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

#pragma once

#include <psp2/ctrl.h>

#include <SDL_gamecontroller.h>
#include <SDL_scancode.h>

#include <cstring>

static uint64_t timestamp;

struct KeyBinding {
    SDL_Scancode scancode;
    uint32_t button;
};

struct ControllerBinding {
    SDL_GameControllerButton controller;
    uint32_t button;
};

static const KeyBinding key_bindings[] = {
    { SDL_SCANCODE_RSHIFT, SCE_CTRL_SELECT },
    { SDL_SCANCODE_RETURN, SCE_CTRL_START },
    { SDL_SCANCODE_UP, SCE_CTRL_UP },
    { SDL_SCANCODE_RIGHT, SCE_CTRL_RIGHT },
    { SDL_SCANCODE_DOWN, SCE_CTRL_DOWN },
    { SDL_SCANCODE_LEFT, SCE_CTRL_LEFT },
    { SDL_SCANCODE_Q, SCE_CTRL_LTRIGGER },
    { SDL_SCANCODE_E, SCE_CTRL_RTRIGGER },
    { SDL_SCANCODE_V, SCE_CTRL_TRIANGLE },
    { SDL_SCANCODE_C, SCE_CTRL_CIRCLE },
    { SDL_SCANCODE_X, SCE_CTRL_CROSS },
    { SDL_SCANCODE_Z, SCE_CTRL_SQUARE },
    { SDL_SCANCODE_H, SCE_CTRL_PSBUTTON },
};

static const KeyBinding key_bindings_ext[] = {
    { SDL_SCANCODE_RSHIFT, SCE_CTRL_SELECT },
    { SDL_SCANCODE_RETURN, SCE_CTRL_START },
    { SDL_SCANCODE_UP, SCE_CTRL_UP },
    { SDL_SCANCODE_RIGHT, SCE_CTRL_RIGHT },
    { SDL_SCANCODE_DOWN, SCE_CTRL_DOWN },
    { SDL_SCANCODE_LEFT, SCE_CTRL_LEFT },
    { SDL_SCANCODE_Q, SCE_CTRL_L1 },
    { SDL_SCANCODE_E, SCE_CTRL_R1 },
    { SDL_SCANCODE_U, SCE_CTRL_L2 },
    { SDL_SCANCODE_O, SCE_CTRL_R2 },
    { SDL_SCANCODE_F, SCE_CTRL_L3 },
    { SDL_SCANCODE_H, SCE_CTRL_R3 },
    { SDL_SCANCODE_V, SCE_CTRL_TRIANGLE },
    { SDL_SCANCODE_C, SCE_CTRL_CIRCLE },
    { SDL_SCANCODE_X, SCE_CTRL_CROSS },
    { SDL_SCANCODE_Z, SCE_CTRL_SQUARE },
    { SDL_SCANCODE_H, SCE_CTRL_PSBUTTON },
};

static const size_t key_binding_count = sizeof(key_bindings) / sizeof(key_bindings[0]);
static const size_t key_binding_ext_count = sizeof(key_bindings_ext) / sizeof(key_bindings_ext[0]);

static const ControllerBinding controller_bindings[] = {
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
};

static const ControllerBinding controller_bindings_ext[] = {
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
};

static const size_t controller_binding_count = sizeof(controller_bindings) / sizeof(controller_bindings[0]);

static bool operator<(const SDL_JoystickGUID &a, const SDL_JoystickGUID &b) {
    return memcmp(&a, &b, sizeof(a)) < 0;
}
