// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/vulkan/triangle_fan.h>

#include <array>
#include <gtest/gtest.h>

using renderer::vulkan::triangle_fan_to_list;
using renderer::vulkan::triangle_fan_fits;

TEST(triangle_fan, falls_back_when_expansion_exceeds_ring_capacity) {
    constexpr size_t capacity = 64 * 1024 * 1024;
    EXPECT_TRUE(triangle_fan_fits<uint16_t>(capacity / 6 + 2, capacity));
    EXPECT_FALSE(triangle_fan_fits<uint16_t>(capacity / 6 + 3, capacity));
    EXPECT_TRUE(triangle_fan_fits<uint32_t>(capacity / 12 + 2, capacity));
    EXPECT_FALSE(triangle_fan_fits<uint32_t>(capacity / 12 + 3, capacity));
    EXPECT_FALSE(triangle_fan_fits<uint32_t>(std::numeric_limits<size_t>::max(), capacity));
    EXPECT_FALSE(triangle_fan_fits<uint16_t>(2, capacity));
    EXPECT_FALSE(triangle_fan_fits<uint16_t>(3, 5));
    EXPECT_TRUE(triangle_fan_fits<uint16_t>(3, 6));
}

TEST(triangle_fan, incomplete_primitives_do_not_read_indices) {
    for (size_t count = 0; count < 3; ++count) {
        EXPECT_TRUE(triangle_fan_to_list<uint16_t>(nullptr, count).empty());
        EXPECT_TRUE(triangle_fan_to_list<uint32_t>(nullptr, count).empty());
    }
}

TEST(triangle_fan, preserves_winding_and_first_provoking_vertex) {
    const std::array<uint16_t, 5> input{ 9, 7, 3, 11, 5 };
    // Each fan triangle starts with its outer vertex for flat interpolation.
    const std::vector<uint16_t> expected{ 7, 3, 9, 3, 11, 9, 11, 5, 9 };
    EXPECT_EQ(triangle_fan_to_list(input.data(), input.size()), expected);
    EXPECT_EQ(input, (std::array<uint16_t, 5>{ 9, 7, 3, 11, 5 }));
}

TEST(triangle_fan, keeps_full_width_indices_and_degenerate_triangles) {
    const std::array<uint32_t, 5> input{ 0x12345678, 70000, 70000, 0xffffffff, 0 };
    const std::vector<uint32_t> expected{ 70000, 70000, 0x12345678,
        70000, 0xffffffff, 0x12345678, 0xffffffff, 0, 0x12345678 };
    EXPECT_EQ(triangle_fan_to_list(input.data(), input.size()), expected);
}

TEST(triangle_fan, max_u16_is_a_vertex_when_restart_is_disabled) {
    const std::array<uint16_t, 3> input{ 0, 65535, 2 };
    EXPECT_EQ(triangle_fan_to_list(input.data(), input.size()),
        (std::vector<uint16_t>{ 65535, 2, 0 }));
}

TEST(triangle_fan, rejects_byte_count_overflow_before_reading_indices) {
    const size_t max_bytes = std::numeric_limits<uint32_t>::max();
    EXPECT_THROW(triangle_fan_to_list<uint16_t>(nullptr, max_bytes / 6 + 3), std::length_error);
    EXPECT_THROW(triangle_fan_to_list<uint32_t>(nullptr, max_bytes / 12 + 3), std::length_error);
    EXPECT_THROW(triangle_fan_to_list<uint32_t>(nullptr, std::numeric_limits<size_t>::max()), std::length_error);
}
