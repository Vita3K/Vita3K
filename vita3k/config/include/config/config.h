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

#include <psp2/system_param.h>
#include <spdlog/spdlog.h>

// clang-format off
// Singular options produced in config file
// Order is code(option_type, option_name, option_default, member_name)
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_INDIVIDUAL(code)                                                                         \
    code(bool, "log-imports", false, log_imports)                                                       \
    code(bool, "log-exports", false, log_exports)                                                       \
    code(bool, "log-active-shaders", false, log_active_shaders)                                         \
    code(bool, "log-uniforms", false, log_uniforms)                                                     \
    code(bool, "pstv-mode", false, pstv_mode)                                                           \
    code(bool, "show-gui", true, show_gui)                                                              \
    code(bool, "show-game-background", false, show_game_background)                                     \
    code(int, "icon-size", 64, icon_size)                                                               \
    code(bool, "archive-log", false, archive_log)                                                       \
    code(bool, "texture-cache", true, texture_cache)                                                    \
    code(int, "sys-button", static_cast<int>(SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS), sys_button)          \
    code(int, "sys-lang", static_cast<int>(SCE_SYSTEM_PARAM_LANG_ENGLISH_US), sys_lang)                 \
    code(std::string, "background-image", std::string{}, background_image)                              \
    code(float, "background-alpha", .300f, background_alpha)                                            \
    code(int, "log-level", static_cast<int>(spdlog::level::trace), log_level)                           \
    code(std::string, "pref-path", std::string{}, pref_path)                                            \
    code(bool, "discord-rich-presence", true, discord_rich_presence)                                    \
    code(bool, "wait-for-debugger", false, wait_for_debugger)                                           \
    code(bool, "color-surface-debug", false, color_surface_debug)                                       \
    code(bool, "hardware-flip", false, hardware_flip)                                                   \
    code(bool, "performance-overlay", false, performance_overlay)                                       \
    code(std::string, "backend-renderer", "OpenGL", backend_renderer)                                   \
    code(int, "user-id", 0, user_id)

// Vector members produced in the config file
// Order is code(option_type, option_name, default_value)
// If you are going to implement a dynamic list in the YAML, add it here instead
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_VECTOR(code)                                                                             \
    code(std::vector<std::string>, "lle-modules", std::vector<std::string>{}, lle_modules)              \
    code(std::vector<std::string>, "online-id", std::vector<std::string>{"Vita3K"}, online_id)

// Parent macro for easier generation
#define CONFIG_LIST(code)                                                                               \
    CONFIG_INDIVIDUAL(code)                                                                             \
    CONFIG_VECTOR(code)

// clang-format on
