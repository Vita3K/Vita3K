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

/**
 * @file filesystem.hpp
 * @brief Filesystem-related dialogs
 *
 * This file contains all the declarations related to the implementation and
 * abstraction of user interface dialogs from the host operating system
 * related to filesystem interaction such as file or folder opening dialogs.
 */

#pragma once

#include <util/fs.h>

#include <string>
#include <vector>

#ifdef ERROR
#undef ERROR
#endif

namespace host::dialog::filesystem {

/**
 * @brief Result IDs for filesystem-related dialogs
 */
enum Result {
    /**
     * @brief An error has occurred
     */
    ERROR,

    /**
     * @brief Successful interaction with user
     */
    SUCCESS,

    /**
     * @brief User has cancelled file/folder selection
     */
    CANCEL,
};

/**
 * @brief File filter specification for native file browser file-picking dialogs
 *
 * A file filter represents an option in the "file type" menu that OS
 * file explorers usually have when selecting files to only show files of a certain
 * type and help the user narrow down their selection.
 *
 * On macOS however, the display name is ignored and only the file extensions are used.
 */
struct FileFilter {
    /**
     * @brief Human-friendly name for the file type represented by the filter
     * (ex. `"Text document"`, `"PNG image file"`...)
     */
    std::string display_name = "";

    /**
     * @brief List of possible file extension(s) for the file type represented by the filter
     * without the `.` prefix
     *
     * Example: `{"cpp", "cc", ...}`
     */
    std::vector<std::string> file_extensions = {};
};

/**
 * @brief Open a native file browser dialog to request a file path from the user
 *
 * @param resulting_path A reference to a path variable to use for storing the path of the file chosen by the user
 * @param file_filters A list with all the different file filters the user might want to use
 * @param default_path Path to the folder the file browser dialog should show first when opened
 * @return Result code of the operation as specified in `host::dialog::filesystem::Result`
 */
Result open_file(fs::path &resulting_path, const std::vector<FileFilter> &file_filters = {}, const fs::path &default_path = "");

/**
 * @brief Open a native file browser dialog to request a directory path from the user
 *
 * @param resulting_path A reference to a path variable to use for storing the path of the file chosen by the user
 * @param default_path Path to the folder the file browser dialog should show first when opened
 * @return Result code of the operation as specified in `host::dialog::filesystem::Result`
 */
Result pick_folder(fs::path &resulting_path, const fs::path &default_path = "");

/**
 * @brief Get a string describing the last dialog error
 *
 * @return A string in English describing the last dialog error
 */
std::string get_error();

} // namespace host::dialog::filesystem
