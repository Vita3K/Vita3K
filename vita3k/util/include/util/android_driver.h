// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <optional>
#include <string>
#include <string_view>
#include <vulkan/vulkan.h>

namespace android_driver {

bool is_safe_driver_relative_path(const fs::path &relative_path);
std::optional<std::string> read_driver_library_name_from_meta(const fs::path &meta_path);
std::optional<std::string> resolve_custom_driver_library_name(const fs::path &driver_path);
bool is_custom_driver_loaded(const std::string &driver_name, uint32_t vendor_id, uint32_t driver_version, std::string_view device_name);
PFN_vkGetInstanceProcAddr resolve_vk_get_instance_proc_addr(const std::string &driver_name);

} // namespace android_driver
