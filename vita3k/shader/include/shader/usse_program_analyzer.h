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

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <queue>
#include <shader/usse_types.h>
#include <tuple>

using UniformBufferSizes = std::array<std::uint32_t, 15>;

struct SceGxmProgramParameter;
struct SceGxmProgram;

namespace shader::usse {
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
bool is_kill(const std::uint64_t inst);
bool is_branch(const std::uint64_t inst, std::uint8_t &pred, std::uint32_t &br_off);
bool is_buffer_fetch_or_store(const std::uint64_t inst, int &base, int &cursor, int &offset, int &size);
bool does_write_to_predicate(const std::uint64_t inst, std::uint8_t &pred);
std::uint8_t get_predicate(const std::uint64_t inst);

struct USSEBlock {
    std::uint32_t offset;
    std::uint32_t size;

    std::uint8_t pred; ///< The predicator requires to execute this block.
    std::int32_t offset_link; ///< The offset of block to link at the end of the block. -1 for none.

    USSEBlock() = default;
    USSEBlock(const std::uint32_t off, const std::uint32_t size, const std::uint8_t pred, const std::int32_t off_link)
        : offset(off)
        , size(size)
        , pred(pred)
        , offset_link(off_link) {
    }

    bool operator==(const USSEBlock &rhs) const {
        return (offset == rhs.offset) && (size == rhs.size) && (pred == rhs.pred) && (offset_link == rhs.offset_link);
    }
};

struct AttributeInformation {
    std::uint32_t info;

    explicit AttributeInformation()
        : info(0) {
    }

    explicit AttributeInformation(const std::uint16_t loc, const std::uint16_t gxm_type)
        : info(loc | (static_cast<std::uint32_t>(gxm_type) << 16)) {
    }

    std::uint16_t location() const {
        return static_cast<std::uint16_t>(info);
    }

    std::uint16_t gxm_type() const {
        return static_cast<std::uint16_t>(info >> 16);
    }
};

using UniformBufferMap = std::map<int, UniformBuffer>;
using AttributeInformationMap = std::map<int, AttributeInformation>;

using USSEOffset = std::uint32_t;

void get_attribute_informations(const SceGxmProgram &program, AttributeInformationMap &locmap);
void get_uniform_buffer_sizes(const SceGxmProgram &program, UniformBufferSizes &sizes);
int match_uniform_buffer_with_buffer_size(const SceGxmProgram &program, const SceGxmProgramParameter &parameter, const shader::usse::UniformBufferMap &buffers);

template <typename F, typename H>
void analyze(USSEOffset end_offset, F read_func, H handler_func) {
    std::queue<USSEBlock *> blocks_queue;

    auto add_block = [&](USSEOffset block_addr) {
        USSEBlock routine = { block_addr, 0, 0, -1 };

        if (auto sub_ptr = handler_func(routine)) {
            blocks_queue.push(sub_ptr);
        }
    };

    // Initial block, start at offset 0 of the program
    add_block(0);

    for (auto block = blocks_queue.front(); !blocks_queue.empty();) {
        bool should_stop = false;

        for (auto baddr = block->offset; baddr <= end_offset && !should_stop;) {
            auto inst = read_func(baddr);

            if (inst == 0) {
                block->size = baddr - block->offset;
                should_stop = true;

                break;
            }

            std::uint8_t pred = get_predicate(inst);
            std::uint32_t br_off = 0;

            if (baddr == block->offset) {
                block->pred = pred;
            }

            if (is_branch(inst, pred, br_off)) {
                add_block(baddr + br_off);

                if (block->offset == baddr) {
                    block->pred = 0;
                }

                // No need to specify link offset. The translator will do it for us once it met BR.
                block->size = baddr - block->offset + 1;
                should_stop = true;

                if (pred != 0) {
                    add_block(baddr + 1);
                }
            } else if (is_kill(inst)) {
                add_block(baddr + 1);

                if (block->offset == baddr) {
                    block->pred = 0;
                }

                block->size = baddr - block->offset + 1;
                should_stop = true;
            } else {
                bool is_predicate_invalidated = false;

                std::uint8_t predicate_writed_to = 0;
                if (does_write_to_predicate(inst, predicate_writed_to)) {
                    is_predicate_invalidated = ((predicate_writed_to + 1) == block->pred) || ((predicate_writed_to + 5) == block->pred);
                }

                // Either if the instruction has different predicate with the block,
                // or the predicate value is being invalidated (overwritten)
                // which means continuing is obselete. Stop now
                if (pred != block->pred) {
                    add_block(baddr);

                    block->size = baddr - block->offset;
                    block->offset_link = baddr;

                    should_stop = true;
                } else if (is_predicate_invalidated) {
                    add_block(baddr + 1);

                    block->size = baddr - block->offset + 1;
                    block->offset_link = baddr + 1;

                    should_stop = true;
                }
            }

            baddr += 1;
        }

        if (block->size == 0) {
            block->size = end_offset - block->offset + 1;
        }

        blocks_queue.pop();

        if (!blocks_queue.empty()) {
            block = blocks_queue.front();
        }
    }

    // Add dummy block
    handler_func({ end_offset + 1, 0, 0, -1 });
}
} // namespace shader::usse
