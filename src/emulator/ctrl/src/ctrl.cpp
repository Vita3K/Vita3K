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

#include <ctrl/definitions.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>

#include <SDL_haptic.h>
#include <SDL_keyboard.h>

#include <algorithm>
#include <array>
#include <cassert>

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
    const auto unsigned_axis = static_cast<float>(axis - INT16_MIN);
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

static int reserve_port(CtrlState &ctrl) {
    for (int i = 0; i < 4; i++) {
        if (ctrl.free_ports[i]) {
            ctrl.free_ports[i] = false;
            return i + 1;
        }
    }

    // No free port found.
    return 0;
}

void remove_disconnected_controllers(CtrlState &ctrl) {
    for (auto controller = ctrl.controllers.begin(); controller != ctrl.controllers.end();) {
        if (SDL_GameControllerGetAttached(controller->second.controller.get())) {
            ++controller;
        } else {
            ctrl.free_ports[controller->second.port - 1] = true;
            controller = ctrl.controllers.erase(controller);
            ctrl.controllers_num--;
        }
    }
}

void add_new_controllers(CtrlState &ctrl) {
    const int num_joysticks = SDL_NumJoysticks();
    for (int joystick_index = 0; joystick_index < num_joysticks; ++joystick_index) {
        if (ctrl.controllers_num >= 4) {
            return;
        }
        if (SDL_IsGameController(joystick_index)) {
            const SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(joystick_index);
            if (ctrl.controllers.find(guid) == ctrl.controllers.end()) {
                Controller new_controller;
                const GameControllerPtr controller(SDL_GameControllerOpen(joystick_index), SDL_GameControllerClose);
                new_controller.controller = controller;
                SDL_Haptic *haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(controller.get()));
                SDL_HapticRumbleInit(haptic);
                const HapticPtr handle(haptic, SDL_HapticClose);
                new_controller.haptic = handle;
                new_controller.port = reserve_port(ctrl);
                ctrl.controllers.emplace(guid, new_controller);
                ctrl.controllers_num++;
            }
        }
    }
}

int peek_buffer(CtrlState &ctrl, int port, SceCtrlData *&pad_data, int count, bool ext, bool negative) {
    if (port == 0) {
        port++;
    }
    remove_disconnected_controllers(ctrl);
    add_new_controllers(ctrl);

    memset(pad_data, 0, sizeof(*pad_data));
    pad_data->timeStamp = timestamp++; // TODO Use the real time and units.

    std::array<float, 4> axes{ 0, 0, 0, 0 };
    if (port == 1) {
        apply_keyboard(&pad_data->buttons, axes.data(), ext);
    }
    for (const auto &controller : ctrl.controllers) {
        if (controller.second.port == port) {
            apply_controller(&pad_data->buttons, axes.data(), controller.second.controller.get(), ext);
        }
    }

    SceCtrlPadInputMode mode = ext ? ctrl.input_mode_ext : ctrl.input_mode;
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
