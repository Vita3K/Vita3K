// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/vulkan/triangle_fan.h>

#include <array>
#include <gtest/gtest.h>

using renderer::vulkan::triangle_fan_to_list;
using renderer::vulkan::triangle_fan_fits;
using renderer::vulkan::triangle_fan_fits_without_wrap;

TEST(triangle_fan, expands_only_when_upload_does_not_wrap) {
    constexpr size_t capacity = 64 * 1024 * 1024;
    constexpr size_t count = 4'000'000;
    constexpr size_t expanded_bytes = (count - 2) * 3 * sizeof(uint16_t);
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, 0, 0, 0));
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, expanded_bytes, 0, 0));
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, expanded_bytes * 2, 0, 0));
    EXPECT_TRUE(count * sizeof(uint16_t) <= capacity - expanded_bytes * 2);
    // The index ring uses four-byte alignment; a six-byte upload rounds up to eight.
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(3, capacity, capacity - 8, 0, 0));
    EXPECT_EQ((capacity - 8 + 6 + 3) & ~size_t(3), capacity);
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(3, capacity, capacity - 4, 0, 0));
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint32_t>(3, capacity, capacity - 12, 0, 0));
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint32_t>(3, capacity, capacity, 0, 0));
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(3, capacity, capacity + 1, 0, 0));
}

TEST(triangle_fan, native_wrap_blocks_later_expansion_into_live_indices) {
    constexpr size_t capacity = 64 * 1024 * 1024;
    constexpr size_t first_cursor = 48 * 1024 * 1024;
    constexpr size_t first_native_bytes = 8 * 1024 * 1024;
    constexpr size_t second_count = 5 * 1024 * 1024;
    constexpr size_t second_native_bytes = second_count * sizeof(uint16_t);
    constexpr size_t third_count = 8 * 1024 * 1024 + 2;
    constexpr size_t third_expanded_bytes = (third_count - 2) * 3 * sizeof(uint16_t);
    constexpr size_t second_cursor = first_cursor + first_native_bytes;
    constexpr size_t third_cursor = second_native_bytes;

    EXPECT_EQ(second_cursor, 56 * 1024 * 1024);
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(second_count, capacity, second_cursor, 0, 0));
    EXPECT_TRUE(second_cursor + second_native_bytes > capacity); // Native upload wraps to zero.
    EXPECT_EQ(third_expanded_bytes, 48 * 1024 * 1024);
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(third_count, capacity, third_cursor, 0, 0));
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(third_count, capacity, third_cursor, 1, 0));
    // Submission of the wrapped upload retires generation 1 after its fence wait.
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(third_count, capacity, third_cursor, 1, 1));
}

TEST(triangle_fan, newer_and_unsubmitted_wraps_wait_for_their_own_fence) {
    constexpr size_t capacity = 64 * 1024 * 1024;
    constexpr size_t cursor = 8 * 1024 * 1024;
    constexpr size_t count = 4;
    // The first submission is in flight; a second, unsubmitted recording wraps.
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, cursor, 2, 0));
    // Waiting on the older fence cannot retire the newer wrap.
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, cursor, 2, 1));
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, cursor, 2, 2));
    // Repeated wraps need their own submission and retirement.
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, cursor, 3, 2));
    EXPECT_FALSE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, cursor, 4, 3));
    EXPECT_TRUE(triangle_fan_fits_without_wrap<uint16_t>(count, capacity, cursor, 4, 4));
}

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
