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

#ifdef __APPLE__

#include <vulkan/vulkan_core.h>

#include <string>

namespace vkutil::apple {

struct VulkanLibrary {
    // path (or leaf name) of the library that was loaded
    std::string path;
    // true when this is the Khronos loader, false when MoltenVK was loaded directly
    bool is_loader = false;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
};

// Loads the Vulkan implementation once and returns it. Every candidate is probed by creating
// an instance and checking that a physical device exists, so a Khronos loader without any
// registered ICD is skipped instead of making instance creation fail later.
// When prefer_loader is true the Khronos loader is tried before the bundled MoltenVK (needed for
// validation layers), otherwise MoltenVK is loaded directly.
// The library is loaded with global symbol visibility so that SDL picks the same implementation.
// Returns nullptr when nothing usable was found.
const VulkanLibrary *load_vulkan_library(bool prefer_loader);

} // namespace vkutil::apple

#endif
