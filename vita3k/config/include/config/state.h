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

#include <config/config.h>
#include <config/yaml.h>

#include <util/fs.h>
#include <util/optional.h>
#include <util/vector_utils.h>

// Enum based on members in Config file
// Used for easier getting of options and their names for config files
enum file_config {
#define GENERATE_ENUM(option_type, option_name, option_default, member_name) \
    e_##member_name,

    CONFIG_LIST(GENERATE_ENUM)

        _INVALID
#undef GENERATE_ENUM
};

// Configuration File options
struct Config : YamlLoader {
private:
    // Privately update members
    // Use this function when the YAML file is updated before the members.
    void update_members() {
#define UPDATE_MEMBERS(option_type, option_name, option_default, member_name) \
    if (yaml_node[option_name].IsDefined()) {                                 \
        member_name = get_member<option_type>(option_name);                   \
    } else {                                                                  \
        member_name = option_default;                                         \
    }

        CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
    }

    // Perform comparisons with optional settings
    void check_members(const Config &rhs) {
        if (rhs.content_path.has_value())
            content_path = rhs.content_path;
        if (rhs.run_app_path.has_value())
            run_app_path = rhs.run_app_path;
        if (rhs.recompile_shader_path.has_value())
            recompile_shader_path = rhs.recompile_shader_path;
        if (rhs.delete_title_id.has_value())
            delete_title_id = rhs.delete_title_id;
        if (rhs.pkg_path.has_value())
            pkg_path = rhs.pkg_path;
        if (rhs.pkg_zrif.has_value())
            pkg_zrif = rhs.pkg_zrif;

        if (!rhs.config_path.empty())
            config_path = rhs.config_path;

        overwrite_config = rhs.overwrite_config;
        load_config = rhs.load_config;
        fullscreen = rhs.fullscreen;
        console = rhs.console;
        app_args = rhs.app_args;
        load_app_list = rhs.load_app_list;
        self_path = rhs.self_path;
    }

public:
    // Optional config settings
    optional<fs::path> content_path;
    optional<std::string> run_app_path;
    optional<std::string> recompile_shader_path;
    optional<std::string> delete_title_id;
    optional<std::string> pkg_path;
    optional<std::string> pkg_zrif;

    // Setting not present in the YAML file
    fs::path config_path = {};
    std::string app_args;
    std::string self_path;
    bool overwrite_config = true;
    bool load_config = false;
    bool fullscreen = false;
    bool console = false;
    bool load_app_list = false;

    // Current config
    struct CurrentConfig {
        std::string cpu_backend;
        bool cpu_opt = true;
        bool lle_driver_user = false;
        bool auto_lle = false;
        std::vector<std::string> lle_modules = {};
        bool pstv_mode = false;
        bool disable_ngs = false;
        bool video_playing = true;
        bool disable_at9_decoder = false;
    };

    CurrentConfig current_config;

    Config() {
        update_yaml();
    }

    ~Config() override = default;

    Config(const Config &rhs) {
        yaml_node = rhs.get();
        check_members(rhs);
        update_members();
    }

    Config(Config &&rhs) noexcept {
        yaml_node = rhs.get();
        check_members(rhs);
        update_members();
    }

    Config &operator=(const Config &rhs) {
        if (this != &rhs) {
            yaml_node = rhs.get();
            check_members(rhs);
            update_members();
        }
        return *this;
    }

    Config &operator=(Config &&rhs) noexcept {
        yaml_node = rhs.get();
        check_members(rhs);
        update_members();
        return *this;
    }

    // Merge two Config nodes, respecting both options and preferring the left-hand side
    Config &operator+=(const Config &rhs) {
        bool init = false;
        if (rhs.yaml_node == Config{}.get()) {
            init = true;
        }

        // if (log_imports != rhs.log_imports && (init || rhs.log_imports != false))
#define COMBINE_INDIVIDUAL(option_type, option_name, option_default, member_name)      \
    if (member_name != rhs.member_name && (init || rhs.member_name != option_default)) \
        member_name = rhs.member_name;

        CONFIG_INDIVIDUAL(COMBINE_INDIVIDUAL)
#undef COMBINE_INDIVIDUAL

#define COMBINE_VECTOR(option_type, option_name, option_default, member_name) \
    if (member_name != rhs.member_name && (init || !rhs.member_name.empty())) \
        vector_utils::merge_vectors(member_name, rhs.member_name);

        CONFIG_VECTOR(COMBINE_VECTOR)
#undef COMBINE_VECTOR

        check_members(rhs);
        update_yaml();
        return *this;
    }

    // Return the name of the configuration as named in the config file
    std::string operator[](const file_config &name) const {
#define SWITCH_NAMES(option_type, option_name, option_default, member_name) \
    case file_config::e_##member_name:                                      \
        return option_name;

        switch (name) {
            CONFIG_LIST(SWITCH_NAMES)

        case _INVALID:
        default: {
            return nullptr;
        }
        }
#undef SWITCH_NAMES
    }

    // Return the value of the YAML node
    // If the YAML looks outdated, call update_yaml() first, and then use this operator
    template <typename T>
    T get_from_yaml(const file_config &name) const {
        return get_member<T>(this[name]);
    }

    // Generate a YAML node based on the current values of the members.
    YAML::Node get() const {
        YAML::Node out;

#define GEN_VALUES(option_type, option_name, option_default, member_name) \
    out[option_name] = member_name;

        CONFIG_LIST(GEN_VALUES)
#undef GEN_VALUES

        return out;
    }

    // Update the YAML node based on the current values of the members
    void update_yaml() {
        yaml_node = get();
    }

    // Load a function to the node network, and then update the members
    void load_new_config(const fs::path &path) override {
        yaml_node = YAML::LoadFile(path.generic_path().string());
        update_members();
    }

    // Generate members in the preprocessor with default values
#define CREATE_MEMBERS(option_type, option_name, option_default, member_name) \
    YAML_MEMBER(option_type, member_name, option_default)

    CONFIG_LIST(CREATE_MEMBERS)
#undef GENERATE_MEMBERS
};
