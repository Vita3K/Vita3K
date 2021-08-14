// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

static bool operator==(const LayoutArgsState &a, const LayoutArgsState &b) {
    return (a.float_used == b.float_used) && (a.gpr_used == b.gpr_used) && (a.stack_used == b.stack_used);
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
    const auto actual = lay_out<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t>();
    const std::array<ArgLayout, 6> layouts = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 1 },
        { ArgLocation::gpr, 2 },
        { ArgLocation::gpr, 3 },
        { ArgLocation::stack, 0 },
        { ArgLocation::stack, 4 },
    } };

    // Using full 4 gprs. Stack used for int32_t and uint32_t, which is total of 8 bytes
    const LayoutArgsState state = {
        4, 8, 0
    };

    ASSERT_EQ(std::get<0>(actual), layouts);
    ASSERT_EQ(std::get<1>(actual), state);
}

TEST(lay_out, dword_uses_two_gpr) {
    const auto actual = lay_out<int64_t, int32_t>();
    const std::array<ArgLayout, 2> layouts = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 2 },
    } };

    const LayoutArgsState state = {
        3, 0, 0
    };

    ASSERT_EQ(std::get<0>(actual), layouts);
    ASSERT_EQ(std::get<1>(actual), state);
}

TEST(lay_out, dword_aligned_to_two_gpr) {
    const auto actual = lay_out<int32_t, int64_t>();
    const std::array<ArgLayout, 2> layouts = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 2 },
    } };

    const LayoutArgsState state = {
        4, 0, 0
    };

    ASSERT_EQ(std::get<0>(actual), layouts);
    ASSERT_EQ(std::get<1>(actual), state);
}

TEST(lay_out, dword_alignment_can_waste_gprs) {
    // Arg 3 could fit in r1, but alignment of arg 2 skipped it.
    // I've not observed this, but this is the current behaviour.
    const auto actual = lay_out<int32_t, int64_t, int32_t>();
    const std::array<ArgLayout, 3> layouts = { {
        { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 2 },
        { ArgLocation::stack, 0 },
    } };

    const LayoutArgsState state = {
        4, 4, 0
    };

    ASSERT_EQ(std::get<0>(actual), layouts);
    ASSERT_EQ(std::get<1>(actual), state);
}

TEST(lay_out, vargs_does_not_affect_layout) {
    const auto actual = lay_out<int16_t, int16_t, module::vargs>();
    const std::array<ArgLayout, 3> layouts = { { { ArgLocation::gpr, 0 },
        { ArgLocation::gpr, 1 } } };

    const LayoutArgsState state = {
        2, 0, 0
    };

    ASSERT_EQ(std::get<0>(actual), layouts);
    ASSERT_EQ(std::get<1>(actual), state);
}
