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

#include <module/lay_out_args.h>

#include <gtest/gtest.h>

static bool operator==(const ArgLayout &a, const ArgLayout &b) {
    return (a.location == b.location) && (a.offset == b.offset);
}

static std::ostream &operator<<(std::ostream &out, const ArgLayout &layout) {
    switch (layout.location) {
    case ArgLocation::gpr:
        out << "r" << layout.offset;
        break;
    case ArgLocation::stack:
        out << "stack + " << layout.offset;
        break;
    case ArgLocation::fp:
        out << "s" << layout.offset;
        break;
    }

    return out;
}

TEST(lay_out, gpr_overflows_to_stack) {
    const auto actual = std::get<0>(lay_out<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t>());
    const std::array<ArgLayout, 6> expected = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 1 },
        { ArgLocation::gpr, 2 },
        { ArgLocation::gpr, 3 },
        { ArgLocation::stack, 0 },
        { ArgLocation::stack, 4 },
    } };

    ASSERT_EQ(actual, expected);
}

TEST(lay_out, dword_uses_two_gpr) {
    const auto actual = std::get<0>(lay_out<int64_t, int32_t>());
    const std::array<ArgLayout, 2> expected = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 2 },
    } };

    ASSERT_EQ(actual, expected);
}

TEST(lay_out, dword_aligned_to_two_gpr) {
    const auto actual = std::get<0>(lay_out<int32_t, int64_t>());
    const std::array<ArgLayout, 2> expected = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 2 },
    } };

    ASSERT_EQ(actual, expected);
}

TEST(lay_out, dword_alignment_can_waste_gprs) {
    // Arg 3 could fit in r1, but alignment of arg 2 skipped it.
    // I've not observed this, but this is the current behaviour.
    const auto actual = std::get<0>(lay_out<int32_t, int64_t, int32_t>());
    const std::array<ArgLayout, 3> expected = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 2 },
        { ArgLocation::stack, 0 },
    } };

    ASSERT_EQ(actual, expected);
}
