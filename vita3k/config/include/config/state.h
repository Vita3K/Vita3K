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

// Configuration File options
struct Config : YamlLoader {
private:
    // Privately update members
    // Use this function when the YAML file is updated before the members.
    void update_members() {
#define UPDATE_MEMBERS(option_type, option_name, option_default) \
    option_name = get_member<option_type>(#option_name);
        CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
    }

    // Perform comparisons with optional settings, and then update the members
    void check_members(const Config &rhs) {
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

public:
    // Optional config settings
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<std::string> recompile_shader_path;

    // Setting not present in the YAML file
    fs::path config_path = {};
    bool overwrite_config = true;
    bool load_config = false;

    Config &operator=(const Config &rhs) {
        if (this != &rhs) {
            yaml_node = rhs.get();

            check_members(rhs);
        }
        return *this;
    }

    Config &operator=(Config &&rhs) noexcept {
        yaml_node = rhs.get();

        check_members(rhs);

        return *this;
    }

    // Merge two Config nodes, respecting both options and preferring the left-hand side
    Config &operator+=(const Config &rhs) {
        bool init = false;
        Config def{};
        if (rhs.yaml_node == def.get()) {
            init = true;
        }

        // if(log_imports != rhs.log_imports && (!init || rhs.log_imports != false))
#define COMBINE_INDIVIDUAL(option_type, option_name, option_default)                    \
    if (option_name != rhs.option_name && (!init || rhs.option_name != option_default)) \
        option_name = rhs.option_name;

        CONFIG_INDIVIDUAL(COMBINE_INDIVIDUAL)
#undef COMBINE_INDIVIDUAL

#define COMBINE_VECTOR(option_type, option_name, option_default)               \
    if (option_name != rhs.option_name && (rhs.option_name != option_default)) \
        vector_utils::merge_vectors(option_name, rhs.option_name);

        CONFIG_VECTOR(COMBINE_VECTOR)
#undef COMBINE_VECTOR

        update_yaml();
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

    // Update the YAML node based on the current values of the members
    void update_yaml() {
        yaml_node = get();
    }

    // Load a function to the node network, and then update the members
    void load_new_config(const fs::path &path) {
        yaml_node = YAML::LoadFile(path.generic_path().string());
        update_members();
    }

    // Generate members in the preprocessor
    CONFIG_LIST(YAML_MEMBER)
};
