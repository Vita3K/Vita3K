// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace renderer::vulkan {

template <typename T>
constexpr bool triangle_fan_fits(size_t count, size_t capacity_bytes) {
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>);
    return count >= 3 && count - 2 <= capacity_bytes / (3 * sizeof(T));
}

// For Vulkan's default first provoking vertex, with primitive restart disabled.
// Keep the same cyclic winding and flat-shaded vertex as MoltenVK's conversion.
template <typename T>
std::vector<T> triangle_fan_to_list(const T *indices, size_t count) {
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>);
    if (count < 3)
        return {};

    const size_t triangles = count - 2;
    if (!triangle_fan_fits<T>(count, std::numeric_limits<uint32_t>::max()))
        throw std::length_error("Triangle fan expansion exceeds index buffer limits");

    std::vector<T> result(triangles * 3);
    for (size_t triangle = 0; triangle < triangles; ++triangle) {
        result[triangle * 3] = indices[triangle + 1];
        result[triangle * 3 + 1] = indices[triangle + 2];
        result[triangle * 3 + 2] = indices[0];
    }
    return result;
}

} // namespace renderer::vulkan
