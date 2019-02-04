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

    USSETranslatorVisitor() = delete;
    explicit USSETranslatorVisitor(spv::Builder &_b, const uint64_t &_instr, const SpirvShaderParameters &spirv_params, emu::SceGxmProgramType program_type)
        : m_b(_b)
        , m_instr(_instr)
        , m_spirv_params(spirv_params)
        , m_program_type(program_type) {}

private:
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

        inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, true);
        inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, true);
        inst.opr.src2 = decode_src12(src2_n, src2_bank_sel, src2_bank_ext, true);

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

    bool vmov(ExtPredicate pred, bool skipinv, bool test_2, Imm1 src2_bank_sel, bool syncstart, Imm1 dest_bank_ext, Imm1 end_or_src0_ext_bank, Imm1 src1_bank_ext, Imm1 src2_bank_ext, MoveType move_type, RepeatCount repeat_count, bool nosched, MoveDataType move_data_type, bool test_1, Imm4 src_swiz, Imm1 src0_bank_sel, Imm2 dest_bank_sel, Imm2 src1_bank_sel, Imm2 src0_comp_sel, DestinationMask dest_mask, Imm6 dest_n, Imm6 src0_n, Imm6 src1_n, Imm6 src2_n) {
        Instruction inst{};

        static const Opcode tb_decode_vmov[] = {
            Opcode::VMOV,
            Opcode::VMOVC,
            Opcode::VMOVCU8,
            Opcode::INVALID,
        };

        inst.opcode = tb_decode_vmov[(Imm3)move_type];

        LOG_DISASM("{:016x}: {}{}.{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), disasm::move_data_type_str(move_data_type));

        // TODO: dest mask
        // TODO: flags
        // TODO: test type

        const bool is_double_regs = move_data_type == MoveDataType::C10 || move_data_type == MoveDataType::F16 || move_data_type == MoveDataType::F32;
        const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);

        // Decode operands

        inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, is_double_regs);
        inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, is_double_regs);

        static const Swizzle4 tb_decode_src1_swizzle[] = {
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

        // TODO(?): Do this in decode_*
        inst.opr.src1.swizzle = tb_decode_src1_swizzle[src_swiz];

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
        //     inst.operands.src0 = decode_src0(src0_n, src0_bank_sel, end_or_src0_ext_bank, is_double_regs);
        //     inst.operands.src2 = decode_src12(src2_n, src2_bank_sel, src2_bank_ext, is_double_regs);
        // }

        // Recompile

        m_b.setLine(usse::instr_idx);

        auto src_swz = inst.opr.src1.swizzle;

        const SpirvVarRegBank &dest_regs = get_reg_bank(inst.opr.dest.bank);
        const SpirvVarRegBank &src_regs = get_reg_bank(inst.opr.src1.bank);

        // TODO: There can be more than one source regs

        const auto spv_float = m_b.makeFloatType(32);
        const auto spv_vec4 = m_b.makeVectorType(spv_float, 4);

        const auto repeat_count_num = (uint8_t)repeat_count + 1;
        const auto repeat_offset_jump = 2; // use dest_mask # of bits?

        for (auto repeat_offset = 0;
             repeat_offset < repeat_count_num * repeat_offset_jump;
             repeat_offset += repeat_offset_jump) {
            SpirvReg src_reg;
            SpirvReg dest_reg;
            uint32_t src_comp_offset;
            uint32_t dest_comp_offset;

            if (!src_regs.find_reg_at(inst.opr.src1.num + repeat_offset, src_reg, src_comp_offset)) {
                LOG_WARN("Register with num {} (src1) not found.", log_hex(inst.opr.src1.num));
                return false;
            }
            if (!dest_regs.find_reg_at(inst.opr.dest.num + repeat_offset, dest_reg, dest_comp_offset)) {
                LOG_WARN("Register with num {} (dest) not found.", log_hex(inst.opr.dest.num));
                return false;
            }

            const auto src_var_id = src_reg.var_id;
            const auto dest_var_id = dest_reg.var_id;

            uint32_t shuffle_components[4];
            for (auto c = 0; c < 4; ++c) {
                auto cycling_dest_mask_offset = (c + (uint8_t)repeat_offset) % 4;

                if (dest_mask.val & (1 << cycling_dest_mask_offset)) {
                    // use modified, source component
                    shuffle_components[c] = (uint32_t)src_swz[c];
                } else {
                    // use original, dest component
                    shuffle_components[c] = 4 + c;
                }
            }

            auto shuffle = m_b.createOp(spv::OpVectorShuffle, spv_vec4,
                { src_var_id, dest_var_id, shuffle_components[0], shuffle_components[1], shuffle_components[2], shuffle_components[3] });

            m_b.createStore(shuffle, dest_var_id);
        }

        return true;
    }

    bool vpck(ExtPredicate pred, bool skipinv, bool nosched, Imm1 src2_bank_sel, bool syncstart, Imm1 end, Imm1 src1_ext_bank, Imm2 src2_ext_bank, RepeatCount repeat_count, Imm3 src_fmt, Imm3 dest_fmt, DestinationMask dest_mask, Imm2 src1_bank_sel, Imm5 dest_n, Imm2 comp_sel_3, Imm1 scale, Imm2 comp_sel_1, Imm2 comp_sel_2, Imm5 src1_n, Imm1 comp0_sel_bit1, Imm4 src2_n, Imm1 comp_sel_0_bit0) {
        Instruction inst{};

        LOG_DISASM("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), "VPCK");
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
