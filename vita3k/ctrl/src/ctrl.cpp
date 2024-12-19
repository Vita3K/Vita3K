// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <ctrl/ctrl.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>

#include <config/state.h>
#include <dialog/state.h>
#include <display/functions.h>
#include <display/state.h>
#include <kernel/state.h>

#include <SDL_keyboard.h>

static int reserve_port(CtrlState &state) {
    for (int i = 0; i < SCE_CTRL_MAX_WIRELESS_NUM; i++) {
        if (state.free_ports[i]) {
            state.free_ports[i] = false;
            return i + 1;
        }
    }

    // No free port found.
    return 0;
}

SceCtrlExternalInputMode get_type_of_controller(const int idx) {
    const auto type = SDL_GameControllerTypeForIndex(idx);
    return (type == SDL_CONTROLLER_TYPE_PS4) || (type == SDL_CONTROLLER_TYPE_PS5) ? SCE_CTRL_TYPE_DS4 : SCE_CTRL_TYPE_DS3;
}

void refresh_controllers(CtrlState &state, EmuEnvState &emuenv) {
    // Remove disconnected controllers
    bool found_gyro = false;
    bool found_accel = false;
    bool controllers_changed = false;
    for (ControllerList::iterator controller = state.controllers.begin(); controller != state.controllers.end();) {
        if (SDL_GameControllerGetAttached(controller->second.controller.get())) {
            found_accel |= controller->second.has_accel;
            found_gyro |= controller->second.has_gyro;

            ++controller;
        } else {
            state.free_ports[controller->second.port - 1] = true;
            controller = state.controllers.erase(controller);
            state.controllers_num--;
            controllers_changed = true;
        }
    }

    // Add new controllers
    const int num_joysticks = SDL_NumJoysticks();
    for (int joystick_index = 0; joystick_index < num_joysticks; ++joystick_index) {
        if (state.controllers_num >= SCE_CTRL_MAX_WIRELESS_NUM) {
            return;
        }
        if (SDL_IsGameController(joystick_index)) {
            const SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(joystick_index);
            if (!state.controllers.contains(guid)) {
                Controller new_controller;
                const GameControllerPtr controller(SDL_GameControllerOpen(joystick_index), SDL_GameControllerClose);
                new_controller.controller = controller;
                new_controller.port = reserve_port(state);

                new_controller.has_gyro = SDL_GameControllerHasSensor(controller.get(), SDL_SENSOR_GYRO);
                if (new_controller.has_gyro)
                    SDL_GameControllerSetSensorEnabled(controller.get(), SDL_SENSOR_GYRO, SDL_TRUE);
                new_controller.has_accel = SDL_GameControllerHasSensor(controller.get(), SDL_SENSOR_ACCEL);
                if (new_controller.has_accel)
                    SDL_GameControllerSetSensorEnabled(controller.get(), SDL_SENSOR_ACCEL, SDL_TRUE);

                new_controller.has_led = SDL_GameControllerHasLED(controller.get());
                if (new_controller.has_led) {
                    auto &color = emuenv.cfg.controller_led_color;
                    if (!color.empty()) {
                        color.resize(3);
                        SDL_GameControllerSetLED(controller.get(), color[0], color[1], color[2]);
                    }
                }

                found_gyro |= new_controller.has_gyro;
                found_accel |= new_controller.has_accel;

                state.controllers.emplace(guid, new_controller);
                state.controllers_name[joystick_index] = SDL_GameControllerNameForIndex(joystick_index);
                state.controllers_has_motion_support[joystick_index] = found_gyro && found_accel;
                state.controllers_num++;
                controllers_changed = true;
            }
        }
    }

    state.has_motion_support = found_gyro && found_accel;

    if (controllers_changed) {
        auto &io = ImGui::GetIO();
        if (state.controllers_num == 0) {
            io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
        } else {
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        }
    }
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

static void apply_keyboard(uint32_t *buttons, float axes[4], bool ext, EmuEnvState &emuenv) {
    const uint8_t *const keys = SDL_GetKeyboardState(nullptr);
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) {
        return;
    }
    if (ext) {
        if (keys[emuenv.cfg.keyboard_button_l1])
            *buttons |= SCE_CTRL_L1;
        if (keys[emuenv.cfg.keyboard_button_r1])
            *buttons |= SCE_CTRL_R1;
        if (keys[emuenv.cfg.keyboard_button_l2])
            *buttons |= SCE_CTRL_L2;
        if (keys[emuenv.cfg.keyboard_button_r2])
            *buttons |= SCE_CTRL_R2;
        if (keys[emuenv.cfg.keyboard_button_l3])
            *buttons |= SCE_CTRL_L3;
        if (keys[emuenv.cfg.keyboard_button_r3])
            *buttons |= SCE_CTRL_R3;
    } else {
        if (keys[emuenv.cfg.keyboard_button_l1])
            *buttons |= SCE_CTRL_L;
        if (keys[emuenv.cfg.keyboard_button_r1])
            *buttons |= SCE_CTRL_R;
    }
    if (keys[emuenv.cfg.keyboard_button_select])
        *buttons |= SCE_CTRL_SELECT;
    if (keys[emuenv.cfg.keyboard_button_start])
        *buttons |= SCE_CTRL_START;
    if (keys[emuenv.cfg.keyboard_button_up])
        *buttons |= SCE_CTRL_UP;
    if (keys[emuenv.cfg.keyboard_button_right])
        *buttons |= SCE_CTRL_RIGHT;
    if (keys[emuenv.cfg.keyboard_button_down])
        *buttons |= SCE_CTRL_DOWN;
    if (keys[emuenv.cfg.keyboard_button_left])
        *buttons |= SCE_CTRL_LEFT;
    if (keys[emuenv.cfg.keyboard_button_triangle])
        *buttons |= SCE_CTRL_TRIANGLE;
    if (keys[emuenv.cfg.keyboard_button_circle])
        *buttons |= SCE_CTRL_CIRCLE;
    if (keys[emuenv.cfg.keyboard_button_cross])
        *buttons |= SCE_CTRL_CROSS;
    if (keys[emuenv.cfg.keyboard_button_square])
        *buttons |= SCE_CTRL_SQUARE;
    if (keys[emuenv.cfg.keyboard_button_psbutton])
        *buttons |= SCE_CTRL_PSBUTTON;

    axes[0] += keys_to_axis(keys, static_cast<SDL_Scancode>(emuenv.cfg.keyboard_leftstick_left), static_cast<SDL_Scancode>(emuenv.cfg.keyboard_leftstick_right));
    axes[1] += keys_to_axis(keys, static_cast<SDL_Scancode>(emuenv.cfg.keyboard_leftstick_up), static_cast<SDL_Scancode>(emuenv.cfg.keyboard_leftstick_down));
    axes[2] += keys_to_axis(keys, static_cast<SDL_Scancode>(emuenv.cfg.keyboard_rightstick_left), static_cast<SDL_Scancode>(emuenv.cfg.keyboard_rightstick_right));
    axes[3] += keys_to_axis(keys, static_cast<SDL_Scancode>(emuenv.cfg.keyboard_rightstick_up), static_cast<SDL_Scancode>(emuenv.cfg.keyboard_rightstick_down));
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

static uint8_t float_to_byte(float f) {
    const float mapped = (f * 0.5f) + 0.5f;
    const float clamped = std::max<float>(0.0f, std::min<float>(mapped, 1.0f));
    assert(clamped >= 0);
    assert(clamped <= 1);

    return static_cast<uint8_t>(clamped * 255);
}

static std::array<ControllerBinding, 13> get_controller_bindings(EmuEnvState &emuenv) {
    return { {
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_BACK]), SCE_CTRL_SELECT },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_START]), SCE_CTRL_START },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_UP]), SCE_CTRL_UP },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]), SCE_CTRL_RIGHT },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_DOWN]), SCE_CTRL_DOWN },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_LEFT]), SCE_CTRL_LEFT },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]), SCE_CTRL_L },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]), SCE_CTRL_R },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_Y]), SCE_CTRL_TRIANGLE },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_B]), SCE_CTRL_CIRCLE },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_A]), SCE_CTRL_CROSS },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_X]), SCE_CTRL_SQUARE },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_GUIDE]), SCE_CTRL_PSBUTTON },
    } };
}

std::array<ControllerBinding, 15> get_controller_bindings_ext(EmuEnvState &emuenv) {
    return { {
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_BACK]), SCE_CTRL_SELECT },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_LEFTSTICK]), SCE_CTRL_L3 },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_RIGHTSTICK]), SCE_CTRL_R3 },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_START]), SCE_CTRL_START },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_UP]), SCE_CTRL_UP },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]), SCE_CTRL_RIGHT },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_DOWN]), SCE_CTRL_DOWN },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_DPAD_LEFT]), SCE_CTRL_LEFT },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]), SCE_CTRL_L1 },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]), SCE_CTRL_R1 },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_Y]), SCE_CTRL_TRIANGLE },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_B]), SCE_CTRL_CIRCLE },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_A]), SCE_CTRL_CROSS },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_X]), SCE_CTRL_SQUARE },
        { SDL_GameControllerButton(emuenv.cfg.controller_binds[SDL_CONTROLLER_BUTTON_GUIDE]), SCE_CTRL_PSBUTTON },
    } };
}

static void apply_controller(EmuEnvState &emuenv, uint32_t *buttons, float axes[4], SDL_GameController *controller, bool ext) {
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) {
        return;
    }

    if (ext) {
        for (const auto &binding : get_controller_bindings_ext(emuenv)) {
            if (SDL_GameControllerGetButton(controller, binding.controller)) {
                *buttons |= binding.button;
            }
        }

        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0x3FFF) {
            *buttons |= SCE_CTRL_L2;
        }
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0x3FFF) {
            *buttons |= SCE_CTRL_R2;
        }
    } else {
        for (const auto &binding : get_controller_bindings(emuenv)) {
            if (SDL_GameControllerGetButton(controller, binding.controller)) {
                *buttons |= binding.button;
            }
        }
    }

    axes[0] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
    axes[1] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY));
    axes[2] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX));
    axes[3] += axis_to_axis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY));
}

static void retrieve_ctrl_data(EmuEnvState &emuenv, int port, bool is_v2, bool negative, bool from_ext_function, SceUInt32 &buttons, SceUInt8 &lx, SceUInt8 &ly, SceUInt8 &rx, SceUInt8 &ry) {
    std::lock_guard<std::mutex> guard(emuenv.ctrl.mutex);

    if (port == 0) {
        port++;
    }
    CtrlState &state = emuenv.ctrl;
    refresh_controllers(state, emuenv);

    std::array<float, 4> axes;
    axes.fill(0);

    const auto reset_axes = [&]() {
        SceCtrlPadInputMode mode = from_ext_function ? state.input_mode_ext : state.input_mode;
        if (mode == SCE_CTRL_MODE_DIGITAL) {
            lx = 0x80;
            ly = 0x80;
            rx = 0x80;
            ry = 0x80;
        } else {
            lx = float_to_byte(axes[0]);
            ly = float_to_byte(axes[1]);
            rx = float_to_byte(axes[2]);
            ry = float_to_byte(axes[3]);
        }
    };

    if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        if (negative)
            buttons ^= ~0;
        reset_axes();
        return;
    }

    if (port == 1) {
        apply_keyboard(&buttons, axes.data(), is_v2, emuenv);
    }
    for (const auto &[_, controller] : state.controllers) {
        if (controller.port == port) {
            apply_controller(emuenv, &buttons, axes.data(), controller.controller.get(), is_v2);
        }
    }

    reset_axes();

    if (negative)
        buttons ^= ~0;
}

int ctrl_get(const SceUID thread_id, EmuEnvState &emuenv, int port, SceCtrlData2 *pData, SceUInt32 count, bool negative, bool is_peek, bool is_v2, bool from_ext) {
    if (port > 1 && !emuenv.cfg.current_config.pstv_mode) {
        const char *export_name = "sceCtrl*Buffer*";
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }

    memset(pData, 0, sizeof(SceCtrlData2) * count);

    CtrlState &state = emuenv.ctrl;

    int nb_returned_data = 1;
    if (is_peek) {
        nb_returned_data = count;
    } else {
        if (emuenv.display.vblank_count.load() <= state.last_vcount[port]) {
            // sceCtrlRead is blocking, wait for the next vsync for the buffer to be updated
            auto thread = emuenv.kernel.get_thread(thread_id);

            wait_vblank(emuenv.display, emuenv.kernel, thread, state.last_vcount[port] + 1, false);
        }
        uint64_t vblank_count = emuenv.display.vblank_count.load();
        nb_returned_data = std::min<int32_t>(count, vblank_count - state.last_vcount[port]);
        state.last_vcount[port] = vblank_count;
    }

    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
    pData->timeStamp = timestamp;
    retrieve_ctrl_data(emuenv, port, is_v2, negative, from_ext, pData->buttons, pData->lx, pData->ly, pData->rx, pData->ry);

    for (int i = 1; i < nb_returned_data; i++) {
        memcpy(&pData[i], &pData[0], sizeof(SceCtrlData2));

        // update the timestamp, 1 vsync = 1/60 sec = 16 667 us earlier
        pData[i].timeStamp -= i * 16667ULL;
    }

    return nb_returned_data;
}
