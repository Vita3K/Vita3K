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

#include <algorithm>

#include <io/device.h>

namespace device {

std::string construct_normalized_path(const VitaIoDevice dev, const std::string &path, const std::string &ext) {
    const auto device_path = get_device_string(dev, true);
    if (path.empty()) { // Wants the device only
        auto ret = device_path + "/";
        return ret;
    }

    // Normalize the path
    auto normalized = path;
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
        !out.empty() && out.front() == '/' ? out = mod_path + out : out = mod_path + '/' + out;
    if (!out.empty() && out.front() == '/')
        out = out.substr(1, out.length());

    return out;
}

VitaIoDevice get_device(const std::string &path) {
    if (path.empty())
        return VitaIoDevice::_INVALID;

    const auto colon = path.find_first_of(':');
    if (colon == std::string::npos)
        return VitaIoDevice::_INVALID;

    auto p = path.substr(0, colon);
    std::transform(p.begin(), p.end(), p.begin(), tolower);
    if (VitaIoDevice::_is_valid_nocase(p.c_str()))
        return VitaIoDevice::_from_string(p.c_str());

    return VitaIoDevice::_INVALID;
}

std::string get_device_string(const VitaIoDevice dev, const bool with_colon) {
    return with_colon ? std::string(dev._to_string()).append(":") : dev._to_string();
}

bool is_valid_output_path(const VitaIoDevice device) {
    return !(device == VitaIoDevice::savedata0 || device == VitaIoDevice::savedata1 || device == VitaIoDevice::app0
        || device == VitaIoDevice::_INVALID || device == VitaIoDevice::addcont0
        || device == VitaIoDevice::tty0 || device == VitaIoDevice::tty1
        || device == VitaIoDevice::tty2 || device == VitaIoDevice::tty3
        || device == VitaIoDevice::cache0 || device == VitaIoDevice::music0
        || device == VitaIoDevice::photo0 || device == VitaIoDevice::video0);
}

bool is_valid_output_path(const std::string &device) {
    return !(device == (+VitaIoDevice::savedata0)._to_string() || device == (+VitaIoDevice::savedata1)._to_string() || device == (+VitaIoDevice::app0)._to_string()
        || device == (+VitaIoDevice::_INVALID)._to_string() || device == (+VitaIoDevice::addcont0)._to_string()
        || device == (+VitaIoDevice::tty0)._to_string() || device == (+VitaIoDevice::tty1)._to_string()
        || device == (+VitaIoDevice::tty2)._to_string() || device == (+VitaIoDevice::tty3)._to_string()
        || device == (+VitaIoDevice::cache0)._to_string() || device == (+VitaIoDevice::music0)._to_string()
        || device == (+VitaIoDevice::photo0)._to_string() || device == (+VitaIoDevice::video0)._to_string());
}

std::string remove_duplicate_device(const std::string &path, VitaIoDevice &device) {
    auto cur_path = remove_device_from_path(path, device);
    if (get_device(cur_path) != VitaIoDevice::_INVALID) {
        device = get_device(cur_path);
        if (cur_path.find_first_of(':') != std::string::npos)
            cur_path = remove_duplicate_device(cur_path, device);
        return cur_path;
    }

    return path;
}

fs::path construct_emulated_path(const VitaIoDevice dev, const fs::path &path, const fs::path &base_path, const bool redirect_pwd, const std::string &ext) {
    if (redirect_pwd && dev == +VitaIoDevice::host0) {
        return fs::current_path() / path;
    }
    return fs_utils::construct_file_name(base_path, get_device_string(dev, false), path, ext);
}

} // namespace device
