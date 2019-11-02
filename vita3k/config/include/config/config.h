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

#include <config/yaml.h>

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

struct ConfigParent : YamlLoader {
    // Settings not in the config.yml file
    fs::path config_path = {};
    bool overwrite_config = true;
    bool load_config = false;

    ConfigParent &operator=(const ConfigParent &rhs) {
        if (this != &rhs) {
            yaml_node = rhs.yaml_node;

            config_path = rhs.config_path;
            overwrite_config = rhs.overwrite_config;
            load_config = rhs.load_config;
        }
        return *this;
    }

    ConfigParent &operator=(ConfigParent &&rhs) noexcept {
        yaml_node = rhs.yaml_node;

        config_path = rhs.config_path;
        overwrite_config = rhs.overwrite_config;
        load_config = rhs.load_config;

        return *this;
    }
};

// Configuration File options
struct Config : ConfigParent {
private:
    void update_members() {
#define UPDATE_MEMBERS(option_type, option_name, option_default) \
    option_name = yaml_node[#option_name].as<##option_type>();
        CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
    }

public:
    // Optional config settings
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<std::string> recompile_shader_path;

    Config &operator=(const Config &rhs) {
        if (this != &rhs) {
            yaml_node = get_as_node(rhs);

            if (rhs.vpk_path.is_initialized())
                vpk_path = rhs.vpk_path;
            if (rhs.run_title_id.is_initialized())
                run_title_id = rhs.run_title_id;
            if (rhs.recompile_shader_path.is_initialized())
                run_title_id = rhs.run_title_id;

            config_path = rhs.config_path;
            overwrite_config = rhs.overwrite_config;
            load_config = rhs.load_config;

            update_members();
        }
        return *this;
    }

    Config &operator=(Config &&rhs) noexcept {
        yaml_node = get_as_node(rhs);

        if (rhs.vpk_path.is_initialized())
            vpk_path = rhs.vpk_path;
        if (rhs.run_title_id.is_initialized())
            run_title_id = rhs.run_title_id;
        if (rhs.recompile_shader_path.is_initialized())
            run_title_id = rhs.run_title_id;

        config_path = rhs.config_path;
        overwrite_config = rhs.overwrite_config;
        load_config = rhs.load_config;

        update_members();

        return *this;
    }

    // Generate a YAML node based on the current values of the members.
    YAML::Node get() const {
        YAML::Node out;

#define GEN_VALUES(option_type, option_name, option_default) \
    out[#option_name] = option_name;
        CONFIG_LIST(GEN_VALUES)
#undef GEN_VALUES

        return out;
    }

    YAML::Node get_as_node(const Config &rhs) {
        auto out = rhs.yaml_node;

#define GEN_VALUES(option_type, option_name, option_default) \
    out[#option_name] = rhs.##option_name;
        CONFIG_LIST(GEN_VALUES)
#undef GEN_VALUES

        return out;
    }

    void update_yaml() {
        yaml_node = get();
    }

    // Generate members in the preprocessor
    CONFIG_LIST(YAML_MEMBER)
};
