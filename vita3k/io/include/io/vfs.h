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

#include <util/fs.h>
#include <util/types.h>

class VitaIoDevice;

namespace vfs {

struct SpaceInfo {
    unsigned long long max_capacity;
    unsigned long long used;
    unsigned long long free;
};

using FileBuffer = std::vector<SceUInt8>;

bool read_file(VitaIoDevice device, FileBuffer &buf, const std::wstring &pref_path, const fs::path &vfs_file_path);
bool read_app_file(FileBuffer &buf, const std::wstring &pref_path, const std::string &app_path, const fs::path &vfs_file_path);
SpaceInfo get_space_info(const VitaIoDevice device, const std::string &vfs_path, const std::wstring &pref_path);
} // namespace vfs
