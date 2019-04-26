// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <cstdint>
#include <tuple>
#include <queue>

namespace usse {
    /**
     * \brief Check if an instruction is a branch.
     * 
     * If the instruction is a branch, the passed references will be set
     * with the predicate and branch offset.
     * 
     * \param inst   Instruction.
     * \param pred   Reference to the value which will contains predicate.
     * \param br_off Reference to the value which will contains branch offset.
     * 
     * \returns True on instruction being a branch.
     */
    static bool is_branch(const std::uint64_t inst, std::uint8_t &pred, std::uint32_t &br_off) {
        const std::uint32_t high = (inst >> 32);
        const std::uint32_t low = static_cast<std::uint32_t>(inst);

        const bool br_inst_is = (((high & ~0xFFCFFFFFU) >> 20) == 0) && !(high & 0x00400000) && ((((high & ~0xFFFFFE3FU) >> 6) == 0)
            || ((high & ~0xFFFFFE3FU) >> 6) == 1);
            
        if (br_inst_is) {
            br_off = (low & ((1 << 20) - 1));
            pred = (high & ~0xF8FFFFFFU) >> 24;
        }

        return br_inst_is;
    }

    using usse_block = std::pair<std::uint32_t, std::uint32_t>;
    using usse_offset = std::uint32_t;

    template <typename F, typename H>
    void analyze(usse_offset end_offset, F read_func, H handler_func) {
        std::queue<usse_block*> blocks_queue;

        auto add_block = [&](usse_offset block_addr) {
            usse_block routine = { block_addr, 0 };

            if (auto sub_ptr = handler_func(routine)) {
                blocks_queue.push(sub_ptr);
            }
        };

        // Initial block, start at offset 0 of the program
        add_block(0);

        for (auto block = blocks_queue.front(); !blocks_queue.empty();) {
            bool should_stop = false;

            for (auto baddr = block->first; baddr <= end_offset && !should_stop;) {
                auto inst = read_func(baddr);

                if (inst == 0) {
                    block->second = baddr - block->first;
                    should_stop = true;

                    break;
                }

                std::uint8_t pred = 0;
                std::uint32_t br_off = 0;

                if (is_branch(inst, pred, br_off)) {
                    add_block(baddr + br_off);

                    // An unconditional branch if predicator is 0
                    // This should work fine with the pattern of most shaders encountered
                    // There are places that a link can be used, it certainly will be used, but when generate
                    if (pred == 0) {
                        block->second = baddr - block->first + 1;
                        should_stop = true;
                    }
                }

                baddr += 1;
            }

            if (block->second == 0) {
                block->second = end_offset - block->first;
            }

            blocks_queue.pop();

            if (!blocks_queue.empty()) {
                block = blocks_queue.front();
            }
        }
    }
}