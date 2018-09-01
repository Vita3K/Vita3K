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

#define SCE_ERROR_ERRNO_EBADF 0x80010051 // Invalid descriptor
#define SCE_ERROR_ERRNO_ENOENT 0x80010002 // File doesn't exist

#define JOIN_DEVICE(p) VitaIoDevice::p
#define DEVICE(path, name) name,
enum class VitaIoDevice {
#include <io/VitaIoDevice.def>

    _UKNONWN = -1,
    _INVALID = -2,
};
#undef DEVICE

namespace vfs {
using FileBuffer = std::vector<uint8_t>;

constexpr const char *get_device_string(VitaIoDevice dev, bool with_colon = false);
bool read_file(VitaIoDevice device, FileBuffer &buf, const std::string &pref_path, const std::string &file_path);
bool read_app_file(FileBuffer &buf, const std::string &pref_path, const std::string title_id, const std::string &file_path);
} // namespace vfs
