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

#include <config/config.h>
#include <config/yaml.h>

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
