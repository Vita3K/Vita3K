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
// Options produced in config file; order is code(option_type, option_name, default_value)
// When adding in a new macro for generation, ALL options must be stated.
#define CONFIG_LIST(code)                                                                       \
    code(std::vector<std::string>, lle_modules, std::vector<std::string>{})                     \
    code(bool, log_imports, false)                                                              \
    code(bool, log_exports, false)                                                              \
    code(bool, log_active_shaders, false)                                                       \
    code(bool, log_uniforms, false)                                                             \
    code(bool, pstv_mode, false)                                                                \
    code(bool, show_gui, true)                                                                  \
    code(bool, show_game_background, false)                                                     \
    code(int, icon_size, 64)                                                                    \
    code(bool, archive_log, false)                                                              \
    code(bool, texture_cache, true)                                                             \
    code(int, sys_button, static_cast<int>(SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS))                \
    code(int, sys_lang, static_cast<int>(SCE_SYSTEM_PARAM_LANG_ENGLISH_US))                     \
    code(std::string, background_image, std::string{})                                          \
    code(float, background_alpha, .300f)                                                        \
    code(int, log_level, spdlog::level::trace)                                                  \
    code(std::string, pref_path, std::string{})                                                 \
    code(bool, discord_rich_presence, true)                                                     \
    code(bool, wait_for_debugger, false)                                                        \
    code(bool, color_surface_debug, false)                                                      \
    code(bool, hardware_flip, false)                                                            \
    code(bool, performance_overlay, false)                                                      \
    code(std::string, backend_renderer, "OpenGL")                                               \
    code(int, user_id, 0)                                                                       \
    code(std::vector<std::string>, online_id, std::vector<std::string>{"Vita3K"})
// clang-format on
