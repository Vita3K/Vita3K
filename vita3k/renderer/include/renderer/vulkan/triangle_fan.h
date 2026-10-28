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

template <typename T>
constexpr bool triangle_fan_fits_without_wrap(size_t count, size_t capacity_bytes, size_t cursor_bytes, uint64_t wrap_generation, uint64_t retired_wrap_generation) {
    // A wrap remains unsafe until the submission containing it has completed.
    return wrap_generation <= retired_wrap_generation && cursor_bytes <= capacity_bytes && triangle_fan_fits<T>(count, capacity_bytes - cursor_bytes);
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
