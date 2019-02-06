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

#pragma once

#include <shader/types.h>
#include <shader/usse_decode_helpers.h>
#include <shader/usse_disasm.h>
#include <util/log.h>

#include <SPIRV/SpvBuilder.h>
#include <boost/optional.hpp>
#include <spdlog/fmt/fmt.h>
#include <spirv.hpp>

#include <iostream>
#include <tuple>

using namespace USSE;
using boost::optional;

namespace shader {
namespace usse {

// For debugging SPIR-V output
static uint32_t instr_idx = 0;

class USSETranslatorVisitor final {
public:
    using instruction_return_type = bool;

    spv::Id spv_f32;
    spv::Id spv_vf32s[4];
    spv::Id spv_fconst[4];
    spv::Id spv_v4const[4];

    USSETranslatorVisitor() = delete;
    explicit USSETranslatorVisitor(spv::Builder &_b, const uint64_t &_instr, const SpirvShaderParameters &spirv_params, emu::SceGxmProgramType program_type)
        : m_b(_b)
        , m_instr(_instr)
        , m_spirv_params(spirv_params)
        , m_program_type(program_type) {
        // Build common type here, so builder won't have to look it up later
        spv_f32 = m_b.makeFloatType(32);

        spv_fconst[3] = m_b.makeFpConstant(spv_f32, 0.5f);
        spv_fconst[0] = m_b.makeFpConstant(spv_f32, 0.0f);
        spv_fconst[1] = m_b.makeFpConstant(spv_f32, 1.0f);
        spv_fconst[2] = m_b.makeFpConstant(spv_f32, 2.0f);

        for (std::uint8_t i = 0; i < 4; i++) {
            spv_vf32s[i] = m_b.makeVectorType(spv_f32, i + 1);
            spv_v4const[i] = m_b.makeCompositeConstant(spv_vf32s[i],
                { spv_fconst[i], spv_fconst[i], spv_fconst[i], spv_fconst[i] });
        }
    }

private:
#define BEGIN_REPEAT(repeat_count, roj)                         \
    const auto repeat_count_num = (uint8_t)repeat_count + 1;    \
    const auto repeat_offset_jump = roj;                        \
    for (auto repeat_offset = 0;                                \
         repeat_offset < repeat_count_num * repeat_offset_jump; \
         repeat_offset += repeat_offset_jump) {
#define END_REPEAT() }

    spv::Id mix(SpirvReg &reg1, SpirvReg &reg2, USSE::Swizzle4 swizz, const std::uint32_t shift_offset, const std::uint32_t max_clamp = 20) {
        uint32_t scomp[4];

        // Queue keep track of components to be modified with given constant
        struct constant_queue_member {
            std::uint32_t index;
            spv::Id constant;
        };

        std::vector<constant_queue_member> constant_queue;

        for (int i = 0; i < 4; i++) {
            switch (swizz[i]) {
            case USSE::SwizzleChannel::_X:
            case USSE::SwizzleChannel::_Y:
            case USSE::SwizzleChannel::_Z:
            case USSE::SwizzleChannel::_W: {
                scomp[i] = std::min((uint32_t)swizz[i] + shift_offset, max_clamp);
                break;
            }

            case USSE::SwizzleChannel::_1: case USSE::SwizzleChannel::_0:
            case USSE::SwizzleChannel::_2: case USSE::SwizzleChannel::_H: {
                scomp[i] = (uint32_t)USSE::SwizzleChannel::_X;
                constant_queue_member member;
                member.index = i;
                member.constant = spv_fconst[(uint32_t)swizz[i] - (uint32_t)USSE::SwizzleChannel::_0];

                constant_queue.push_back(std::move(member));

                break;
            }

            default: {
                LOG_ERROR("Unsupport swizzle type");
                break;
            }
            }
        }

        auto shuff_result = m_b.createOp(spv::OpVectorShuffle, spv_vf32s[3],
            { reg1.var_id, reg2.var_id, scomp[0], scomp[1], scomp[2], scomp[3] });

        for (std::size_t i = 0; i < constant_queue.size(); i++) {
            shuff_result = m_b.createOp(spv::OpVectorInsertDynamic, spv_vf32s[3],
                { shuff_result, constant_queue[i].constant,  constant_queue[i].index });
        }

        return shuff_result;
    }

    /*
     * \brief Given an operand, load it and returns a SPIR-V vec4
     * 
     * \returns A vec4 copy of given operand
    */
    spv::Id load(Operand &op, const std::uint8_t offset = 0, bool /* abs */ = false, bool /* neg */ = false) {
        // Optimization: Check for constant swizzle and emit it right away
        for (std::uint8_t i = 0 ; i < 4; i++) {
            USSE::SwizzleChannel channel = static_cast<USSE::SwizzleChannel>(
                static_cast<std::uint32_t>(USSE::SwizzleChannel::_0) + i);
    
            if (op.swizzle == USSE::Swizzle4 { channel, channel, channel, channel }) {
                return spv_v4const[i];
            }
        }

        // TODO: Array (OpAccessChain)
        const SpirvVarRegBank &bank = get_reg_bank(op.bank);

        // Composite a new vector
        SpirvReg reg1;
        std::uint32_t out_comp_offset;
        std::uint32_t reg_offset = op.num;

        switch (op.bank) {
        case USSE::RegisterBank::FPINTERNAL: {
            // Each GPI register is 128 bits long = 16 bytes long
            // So we need to multiply the reg index with 16 in order to get the right offset
            reg_offset *= 16;
            break;
        }

        default:
            break;
        }

        bool result = bank.find_reg_at(reg_offset + offset, reg1, out_comp_offset);
        if (!result) {
            LOG_ERROR("Can't load register {}", disasm::operand_to_str(op, 0));
            return spv::NoResult;
        }

        // If the register is already aligned
        if (out_comp_offset == 0) {
            // If it's not swizzled up
            if (op.swizzle == USSE::Swizzle4 SWIZZLE_CHANNEL_4_DEFAULT) {
                return m_b.createLoad(reg1.var_id);
            }
        }

        // Get the next reg, so we can do a bridge
        SpirvReg reg2;
        std::uint32_t temp = 0;
        result = bank.find_reg_at(reg_offset + offset + reg1.size, reg2, temp);

        uint32_t maximum_border = 20;

        // There is no more register to bridge to. For making it valid, just
        // throws in a valid register and limit swizzle offset to 3
        //
        // I haven't think of any edge case, but this should be looked when there is
        // any problems with bridging
        if (!result) {
            reg2 = reg1;
            maximum_border = 3;
        }

        return mix(reg1, reg2, op.swizzle, offset, maximum_border);
    }

    void store(Operand &dest, spv::Id source, std::uint8_t write_mask = 0xFF, std::uint8_t off = 0) {
        const SpirvVarRegBank &bank = get_reg_bank(dest.bank);

        // Composite a new vector
        SpirvReg dest_reg;
        std::uint32_t out_comp_offset;
        std::uint32_t reg_offset = dest.num;

        switch (dest.bank) {
        case USSE::RegisterBank::FPINTERNAL: {
            reg_offset *= 16;
            break;
        }

        default:
            break;
        }

        bool result = bank.find_reg_at(reg_offset + off, dest_reg, out_comp_offset);
        if (!result) {
            LOG_ERROR("Can't find dest register {}", disasm::operand_to_str(dest, 0));
            return;
        }

        std::vector<spv::Id> ops;
        ops.push_back(source);
        ops.push_back(dest_reg.var_id);

        int bitwrite_count = 0;
        int total_comp = m_b.getNumTypeComponents(dest_reg.type_id);

        // Total comp = 2, limit mask scan to only x, y
        // Total comp = 3, limit mask scan to only x, y, z
        // So on..
        for (std::uint8_t i = 0; i < total_comp; i++) {
            if (write_mask & (1 << ((off + i) % 4))) {
                ops.push_back((i + out_comp_offset % 4) % 4);
                bitwrite_count++;
            } else {
                // Use original
                ops.push_back(4 + i);
            }
        }

        auto source_shuffle = m_b.createOp(spv::OpVectorShuffle, dest_reg.type_id, ops);
        m_b.createStore(source_shuffle, dest_reg.var_id);
    }

    //
    // Translation helpers
    //
    const SpirvVarRegBank &get_reg_bank(RegisterBank reg_bank) const {
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

    spv::Id swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle) {
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

public:
    //
    // Instructions
    //
    bool vnmad(ExtPredicate pred, bool skipinv, Imm2 src1_swiz_10_11, bool syncstart, Imm1 dest_bank_ext, Imm1 src1_swiz_9, Imm1 src1_bank_ext, Imm1 src2_bank_ext, Imm4 src2_swiz, bool nosched, DestinationMask dest_mask, Imm2 src1_mod, Imm1 src2_mod, Imm2 src1_swiz_7_8, Imm2 dest_bank_sel, Imm2 src1_bank_sel, Imm2 src2_bank_sel, Imm6 dest_n, Imm7 src1_swiz_0_6, Imm3 op2, Imm6 src1_n, Imm6 src2_n) {
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

        // Recompile

        m_b.setLine(usse::instr_idx);

        return true;
    }

    bool vmov(ExtPredicate pred, bool skipinv, bool test_2, Imm1 src2_bank_sel, bool syncstart, Imm1 dest_bank_ext, Imm1 end_or_src0_bank_ext, Imm1 src1_bank_ext, Imm1 src2_bank_ext, MoveType move_type, RepeatCount repeat_count, bool nosched, MoveDataType move_data_type, bool test_1, Imm4 src_swiz, Imm1 src0_bank_sel, Imm2 dest_bank_sel, Imm2 src1_bank_sel, Imm2 src0_comp_sel, DestinationMask dest_mask, Imm6 dest_n, Imm6 src0_n, Imm6 src1_n, Imm6 src2_n) {
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
        inst.opr.src1.swizzle = decode_vec34_swizzle(src_swiz, false, 2);

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

        disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask.val), disasm::operand_to_str(inst.opr.src1, dest_mask.val));
        LOG_DISASM(disasm_str);

        // Recompile

        m_b.setLine(usse::instr_idx);

        BEGIN_REPEAT(repeat_count, 2)
        spv::Id source = load(inst.opr.src1, repeat_offset);
        store(inst.opr.dest, source, dest_mask.val, repeat_offset);
        END_REPEAT()

        return true;
    }

    bool vpck(ExtPredicate pred, bool skipinv, bool nosched, Imm1 src2_bank_sel, bool syncstart, Imm1 dest_bank_ext, Imm1 end, Imm1 src1_bank_ext, Imm2 src2_bank_ext, RepeatCount repeat_count, Imm3 src_fmt, Imm3 dest_fmt, DestinationMask dest_mask, Imm2 dest_bank_sel, Imm2 src1_bank_sel, Imm7 dest_n, Imm2 comp_sel_3, Imm1 scale, Imm2 comp_sel_1, Imm2 comp_sel_2, Imm5 src1_n, Imm1 comp0_sel_bit1, Imm4 src2_n, Imm1 comp_sel_0_bit0) {
        Instruction inst{};
        const auto FMT_F32 = 6;

        // TODO: Only simple mov-like f32 to f32 supported
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

        disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask.val), disasm::operand_to_str(inst.opr.src1, dest_mask.val));
        LOG_DISASM(disasm_str);

        // Recompile

        m_b.setLine(usse::instr_idx);

        BEGIN_REPEAT(repeat_count, 2)
        spv::Id source = load(inst.opr.src1, repeat_offset);
        store(inst.opr.dest, source, dest_mask.val, repeat_offset);
        END_REPEAT()

        return true;
    }

    bool vmad(ExtPredicate predicate, bool skipinv, Imm1 gpi1_swizz_extended, Imm1 opcode2, Imm1 dest_use_extended_bank, Imm1 end, Imm1 src0_extended_bank, Imm2 increment_mode, Imm1 gpi0_abs, RepeatCount repeat_count, bool no_sched, Imm4 write_mask, Imm1 src0_neg, Imm1 src0_abs, Imm1 gpi1_neg, Imm1 gpi1_abs, Imm1 gpi0_swizz_extended, Imm2 dest_bank, Imm2 src0_bank, Imm2 gpi0_n, Imm6 dest_n, Imm4 gpi0_swizz, Imm4 gpi1_swizz, Imm2 gpi1_n, Imm1 gpi0_neg, Imm1 src0_swizz_extended, Imm4 src0_swizz, Imm6 src0_n) {
        std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(predicate), "VMAD");

        Instruction inst{};

        // Is this VMAD3 or VMAD4, op2 = 0 => vec3
        int type = 2;

        if (opcode2 == 0) {
            type = 1;
        }

        // Double regs always true for src0, dest
        inst.opr.src0 = decode_src12(src0_n, src0_bank, src0_extended_bank, true, 7);
        inst.opr.dest = decode_dest(dest_n, dest_bank, dest_use_extended_bank, true, 7);

        // GPI0 and GPI1, setup!
        inst.opr.src1.bank = USSE::RegisterBank::FPINTERNAL;
        inst.opr.src1.num = gpi0_n;

        inst.opr.src2.bank = USSE::RegisterBank::FPINTERNAL;
        inst.opr.src2.num = gpi1_n;

        // Swizzleee
        if (type == 1) {
            inst.opr.dest.swizzle[3] = USSE::SwizzleChannel::_X;
        }

        inst.opr.src0.swizzle = decode_vec34_swizzle(src0_swizz, src0_swizz_extended, type);
        inst.opr.src1.swizzle = decode_vec34_swizzle(gpi0_swizz, gpi0_swizz_extended, type);
        inst.opr.src2.swizzle = decode_vec34_swizzle(gpi1_swizz, gpi1_swizz_extended, type);

        disasm_str += fmt::format(" {} {} {} {}", disasm::operand_to_str(inst.opr.dest, write_mask), disasm::operand_to_str(inst.opr.src0, write_mask), disasm::operand_to_str(inst.opr.src1, write_mask), disasm::operand_to_str(inst.opr.src2, write_mask));

        LOG_DISASM("{}", disasm_str);
        m_b.setLine(usse::instr_idx);

        // Write mask is a 4-bit immidiate
        // If a bit is one, a swizzle is active
        BEGIN_REPEAT(repeat_count, 2)
        spv::Id vsrc0 = load(inst.opr.src0, repeat_offset, src0_abs, src0_neg);
        spv::Id vsrc1 = load(inst.opr.src1, repeat_offset, gpi0_abs, gpi0_neg);
        spv::Id vsrc2 = load(inst.opr.src2, repeat_offset, gpi1_abs, gpi1_neg);

        if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
            return false;
        }

        auto mul_result = m_b.createBinOp(spv::OpFMul, spv_vf32s[3], vsrc0, vsrc1);
        auto add_result = m_b.createBinOp(spv::OpFAdd, spv_vf32s[3], mul_result, vsrc2);

        store(inst.opr.dest, add_result, write_mask, repeat_offset);
        END_REPEAT()

        return true;
    }

    bool special(bool special, SpecialCategory category) {
        usse::instr_idx--;
        usse::instr_idx--; // TODO: Remove?
        LOG_DISASM("{:016x}: SPEC category: {}, special: {}", m_instr, (uint8_t)category, special);
        return true;
    }
    bool special_phas() {
        usse::instr_idx--;
        LOG_DISASM("{:016x}: PHAS", m_instr);
        return true;
    }

private:
    // SPIR-V emitter
    spv::Builder &m_b;

    // Instruction word being translated
    const uint64_t &m_instr;

    // SPIR-V IDs
    const SpirvShaderParameters &m_spirv_params;

    // Shader program type
    emu::SceGxmProgramType m_program_type;
};

} // namespace usse
} // namespace shader
