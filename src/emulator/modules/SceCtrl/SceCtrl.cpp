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

#include "SceCtrl.h"

#include <psp2/ctrl.h>

#include <SDL_gamecontroller.h>
#include <SDL_keyboard.h>

#include <algorithm>
#include <array>

// TODO Move elsewhere.
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

static const size_t key_binding_count = sizeof(key_bindings) / sizeof(key_bindings[0]);

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

static const size_t controller_binding_count = sizeof(controller_bindings) / sizeof(controller_bindings[0]);

static bool operator<(const SDL_JoystickGUID &a, const SDL_JoystickGUID &b) {
    return memcmp(&a, &b, sizeof(a)) < 0;
}

static float keys_to_axis(const uint8_t *keys, SDL_Scancode code1, SDL_Scancode code2) {
    float temp = 0;
    if (keys[code1]) {
        temp -= 1;
    }
    if (keys[code2]) {
        temp += 1;
    }

    return temp;
}

static void apply_keyboard(uint32_t *buttons, float axes[4]) {
    const uint8_t *const keys = SDL_GetKeyboardState(nullptr);
    for (int i = 0; i < key_binding_count; ++i) {
        const KeyBinding &binding = key_bindings[i];
        if (keys[binding.scancode]) {
            *buttons |= binding.button;
        }
    }

    axes[0] += keys_to_axis(keys, SDL_SCANCODE_A, SDL_SCANCODE_D);
    axes[1] += keys_to_axis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
    axes[2] += keys_to_axis(keys, SDL_SCANCODE_J, SDL_SCANCODE_L);
    axes[3] += keys_to_axis(keys, SDL_SCANCODE_I, SDL_SCANCODE_K);
}

static float axis_to_axis(int16_t axis) {
    const float unsigned_axis = axis - INT16_MIN;
    assert(unsigned_axis >= 0);
    assert(unsigned_axis <= UINT16_MAX);

    const float f = unsigned_axis / UINT16_MAX;

    const float output = (f * 2) - 1;
    assert(output >= -1);
    assert(output <= 1);

    return output;
}

static void apply_controller(uint32_t *buttons, float axes[4], SDL_GameController *controller) {
    for (int i = 0; i < controller_binding_count; ++i) {
        const ControllerBinding &binding = controller_bindings[i];
        if (SDL_GameControllerGetButton(controller, binding.controller)) {
            *buttons |= binding.button;
        }
    }

    axes[0] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
    axes[1] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY));
    axes[2] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX));
    axes[3] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY));
}

static uint8_t float_to_byte(float f) {
    const float mapped = (f * 0.5f) + 0.5f;
    const float clamped = std::max(0.0f, std::min(mapped, 1.0f));
    assert(clamped >= 0);
    assert(clamped <= 1);

    return static_cast<uint8_t>(clamped * 255);
}

static void remove_disconnected_controllers(CtrlState &state) {
    for (GameControllerList::iterator controller = state.controllers.begin(); controller != state.controllers.end();) {
        if (SDL_GameControllerGetAttached(controller->second.get())) {
            ++controller;
        } else {
            controller = state.controllers.erase(controller);
        }
    }
}

static void add_new_controllers(CtrlState &state) {
    const int num_joysticks = SDL_NumJoysticks();
    for (int joystick_index = 0; joystick_index < num_joysticks; ++joystick_index) {
        if (SDL_IsGameController(joystick_index)) {
            const SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(joystick_index);
            if (state.controllers.find(guid) == state.controllers.end()) {
                const GameControllerPtr controller(SDL_GameControllerOpen(joystick_index), SDL_GameControllerClose);
                state.controllers.insert(GameControllerList::value_type(guid, controller));
            }
        }
    }
}

static int peek_buffer_positive(HostState &host, SceCtrlData *&pad_data) {
    CtrlState &state = host.ctrl;
    remove_disconnected_controllers(state);
    add_new_controllers(state);
    
    memset(pad_data, 0, sizeof(*pad_data));
    pad_data->timeStamp = timestamp++; // TODO Use the real time and units.
    
    std::array<float, 4> axes;
    axes.fill(0);
    apply_keyboard(&pad_data->buttons, axes.data());
    for (const GameControllerList::value_type &controller : state.controllers) {
        apply_controller(&pad_data->buttons, axes.data(), controller.second.get());
    }
    
    pad_data->lx = float_to_byte(axes[0]);
    pad_data->ly = float_to_byte(axes[1]);
    pad_data->rx = float_to_byte(axes[2]);
    pad_data->ry = float_to_byte(axes[3]);
    
    return 0;
}

EXPORT(int, sceCtrlClearRapidFire) {
    return unimplemented("sceCtrlClearRapidFire");
}

EXPORT(int, sceCtrlDisconnect) {
    return unimplemented("sceCtrlDisconnect");
}

EXPORT(int, sceCtrlGetAnalogStickCheckMode) {
    return unimplemented("sceCtrlGetAnalogStickCheckMode");
}

EXPORT(int, sceCtrlGetAnalogStickCheckTarget) {
    return unimplemented("sceCtrlGetAnalogStickCheckTarget");
}

EXPORT(int, sceCtrlGetBatteryInfo) {
    return unimplemented("sceCtrlGetBatteryInfo");
}

EXPORT(int, sceCtrlGetButtonIntercept) {
    return unimplemented("sceCtrlGetButtonIntercept");
}

EXPORT(int, sceCtrlGetControllerPortInfo) {
    return unimplemented("sceCtrlGetControllerPortInfo");
}

EXPORT(int, sceCtrlGetProcessStatus) {
    return unimplemented("sceCtrlGetProcessStatus");
}

EXPORT(int, sceCtrlGetSamplingMode) {
    return unimplemented("sceCtrlGetSamplingMode");
}

EXPORT(int, sceCtrlGetSamplingModeExt) {
    return unimplemented("sceCtrlGetSamplingModeExt");
}

EXPORT(int, sceCtrlGetWirelessControllerInfo) {
    return unimplemented("sceCtrlGetWirelessControllerInfo");
}

EXPORT(int, sceCtrlIsMultiControllerSupported) {
    return unimplemented("sceCtrlIsMultiControllerSupported");
}

EXPORT(int, sceCtrlPeekBufferNegative) {
    return unimplemented("sceCtrlPeekBufferNegative");
}

EXPORT(int, sceCtrlPeekBufferNegative2) {
    return unimplemented("sceCtrlPeekBufferNegative2");
}

EXPORT(int, sceCtrlPeekBufferPositive, int port, SceCtrlData *pad_data, int count) {
    assert(pad_data != nullptr);
    assert(count == 1);

    return peek_buffer_positive(host, pad_data);
}

EXPORT(int, sceCtrlPeekBufferPositive2) {
    return unimplemented("sceCtrlPeekBufferPositive2");
}

EXPORT(int, sceCtrlPeekBufferPositiveExt) {
    return unimplemented("sceCtrlPeekBufferPositiveExt");
}

EXPORT(int, sceCtrlPeekBufferPositiveExt2) {
    return unimplemented("sceCtrlPeekBufferPositiveExt2");
}

EXPORT(int, sceCtrlReadBufferNegative) {
    return unimplemented("sceCtrlReadBufferNegative");
}

EXPORT(int, sceCtrlReadBufferNegative2) {
    return unimplemented("sceCtrlReadBufferNegative2");
}

EXPORT(int, sceCtrlReadBufferPositive, int port, SceCtrlData *pad_data, int count) {
    return peek_buffer_positive(host, pad_data);
}

EXPORT(int, sceCtrlReadBufferPositive2) {
    return unimplemented("sceCtrlReadBufferPositive2");
}

EXPORT(int, sceCtrlReadBufferPositiveExt) {
    return unimplemented("sceCtrlReadBufferPositiveExt");
}

EXPORT(int, sceCtrlReadBufferPositiveExt2) {
    return unimplemented("sceCtrlReadBufferPositiveExt2");
}

EXPORT(int, sceCtrlRegisterBdRMCCallback) {
    return unimplemented("sceCtrlRegisterBdRMCCallback");
}

EXPORT(int, sceCtrlResetLightBar) {
    return unimplemented("sceCtrlResetLightBar");
}

EXPORT(int, sceCtrlSetActuator) {
    return unimplemented("sceCtrlSetActuator");
}

EXPORT(int, sceCtrlSetAnalogStickCheckMode) {
    return unimplemented("sceCtrlSetAnalogStickCheckMode");
}

EXPORT(int, sceCtrlSetAnalogStickCheckTarget) {
    return unimplemented("sceCtrlSetAnalogStickCheckTarget");
}

EXPORT(int, sceCtrlSetButtonIntercept) {
    return unimplemented("sceCtrlSetButtonIntercept");
}

EXPORT(int, sceCtrlSetButtonRemappingInfo) {
    return unimplemented("sceCtrlSetButtonRemappingInfo");
}

EXPORT(int, sceCtrlSetLightBar) {
    return unimplemented("sceCtrlSetLightBar");
}

EXPORT(int, sceCtrlSetRapidFire) {
    return unimplemented("sceCtrlSetRapidFire");
}

EXPORT(int, sceCtrlSetSamplingMode) {
    return unimplemented("sceCtrlSetSamplingMode");
}

EXPORT(int, sceCtrlSetSamplingModeExt) {
    return unimplemented("sceCtrlSetSamplingModeExt");
}

EXPORT(int, sceCtrlSingleControllerMode) {
    return unimplemented("sceCtrlSingleControllerMode");
}

EXPORT(int, sceCtrlUnregisterBdRMCCallback) {
    return unimplemented("sceCtrlUnregisterBdRMCCallback");
}

BRIDGE_IMPL(sceCtrlClearRapidFire)
BRIDGE_IMPL(sceCtrlDisconnect)
BRIDGE_IMPL(sceCtrlGetAnalogStickCheckMode)
BRIDGE_IMPL(sceCtrlGetAnalogStickCheckTarget)
BRIDGE_IMPL(sceCtrlGetBatteryInfo)
BRIDGE_IMPL(sceCtrlGetButtonIntercept)
BRIDGE_IMPL(sceCtrlGetControllerPortInfo)
BRIDGE_IMPL(sceCtrlGetProcessStatus)
BRIDGE_IMPL(sceCtrlGetSamplingMode)
BRIDGE_IMPL(sceCtrlGetSamplingModeExt)
BRIDGE_IMPL(sceCtrlGetWirelessControllerInfo)
BRIDGE_IMPL(sceCtrlIsMultiControllerSupported)
BRIDGE_IMPL(sceCtrlPeekBufferNegative)
BRIDGE_IMPL(sceCtrlPeekBufferNegative2)
BRIDGE_IMPL(sceCtrlPeekBufferPositive)
BRIDGE_IMPL(sceCtrlPeekBufferPositive2)
BRIDGE_IMPL(sceCtrlPeekBufferPositiveExt)
BRIDGE_IMPL(sceCtrlPeekBufferPositiveExt2)
BRIDGE_IMPL(sceCtrlReadBufferNegative)
BRIDGE_IMPL(sceCtrlReadBufferNegative2)
BRIDGE_IMPL(sceCtrlReadBufferPositive)
BRIDGE_IMPL(sceCtrlReadBufferPositive2)
BRIDGE_IMPL(sceCtrlReadBufferPositiveExt)
BRIDGE_IMPL(sceCtrlReadBufferPositiveExt2)
BRIDGE_IMPL(sceCtrlRegisterBdRMCCallback)
BRIDGE_IMPL(sceCtrlResetLightBar)
BRIDGE_IMPL(sceCtrlSetActuator)
BRIDGE_IMPL(sceCtrlSetAnalogStickCheckMode)
BRIDGE_IMPL(sceCtrlSetAnalogStickCheckTarget)
BRIDGE_IMPL(sceCtrlSetButtonIntercept)
BRIDGE_IMPL(sceCtrlSetButtonRemappingInfo)
BRIDGE_IMPL(sceCtrlSetLightBar)
BRIDGE_IMPL(sceCtrlSetRapidFire)
BRIDGE_IMPL(sceCtrlSetSamplingMode)
BRIDGE_IMPL(sceCtrlSetSamplingModeExt)
BRIDGE_IMPL(sceCtrlSingleControllerMode)
BRIDGE_IMPL(sceCtrlUnregisterBdRMCCallback)
