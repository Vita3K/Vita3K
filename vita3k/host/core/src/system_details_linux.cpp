// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

/**
 * @file system_details_linux.cpp
 * @brief This file includes the code related to the retrieval of system
 * details in GNU/Linux.
 *
 * The code is built upon the Freedesktop standard:
 * - XDG Base Directory Specification v0.8
 */

#include <host/core/system_details.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

const std::string PROGRAM_NAME = "Vita3K";

/**
 * @brief Get the path for a XDG base directory
 *
 * This function is not compatible with environment variables that contain
 * directory lists like `XDG_DATA_DIRS`.
 *
 * @param env_var The name of the environment variable that should contain the
 * absolute directory path
 * @param default_path Default path to return if the variable isn't set
 * @return std::filesystem::path The resulting directory path
 */
std::filesystem::path get_xdg_base_directory(std::string env_var, std::filesystem::path default_path) {
    std::filesystem::path result = "";

    // Check if environment variable is defined
    if ((std::getenv(env_var.c_str()) == nullptr)) {
        // Resolve to default directory if env var isn't defined
        result = default_path;
    } else {
        // If env var is defined then get the value
        std::filesystem::path retrieved_path = "";
        retrieved_path.assign(std::getenv(env_var.c_str()));

        // If env var is defined check if the value is empty
        if (std::strcmp(result.c_str(), "") != 0) {
            // Resolve to default directory if env var value is empty
            result = default_path;
        } else {
            // If the value isn't empty check if directory path is relative
            if (retrieved_path.is_relative()) {
                // Per the XDG Base Directory Specification, relative paths aren't
                // allowed and should be considered invalid
                // Resolve to default path if path is relative
                result = default_path;
            } else {
                result = retrieved_path;
            }
        }
    }

    return result;
};

namespace host {
HostInfo::HostInfo() {
    /* --- Initialize paths --- */

    // User directory
    this->paths.user_dir.assign(std::getenv("HOME"));

    // Data directory
    this->paths.data_dir = get_xdg_base_directory("XDG_DATA_HOME", this->paths.user_dir / "/.local/share/") / PROGRAM_NAME;

    // Config directory
    this->paths.config_dir = get_xdg_base_directory("XDG_CONFIG_HOME", this->paths.user_dir / "/.config/") / PROGRAM_NAME;

    // State directory
    this->paths.state_dir = get_xdg_base_directory("XDG_STATE_HOME", this->paths.user_dir / "/.local/state/") / PROGRAM_NAME;

    // Cache directory
    this->paths.cache_dir = get_xdg_base_directory("XDG_CACHE_HOME", this->paths.user_dir / "/.cache/") / PROGRAM_NAME;

    //
}
}; // namespace host
