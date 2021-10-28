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

#include <util/log.h>
#include <util/system.h>

// clang-format off
// Singular options produced in config file
// Order is code(option_type, option_name, option_default, member_name)
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_INDIVIDUAL(code)                                                                         \
    code(bool, "gdbstub", false, gdbstub)                                                               \
    code(bool, "log-active-shaders", false, log_active_shaders)                                         \
    code(bool, "log-uniforms", false, log_uniforms)                                                     \
    code(bool, "pstv-mode", false, pstv_mode)                                                           \
    code(bool, "show-gui", false, show_gui)                                                             \
    code(bool, "show-info-bar", false, show_info_bar)                                                   \
    code(bool, "apps-list-grid", false, apps_list_grid)                                                 \
    code(bool, "show-live-area-screen", true, show_live_area_screen)                                    \
    code(int, "icon-size", 64, icon_size)                                                               \
    code(bool, "archive-log", false, archive_log)                                                       \
    code(bool, "texture-cache", true, texture_cache)                                                    \
    code(bool, "hashless-texture-cache", false, hashless_taexture_cache)                                \
    code(bool, "disable-ngs", false, disable_ngs)                                                       \
    code(int, "sys-button", static_cast<int>(SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS), sys_button)          \
    code(int, "sys-lang", static_cast<int>(SCE_SYSTEM_PARAM_LANG_ENGLISH_US), sys_lang)                 \
    code(bool, "lle-kernel", false, lle_kernel)                                                         \
    code(int, "cpu-pool-size", 10, cpu_pool_size)                                                       \
    code(bool, "auto-lle", false, auto_lle)                                                             \
    code(int, "delay-background", 4, delay_background)                                                  \
    code(int, "delay-start", 10, delay_start)                                                           \
    code(float, "background-alpha", .300f, background_alpha)                                            \
    code(int, "log-level", static_cast<int>(spdlog::level::trace), log_level)                           \
    code(std::string, "cpu-backend", "Dynarmic", cpu_backend)                                           \
    code(bool, "cpu-opt", true, cpu_opt)                                                                \
    code(std::string, "pref-path", std::string{}, pref_path)                                            \
    code(bool, "discord-rich-presence", true, discord_rich_presence)                                    \
    code(bool, "wait-for-debugger", false, wait_for_debugger)                                           \
    code(bool, "color-surface-debug", false, color_surface_debug)                                       \
    code(bool, "performance-overlay", false, performance_overlay)                                       \
    code(std::string, "backend-renderer", "OpenGL", backend_renderer)                                   \
    code(int, "keyboard-button-select", 229, keyboard_button_select)                                    \
    code(int, "keyboard-button-start", 40, keyboard_button_start)                                       \
    code(int, "keyboard-button-up", 82, keyboard_button_up)                                             \
    code(int, "keyboard-button-right", 79, keyboard_button_right)                                       \
    code(int, "keyboard-button-down", 81, keyboard_button_down)                                         \
    code(int, "keyboard-button-left", 80, keyboard_button_left)                                         \
    code(int, "keyboard-button-l1", 20, keyboard_button_l1)                                             \
    code(int, "keyboard-button-r1", 8, keyboard_button_r1)                                              \
    code(int, "keyboard-button-l2", 24, keyboard_button_l2)                                             \
    code(int, "keyboard-button-r2", 27, keyboard_button_r2)                                             \
    code(int, "keyboard-button-l3", 9, keyboard_button_l3)                                              \
    code(int, "keyboard-button-r3", 11, keyboard_button_r3)                                             \
    code(int, "keyboard-button-triangle", 25, keyboard_button_triangle)                                 \
    code(int, "keyboard-button-circle", 6, keyboard_button_circle)                                      \
    code(int, "keyboard-button-cross", 27, keyboard_button_cross)                                       \
    code(int, "keyboard-button-square", 29, keyboard_button_square)                                     \
    code(int, "keyboard-leftstick-left", 4, keyboard_leftstick_left)                                    \
    code(int, "keyboard-leftstick-right", 7, keyboard_leftstick_right)                                  \
    code(int, "keyboard-leftstick-up", 26, keyboard_leftstick_up)                                       \
    code(int, "keyboard-leftstick-down", 22, keyboard_leftstick_down)                                   \
    code(int, "keyboard-rightstick-left", 13, keyboard_rightstick_left)                                 \
    code(int, "keyboard-rightstick-right", 15, keyboard_rightstick_right)                               \
    code(int, "keyboard-rightstick-up", 12, keyboard_rightstick_up)                                     \
    code(int, "keyboard-rightstick-down", 14, keyboard_rightstick_down)                                 \
    code(int, "keyboard-button-psbutton", 19, keyboard_button_psbutton)                                 \
    code(std::string, "user-id", std::string{}, user_id)                                                \
    code(bool, "user-auto-connect", false, auto_user_login)                                             \
    code(bool, "dump-textures", false, dump_textures)                                                   \
    code(bool, "show-welcome", true, show_welcome)                                                      \
    code(bool, "asia-font-support", false, asia_font_support)                                           \
    code(bool, "video-playing", true, video_playing)                                                    \
    code(bool, "spirv-shader", false, spirv_shader)                                                     \
    code(bool, "vsync", true, vsync)                                                                    \
    code(uint64_t, "current-ime-lang", 4, current_ime_lang)                                             \
    code(bool, "disable-at9-decoder", false, disable_at9_decoder)

// Vector members produced in the config file
// Order is code(option_type, option_name, default_value)
// If you are going to implement a dynamic list in the YAML, add it here instead
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_VECTOR(code)                                                                             \
    code(std::vector<std::string>, "lle-modules", std::vector<std::string>{}, lle_modules)              \
    code(std::vector<uint64_t>, "ime-langs", std::vector<uint64_t>{4}, ime_langs)

// Parent macro for easier generation
#define CONFIG_LIST(code)                                                                               \
    CONFIG_INDIVIDUAL(code)                                                                             \
    CONFIG_VECTOR(code)

// clang-format on
