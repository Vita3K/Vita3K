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

#include <shader/usse_translator.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <SPIRV/SpvBuilder.h>
#include <boost/optional.hpp>
#include <spdlog/fmt/fmt.h>
#include <spirv.hpp>

#include <bitset>
#include <iostream>
#include <tuple>

// TODO: Remove
#include "disassemble.h"

using namespace shader;
using namespace usse;

bool shader::usse::USSETranslatorVisitor::get_spirv_reg(usse::RegisterBank bank, std::uint32_t reg_offset, std::uint32_t shift_offset, SpirvReg &reg, std::uint32_t &out_comp_offset, bool get_for_store) {
    const std::uint32_t original_reg_offset = reg_offset;

    if (bank == usse::RegisterBank::FPINTERNAL) {
        // Each GPI register is 128 bits long = 16 bytes long
        // So we need to multiply the reg index with 16 in order to get the right offset
        reg_offset *= 16;
    }

    const auto pa_writeable_idx = (reg_offset + shift_offset) / 4;

    // If we are getting a PRIMATTR reg, we need to check whether a writeable one has already been created
    // If it is, get the writeable one, else proceed
    if (bank == usse::RegisterBank::PRIMATTR) {
        decltype(pa_writeable)::iterator pa;
        if ((pa = pa_writeable.find(pa_writeable_idx)) != pa_writeable.end()) {
            reg = pa->second;
            return true;
        }
    }

    const SpirvVarRegBank &spirv_bank = get_reg_bank(bank);

    bool result = spirv_bank.find_reg_at(reg_offset + shift_offset, reg, out_comp_offset);
    if (!result)
        return false;

    if (bank == usse::RegisterBank::PRIMATTR && get_for_store) {
        if (pa_writeable.count(pa_writeable_idx) == 0) {
            const std::string new_pa_writeable_name = fmt::format("pa{}_temp", std::to_string(reg_offset));
            const spv::Id new_pa_writeable = m_b.createVariable(spv::StorageClassPrivate, reg.type_id, new_pa_writeable_name.c_str());
            m_b.createStore(m_b.createLoad(reg.var_id), new_pa_writeable);

            reg.var_id = new_pa_writeable;
            pa_writeable[pa_writeable_idx] = reg;
        }
    }

    return true;
}

spv::Id USSETranslatorVisitor::bridge(SpirvReg &src1, SpirvReg &src2, usse::Swizzle4 swiz, const std::uint32_t shift_offset, const Imm4 dest_mask) {
    // Queue keep track of components to be modified with given constant
    struct constant_queue_member {
        std::uint32_t index;
        spv::Id constant;
    };

    std::vector<constant_queue_member> constant_queue;

    std::vector<spv::Id> ops;
    ops.push_back(src1.var_id);
    ops.push_back(src2.var_id);

    const uint32_t src1_comp_count = m_b.getNumTypeComponents(src1.type_id);
    const uint32_t src2_comp_count = m_b.getNumTypeComponents(src2.type_id);
    const uint32_t total_comp_count = src1_comp_count + src2_comp_count;

    for (int i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            switch (swiz[i]) {
            case usse::SwizzleChannel::_X:
            case usse::SwizzleChannel::_Y:
            case usse::SwizzleChannel::_Z:
            case usse::SwizzleChannel::_W: {
                ops.push_back(std::min((uint32_t)swiz[i] + shift_offset, total_comp_count));
                break;
            }

            case usse::SwizzleChannel::_1:
            case usse::SwizzleChannel::_0:
            case usse::SwizzleChannel::_2:
            case usse::SwizzleChannel::_H: {
                ops.push_back((uint32_t)usse::SwizzleChannel::_X);
                constant_queue_member member;
                member.index = i;
                member.constant = const_f32[(uint32_t)swiz[i] - (uint32_t)usse::SwizzleChannel::_0];

                constant_queue.push_back(std::move(member));

                break;
            }

            default: {
                LOG_ERROR("Unsupport swizzle type");
                break;
            }
            }
        }
    }

    spv::Id shuff_type = spv::NoResult;

    // Get total swizzle actually use, by subtracting the ops by 2
    switch (ops.size() - 2) {
    case 1:
    case 2:
    case 3:
    case 4: {
        shuff_type = type_f32_v[ops.size() - 2];

        break;
    }
    default:
        return spv::NoResult;
    }

    auto shuff_result = m_b.createOp(spv::OpVectorShuffle, shuff_type, ops);

    for (std::size_t i = 0; i < constant_queue.size(); i++) {
        const auto index  = m_b.makeIntConstant(constant_queue[i].index);
        shuff_result = m_b.createOp(spv::OpVectorInsertDynamic, shuff_type,
            { shuff_result, constant_queue[i].constant, index });
    }

    return shuff_result;
}

spv::Id USSETranslatorVisitor::load(Operand &op, const Imm4 dest_mask, const std::uint8_t offset, bool, bool) {
    /*
    // Optimization: Check for constant swizzle and emit it right away
    for (std::uint8_t i = 0; i < 4; i++) {
        USSE::SwizzleChannel channel = static_cast<USSE::SwizzleChannel>(
            static_cast<std::uint32_t>(USSE::SwizzleChannel::_0) + i);

        if (op.swizzle == USSE::Swizzle4{ channel, channel, channel, channel }) {
            return spv_v4const[i];
        }
    }
    */

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);

    // TODO: Properly handle
    if (op.bank == RegisterBank::FPCONSTANT || op.bank == RegisterBank::IMMEDIATE) {
        auto t = m_b.makeVectorType(type_f32, dest_comp_count);
        auto one = m_b.makeFpConstant(type_f32, 1.0);
        return m_b.makeCompositeConstant(t, { one, one });
    }

    // Composite a new vector
    SpirvReg reg_left{};
    std::uint32_t reg_left_comp_offset{};

    if (!get_spirv_reg(op.bank, op.num, offset, reg_left, reg_left_comp_offset, false)) {
        LOG_ERROR("Can't load register {}", disasm::operand_to_str(op, 0));
        return spv::NoResult;
    }

    // Get the next reg, so we can do a bridge
    SpirvReg reg_right{};
    std::uint32_t reg_right_comp_offset{};

    // There is no more register to bridge to. For making it valid, just
    // throws in a valid register
    //
    // I haven't think of any edge case, but this should be looked when there is
    // any problems with bridging
    if (!get_spirv_reg(op.bank, op.num, offset + dest_comp_count, reg_right, reg_right_comp_offset, false))
        reg_right = reg_left;

    // Optimization: Bridging (VectorShuffle) or even swizzling is not always necessary

    const bool needs_swizzle = !is_default(op.swizzle, dest_comp_count);

    if (!needs_swizzle) {
        const uint32_t reg_left_comp_count = m_b.getNumTypeComponents(reg_left.type_id) - reg_left_comp_offset;
        const uint32_t reg_right_comp_count = m_b.getNumTypeComponents(reg_right.type_id) - reg_right_comp_offset;

        const bool is_equal_comp_count = (reg_left_comp_count == reg_right_comp_count);

        if (is_equal_comp_count) {
            const bool is_reg_left_comp_offset = (reg_left_comp_offset == 0);
            const bool is_reg_right_comp_offset = (reg_left_comp_offset == dest_comp_count);

            if (is_reg_left_comp_offset)
                return m_b.createLoad(reg_left.var_id);
            else if (is_reg_right_comp_offset)
                return m_b.createLoad(reg_right.var_id);            
        }
    }

    // We need to bridge
    return bridge(reg_left, reg_right, op.swizzle, reg_left_comp_offset, dest_mask);
}

void USSETranslatorVisitor::store(Operand &dest, spv::Id source, std::uint8_t dest_mask, std::uint8_t off) {
    // Composite a new vector
    SpirvReg dest_reg;
    std::uint32_t dest_comp_offset;

    if (!get_spirv_reg(dest.bank, dest.num, off, dest_reg, dest_comp_offset, true)) {
        LOG_ERROR("Can't find dest register {}", disasm::operand_to_str(dest, 0));
        return;
    }

    // If dest has default swizzle, is full-length (all dest component) and starts at a
    // register boundary, translate it to just a createStore

    const auto total_comp_source = static_cast<std::uint8_t>(m_b.getNumComponents(source));
    const auto total_comp_dest = static_cast<std::uint8_t>(m_b.getNumTypeComponents(dest_reg.type_id));

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);

    const bool needs_swizzle = !is_default(dest.swizzle, dest_comp_count);

    // The source needs to fit in both the dest vector and the total write components must be equal to total source components
    const bool full_length = (total_comp_dest == total_comp_source) && (dest_comp_count == total_comp_source);
    const bool starts_at_0 = (dest_comp_offset == 0);

    if (!needs_swizzle && full_length && starts_at_0) {
        m_b.createStore(source, dest_reg.var_id);
        return;
    }

    // Else do the shifting/swizzling with OpVectorShuffle

    std::vector<spv::Id> ops;
    ops.push_back(source);
    ops.push_back(dest_reg.var_id);

    // Total comp = 2, limit mask scan to only x, y
    // Total comp = 3, limit mask scan to only x, y, z
    // So on..
    for (std::uint8_t i = 0; i < total_comp_dest; i++) {
        if (dest_mask & (1 << (dest_comp_offset + i) % 4)) {
            ops.push_back((i + dest_comp_offset % 4) % 4);
        } else {
            // Use original
            ops.push_back(total_comp_source + i);
        }
    }

    auto source_shuffle = m_b.createOp(spv::OpVectorShuffle, dest_reg.type_id, ops);
    m_b.createStore(source_shuffle, dest_reg.var_id);
}

const SpirvVarRegBank &USSETranslatorVisitor::get_reg_bank(RegisterBank reg_bank) const {
    switch (reg_bank) {
    case RegisterBank::PRIMATTR:
        return m_spirv_params.ins;
    case RegisterBank::SECATTR:
        return m_spirv_params.uniforms;
    case RegisterBank::OUTPUT:
        return m_spirv_params.outs;
    case RegisterBank::TEMP:
        return m_spirv_params.temps;
    case RegisterBank::FPINTERNAL:
        return m_spirv_params.internals;
    }

    LOG_WARN("Reg bank {} unsupported", static_cast<uint8_t>(reg_bank));
    return {};
}

spv::Id USSETranslatorVisitor::swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle) {
    switch (swizzle) {
    case SwizzleChannel::_X:
    case SwizzleChannel::_Y:
    case SwizzleChannel::_Z:
    case SwizzleChannel::_W:
        return m_b.createCompositeExtract(composite, type, static_cast<Imm4>(swizzle));

        // TODO: Implement these with OpCompositeExtract
    case SwizzleChannel::_0: break;
    case SwizzleChannel::_1: break;
    case SwizzleChannel::_2: break;

    case SwizzleChannel::_H: break;
    }

    LOG_WARN("Swizzle channel {} unsupported", static_cast<Imm4>(swizzle));
    return spv::NoResult;
}

size_t USSETranslatorVisitor::dest_mask_to_comp_count(Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    assert(bit_count <= 4 && bit_count > 0);
    return bit_count;
}

void USSETranslatorVisitor::emit_non_native_frag_output() {
    Operand pa0_operand;
    pa0_operand.bank = RegisterBank::PRIMATTR;
    pa0_operand.num = 0;
    pa0_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    Operand o0_operand;
    o0_operand.bank = RegisterBank::OUTPUT;
    o0_operand.num = 0;
    o0_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    spv::Id pa0_var = load(pa0_operand, 0xF, 0);
    store(o0_operand, pa0_var, 0xF, 0);
}

bool USSETranslatorVisitor::vmov(
    ExtPredicate pred,
    bool skipinv,
    Imm1 test_bit_2,
    Imm1 src2_bank_sel,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 end_or_src0_bank_ext,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    MoveType move_type,
    RepeatCount repeat_count,
    bool nosched,
    MoveDataType move_data_type,
    Imm1 test_bit_1,
    Imm4 src0_swiz,
    Imm1 src0_bank_sel,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src0_comp_sel,
    Imm4 dest_mask,
    Imm6 dest_n,
    Imm6 src0_n,
    Imm6 src1_n,
    Imm6 src2_n) {
    Instruction inst{};

    static const Opcode tb_decode_vmov[] = {
        Opcode::VMOV,
        Opcode::VMOVC,
        Opcode::VMOVCU8,
        Opcode::INVALID,
    };

    inst.opcode = tb_decode_vmov[(Imm3)move_type];

    std::string disasm_str = fmt::format("{:016x}: {}{}.{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), disasm::move_data_type_str(move_data_type));

    // TODO: dest mask
    // TODO: flags
    // TODO: test type

    const bool is_double_regs = move_data_type == MoveDataType::C10 || move_data_type == MoveDataType::F16 || move_data_type == MoveDataType::F32;
    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);

    // Decode operands
    uint8_t reg_bits = is_double_regs ? 7 : 6;

    inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, is_double_regs, reg_bits);
    inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, is_double_regs, reg_bits);

    // Velocity uses a vec4 table, non-extended, so i assumes type=vec4, extended=false
    inst.opr.src1.swizzle = decode_vec34_swizzle(src0_swiz, false, 2);

    // TODO: adjust dest mask if needed
    if (move_data_type != MoveDataType::F32) {
        LOG_WARN("Data type != F32 unsupported");
        return false;
    }

    if (is_conditional) {
        LOG_WARN("Conditional vmov instructions unsupported");
        return false;
    }

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    // if (is_conditional) {
    //     inst.operands.src0 = decode_src0(src0_n, src0_bank_sel, end_or_src0_bank_ext, is_double_regs);
    //     inst.operands.src2 = decode_src12(src2_n, src2_bank_sel, src2_bank_ext, is_double_regs);
    // }

    disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask));
    LOG_DISASM(disasm_str);

    // Recompile

    m_b.setLine(usse::instr_idx);

    BEGIN_REPEAT(repeat_count, 2)
    spv::Id source = load(inst.opr.src1, dest_mask, repeat_offset);
    store(inst.opr.dest, source, dest_mask, repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::vmad(
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 gpi1_swiz_ext,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src0_bank_ext,
    Imm2 increment_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src0_neg,
    Imm1 src0_abs,
    Imm1 gpi1_neg,
    Imm1 gpi1_abs,
    Imm1 gpi0_swiz_ext,
    Imm2 dest_bank,
    Imm2 src0_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm4 gpi1_swiz,
    Imm2 gpi1_n,
    Imm1 gpi0_neg,
    Imm1 src0_swiz_ext,
    Imm4 src0_swiz,
    Imm6 src0_n) {
    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), "VMAD");

    Instruction inst{};

    // Is this VMAD3 or VMAD4, op2 = 0 => vec3
    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    // Double regs always true for src0, dest
    inst.opr.src0 = decode_src12(src0_n, src0_bank, src0_bank_ext, true, 7);
    inst.opr.dest = decode_dest(dest_n, dest_bank, dest_use_bank_ext, true, 7);

    // GPI0 and GPI1, setup!
    inst.opr.src1.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src1.num = gpi0_n;

    inst.opr.src2.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src2.num = gpi1_n;

    // Swizzleee
    if (type == 1) {
        inst.opr.dest.swizzle[3] = usse::SwizzleChannel::_X;
    }

    inst.opr.src0.swizzle = decode_vec34_swizzle(src0_swiz, src0_swiz_ext, type);
    inst.opr.src1.swizzle = decode_vec34_swizzle(gpi0_swiz, gpi0_swiz_ext, type);
    inst.opr.src2.swizzle = decode_vec34_swizzle(gpi1_swiz, gpi1_swiz_ext, type);

    disasm_str += fmt::format(" {} {} {} {}", disasm::operand_to_str(inst.opr.dest, write_mask), disasm::operand_to_str(inst.opr.src0, write_mask), disasm::operand_to_str(inst.opr.src1, write_mask), disasm::operand_to_str(inst.opr.src2, write_mask));

    LOG_DISASM("{}", disasm_str);
    m_b.setLine(usse::instr_idx);

    // Write mask is a 4-bit immidiate
    // If a bit is one, a swizzle is active
    BEGIN_REPEAT(repeat_count, 2)
    spv::Id vsrc0 = load(inst.opr.src0, write_mask, repeat_offset, src0_abs, src0_neg);
    spv::Id vsrc1 = load(inst.opr.src1, write_mask, repeat_offset, gpi0_abs, gpi0_neg);
    spv::Id vsrc2 = load(inst.opr.src2, write_mask, repeat_offset, gpi1_abs, gpi1_neg);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(inst.opr.dest, add_result, write_mask, repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::vnmad32(
    ExtPredicate pred,
    bool skipinv,
    Imm2 src1_swiz_10_11,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 src1_swiz_9,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm4 src2_swiz,
    bool nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm1 src2_mod,
    Imm2 src1_swiz_7_8,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm6 dest_n,
    Imm7 src1_swiz_0_6,
    Imm3 op2,
    Imm6 src1_n,
    Imm6 src2_n) {
    Instruction inst{};

    static const Opcode tb_decode_vop_f32[] = {
        Opcode::VMUL,
        Opcode::VADD,
        Opcode::VFRC,
        Opcode::VDSX,
        Opcode::VDSY,
        Opcode::VMIN,
        Opcode::VMAX,
        Opcode::VDP,
    };
    static const Opcode tb_decode_vop_f16[] = {
        Opcode::VF16MUL,
        Opcode::VF16ADD,
        Opcode::VF16FRC,
        Opcode::VF16DSX,
        Opcode::VF16DSY,
        Opcode::VF16MIN,
        Opcode::VF16MAX,
        Opcode::VF16DP,
    };
    Opcode opcode;
    const bool is_32_bit = m_instr & (1ull << 59);
    if (is_32_bit)
        opcode = tb_decode_vop_f32[op2];
    else
        opcode = tb_decode_vop_f16[op2];

    LOG_DISASM("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(opcode));

    // Decode operands
    // TODO: modifiers

    inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, true, 7);
    inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, true, 7);
    inst.opr.src2 = decode_src12(src2_n, src2_bank_sel, src2_bank_ext, true, 7);

    const auto src1_swizzle_enc = src1_swiz_0_6 | src1_swiz_7_8 << 7 | src1_swiz_9 << 9 | src1_swiz_10_11 << 10;
    inst.opr.src1.swizzle = decode_swizzle4(src1_swizzle_enc);

    static const Swizzle4 tb_decode_src2_swizzle[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, Z),
        SWIZZLE_CHANNEL_4(X, X, Y, Z),
        SWIZZLE_CHANNEL_4(X, Y, X, Y),
        SWIZZLE_CHANNEL_4(X, Y, W, Z),
        SWIZZLE_CHANNEL_4(Z, X, Y, W),
        SWIZZLE_CHANNEL_4(Z, W, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, X, Z),
        SWIZZLE_CHANNEL_4(X, X, Y, Y),
        SWIZZLE_CHANNEL_4(X, Z, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, 1),
    };

    inst.opr.src2.swizzle = tb_decode_src2_swizzle[src2_swiz];

    // TODO: source modifiers

    auto dest_comp_count = dest_mask_to_comp_count(dest_mask);

    // Recompile

    m_b.setLine(usse::instr_idx);

    spv::Id vsrc1 = load(inst.opr.src1, dest_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, dest_mask, 0);

    if (vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_WARN("Could not find a src register");
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, type_f32_v[dest_comp_count], vsrc1, vsrc2);

    store(inst.opr.dest, mul_result, dest_mask, 0);

    return true;
}

bool USSETranslatorVisitor::vnmad16(
    ExtPredicate pred,
    bool skipinv,
    Imm2 src1_swiz_10_11,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 src1_swiz_9,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm4 src2_swiz,
    bool nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm1 src2_mod,
    Imm2 src1_swiz_7_8,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm6 dest_n,
    Imm7 src1_swiz_0_6,
    Imm3 op2,
    Imm6 src1_n,
    Imm6 src2_n) {
    // TODO
    return vnmad32(pred, skipinv, src1_swiz_10_11, syncstart, dest_bank_ext, src1_swiz_9, src1_bank_ext, src2_bank_ext, src2_swiz, nosched, dest_mask, src1_mod, src2_mod, src1_swiz_7_8, dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_n, src1_swiz_0_6, op2, src1_n, src2_n);
}

bool USSETranslatorVisitor::vpck(
    ExtPredicate pred,
    bool skipinv,
    bool nosched,
    Imm1 src2_bank_sel,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    RepeatCount repeat_count,
    Imm3 src_fmt,
    Imm3 dest_fmt,
    Imm4 dest_mask,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm7 dest_n,
    Imm2 comp_sel_3,
    Imm1 scale,
    Imm2 comp_sel_1,
    Imm2 comp_sel_2,
    Imm5 src1_n,
    Imm1 comp0_sel_bit1,
    Imm4 src2_n,
    Imm1 comp_sel_0_bit0) {
    Instruction inst{};
    const auto FMT_F32 = 6;

    // TODO: Only simple mov-like f32 to f32 supported
    // TODO: Seems like decoding for these is wrong?
    if (src_fmt != FMT_F32 || src_fmt != FMT_F32)
        return true;

    inst.opcode = Opcode::VPCKF32F32;
    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));

    inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, false, 7);
    inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, true, 7);

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    Imm2 comp_sel_0 = comp_sel_0_bit0;
    if (src_fmt == FMT_F32)
        comp_sel_0 |= (comp0_sel_bit1 & 1) << 1;
    else
        comp_sel_0 |= (src2_n & 1) << 1;

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_CAST(comp_sel_0, comp_sel_1, comp_sel_2, comp_sel_3);

    disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask));
    LOG_DISASM(disasm_str);

    // Recompile

    m_b.setLine(usse::instr_idx);

    BEGIN_REPEAT(repeat_count, 2)
    spv::Id source = load(inst.opr.src1, dest_mask, repeat_offset);
    store(inst.opr.dest, source, dest_mask, repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::phas() {
    usse::instr_idx--;
    LOG_DISASM("{:016x}: PHAS", m_instr);
    return true;
}

bool USSETranslatorVisitor::spec(
    bool special,
    SpecialCategory category) {
    usse::instr_idx--;
    usse::instr_idx--; // TODO: Remove?
    LOG_DISASM("{:016x}: SPEC category: {}, special: {}", m_instr, (uint8_t)category, special);
    return true;
}
