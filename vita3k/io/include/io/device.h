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

#include <io/VitaIoDevice.h>

#include <util/fs.h>

#include <string>

namespace device {

/**
  * \brief Get a Vita device from a given path.
  * \param path The input path to be tested.
  * \return The path's root as an enumeration. Otherwise, VitaIoDevice::_INVALID.
  */
inline VitaIoDevice get_device(const std::string &path) {
    if (path.empty())
        return VitaIoDevice::_INVALID;

    const auto colon = path.find_first_of(':');
    if (colon == std::string::npos)
        return VitaIoDevice::_INVALID;

    const auto p = path.substr(0, colon);
    if (VitaIoDevice::_is_valid_nocase(p.c_str()))
        return VitaIoDevice::_from_string(p.c_str());

    return VitaIoDevice::_INVALID;
}

/**
  * \brief Get a valid Vita device as a string.
  * \param dev The input Vita device needed.
  * \param with_colon Output the string appended with a colon (default: false)
  * \return A string version of the Vita device.
  */
inline std::string get_device_string(const VitaIoDevice dev, const bool with_colon = false) {
    return with_colon ? std::string(dev._to_string()).append(":") : dev._to_string();
}

/**
  * \brief Check if the device is a valid output path.
  * \param device Input device to be checked.
  * \return True if valid, False otherwise.
  */
inline bool is_valid_output_path(const VitaIoDevice device) {
    return !(device == VitaIoDevice::savedata0 || device == VitaIoDevice::savedata1 || device == VitaIoDevice::app0
        || device == VitaIoDevice::_INVALID || device == VitaIoDevice::addcont0 || device == VitaIoDevice::tty0
        || device == VitaIoDevice::tty1);
}

/**
  * \brief Check if the device string is valid.
  * \param device Input device to be checked.
  * \return True if valid, False otherwise.
  */
inline bool is_valid_output_path(const std::string &device) {
    return !(device == (+VitaIoDevice::savedata0)._to_string() || device == (+VitaIoDevice::savedata1)._to_string() || device == (+VitaIoDevice::app0)._to_string()
        || device == (+VitaIoDevice::_INVALID)._to_string() || device == (+VitaIoDevice::addcont0)._to_string() || device == (+VitaIoDevice::tty0)._to_string()
        || device == (+VitaIoDevice::tty1)._to_string());
}

/**
  * \brief Construct a normalized path (optionally with an extension) to be outputted onto the Vita.
  * \param dev The input Vita device.
  * \param path The Vita location needed.
  * \param ext The extension of the file (optional).
  * \return An std::string of a real Vita translated path.
  */
std::string construct_normalized_path(VitaIoDevice dev, const std::string &path, const std::string &ext = "");

/**
  * \brief Remove a device string from the path, and optionally prepend it with a different string.
  * \param path Input path to be modified.
  * \param device The device to be removed.
  * \param mod_path The new path to prepend the path (optional).
  * \return The string without the device, normalized.
  */
std::string remove_device_from_path(const std::string &path, VitaIoDevice device, const std::string &mod_path = "");

/**
  * \brief Used for removing an extra device from a path. (Note: Not intended to be a permanent solution.)
  * \param path The input Vita path.
  * \param device The device to be removed.
  * \return New path without the duplicate device.
  */
inline std::string remove_duplicate_device(const std::string &path, VitaIoDevice &device) {
    auto cur_path = remove_device_from_path(path, device);
    if (get_device(cur_path) != VitaIoDevice::_INVALID) {
        device = get_device(cur_path);
        return cur_path;
    }

    return path;
}

/**
  * \brief Construct the emulated Vita path (optionally with an extension).
  * \param dev The input Vita device used.
  * \param path The path of the file.
  * \param base_path The main output for the file.
  * \param ext The extension of the file (optional).
  * \return A complete Boost.Filesystem path normalized.
  */
inline fs::path construct_emulated_path(const VitaIoDevice dev, const fs::path &path, const std::string &base_path, const std::string &ext = "") {
    return fs_utils::construct_file_name(base_path, get_device_string(dev, false), path, ext);
}
} // namespace device
