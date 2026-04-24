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

#pragma once

#include <filesystem>
#include <string>

// Define constants known at compile-time
#ifdef _WIN32

#define HOST_PLATFORM_NAME "Windows"

#elif __APPLE__ && __MACH__

#define HOST_PLATFORM_NAME "macOS"

#elif __linux__

#define HOST_PLATFORM_NAME "Linux"

#elif __linux__ && __flatpak__

#define HOST_PLATFORM_NAME "Linux (Flatpak)"

#endif

namespace host {

struct HostPaths {
    /**
     * @brief User home directory
     */
    std::filesystem::path user_dir = "";

    /**
     * @brief User-specific data files directory.
     *
     * The directory path already points at the program-specific folder,
     * so appending the program name to the path isn't needed.
     */
    std::filesystem::path data_dir = "";

    /**
     * @brief User-specific configuration files directory
     *
     * The directory path already points at the program-specific folder,
     * so appending the program name to the path isn't needed.
     */
    std::filesystem::path config_dir = "";

    /**
     * @brief User-specific state files directory
     *
     * Contains state files that should persist between (application) restarts
     * but that are not portable or important enough to the user to be stored
     * in the data directory.
     *
     * In this directory should go things that record either actions
     * (ex. logs, history, recently used files...) or details about the state of
     * the application that can be used in later execution sessions
     * (ex. last opened file before closure, current view, undo history, current
     * window size...).
     *
     * The directory path already points at the program-specific folder,
     * so appending the program name to the path isn't needed.
     */
    std::filesystem::path state_dir = "";

    /**
     * @brief User-specific cache files directory
     *
     * The directory path already points at the program-specific folder,
     * so appending the program name to the path isn't needed.
     */
    std::filesystem::path cache_dir = "";
};

/**
 * @brief Host operating system abstraction class
 *
 * An object of this class is meant to include useful information about the
 * host running environment to use during runtime.
 */
class HostInfo {
public:
    /**
     * @brief Name of the operating system
     */
    const std::string name = HOST_PLATFORM_NAME;

    /**
     * @brief Useful paths of the host filesystem
     */
    HostPaths paths;

    HostInfo();
};
}; // namespace host
