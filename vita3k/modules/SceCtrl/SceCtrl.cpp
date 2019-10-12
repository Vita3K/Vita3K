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

#include <config/config.h>
#include <util/log.h>

#include <psp2/common_dialog.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/error.h>

#include <SDL_gamecontroller.h>
#include <SDL_haptic.h>
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

static void apply_keyboard(uint32_t *buttons, float axes[4], bool ext) {
    const uint8_t *const keys = SDL_GetKeyboardState(nullptr);
    const size_t selected_key_binding_count = ext ? key_binding_ext_count : key_binding_count;
    for (int i = 0; i < selected_key_binding_count; ++i) {
        const KeyBinding &binding = ext ? key_bindings_ext[i] : key_bindings[i];
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
    const float unsigned_axis = static_cast<float>(axis - INT16_MIN);
    assert(unsigned_axis >= 0);
    assert(unsigned_axis <= UINT16_MAX);

    const float f = unsigned_axis / UINT16_MAX;

    const float output = (f * 2) - 1;
    assert(output >= -1);
    assert(output <= 1);

    return output;
}

static void apply_controller(uint32_t *buttons, float axes[4], SDL_GameController *controller, bool ext) {
    for (int i = 0; i < controller_binding_count; ++i) {
        const ControllerBinding &binding = ext ? controller_bindings_ext[i] : controller_bindings[i];
        if (SDL_GameControllerGetButton(controller, binding.controller)) {
            *buttons |= binding.button;
        }
    }

    if (ext) {
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0x3FFF) {
            *buttons |= SCE_CTRL_L2;
        }
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0x3FFF) {
            *buttons |= SCE_CTRL_R2;
        }
    }

    axes[0] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
    axes[1] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY));
    axes[2] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX));
    axes[3] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY));
}

static uint8_t float_to_byte(float f) {
    const float mapped = (f * 0.5f) + 0.5f;
    const float clamped = std::max<float>(0.0f, std::min<float>(mapped, 1.0f));
    assert(clamped >= 0);
    assert(clamped <= 1);

    return static_cast<uint8_t>(clamped * 255);
}

static int reserve_port(CtrlState &state) {
    for (int i = 0; i < 4; i++) {
        if (state.free_ports[i]) {
            state.free_ports[i] = false;
            return i + 1;
        }
    }

    // No free port found.
    return 0;
}

static void remove_disconnected_controllers(CtrlState &state) {
    for (ControllerList::iterator controller = state.controllers.begin(); controller != state.controllers.end();) {
        if (SDL_GameControllerGetAttached(controller->second.controller.get())) {
            ++controller;
        } else {
            state.free_ports[controller->second.port - 1] = true;
            controller = state.controllers.erase(controller);
            state.controllers_num--;
        }
    }
}

static void add_new_controllers(CtrlState &state) {
    const int num_joysticks = SDL_NumJoysticks();
    for (int joystick_index = 0; joystick_index < num_joysticks; ++joystick_index) {
        if (state.controllers_num >= 4) {
            return;
        }
        if (SDL_IsGameController(joystick_index)) {
            const SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(joystick_index);
            if (state.controllers.find(guid) == state.controllers.end()) {
                Controller new_controller;
                const GameControllerPtr controller(SDL_GameControllerOpen(joystick_index), SDL_GameControllerClose);
                new_controller.controller = controller;
                SDL_Haptic *haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(controller.get()));
                SDL_HapticRumbleInit(haptic);
                const HapticPtr handle(haptic, SDL_HapticClose);
                new_controller.haptic = handle;
                new_controller.port = reserve_port(state);
                state.controllers.emplace(guid, new_controller);
                state.controllers_num++;
            }
        }
    }
}

static int peek_buffer(HostState &host, int port, SceCtrlData *&pad_data, int count, bool ext, bool negative) {
    if (port == 0) {
        port++;
    }
    CtrlState &state = host.ctrl;
    remove_disconnected_controllers(state);
    add_new_controllers(state);

    memset(pad_data, 0, sizeof(*pad_data));
    pad_data->timeStamp = timestamp++; // TODO Use the real time and units.

    if (host.common_dialog.status != SCE_COMMON_DIALOG_STATUS_NONE) {
        return 0;
    }

    std::array<float, 4> axes;
    axes.fill(0);
    if (port == 1) {
        apply_keyboard(&pad_data->buttons, axes.data(), ext);
    }
    for (const auto &controller : state.controllers) {
        if (controller.second.port == port) {
            apply_controller(&pad_data->buttons, axes.data(), controller.second.controller.get(), ext);
        }
    }

    SceCtrlPadInputMode mode = ext ? state.input_mode_ext : state.input_mode;
    if (mode == SCE_CTRL_MODE_DIGITAL) {
        pad_data->lx = 0x80;
        pad_data->ly = 0x80;
        pad_data->rx = 0x80;
        pad_data->ry = 0x80;
    } else {
        pad_data->lx = float_to_byte(axes[0]);
        pad_data->ly = float_to_byte(axes[1]);
        pad_data->rx = float_to_byte(axes[2]);
        pad_data->ry = float_to_byte(axes[3]);
    }

    if (negative) {
        pad_data->buttons = 0xFFFFFFFF - pad_data->buttons;
    }

    for (int i = 1; i < count; i++) {
        memcpy(&pad_data[i], &pad_data[0], sizeof(SceCtrlData));
    }

    return count;
}

EXPORT(int, sceCtrlClearRapidFire) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlDisconnect) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetAnalogStickCheckMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetAnalogStickCheckTarget) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetBatteryInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetButtonIntercept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetControllerPortInfo, SceCtrlPortInfo *info) {
    CtrlState &state = host.ctrl;
    remove_disconnected_controllers(state);
    add_new_controllers(state);
    info->port[0] = host.cfg->pstv_mode ? SCE_CTRL_TYPE_VIRT : SCE_CTRL_TYPE_PHY;
    for (int i = 0; i < 4; i++) {
        info->port[i + 1] = (host.cfg->pstv_mode && !host.ctrl.free_ports[i]) ? SCE_CTRL_TYPE_DS3 : SCE_CTRL_TYPE_UNPAIRED;
    }
    return 0;
}

EXPORT(int, sceCtrlGetProcessStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetSamplingMode, SceCtrlPadInputMode *mode) {
    if (mode == nullptr) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    *mode = host.ctrl.input_mode;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceCtrlGetSamplingModeExt, SceCtrlPadInputMode *mode) {
    if (mode == nullptr) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    *mode = host.ctrl.input_mode_ext;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceCtrlGetWirelessControllerInfo) {
    return UNIMPLEMENTED();
}

EXPORT(bool, sceCtrlIsMultiControllerSupported) {
    return host.cfg->pstv_mode;
}

EXPORT(int, sceCtrlPeekBufferNegative, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, false, true);
}

EXPORT(int, sceCtrlPeekBufferNegative2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, true, true);
}

EXPORT(int, sceCtrlPeekBufferPositive, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlPeekBufferPositive2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlPeekBufferPositiveExt, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlPeekBufferPositiveExt2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlReadBufferNegative, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, false, true);
}

EXPORT(int, sceCtrlReadBufferNegative2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, true, true);
}

EXPORT(int, sceCtrlReadBufferPositive, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlReadBufferPositive2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlReadBufferPositiveExt, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlReadBufferPositiveExt2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlRegisterBdRMCCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlResetLightBar) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetActuator, int port, const SceCtrlActuator *pState) {
    if (!host.cfg->pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NOT_SUPPORTED);
    }

    CtrlState &state = host.ctrl;
    remove_disconnected_controllers(state);
    add_new_controllers(state);

    for (const auto &controller : state.controllers) {
        if (controller.second.port == port) {
            SDL_Haptic *handle = controller.second.haptic.get();
            if (pState->small == 0 && pState->large == 0) {
                SDL_HapticRumbleStop(handle);
            } else {
                // TODO: Look into a better implementation to distinguish both motors when available
                SDL_HapticRumblePlay(handle, ((pState->small * 1.0f) / 510.0f) + ((pState->large * 1.0f) / 510.0f), SDL_HAPTIC_INFINITY);
            }
            return 0;
        }
    }

    return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
}

EXPORT(int, sceCtrlSetAnalogStickCheckMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetAnalogStickCheckTarget) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetButtonIntercept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetButtonRemappingInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetLightBar) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetRapidFire) {
    return UNIMPLEMENTED();
}

#define SCE_CTRL_MODE_UNKNOWN 3 // missing in vita-headers

EXPORT(int, sceCtrlSetSamplingMode, SceCtrlPadInputMode mode) {
    SceCtrlPadInputMode old = host.ctrl.input_mode;
    if (mode < SCE_CTRL_MODE_DIGITAL || mode > SCE_CTRL_MODE_UNKNOWN) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    host.ctrl.input_mode = mode;
    return old;
}

EXPORT(int, sceCtrlSetSamplingModeExt, SceCtrlPadInputMode mode) {
    SceCtrlPadInputMode old = host.ctrl.input_mode_ext;
    if (mode < SCE_CTRL_MODE_DIGITAL || mode > SCE_CTRL_MODE_UNKNOWN) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    host.ctrl.input_mode_ext = mode;
    return old;
}

EXPORT(int, sceCtrlSingleControllerMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlUnregisterBdRMCCallback) {
    return UNIMPLEMENTED();
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
