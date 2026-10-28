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

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace renderer::vulkan {

// Expand a triangle fan into an equivalent list of triangles.
// For a fan [c, r1, r2, r3, ...] we emit: (r1,r2,c), (r2,r3,c), ...
// Note how the two rim vertices come first and the center last.
// Ordering matters due to winding (unwrapping the fan and ensuring faces are culled) and flat shading (how triangles get their colors)
// This attempts to reproduce the way MoltenVK does it with no primitive restart (also something metal is finicky about)
template <typename T>
std::vector<T> triangle_fan_to_list(const T *indices, size_t count) {
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>);
    if (count < 3)
        return {};

    const size_t triangles = count - 2;
    std::vector<T> result(triangles * 3);
    for (size_t triangle = 0; triangle < triangles; ++triangle) {
        result[triangle * 3] = indices[triangle + 1]; // rim vertex i+1 goes first
        result[triangle * 3 + 1] = indices[triangle + 2];
        result[triangle * 3 + 2] = indices[0];
    }
    return result;
}

} // namespace renderer::vulkan
