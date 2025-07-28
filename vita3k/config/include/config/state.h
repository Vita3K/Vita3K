// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <config/config.h>

#include <util/fs.h>

#include <optional>

// Configuration File options
struct Config {
    // Optional config settings
    std::optional<fs::path> content_path;
    std::optional<std::string> run_app_path;
    std::optional<std::string> recompile_shader_path;
    std::optional<std::string> delete_title_id;
    std::optional<std::string> pkg_path;
    std::optional<std::string> pkg_zrif;
    std::optional<std::string> pup_path;

    // Setting not present in the YAML file
    fs::path config_path = {};
    std::string app_args;
    std::string self_path;
    bool overwrite_config = true;
    bool load_config = false;
    bool fullscreen = false;
    bool console = false;
    bool load_app_list = false;

    fs::path get_pref_path() const {
        return fs_utils::utf8_to_path(pref_path);
    }

    void set_pref_path(const fs::path &new_pref_path) {
        pref_path = fs_utils::path_to_utf8(new_pref_path);
    }

    /**
     * @brief Config struct for per-app configurable settings
     *
     * All the settings that appear in this struct can be adjusted using app-specific custom config files.
     */
    struct CurrentConfig {
        bool cpu_opt = true;
        int modules_mode = ModulesMode::AUTOMATIC;
        std::vector<std::string> lle_modules = {};
        int audio_volume = 100;
        bool ngs_enable = true;
        bool pstv_mode = false;
        bool high_accuracy = false;
        float resolution_multiplier = 1.0f;
        bool disable_surface_sync = false;
        std::string screen_filter = "Bilinear";
        bool v_sync = true;
        int anisotropic_filtering = 1;
        bool async_pipeline_compilation = true;
        bool import_textures = false;
        bool export_textures = false;
        bool export_as_png = false;
        bool fps_hack = false;
        bool stretch_the_display_area = false;
        bool fullscreen_hd_res_pixel_perfect = false;
        bool show_touchpad_cursor = true;
        int file_loading_delay = 0;
        bool psn_signed_in = false;
    };

    /**
     * @brief Config struct for per-app configurable settings
     *
     * All the settings that appear in this struct can be adjusted using app-specific custom config files.
     *
     * If no app-specific config file is loaded, the values of these settings will be exactly
     * the same values as the ones set on an app-agnostic level (global emulator settings). If an app-specific config
     * file is loaded, the values of these settings will match those of the loaded app-specific config file.
     */
    CurrentConfig current_config;

    Config() = default;
    Config(const Config &rhs) = delete;
    Config(Config &&rhs) = delete;

    Config &operator=(Config &&rhs) noexcept;
    Config &operator=(const Config &rhs) noexcept;

    // Generate members in the preprocessor with default values
#define CREATE_MEMBERS(option_type, option_name, option_default, member_name) \
    option_type member_name = option_default;

    CONFIG_LIST(CREATE_MEMBERS)
#undef CREATE_MEMBERS
};
