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

#include <util/vector_utils.h>

#include <boost/optional.hpp>

using boost::optional;

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
    member_name = get_member<option_type>(option_name);

        CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
    }

    // Perform comparisons with optional settings
    void check_members(const Config &rhs) {
        if (rhs.vpk_path.is_initialized())
            vpk_path = rhs.vpk_path;
        if (rhs.run_title_id.is_initialized())
            run_title_id = rhs.run_title_id;
        if (rhs.recompile_shader_path.is_initialized())
            run_title_id = rhs.run_title_id;

        if (!rhs.config_path.empty())
            config_path = rhs.config_path;

        overwrite_config = rhs.overwrite_config;
        load_config = rhs.load_config;
    }

public:
    // Optional config settings
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<std::string> recompile_shader_path;

    // Setting not present in the YAML file
    fs::path config_path = {};
    bool overwrite_config = true;
    bool load_config = false;

    Config() {
        update_yaml();
    }

    ~Config() = default;

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
