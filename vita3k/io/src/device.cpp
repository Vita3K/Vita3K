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

#include <io/device.h>

#include <util/string_utils.h>

namespace device {

std::string construct_normalized_path(const VitaIoDevice dev, const std::string &path, const std::string &ext) {
    const auto device_path = get_device_string(dev, true);
    if (path.empty()) { // Wants the device only
        const auto ret = device_path + "/";
        return ret;
    }

    // Normalize the path
    auto normalized = path;
    string_utils::replace(normalized, "\\", "/");
    string_utils::replace(normalized, "/./", "/");
    string_utils::replace(normalized, "//", "/");
    // TODO: Handle dot-dot paths
    normalized.front() == '/' ? normalized = device_path + normalized : normalized = device_path + '/' + normalized;

    if (!ext.empty()) {
        if (fs::path(normalized).has_extension()) {
            const auto last_index = normalized.find_last_of('.');
            normalized = normalized.substr(0, last_index + 1) + ext;
        }
        normalized += '.' + ext;
    }

    return normalized;
}

std::string remove_device_from_path(const std::string &path, const VitaIoDevice device, const std::string &mod_path) {
    // Trim the path to include only the substring after the device string
    const auto device_length = get_device_string(device, true).length();
    if (device == VitaIoDevice::_INVALID)
        return std::string{};

    auto out = path;
    out = out.substr(device_length, out.size());
    if (!mod_path.empty())
        out.front() == '/' ? out = mod_path + out : out = mod_path + '/' + out;
    if (out.front() == '/')
        out = out.substr(1, out.length());

    return out;
}

} // namespace device
