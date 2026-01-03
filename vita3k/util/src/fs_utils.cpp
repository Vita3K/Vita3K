// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <util/fs.h>
#include <util/string_utils.h>

#include <SDL3/SDL_iostream.h>

namespace fs_utils {

fs::path construct_file_name(const fs::path &base_path, const fs::path &folder_path, const fs::path &file_name, const fs::path &extension) {
    fs::path full_file_path{ base_path / folder_path / file_name };
    if (!extension.empty())
        full_file_path.replace_extension(extension);

    return full_file_path.generic_path();
}

std::string path_to_utf8(const fs::path &path) {
    if constexpr (sizeof(fs::path::value_type) == sizeof(wchar_t)) {
        return string_utils::wide_to_utf(path.wstring());
    } else {
        return path.string();
    }
}

fs::path utf8_to_path(const std::string &str) {
    if constexpr (sizeof(fs::path::value_type) == sizeof(wchar_t)) {
        return fs::path{ string_utils::utf_to_wide(str) };
    } else {
        return fs::path{ str };
    }
}

fs::path path_concat(const fs::path &path1, const fs::path &path2) {
    return fs::path{ path1.native() + path2.native() };
}

void dump_data(const fs::path &path, const void *data, const std::streamsize size) {
    fs::ofstream of{ path, fs::ofstream::binary };
    if (!of.fail()) {
        of.write(static_cast<const char *>(data), size);
        of.close();
    }
}

FILE *open_file_handle_from_path(const fs::path &path) {
    FILE *result;
#ifdef _WIN32
    _wfopen_s(&result, path.c_str(), L"rb");
#else
    result = fopen(path.c_str(), "rb");
#endif
    return result;
}

template <typename T>
static bool read_data(const fs::path &path, std::vector<T> &data) {
    data.clear();
    SDL_IOStream *file = SDL_IOFromFile(fs_utils::path_to_utf8(path).c_str(), "rb");
    if (!file) {
        return false;
    }

    // Get the size of the file
    const Sint64 size = SDL_GetIOSize(file);
    if (size <= 0) {
        SDL_CloseIO(file);
        return false;
    }

    // Resize the vector to fit the file content
    data.resize(size);

    // Read the content of the file
    if (SDL_ReadIO(file, data.data(), size) != size) {
        SDL_CloseIO(file);
        data.clear();
        return false;
    }

    SDL_CloseIO(file);
    return true;
}

bool read_data(const fs::path &path, std::vector<uint8_t> &data) { return read_data<uint8_t>(path, data); }
bool read_data(const fs::path &path, std::vector<int8_t> &data) { return read_data<int8_t>(path, data); }
bool read_data(const fs::path &path, std::vector<char> &data) { return read_data<char>(path, data); }

} // namespace fs_utils
