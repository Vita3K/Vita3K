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

#include <mem/allocator.h>
#include <mem/util.h>

#include <gtest/gtest.h>

TEST(bitmap_allocator, one_bit_allocation) {
    BitmapAllocator allocator(KB(10));

    for (int i = 0; i < KB(10); ++i) {
        int size = 1;
        int inoffset = 31 - i & 31;
        int bit = (allocator.words[i >> 5] & (1 << inoffset)) >> inoffset;
        ASSERT_EQ(bit, 1);
        int ret = allocator.allocate_from(i, size, false);
        ASSERT_EQ(ret, i);
        bit = (allocator.words[i >> 5] & (1 << inoffset)) >> inoffset;
        ASSERT_EQ(bit, 0);
    }
}

TEST(bitmap_allocator, one_bit_free_slot) {
    BitmapAllocator allocator(KB(10));
    ASSERT_EQ(allocator.free_slot_count(0, KB(10)), KB(10));
    for (int i = 0; i < KB(10); ++i) {
        int size = 1;
        ASSERT_EQ(allocator.free_slot_count(i, i + 1), 1);
        int ret = allocator.allocate_from(i, size, false);
        ASSERT_EQ(ret, i);
        ASSERT_EQ(size, 1);
        ASSERT_EQ(allocator.free_slot_count(i, i + 1), 0);
    }
    ASSERT_EQ(allocator.free_slot_count(0, KB(10)), 0);
}

TEST(bitmap_allocator, free_slot_count_aligned) {
    BitmapAllocator allocator(KB(10));

    for (int i = 0; i < KB(10); i += 2) {
        int size = 2;
        ASSERT_EQ(allocator.free_slot_count(i, i + 2), 2);
        int ret = allocator.allocate_from(i, size, false);
        ASSERT_EQ(ret, i);
        ASSERT_EQ(size, 2);
        ASSERT_EQ(allocator.free_slot_count(i, i + 2), 0);
    }
}

TEST(bitmap_allocator, free_slot_count_unaligned) {
    BitmapAllocator allocator(KB(10));

    for (int i = 0; KB(10) - i >= 3; i += 3) {
        int size = 3;
        ASSERT_EQ(allocator.free_slot_count(i, i + 3), 3);
        int ret = allocator.allocate_from(i, size, false);
        ASSERT_EQ(ret, i);
        ASSERT_EQ(size, 3);
        ASSERT_EQ(allocator.free_slot_count(i, i + 3), 0);
    }
}

// These tests are from EKA2L1 (https://github.com/EKA2L1/EKA2L1/blob/4fbd057da2a0c4f66a5c0f9dfc406c5d90f7531f/src/tests/common/allocator.cpp)
TEST(bitmap_allocator, no_best_fit_only_one_fit) {
    BitmapAllocator alloc(32);

    // Bitmap:              1000 0101 0001 0001 0101 0001 00[11 1]001
    alloc.words[0] = 0b10000101000100010101000100111001;

    int to_alloc = 3;
    ASSERT_EQ(alloc.allocate_from(0, to_alloc), 26);
    ASSERT_EQ(to_alloc, 3);

    // After allocate:      1000 0101 0001 0001 0101 0001 00[00 0]001
    ASSERT_EQ(alloc.words[0], 0b10000101000100010101000100000001);
}

TEST(bitmap_allocator, no_best_fit_multiple_fit) {
    BitmapAllocator alloc(32);

    // Bitmap 1:            1000 0[111] 1001 0001 0101 0001 0011 1001
    alloc.words[0] = 0b10000111100100010101000100111001;

    int to_alloc = 3;
    ASSERT_EQ(alloc.allocate_from(0, to_alloc), 5);
    ASSERT_EQ(to_alloc, 3);

    // After allocate:      1000 0[000] 1001 0001 0101 0001 0011 1001
    ASSERT_EQ(alloc.words[0], 0b10000000100100010101000100111001);
}

TEST(bitmap_allocator, no_best_fit_alloc_across) {
    BitmapAllocator alloc(64);

    alloc.words[0] = 0b111;
    alloc.words[1] = 0b11100000000000000000000000000000;

    int to_alloc = 5;
    ASSERT_EQ(alloc.allocate_from(0, to_alloc), 29);
    ASSERT_EQ(to_alloc, 5);

    ASSERT_EQ(alloc.words[0], 0);
    ASSERT_EQ(alloc.words[1], 0b00100000000000000000000000000000);
}

TEST(bitmap_allocator, best_fit_multiple_fit) {
    BitmapAllocator alloc(32);

    // Bitmap 1:            1000 0111 1001 0001 0101 0001 00[11 1]001
    alloc.words[0] = 0b10000111100100010101000100111001;

    int to_alloc = 3;
    ASSERT_EQ(alloc.allocate_from(0, to_alloc, true), 26);
    ASSERT_EQ(to_alloc, 3);

    // After allocate:      1000 0111 1001 0001 0101 0001 00[00 0]001
    ASSERT_EQ(alloc.words[0], 0b10000111100100010101000100000001);
}

TEST(bitmap_allocator, count_bit_aligned) {
    BitmapAllocator alloc(32 * 3);

    // Bitmap 1: All bit on
    alloc.words[0] = 0xFFFFFFFF;

    // Bitmap 2: 12 bits on
    alloc.words[1] = 0xF00F00F0;

    // Bitmap 3: 12 bits on
    alloc.words[2] = 0x00F00F0F;

    ASSERT_EQ(alloc.free_slot_count(0, 32 * 3), 56);
    ASSERT_EQ(alloc.free_slot_count(0, 32 * 3 - 1), 55);
}

TEST(bitmap_allocator, count_unaligned) {
    BitmapAllocator alloc(32 * 3);

    // Bitmap 1: 0b11 | 0011000011
    //      ignored ^
    // 4 valid bits on
    alloc.words[0] = 0b110011000011;

    // Bitmap 2: 12 bits on
    alloc.words[1] = 0xF00F00F0;

    // Bitmap 3: 0b1010111000 | 1111
    //                          ^ ignored
    // 5 valid bits on
    alloc.words[2] = 0b10101110001111;

    // 4 valid bits + 12 bits + 5 valid bits = 21
    ASSERT_EQ(alloc.free_slot_count(22, 92), 21);
}
