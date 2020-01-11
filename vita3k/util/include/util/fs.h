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

#include <util/preprocessor.h>

#ifdef COMPILE_MODERN_CPP
#if VITA3K_CPP17
#include <filesystem>
namespace fs = std::filesystem;
#elif VITA3K_CPP14
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#else
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

class Root {
    fs::path base_path;
    fs::path pref_path;

public:
    void set_base_path(const fs::path &p) {
        base_path = p;
    }

    fs::path get_base_path() const {
        return base_path;
    }

    std::string get_base_path_string() const {
        return base_path.generic_string();
    }

    void set_pref_path(const fs::path &p) {
        pref_path = p;
    }

    fs::path get_pref_path() const {
        return pref_path;
    }

    std::string get_pref_path_string() const {
        return pref_path.generic_string();
    }
}; // class root

namespace fs_utils {

/**
  * \brief  Construct a file name (optionally with an extension) to be placed in a Vita3K directory.
  * \param  base_path   The main output path for the file.
  * \param  folder_path The sub-directory/sub-directories to output to.
  * \param  file_name   The name of the file.
  * \param  extension   The extension of the file (optional)
  * \return A complete std::filesystem file path normalized.
  */
inline fs::path construct_file_name(const fs::path &base_path, const fs::path &folder_path, const fs::path &file_name, const fs::path &extension = "") {
    fs::path full_file_path{ base_path / folder_path / file_name };
    if (!extension.empty())
        full_file_path.replace_extension(extension);

    return full_file_path;
}

} // namespace fs_utils
