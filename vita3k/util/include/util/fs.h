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

#pragma once

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>

#ifdef _WIN32
#define fseek _fseeki64
#define ftell _ftelli64
#endif

namespace fs = boost::filesystem;

#ifdef _WIN32
#define FOPEN(filename, params) _wfopen(filename, L##params)
#else
#define FOPEN(filename, params) fopen(filename, params)
#endif

namespace fs_utils {

/**
 * \brief  Construct a file name (optionally with an extension) to be placed in a Vita3K directory.
 * \param  base_path   The main output path for the file.
 * \param  folder_path The sub-directory/sub-directories to output to.
 * \param  file_name   The name of the file.
 * \param  extension   The extension of the file (optional)
 * \return A complete Boost.Filesystem file path normalized.
 */
fs::path construct_file_name(const fs::path &base_path, const fs::path &folder_path, const fs::path &file_name, const fs::path &extension = "");

/**
 * \brief Convert a Boost.Filesystem path to a UTF-8 string.
 * \param path path to convert
 * \return UTF-8 string
 */
std::string path_to_utf8(const fs::path &path);

/**
 * \brief Convert a UTF-8 string to a Boost.Filesystem path.
 * \param str UTF-8 string to convert
 * \return Boost.Filesystem path
 */
fs::path utf8_to_path(const std::string &str);

/**
 * \brief Concatenate two Boost.Filesystem paths. Usable to add extension to path.
 * \param path1 First path
 * \param path2 Second path
 * \return First path + second path
 */
fs::path path_concat(const fs::path &path1, const fs::path &path2);

/**
 * \brief Store data buffer into file.
 * \param path full file name to store data
 * \param data pointer to data buffer
 * \param size size of data buffer in bytes
 */
void dump_data(const fs::path &path, const void *data, const std::streamsize size);

FILE *open_file_handle_from_path(const fs::path &path);

bool read_data(const fs::path &path, std::vector<uint8_t> &data);
bool read_data(const fs::path &path, std::vector<int8_t> &data);
bool read_data(const fs::path &path, std::vector<char> &data);

} // namespace fs_utils

template <>
struct fmt::formatter<boost::filesystem::path> : ostream_formatter {};

class Root {
    fs::path base_path;
    fs::path pref_path;
    fs::path patch_path;
    fs::path log_path;
    fs::path config_path;
    fs::path shared_path;
    fs::path cache_path;
    fs::path static_assets_path;

public:
    void set_base_path(const fs::path &p) {
        base_path = p;
    }
    fs::path get_base_path() const {
        return base_path;
    }

    void set_pref_path(const fs::path &p) {
        pref_path = p;
    }
    fs::path get_pref_path() const {
        return pref_path;
    }

    void set_patch_path(const fs::path &p) {
        patch_path = p;
    }
    fs::path get_patch_path() const {
        return patch_path;
    }

    void set_log_path(const fs::path &p) {
        log_path = p;
    }
    fs::path get_log_path() const {
        return log_path;
    }

    void set_config_path(const fs::path &p) {
        config_path = p;
    }
    fs::path get_config_path() const {
        return config_path;
    }

    void set_shared_path(const fs::path &p) {
        shared_path = p;
    }
    fs::path get_shared_path() const {
        return shared_path;
    }

    void set_cache_path(const fs::path &p) {
        cache_path = p;
    }
    fs::path get_cache_path() const {
        return cache_path;
    }
    void set_static_assets_path(const fs::path &p) {
        static_assets_path = p;
    }
    fs::path get_static_assets_path() const {
        return static_assets_path;
    }
}; // class root
