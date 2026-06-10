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

#include <input/physical_key.h>
#include <util/system.h>

enum ModulesMode {
    AUTOMATIC,
    AUTO_MANUAL,
    MANUAL
};

enum PerformanceOverlayDetail {
    MINIMUM,
    LOW,
    MEDIUM,
    MAXIMUM,
};

enum PerformanceOverlayPosition {
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
};

enum ScreenshotFormat {
    None,
    JPEG,
    PNG,
};

enum UpdateStartupMode {
    UPDATE_STARTUP_OFF,
    UPDATE_STARTUP_PROMPT,
    UPDATE_STARTUP_BACKGROUND,
    UPDATE_STARTUP_AUTO,
};

using PhysicalKeyCode = input::PhysicalKeyCode;

// clang-format off
// Singular options produced in config file
// Order is code(option_type, option_name, option_default, member_name)
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_KEYBOARD(code)                                                                                                          \
    code(PhysicalKeyCode, "keyboard-button-select", PhysicalKeyCode::ShiftRight, keyboard_button_select)                               \
    code(PhysicalKeyCode, "keyboard-button-start", PhysicalKeyCode::Enter, keyboard_button_start)                                      \
    code(PhysicalKeyCode, "keyboard-button-up", PhysicalKeyCode::ArrowUp, keyboard_button_up)                                          \
    code(PhysicalKeyCode, "keyboard-button-right", PhysicalKeyCode::ArrowRight, keyboard_button_right)                                 \
    code(PhysicalKeyCode, "keyboard-button-down", PhysicalKeyCode::ArrowDown, keyboard_button_down)                                    \
    code(PhysicalKeyCode, "keyboard-button-left", PhysicalKeyCode::ArrowLeft, keyboard_button_left)                                    \
    code(PhysicalKeyCode, "keyboard-button-l1", PhysicalKeyCode::KeyQ, keyboard_button_l1)                                             \
    code(PhysicalKeyCode, "keyboard-button-r1", PhysicalKeyCode::KeyE, keyboard_button_r1)                                             \
    code(PhysicalKeyCode, "keyboard-button-l2", PhysicalKeyCode::KeyU, keyboard_button_l2)                                             \
    code(PhysicalKeyCode, "keyboard-button-r2", PhysicalKeyCode::KeyO, keyboard_button_r2)                                             \
    code(PhysicalKeyCode, "keyboard-button-l3", PhysicalKeyCode::KeyF, keyboard_button_l3)                                             \
    code(PhysicalKeyCode, "keyboard-button-r3", PhysicalKeyCode::KeyH, keyboard_button_r3)                                             \
    code(PhysicalKeyCode, "keyboard-button-triangle", PhysicalKeyCode::KeyV, keyboard_button_triangle)                                 \
    code(PhysicalKeyCode, "keyboard-button-circle", PhysicalKeyCode::KeyC, keyboard_button_circle)                                     \
    code(PhysicalKeyCode, "keyboard-button-cross", PhysicalKeyCode::KeyX, keyboard_button_cross)                                       \
    code(PhysicalKeyCode, "keyboard-button-square", PhysicalKeyCode::KeyZ, keyboard_button_square)                                     \
    code(PhysicalKeyCode, "keyboard-leftstick-left", PhysicalKeyCode::KeyA, keyboard_leftstick_left)                                   \
    code(PhysicalKeyCode, "keyboard-leftstick-right", PhysicalKeyCode::KeyD, keyboard_leftstick_right)                                 \
    code(PhysicalKeyCode, "keyboard-leftstick-up", PhysicalKeyCode::KeyW, keyboard_leftstick_up)                                       \
    code(PhysicalKeyCode, "keyboard-leftstick-down", PhysicalKeyCode::KeyS, keyboard_leftstick_down)                                   \
    code(PhysicalKeyCode, "keyboard-rightstick-left", PhysicalKeyCode::KeyJ, keyboard_rightstick_left)                                 \
    code(PhysicalKeyCode, "keyboard-rightstick-right", PhysicalKeyCode::KeyL, keyboard_rightstick_right)                               \
    code(PhysicalKeyCode, "keyboard-rightstick-up", PhysicalKeyCode::KeyI, keyboard_rightstick_up)                                     \
    code(PhysicalKeyCode, "keyboard-rightstick-down", PhysicalKeyCode::KeyK, keyboard_rightstick_down)                                 \
    code(PhysicalKeyCode, "keyboard-button-psbutton", PhysicalKeyCode::KeyP, keyboard_button_psbutton)                                 \
    code(PhysicalKeyCode, "keyboard-gui-fullscreen", PhysicalKeyCode::F11, keyboard_gui_fullscreen)                                    \
    code(PhysicalKeyCode, "keyboard-gui-toggle-touch", PhysicalKeyCode::KeyT, keyboard_gui_toggle_touch)                               \
    code(PhysicalKeyCode, "keyboard-toggle-texture-replacement", PhysicalKeyCode::Unbound, keyboard_toggle_texture_replacement)        \
    code(PhysicalKeyCode, "keyboard-take-screenshot", PhysicalKeyCode::Unbound, keyboard_take_screenshot)                              \
    code(PhysicalKeyCode, "keyboard-pinch-modifier", PhysicalKeyCode::Unbound, keyboard_pinch_modifier)                                \
    code(PhysicalKeyCode, "keyboard-alternate-pinch-in", PhysicalKeyCode::Unbound, keyboard_alternate_pinch_in)                        \
    code(PhysicalKeyCode, "keyboard-alternate-pinch-out", PhysicalKeyCode::Unbound, keyboard_alternate_pinch_out)                      \
    code(PhysicalKeyCode, "keyboard-button-select-alt", PhysicalKeyCode::Unbound, keyboard_button_select_alt)                          \
    code(PhysicalKeyCode, "keyboard-button-start-alt", PhysicalKeyCode::Unbound, keyboard_button_start_alt)                            \
    code(PhysicalKeyCode, "keyboard-button-up-alt", PhysicalKeyCode::Unbound, keyboard_button_up_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-right-alt", PhysicalKeyCode::Unbound, keyboard_button_right_alt)                            \
    code(PhysicalKeyCode, "keyboard-button-down-alt", PhysicalKeyCode::Unbound, keyboard_button_down_alt)                              \
    code(PhysicalKeyCode, "keyboard-button-left-alt", PhysicalKeyCode::Unbound, keyboard_button_left_alt)                              \
    code(PhysicalKeyCode, "keyboard-button-l1-alt", PhysicalKeyCode::Unbound, keyboard_button_l1_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-r1-alt", PhysicalKeyCode::Unbound, keyboard_button_r1_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-l2-alt", PhysicalKeyCode::Unbound, keyboard_button_l2_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-r2-alt", PhysicalKeyCode::Unbound, keyboard_button_r2_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-l3-alt", PhysicalKeyCode::Unbound, keyboard_button_l3_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-r3-alt", PhysicalKeyCode::Unbound, keyboard_button_r3_alt)                                  \
    code(PhysicalKeyCode, "keyboard-button-triangle-alt", PhysicalKeyCode::Unbound, keyboard_button_triangle_alt)                      \
    code(PhysicalKeyCode, "keyboard-button-circle-alt", PhysicalKeyCode::Unbound, keyboard_button_circle_alt)                          \
    code(PhysicalKeyCode, "keyboard-button-cross-alt", PhysicalKeyCode::Unbound, keyboard_button_cross_alt)                            \
    code(PhysicalKeyCode, "keyboard-button-square-alt", PhysicalKeyCode::Unbound, keyboard_button_square_alt)                          \
    code(PhysicalKeyCode, "keyboard-leftstick-left-alt", PhysicalKeyCode::Unbound, keyboard_leftstick_left_alt)                        \
    code(PhysicalKeyCode, "keyboard-leftstick-right-alt", PhysicalKeyCode::Unbound, keyboard_leftstick_right_alt)                      \
    code(PhysicalKeyCode, "keyboard-leftstick-up-alt", PhysicalKeyCode::Unbound, keyboard_leftstick_up_alt)                            \
    code(PhysicalKeyCode, "keyboard-leftstick-down-alt", PhysicalKeyCode::Unbound, keyboard_leftstick_down_alt)                        \
    code(PhysicalKeyCode, "keyboard-rightstick-left-alt", PhysicalKeyCode::Unbound, keyboard_rightstick_left_alt)                      \
    code(PhysicalKeyCode, "keyboard-rightstick-right-alt", PhysicalKeyCode::Unbound, keyboard_rightstick_right_alt)                    \
    code(PhysicalKeyCode, "keyboard-rightstick-up-alt", PhysicalKeyCode::Unbound, keyboard_rightstick_up_alt)                          \
    code(PhysicalKeyCode, "keyboard-rightstick-down-alt", PhysicalKeyCode::Unbound, keyboard_rightstick_down_alt)                      \
    code(PhysicalKeyCode, "keyboard-button-psbutton-alt", PhysicalKeyCode::Unbound, keyboard_button_psbutton_alt)                      \
    code(PhysicalKeyCode, "keyboard-gui-fullscreen-alt", PhysicalKeyCode::Unbound, keyboard_gui_fullscreen_alt)                        \
    code(PhysicalKeyCode, "keyboard-gui-toggle-touch-alt", PhysicalKeyCode::Unbound, keyboard_gui_toggle_touch_alt)                    \
    code(PhysicalKeyCode, "keyboard-toggle-texture-replacement-alt", PhysicalKeyCode::Unbound, keyboard_toggle_texture_replacement_alt)\
    code(PhysicalKeyCode, "keyboard-take-screenshot-alt", PhysicalKeyCode::Unbound, keyboard_take_screenshot_alt)                      \
    code(PhysicalKeyCode, "keyboard-pinch-modifier-alt", PhysicalKeyCode::Unbound, keyboard_pinch_modifier_alt)                        \
    code(PhysicalKeyCode, "keyboard-alternate-pinch-in-alt", PhysicalKeyCode::Unbound, keyboard_alternate_pinch_in_alt)                \
    code(PhysicalKeyCode, "keyboard-alternate-pinch-out-alt", PhysicalKeyCode::Unbound, keyboard_alternate_pinch_out_alt)

#define CONFIG_INDIVIDUAL(code)                                                                         \
    code(bool, "initial-setup", false, initial_setup)                                                   \
    code(bool, "gdbstub", false, gdbstub)                                                               \
    code(bool, "log-active-shaders", false, log_active_shaders)                                         \
    code(bool, "log-uniforms", false, log_uniforms)                                                     \
    code(bool, "log-compat-warn", false, log_compat_warn)                                               \
    code(bool, "validation-layer", true, validation_layer)                                              \
    code(bool, "pstv-mode", false, pstv_mode)                                                           \
    code(bool, "show-mode", false, show_mode)                                                           \
    code(bool, "demo-mode", false, demo_mode)                                                           \
    code(bool, "apps-list-grid", false, apps_list_grid)                                                 \
    code(bool, "stretch_the_display_area", false, stretch_the_display_area)                             \
    code(bool, "fullscreen_hd_res_pixel_perfect", false, fullscreen_hd_res_pixel_perfect)               \
    code(bool, "archive-log", false, archive_log)                                                       \
    code(std::string, "backend-renderer", "Vulkan", backend_renderer)                                   \
    code(std::string, "custom-driver-name", "", custom_driver_name)                                     \
    code(bool, "turbo-mode", false, turbo_mode)                                                         \
    code(int, "gpu-idx", 0, gpu_idx)                                                                    \
    code(bool, "high-accuracy", false, high_accuracy)                                                   \
    code(float, "resolution-multiplier", 1.0f, resolution_multiplier)                                   \
    code(bool, "disable-surface-sync", true, disable_surface_sync)                                      \
    code(std::string, "screen-filter", "Bilinear", screen_filter)                                       \
    code(bool, "v-sync", true, v_sync)                                                                  \
    code(int, "anisotropic-filtering", 1, anisotropic_filtering)                                        \
    code(bool, "texture-cache", true, texture_cache)                                                    \
    code(bool, "async-pipeline-compilation", true, async_pipeline_compilation)                          \
    code(bool, "show-compile-shaders", true, show_compile_shaders)                                      \
    code(bool, "hashless-texture-cache", false, hashless_texture_cache)                                 \
    code(bool, "import-textures", false, import_textures)                                               \
    code(bool, "export-textures", false, export_textures)                                               \
    code(bool, "export-as-png", true, export_as_png)                                                    \
    code(std::string, "memory-mapping", "double-buffer", memory_mapping)                                \
    code(bool, "boot-apps-full-screen", false, boot_apps_full_screen)                                   \
    code(bool, "show-live-area-screen", false, show_live_area_screen)                                   \
    code(std::string, "audio-backend", "SDL", audio_backend)                                            \
    code(int, "audio-volume", 100, audio_volume)                                                        \
    code(bool, "ngs-enable", true, ngs_enable)                                                          \
    code(int, "sys-button", static_cast<int>(SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS), sys_button)          \
    code(int, "sys-lang", static_cast<int>(SCE_SYSTEM_PARAM_LANG_ENGLISH_US), sys_lang)                 \
    code(int, "sys-date-format", (int)SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY, sys_date_format)           \
    code(int, "sys-time-format", (int)SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR, sys_time_format)             \
    code(int, "cpu-pool-size", 10, cpu_pool_size)                                                       \
    code(int, "modules-mode", static_cast<int>(ModulesMode::AUTOMATIC), modules_mode)                   \
    code(int, "delay-background", 4, delay_background)                                                  \
    code(int, "delay-start", 30, delay_start)                                                           \
    code(float, "background-alpha", .300f, background_alpha)                                            \
    code(int, "log-level", 0 /*SPDLOG_LEVEL_TRACE*/, log_level)                                         \
    code(bool, "cpu-opt", true, cpu_opt)                                                                \
    code(std::string, "pref-path", std::string{}, vita_fs_path)                                         \
    code(bool, "discord-rich-presence", true, discord_rich_presence)                                    \
    code(bool, "wait-for-debugger", false, wait_for_debugger)                                           \
    code(bool, "color-surface-debug", false, color_surface_debug)                                       \
    code(bool, "performance-overlay", false, performance_overlay)                                       \
    code(int, "performance-overlay-detail", static_cast<int>(MINIMUM), performance_overlay_detail)      \
    code(int, "performance-overlay-position", static_cast<int>(TOP_LEFT), performance_overlay_position) \
    code(int, "screenshot-format", static_cast<int>(JPEG), screenshot_format)                           \
    code(bool, "disable-motion", false, disable_motion)                                                 \
    code(float, "controller-analog-multiplier", 1.0f, controller_analog_multiplier)                     \
    CONFIG_KEYBOARD(code)                                                                               \
    code(std::string, "user-id", std::string{}, user_id)                                                \
    code(bool, "user-auto-connect", false, auto_user_login)                                             \
    code(std::string, "user-lang", std::string{}, user_lang)                                            \
    code(bool, "show-welcome", true, show_welcome)                                                      \
    code(bool, "warn-missing-firmware", true, warn_missing_firmware)                                    \
    code(bool, "check-for-updates", true, check_for_updates)                                            \
    code(int, "check-for-updates-mode", static_cast<int>(UPDATE_STARTUP_PROMPT), check_for_updates_mode)\
    code(int, "file-loading-delay", 0, file_loading_delay)                                              \
    code(bool, "shader-cache", true, shader_cache)                                                      \
    code(bool, "spirv-shader", false, spirv_shader)                                                     \
    code(bool, "fps-hack", false, fps_hack)                                                             \
    code(uint64_t, "current-ime-lang", 4, current_ime_lang)                                             \
    code(int, "psn-signed-in", false, psn_signed_in)                                                    \
    code(bool, "http-enable", true, http_enable)                                                        \
    code(int, "http-timeout-attempts", 50, http_timeout_attempts)                                       \
    code(int, "http-timeout-sleep-ms", 100, http_timeout_sleep_ms)                                      \
    code(int, "http-read-end-attempts", 10, http_read_end_attempts)                                     \
    code(int, "http-read-end-sleep-ms", 250, http_read_end_sleep_ms)                                    \
    code(int, "adhoc-addr", 0, adhoc_addr)                                                              \
    code(int, "front-camera-type", 2, front_camera_type)                                                \
    code(std::string, "front-camera-id", std::string{}, front_camera_id)                                \
    code(std::string, "front-camera-image", std::string{}, front_camera_image)                          \
    code(uint32_t, "front-camera-color", 0, front_camera_color)                                         \
    code(int, "back-camera-type", 2, back_camera_type)                                                  \
    code(std::string, "back-camera-id", std::string{}, back_camera_id)                                  \
    code(std::string, "back-camera-image", std::string{}, back_camera_image)                            \
    code(uint32_t, "back-camera-color", 0, back_camera_color)                                           \
    code(bool, "tracy-primitive-impl", false, tracy_primitive_impl)

// Vector members produced in the config file
// Order is code(option_type, option_name, default_value)
// If you are going to implement a dynamic list in the YAML, add it here instead
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_VECTOR(code)                                                                             \
    code(std::vector<short>, "controller-binds", std::vector<short>{}, controller_binds)                \
    code(std::vector<short>, "controller-axis-binds", std::vector<short>{}, controller_axis_binds)      \
    code(std::vector<int>, "controller-led-color", std::vector<int>{}, controller_led_color)            \
    code(std::vector<std::string>, "lle-modules", std::vector<std::string>{}, lle_modules)              \
    code(std::vector<uint64_t>, "ime-langs", std::vector<uint64_t>{4}, ime_langs)                       \
    code(std::vector<std::string>, "tracy-advanced-profiling-modules", std::vector<std::string>{}, tracy_advanced_profiling_modules)

// Parent macro for easier generation
#define CONFIG_LIST(code)                                                                               \
    CONFIG_INDIVIDUAL(code)                                                                             \
    CONFIG_VECTOR(code)

// clang-format on
