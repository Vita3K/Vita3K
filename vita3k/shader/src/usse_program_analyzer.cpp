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

#include <gxm/functions.h>
#include <gxm/types.h>
#include <shader/gxp_parser.h>
#include <shader/usse_program_analyzer.h>

#include <cassert>

#include <shader/usse_types.h>

namespace shader::usse {
bool is_kill(const std::uint64_t inst) {
    return (inst >> 59) == 0b11111 && (((inst >> 32) & ~0xF8FFFFFF) >> 24) == 1 && ((inst >> 32) & ~0xFFCFFFFF) >> 20 == 3;
}

bool is_branch(const std::uint64_t inst, std::uint8_t &pred, std::int32_t &br_off) {
    const std::uint32_t high = (inst >> 32);
    const std::uint32_t low = static_cast<std::uint32_t>(inst);

    const bool br_inst_is = ((high & ~0x07FFFFFFU) >> 27 == 0b11111) && (((high & ~0xFFCFFFFFU) >> 20) == 0) && !(high & 0x00400000) && ((((high & ~0xFFFFFE3FU) >> 6) == 0) || ((high & ~0xFFFFFE3FU) >> 6) == 1);

    if (br_inst_is) {
        br_off = static_cast<std::int32_t>(low & ((1 << 20) - 1));
        pred = (high & ~0xF8FFFFFFU) >> 24;

        if (((inst & (1ULL << 38)) != 0) && (br_off & (1 << 19))) {
            // PC bits on SGX543 is 20 bits
            br_off |= 0xFFFFFFFF << 20;
        }
    }

    return br_inst_is;
}

bool does_write_to_predicate(const std::uint64_t inst, std::uint8_t &pred) {
    if ((((inst >> 59) & 0b11111) == 0b01001) || (((inst >> 59) & 0b11111) == 0b01111)) {
        pred = static_cast<std::uint8_t>((inst & ~0xFFFFFFF3FFFFFFFF) >> 34);
        return true;
    }

    return false;
}

std::uint8_t get_predicate(const std::uint64_t inst) {
    switch (inst >> 59) {
    // Special instructions
    case 0b11111: {
        if ((((inst >> 32) & ~0xFF8FFFFF) >> 20) == 0) {
            break;
        }

        // Kill
        if ((((inst >> 32) & ~0xF8FFFFFF) >> 24) == 1 && ((inst >> 32) & ~0xFFCFFFFF) >> 20 == 3) {
            uint8_t pred = ((inst >> 32) & (~0xFFFFF9FF)) >> 9;
            switch (pred) {
            case 0:
                return static_cast<uint8_t>(ExtPredicate::NONE);
            case 1:
                return static_cast<uint8_t>(ExtPredicate::NEGP0);
            case 2:
                return static_cast<uint8_t>(ExtPredicate::NEGP1);
            case 3:
                return static_cast<uint8_t>(ExtPredicate::P0);
            default:
                return 0;
            }
        }

        return 0;
    }
    // VMAD normal version, predicates only occupied two bits
    case 0b00100:
    case 0b00101: {
        uint8_t predicate = ((inst >> 32) & ~0xFCFFFFFF) >> 24;
        switch (predicate) {
        case 0:
            return static_cast<uint8_t>(ExtPredicate::NONE);
        case 1:
            return static_cast<uint8_t>(ExtPredicate::P0);
        case 2:
            return static_cast<uint8_t>(ExtPredicate::NEGP0);
        case 3:
            return static_cast<uint8_t>(ExtPredicate::PN);
        default:
            return 0;
        }
    }
    // SOP2, I32MAD
    case 0b10000:
    case 0b10101:
    case 0b10011: {
        uint8_t predicate = ((inst >> 32) & ~0xF9FFFFFF) >> 25;
        return static_cast<uint8_t>(short_predicate_to_ext(static_cast<ShortPredicate>(predicate)));
    }
    // V16NMAD, V32NMAD, VMAD, VDP
    case 0b00011:
    case 0b00010:
    case 0b00001: {
        uint8_t predicate = ((inst >> 32) & ~0xF8FFFFFFU) >> 24;
        return static_cast<uint8_t>(ext_vec_predicate_to_ext(static_cast<ExtVecPredicate>(predicate)));
    }
    case 0b00000:
        return ((inst >> 32) & ~0xFCFFFFFF) >> 24;

    default:
        break;
    }

    return ((inst >> 32) & ~0xF8FFFFFFU) >> 24;
}

bool is_buffer_fetch_or_store(const std::uint64_t inst, int &base, int &cursor, int &offset, int &size) {
    // TODO: Is there any exception? Like any instruction use pre or post increment addressing mode.
    cursor = 0;

    // Are you me? Or am i you
    if (((inst >> 59) & 0b11111) == 0b11101 || ((inst >> 59) & 0b11111) == 0b11110) {
        // Get the base
        offset = cursor + (inst >> 7) & 0b1111111;
        base = (inst >> 14) & 0b1111111;

        // Data type downwards: 4 bytes, 2 bytes, 1 bytes, 0 bytes
        // Total to fetch is at bit 44, and last for 4 bits.
        size = 4 / (((inst >> 36) & 0b11) + 1) * (((inst >> 44) & 0b1111) + 1);

        return true;
    }

    return false;
}

int match_uniform_buffer_with_buffer_size(const SceGxmProgram &program, const SceGxmProgramParameter &parameter, const shader::usse::UniformBufferMap &buffers) {
    assert(parameter.component_count == 0 && parameter.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER);

    // Determine base value
    int base = gxp::get_uniform_buffer_base(program, parameter);

    // Search for the buffer from analyzed list
    if (buffers.find(base) != buffers.end()) {
        return (buffers.at(base).size + 3) / 4;
    }

    return -1;
}

void get_uniform_buffer_sizes(const SceGxmProgram &program, UniformBufferSizes &sizes) {
    const auto program_input = shader::get_program_input(program);
    for (const auto buffer : program_input.uniform_buffers) {
        uint32_t index = buffer.index;
        if (index == 0) {
            index = 14;
        } else {
            --index;
        }
        sizes[index] = buffer.size;
    }
}

void get_attribute_informations(const SceGxmProgram &program, AttributeInformationMap &locmap) {
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);
    std::uint32_t fcount_allocated = 0;

    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = gxp_parameters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
            locmap.emplace(parameter.resource_index, AttributeInformation(fcount_allocated / 4, parameter.type));
            fcount_allocated += ((parameter.array_size * parameter.component_count + 3) / 4) * 4;
        }
    }
}

USSEBaseNode *USSEBaseNode::add_children_protected(USSEBaseNodeInstance &instance) {
    if (!instance) {
        return nullptr;
    }

    if (instance->node_type() == USSE_CODE_NODE) {
        USSECodeNode *code = reinterpret_cast<USSECodeNode *>(instance.get());
        if (code->size == 0) {
            return nullptr;
        }
    }

    children.push_back(std::move(instance));
    return children.back().get();
}

USSEBlockNode::USSEBlockNode(USSEBaseNode *parent, const std::uint32_t start)
    : USSEBaseNode(parent, USSE_BLOCK_NODE)
    , offset(start) {
}

USSEBaseNode *USSEBlockNode::add_children(USSEBaseNodeInstance &instance) {
    return add_children_protected(instance);
}

USSECodeNode::USSECodeNode(USSEBaseNode *parent)
    : USSEBaseNode(parent, USSE_CODE_NODE)
    , offset(0)
    , size(0)
    , condition(0) {
}

USSEConditionalNode::USSEConditionalNode(USSEBaseNode *parent, const std::uint32_t merge_point)
    : USSEBaseNode(parent, USSE_CONDITIONAL_NODE)
    , merge_point(merge_point) {
    children.resize(2);
}

USSEBlockNode *USSEConditionalNode::if_block() const {
    return reinterpret_cast<USSEBlockNode *>(children[0].get());
}

USSEBlockNode *USSEConditionalNode::else_block() const {
    return reinterpret_cast<USSEBlockNode *>(children[1].get());
}

void USSEConditionalNode::set_if_block(USSEBaseNodeInstance &node) {
    children[0] = std::move(node);
}

void USSEConditionalNode::set_else_block(USSEBaseNodeInstance &node) {
    children[1] = std::move(node);
}

USSELoopNode::USSELoopNode(USSEBaseNode *parent, const std::uint32_t loop_end_offset)
    : USSEBaseNode(parent, USSE_LOOP_NODE)
    , loop_end_offset(loop_end_offset) {
    children.resize(1);
}

USSEBlockNode *USSELoopNode::content_block() const {
    return reinterpret_cast<USSEBlockNode *>(children[0].get());
}

void USSELoopNode::set_content_block(USSEBaseNodeInstance &node) {
    children[0] = std::move(node);
}

void analyze(USSEBlockNode &root, USSEOffset end_offset, AnalyzeReadFunction read_func) {
    struct BlockInvestigateRequest {
        USSEOffset begin_offset;
        USSEOffset end_offset;

        USSEBlockNode *block_node;
    };

    struct BranchInfo {
        std::uint32_t offset;
        std::uint32_t dest;

        std::uint8_t pred;
    };

    root.reset();

    std::multimap<std::uint32_t, BranchInfo> branches_to_back;
    std::map<std::uint32_t, BranchInfo> branches_from;

    std::queue<BlockInvestigateRequest> investigate_queue;
    investigate_queue.push({ 0, end_offset, &root });

    // First off query all branches first
    // This is for easy tracing of loops and branches later, without complicating the algorithm
    // For example, loop might be in a loop :(
    for (usse::USSEOffset baddr = 0; baddr <= end_offset; baddr += 1) {
        auto inst = read_func(baddr);

        std::uint8_t pred = 0;
        std::int32_t br_off = 0;

        if (is_branch(inst, pred, br_off)) {
            const std::uint32_t dest = baddr + br_off;
            BranchInfo info = { baddr, dest, pred };

            if (br_off < 0)
                branches_to_back.emplace(dest, info);

            branches_from.emplace(baddr, info);
        }
    }

    while (!investigate_queue.empty()) {
        BlockInvestigateRequest request = std::move(investigate_queue.front());
        investigate_queue.pop();

        std::unique_ptr<USSEBaseNode> current_code_inst = std::make_unique<USSECodeNode>(request.block_node);
        USSECodeNode *current_code = reinterpret_cast<USSECodeNode *>(current_code_inst.get());

        current_code->offset = request.begin_offset;
        current_code->size = 0;

        for (auto baddr = request.begin_offset; baddr <= request.end_offset; baddr += 1) {
            auto inst = read_func(baddr);

            if (inst == 0) {
                break;
            }

            std::uint8_t pred = get_predicate(inst);

            if (baddr == current_code->offset) {
                current_code->condition = pred;
            }

            auto branch_from_result = branches_from.find(baddr);
            auto branch_to_result = branches_to_back.equal_range(baddr);

            if (branch_from_result != branches_from.end()) {
                bool is_loop_stmt = false;

                // It might be a break to exit a loop
                // Get the nearest parent that is a loop
                USSEBaseNode *loop_parent = request.block_node;
                while (loop_parent && (loop_parent->node_type() != USSE_LOOP_NODE)) {
                    loop_parent = loop_parent->get_parent();
                }

                if (loop_parent) {
                    USSELoopNode *loop_node = reinterpret_cast<USSELoopNode *>(loop_parent);
                    std::unique_ptr<USSEBaseNode> node_to_add;

                    if (loop_node->get_loop_end_offset() <= branch_from_result->second.dest - 1) {
                        node_to_add = std::make_unique<USSEBreakNode>(request.block_node, branch_from_result->second.pred);

                        is_loop_stmt = true;
                    } else if (loop_node->content_block()->start_offset() == branch_from_result->second.dest) {
                        assert((branch_from_result->second.pred == 0) && "Continuing and abadon further statement without a condition is crazy");

                        // The loop is continuing!!!!
                        node_to_add = std::make_unique<USSEContinueNode>(request.block_node, branch_from_result->second.pred);
                        is_loop_stmt = true;
                    }

                    if (node_to_add) {
                        current_code->size = baddr - current_code->offset;

                        request.block_node->add_children(current_code_inst);
                        request.block_node->add_children(node_to_add);

                        current_code_inst = std::make_unique<USSECodeNode>(request.block_node);
                        current_code = reinterpret_cast<USSECodeNode *>(current_code_inst.get());

                        current_code->offset = baddr + 1;
                    }
                }

                // Likely a normal if
                if (!is_loop_stmt) {
                    // Some ifs may be trying to jump to parent's merge point. It's also means there's no more content
                    // further in this block.
                    if (branch_from_result->second.pred == 0) {
                        // Execution changed direction, follow it
                        current_code->size = baddr - current_code->offset;
                        request.block_node->add_children(current_code_inst);

                        baddr = branch_from_result->second.dest - 1;

                        std::unique_ptr<USSEBaseNode> current_code_inst = std::make_unique<USSECodeNode>(request.block_node);
                        USSECodeNode *current_code = reinterpret_cast<USSECodeNode *>(current_code_inst.get());

                        current_code->offset = baddr;
                    } else {
                        bool else_exist = false;
                        auto branch_from_else_result = branches_from.find(branch_from_result->second.dest - 1);

                        std::uint32_t else_end_offset = 0;

                        // Simply a jump to after else
                        if (branch_from_else_result != branches_from.end()) {
                            assert((branch_from_else_result->second.pred == 0) && "Unhandled!");

                            // Note: There might be nastier situation where the if block ends sooner then after that is some block
                            // of other blocks. Hope the compiler is not that nasty.
                            // The solution is to keep track of the statement stack and finally get the final branch, but with loop also available
                            // it is presenting itself as kind of hard
                            if (branch_from_else_result->second.dest >= branch_from_result->second.dest) {
                                else_end_offset = branch_from_else_result->second.dest;
                                else_exist = true;
                            } else {
                                // Some blocks optimized by throwing merge to after else into inner for loop
                                // This is logical in case the if only contains the loop.
                                auto begin_ite = branches_from.upper_bound(branch_from_else_result->second.dest + 1);
                                auto end_ite = branches_from.lower_bound(branch_from_result->second.dest - 1);

                                if ((begin_ite != branches_from.end()) && (end_ite != branches_from.end())) {
                                    for (; begin_ite != end_ite; begin_ite++) {
                                        if (begin_ite->second.dest > branch_from_result->second.dest) {
                                            else_exist = true;
                                            else_end_offset = begin_ite->second.dest;
                                        }
                                    }
                                }
                            }
                        }

                        std::uint32_t merge_point = (else_exist ? else_end_offset : branch_from_result->second.dest);

                        // Create a new if/else tree (there might be no else :D)
                        // The space between the branch and the jump is the content block of the if
                        // If assuming the condition of the jump is p0, then the if will be if (!p0) content
                        // If the instruction before the destinated jump location is another branch, it is likely the else block
                        std::unique_ptr<USSEBaseNode> conditional_inst = std::make_unique<USSEConditionalNode>(request.block_node, merge_point);
                        USSEConditionalNode *conditional = reinterpret_cast<USSEConditionalNode *>(conditional_inst.get());

                        conditional->set_negif_condition(pred);

                        std::unique_ptr<USSEBaseNode> if_block = std::make_unique<USSEBlockNode>(conditional_inst.get(), baddr);
                        conditional->set_if_block(if_block);

                        // Sometimes it jumps to the merge point of mother, so we limit the range
                        investigate_queue.push({ baddr + 1, std::min<USSEOffset>(branch_from_result->second.dest - 1, request.end_offset),
                            conditional->if_block() });

                        if (else_exist) {
                            std::unique_ptr<USSEBaseNode> else_block = std::make_unique<USSEBlockNode>(conditional_inst.get(), branch_from_result->second.dest);
                            conditional->set_else_block(else_block);

                            investigate_queue.push({ branch_from_result->second.dest, else_end_offset - 1, conditional->else_block() });
                        }

                        // End the current block code, and also add this block in
                        current_code->size = baddr - current_code->offset;
                        request.block_node->add_children(current_code_inst);
                        request.block_node->add_children(conditional_inst);

                        current_code_inst = std::make_unique<USSECodeNode>(request.block_node);
                        current_code = reinterpret_cast<USSECodeNode *>(current_code_inst.get());

                        current_code->offset = merge_point;
                        baddr = current_code->offset - 1;
                    }
                }
            } else if ((branches_to_back.find(baddr) != branches_to_back.end()) && (request.block_node->start_offset() != baddr)) {
                // The loop continue target should be unconditional and farest
                std::uint32_t found_offset = 0xFFFFFFFF;
                for (auto ite = branch_to_result.first; ite != branch_to_result.second; ite++) {
                    if ((ite->second.pred == 0) && ((found_offset == 0xFFFFFFFF) || (found_offset < ite->second.offset))) {
                        found_offset = ite->second.offset;
                    }
                }

                assert((found_offset != 0xFFFFFFFF) && "Offset to end loop not found!");

                // Smell like a loop ! Create a loop node
                // The outer farest with no conditional jump should be the one we are looking for
                std::unique_ptr<USSEBaseNode> loop_node_inst = std::make_unique<USSELoopNode>(request.block_node, found_offset);
                USSELoopNode *loop_node = reinterpret_cast<USSELoopNode *>(loop_node_inst.get());

                std::unique_ptr<USSEBaseNode> content_block = std::make_unique<USSEBlockNode>(loop_node_inst.get(), baddr);
                loop_node->set_content_block(content_block);

                investigate_queue.push({ baddr, found_offset - 1, loop_node->content_block() });

                current_code->size = baddr - current_code->offset;

                request.block_node->add_children(current_code_inst);
                request.block_node->add_children(loop_node_inst);

                current_code_inst = std::make_unique<USSECodeNode>(request.block_node);
                current_code = reinterpret_cast<USSECodeNode *>(current_code_inst.get());

                current_code->offset = found_offset + 1;
                baddr = current_code->offset - 1;
            } else {
                bool is_predicate_invalidated = false;

                std::uint8_t predicate_writed_to = 0;
                if (does_write_to_predicate(inst, predicate_writed_to)) {
                    is_predicate_invalidated = ((predicate_writed_to + 1) == current_code->condition) || ((predicate_writed_to + 5) == current_code->condition);
                }

                std::uint32_t offset_end = 0;

                // Either if the instruction has different predicate with the block,
                // or the predicate value is being invalidated (overwritten)
                // which means continuing is obselete. Stop now
                if (pred != current_code->condition) {
                    current_code->size = baddr - current_code->offset;
                    offset_end = baddr;
                } else if (is_predicate_invalidated) {
                    current_code->size = baddr + 1 - current_code->offset;
                    offset_end = baddr + 1;
                }

                if (offset_end != 0) {
                    request.block_node->add_children(current_code_inst);

                    current_code_inst = std::make_unique<USSECodeNode>(request.block_node);
                    current_code = reinterpret_cast<USSECodeNode *>(current_code_inst.get());

                    current_code->offset = offset_end;
                    baddr = offset_end - 1;
                }
            }
        }

        if (current_code_inst) {
            current_code->size = request.end_offset - current_code->offset + 1;
            request.block_node->add_children(current_code_inst);
        }
    }
}

} // namespace shader::usse
