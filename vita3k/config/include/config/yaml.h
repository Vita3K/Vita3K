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

#include <yaml-cpp/yaml.h>

#include <util/fs.h>

// YAML Interface Struct
// Creates a new YAML Node network.
struct YamlLoader {
    // YAML::Node containing all configurations from file
    YAML::Node yaml_node;

    // Generate a new YAML node
    YamlLoader() {
        yaml_node = YAML::Node{};
    }

    explicit YamlLoader(const YAML::Node &node) {
        yaml_node = node;
    }

    explicit YamlLoader(const fs::path &path) {
        yaml_node = YAML::LoadFile(path.generic_string());
    }

    virtual ~YamlLoader() = default;

    YamlLoader &operator=(const YamlLoader &rhs) {
        if (this != &rhs) {
            yaml_node = rhs.yaml_node;
        }
        return *this;
    }

    YamlLoader &operator=(YamlLoader &&rhs) noexcept {
        yaml_node = rhs.yaml_node;
        return *this;
    }

    virtual void load_new_config(const fs::path &path) {
        yaml_node = YAML::LoadFile(path.generic_string());
    }

    // Check if a node index exists, and return the current value in the node network
    template <typename T>
    T get_member(const std::string &name) {
        assert(!name.empty()); // Make sure the option is actually not empty
        assert(yaml_node[name.c_str()].IsDefined()); // Ensure the option can be accessed!
        return yaml_node[name.c_str()].as<T>();
    }

// Generate a member list based on type, name and default values
// Based on https://softwareengineering.stackexchange.com/a/196810
#define YAML_MEMBER(option_type, option_name, option_default) \
    option_type option_name = option_default;
};
