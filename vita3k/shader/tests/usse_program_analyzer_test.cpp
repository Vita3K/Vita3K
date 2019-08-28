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

#include <gtest/gtest.h>
#include <shader/usse_program_analyzer.h>

#include <unordered_map>

using namespace shader;

TEST(program_analyzer, simple_branching) {
    const std::uint64_t ops[] = {
        17870293491888685056, //     nop
        18230571566473674760, // !p0 br #+8
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870283596284035074, //     br #+2
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870293491888685056, //     nop
        17870293491888685056, //     nop
    };

    std::unordered_map<usse::USSEOffset, usse::USSEBlock> blocks;

    usse::analyze(
        sizeof(ops) / 8 - 1, [&](usse::USSEOffset off) -> std::uint64_t { return ops[off]; },
        [&](const usse::USSEBlock &sub) -> usse::USSEBlock * {
            auto result = blocks.emplace(sub.offset, sub);
            if (result.second) {
                return &(result.first->second);
            }

            return nullptr;
        });

    ASSERT_EQ(blocks.size(), 5);
    ASSERT_EQ(blocks[0], usse::USSEBlock(0, 2, 0, -1));
    ASSERT_EQ(blocks[2], usse::USSEBlock(2, 7, 0, -1));
    ASSERT_EQ(blocks[9], usse::USSEBlock(9, 4, 0, -1));
    ASSERT_EQ(blocks[10], usse::USSEBlock(10, 3, 0, -1));
    ASSERT_EQ(blocks[13], usse::USSEBlock(13, 0, 0, -1));
}
