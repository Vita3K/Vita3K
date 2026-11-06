// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl/sdl_event.h>

#include <archive.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <ime/functions.h>
#include <motion/event_handler.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <renderer/state.h>
#include <rif2zrif.h>
#include <touch/functions.h>
#include <touch/state.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL3/SDL.h>

#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

namespace sdl {

namespace {

std::set<input::PhysicalKeyCode> pressed_keys;
std::vector<fs::path> pending_drop_files;

input::PhysicalKeyCode physical_key_from_scancode(SDL_Scancode scancode) {
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT)
        return input::PhysicalKeyCode::Unbound;

    return static_cast<input::PhysicalKeyCode>(0x00070000u | static_cast<uint32_t>(scancode));
}

uint32_t button_from_physical_key(const EmuEnvState &emuenv, input::PhysicalKeyCode key) {
    const auto &cfg = emuenv.cfg;
    if (key == cfg.keyboard_button_up || key == cfg.keyboard_button_up_alt)
        return SCE_CTRL_UP;
    if (key == cfg.keyboard_button_right || key == cfg.keyboard_button_right_alt)
        return SCE_CTRL_RIGHT;
    if (key == cfg.keyboard_button_down || key == cfg.keyboard_button_down_alt)
        return SCE_CTRL_DOWN;
    if (key == cfg.keyboard_button_left || key == cfg.keyboard_button_left_alt)
        return SCE_CTRL_LEFT;
    if (key == cfg.keyboard_button_l1 || key == cfg.keyboard_button_l1_alt)
        return SCE_CTRL_L1;
    if (key == cfg.keyboard_button_r1 || key == cfg.keyboard_button_r1_alt)
        return SCE_CTRL_R1;
    if (key == cfg.keyboard_button_triangle || key == cfg.keyboard_button_triangle_alt)
        return SCE_CTRL_TRIANGLE;
    if (key == cfg.keyboard_button_square || key == cfg.keyboard_button_square_alt)
        return SCE_CTRL_SQUARE;
    if (key == cfg.keyboard_button_circle || key == cfg.keyboard_button_circle_alt)
        return SCE_CTRL_CIRCLE;
    if (key == cfg.keyboard_button_cross || key == cfg.keyboard_button_cross_alt)
        return SCE_CTRL_CROSS;
    if (key == cfg.keyboard_button_psbutton || key == cfg.keyboard_button_psbutton_alt)
        return SCE_CTRL_PSBUTTON;
    return 0;
}

void update_keyboard_state(EmuEnvState &emuenv, input::PhysicalKeyCode key, bool pressed) {
    if (key == input::PhysicalKeyCode::Unbound)
        return;

    if (pressed)
        pressed_keys.insert(key);
    else
        pressed_keys.erase(key);

    const auto &cfg = emuenv.cfg;
    const auto is_pressed = [](input::PhysicalKeyCode primary, input::PhysicalKeyCode alternate) {
        return (primary != input::PhysicalKeyCode::Unbound && pressed_keys.contains(primary))
            || (alternate != input::PhysicalKeyCode::Unbound && pressed_keys.contains(alternate));
    };
    const auto set_bit_if_pressed = [&is_pressed](input::PhysicalKeyCode primary, input::PhysicalKeyCode alternate,
                                        uint32_t bit, uint32_t &mask) {
        if (is_pressed(primary, alternate))
            mask |= bit;
    };

    float axes[4] = {
        static_cast<float>(is_pressed(cfg.keyboard_leftstick_right, cfg.keyboard_leftstick_right_alt))
            - static_cast<float>(is_pressed(cfg.keyboard_leftstick_left, cfg.keyboard_leftstick_left_alt)),
        static_cast<float>(is_pressed(cfg.keyboard_leftstick_down, cfg.keyboard_leftstick_down_alt))
            - static_cast<float>(is_pressed(cfg.keyboard_leftstick_up, cfg.keyboard_leftstick_up_alt)),
        static_cast<float>(is_pressed(cfg.keyboard_rightstick_right, cfg.keyboard_rightstick_right_alt))
            - static_cast<float>(is_pressed(cfg.keyboard_rightstick_left, cfg.keyboard_rightstick_left_alt)),
        static_cast<float>(is_pressed(cfg.keyboard_rightstick_down, cfg.keyboard_rightstick_down_alt))
            - static_cast<float>(is_pressed(cfg.keyboard_rightstick_up, cfg.keyboard_rightstick_up_alt))
    };
    uint32_t buttons = 0;
    uint32_t buttons_ext = 0;
    const auto set_common_buttons = [&cfg, &set_bit_if_pressed](uint32_t &mask) {
        set_bit_if_pressed(cfg.keyboard_button_select, cfg.keyboard_button_select_alt, SCE_CTRL_SELECT, mask);
        set_bit_if_pressed(cfg.keyboard_button_start, cfg.keyboard_button_start_alt, SCE_CTRL_START, mask);
        set_bit_if_pressed(cfg.keyboard_button_up, cfg.keyboard_button_up_alt, SCE_CTRL_UP, mask);
        set_bit_if_pressed(cfg.keyboard_button_right, cfg.keyboard_button_right_alt, SCE_CTRL_RIGHT, mask);
        set_bit_if_pressed(cfg.keyboard_button_down, cfg.keyboard_button_down_alt, SCE_CTRL_DOWN, mask);
        set_bit_if_pressed(cfg.keyboard_button_left, cfg.keyboard_button_left_alt, SCE_CTRL_LEFT, mask);
        set_bit_if_pressed(cfg.keyboard_button_triangle, cfg.keyboard_button_triangle_alt, SCE_CTRL_TRIANGLE, mask);
        set_bit_if_pressed(cfg.keyboard_button_circle, cfg.keyboard_button_circle_alt, SCE_CTRL_CIRCLE, mask);
        set_bit_if_pressed(cfg.keyboard_button_cross, cfg.keyboard_button_cross_alt, SCE_CTRL_CROSS, mask);
        set_bit_if_pressed(cfg.keyboard_button_square, cfg.keyboard_button_square_alt, SCE_CTRL_SQUARE, mask);
        set_bit_if_pressed(cfg.keyboard_button_psbutton, cfg.keyboard_button_psbutton_alt, SCE_CTRL_PSBUTTON, mask);
    };
    set_common_buttons(buttons);
    set_common_buttons(buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l1, cfg.keyboard_button_l1_alt, SCE_CTRL_L, buttons);
    set_bit_if_pressed(cfg.keyboard_button_r1, cfg.keyboard_button_r1_alt, SCE_CTRL_R, buttons);
    set_bit_if_pressed(cfg.keyboard_button_l1, cfg.keyboard_button_l1_alt, SCE_CTRL_L1, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r1, cfg.keyboard_button_r1_alt, SCE_CTRL_R1, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l2, cfg.keyboard_button_l2_alt, SCE_CTRL_L2, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r2, cfg.keyboard_button_r2_alt, SCE_CTRL_R2, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l3, cfg.keyboard_button_l3_alt, SCE_CTRL_L3, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r3, cfg.keyboard_button_r3_alt, SCE_CTRL_R3, buttons_ext);

    const std::lock_guard<std::mutex> lock(emuenv.ctrl.mutex);
    emuenv.ctrl.keyboard_state.buttons = buttons;
    emuenv.ctrl.keyboard_state.buttons_ext = buttons_ext;
    for (size_t i = 0; i < 4; ++i)
        emuenv.ctrl.keyboard_state.axes[i] = axes[i];
}

bool is_license_file(const fs::path &path) {
    const auto extension = string_utils::tolower(path.extension().string());
    return extension == ".bin" || extension == ".rif" || path.filename() == "work.bin";
}

void process_dropped_firmware(EmuEnvState &emuenv, const fs::path &drop_file, EventResult &result) {
    const std::string fw_version = install_pup(emuenv.vita_fs_path, drop_file);
    if (!fw_version.empty()) {
        LOG_INFO("Firmware {} installed successfully!", fw_version);
        result.firmware_updated = true;
    }
}

void process_dropped_files(EmuEnvState &emuenv, const std::vector<fs::path> &drop_files) {
    std::vector<bool> handled(drop_files.size(), false);
    for (size_t i = 0; i < drop_files.size(); ++i) {
        if (string_utils::tolower(drop_files[i].extension().string()) != ".pkg")
            continue;

        handled[i] = true;
        std::optional<size_t> license_index;
        for (size_t j = 0; j < drop_files.size(); ++j) {
            if (!handled[j] && is_license_file(drop_files[j])) {
                license_index = j;
                handled[j] = true;
                break;
            }
        }

        std::string zrif;
        if (license_index) {
            std::ifstream license_file(drop_files[*license_index], std::ios::binary | std::ios::ate);
            if (!license_file) {
                LOG_ERROR("Could not read dropped license file '{}'.", drop_files[*license_index]);
                continue;
            }
            zrif = rif2zrif(license_file);
            if (!validate_zrif(zrif)) {
                LOG_ERROR("Dropped license file '{}' is invalid.", drop_files[*license_index]);
                continue;
            }
        } else {
            zrif = find_pkg_zrif(drop_files[i], emuenv.vita_fs_path);
            if (zrif.empty()) {
                LOG_ERROR("No valid license was found for dropped package '{}'.", drop_files[i]);
                continue;
            }
        }

        if (install_pkg(drop_files[i], emuenv, zrif, [](float) {}))
            LOG_INFO("Package '{}' installed successfully.", drop_files[i]);
        else
            LOG_ERROR("Failed to install dropped package '{}'.", drop_files[i]);
    }

    for (size_t i = 0; i < drop_files.size(); ++i) {
        if (handled[i])
            continue;

        const auto &drop_file = drop_files[i];
        const auto extension = string_utils::tolower(drop_file.extension().string());
        if (extension == ".vpk" || extension == ".zip") {
            install_archive(emuenv, drop_file);
        } else if (is_license_file(drop_file)) {
            copy_license(emuenv, drop_file);
        } else if (fs::is_directory(drop_file)) {
            install_contents(emuenv, drop_file);
        } else if (drop_file.filename() == "theme.xml") {
            install_content(emuenv, drop_file.parent_path());
        } else {
            LOG_ERROR("File dropped: [{}] is not supported.", drop_file.filename());
        }
    }
}
} // namespace

void clear_keyboard_state(EmuEnvState &emuenv) {
    pressed_keys.clear();
    const std::lock_guard<std::mutex> lock(emuenv.ctrl.mutex);
    emuenv.ctrl.keyboard_state = {};
}

EventResult process_event(EmuEnvState &emuenv, const SDL_Event &event) {
    EventResult result;
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        result.outcome = EventOutcome::QuitRequested;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        result.physical_key = physical_key_from_scancode(event.key.scancode);
        if (is_any_ime_active(emuenv)) {
            clear_keyboard_state(emuenv);
            break;
        }
        update_keyboard_state(emuenv, result.physical_key, event.type == SDL_EVENT_KEY_DOWN);
        if (event.type == SDL_EVENT_KEY_DOWN)
            result.vita_button = button_from_physical_key(emuenv, result.physical_key);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        for (const auto &binding : get_controller_bindings_ext(emuenv)) {
            if (event.gbutton.button == binding.controller) {
                result.vita_button = binding.button;
                break;
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        refresh_controllers(emuenv.ctrl, emuenv);
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: {
        auto touchpad_event = event.gtouchpad;
        handle_touchpad_event(emuenv.touch, touchpad_event);
        break;
    }
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        handle_motion_event(emuenv, event.gsensor.sensor, event.gsensor);
        break;
    case SDL_EVENT_SENSOR_UPDATE:
        handle_motion_event(emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        auto touch_event = event.tfinger;
        handle_touch_event(emuenv.touch, touch_event);
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        clear_keyboard_state(emuenv);
        emuenv.touch.renderer_focused = false;
        emuenv.touch.mouse_button_left = false;
        emuenv.touch.mouse_button_right = false;
        emuenv.ctrl.overlay_mouse.pressed.store(false, std::memory_order_relaxed);
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        emuenv.touch.renderer_focused = true;
        break;
    case SDL_EVENT_DROP_BEGIN:
        pending_drop_files.clear();
        break;
    case SDL_EVENT_DROP_FILE:
        if (event.drop.data)
            pending_drop_files.emplace_back(fs_utils::utf8_to_path(event.drop.data));
        break;
    case SDL_EVENT_DROP_COMPLETE: {
        std::vector<fs::path> other_drop_files;
        for (const auto &drop_file : pending_drop_files) {
            if (string_utils::tolower(drop_file.extension().string()) == ".pup")
                process_dropped_firmware(emuenv, drop_file, result);
            else
                other_drop_files.push_back(drop_file);
        }
        process_dropped_files(emuenv, other_drop_files);
        pending_drop_files.clear();
        break;
    }
    default:
        break;
    }

    return result;
}

bool set_fullscreen(EmuEnvState &emuenv, SDL_Window *window, bool fullscreen) {
    if (!window)
        return false;
    if (!SDL_SetWindowFullscreen(window, fullscreen)) {
        LOG_ERROR("Failed to set fullscreen mode: {}", SDL_GetError());
        return false;
    }

    emuenv.display.fullscreen.store(fullscreen, std::memory_order_relaxed);
    if (emuenv.renderer)
        emuenv.renderer->set_fullscreen(fullscreen);
    return true;
}

bool process_fullscreen_key(EmuEnvState &emuenv, SDL_Window *window, input::PhysicalKeyCode key) {
    if (key == input::PhysicalKeyCode::Unbound || is_any_ime_active(emuenv)
        || (key != emuenv.cfg.keyboard_gui_fullscreen && key != emuenv.cfg.keyboard_gui_fullscreen_alt)) {
        return false;
    }

    return set_fullscreen(emuenv, window, !emuenv.display.fullscreen.load(std::memory_order_relaxed));
}

} // namespace sdl
