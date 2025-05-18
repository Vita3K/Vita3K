// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <SPIRV/SpvBuilder.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>

using namespace shader;
using namespace usse;

bool USSETranslatorVisitor::phas(
    Imm1 sprvv,
    Imm1 end,
    Imm1 imm,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm1 mode,
    Imm1 rate_hi,
    Imm1 rate_lo_or_nosched,
    Imm3 wait_cond,
    Imm8 temp_count,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm6 exe_addr_high,
    Imm7 src1_n_or_exe_addr_mid,
    Imm7 src2_n_or_exe_addr_low) {
    LOG_DISASM("{:016x}: PHAS", m_instr);
    return true;
}

bool USSETranslatorVisitor::nop() {
    LOG_DISASM("{:016x}: NOP", m_instr);
    return true;
}

bool USSETranslatorVisitor::spec(
    bool special,
    SpecialCategory category) {
    LOG_DISASM("{:016x}: SPEC category: {}, special: {}", m_instr, (uint8_t)category, special);
    return true;
}

bool USSETranslatorVisitor::depthf(
    bool sync,
    bool src0_bank_ext,
    bool end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    bool nosched,
    ShortPredicate pred,
    bool two_sided,
    Imm2 feedback,
    Imm1 src0_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    Instruction inst;
    inst.opcode = Opcode::DEPTHF;

    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, src0_bank_ext, true, 7, m_second_program);
    // TODO: Figure out how/when to use src1 and src2
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::s_predicate_str(pred), disasm::opcode_str(inst.opcode),
        disasm::operand_to_str(inst.opr.src0, 0b0001, 0), disasm::operand_to_str(inst.opr.src1, 0b0001, 0),
        disasm::operand_to_str(inst.opr.src2, 0b0001, 0));

    m_b.setDebugSourceLocation(m_recompiler.cur_pc, nullptr);

    if (frag_depth_id == 0) {
        frag_depth_id = m_b.createVariable(spv::NoPrecision, spv::StorageClassOutput, type_f32, "gl_FragDepth");
        m_b.addDecoration(frag_depth_id, spv::DecorationBuiltIn, spv::BuiltInFragDepth);
    }

    spv::Id depth = load(inst.opr.src0, 0b1);
    m_b.createStore(depth, frag_depth_id);

    return true;
}

bool USSETranslatorVisitor::smlsi(
    Imm1 nosched,
    Imm4 temp_limit,
    Imm4 pa_limit,
    Imm4 sa_limit,
    Imm1 dest_inc_mode,
    Imm1 src0_inc_mode,
    Imm1 src1_inc_mode,
    Imm1 src2_inc_mode,
    Imm8 dest_inc,
    Imm8 src0_inc,
    Imm8 src1_inc,
    Imm8 src2_inc) {
    std::string disasm_str = fmt::format("{:016x}: SMLSI ", m_instr);

    auto parse_increment = [&](const int idx, const Imm1 inc_mode, const Imm8 inc_value) {
        if (inc_mode) {
            disasm_str += "swizz.(";

            // Parse value as swizzle
            for (int i = 0; i < 4; i++) {
                repeat_increase[idx][i] = ((inc_value >> (2 * i)) & 0b11);
                disasm_str += fmt::format("{}", repeat_increase[idx][i]);
            }

            disasm_str += ") ";
        } else {
            // Parse value as immediate
            for (int i = 0; i < 17; i++) {
                repeat_increase[idx][i] = i * static_cast<std::int8_t>(inc_value);
            }

            disasm_str += fmt::format(" inc.{} ", static_cast<std::int8_t>(inc_value));
        }
    };

    parse_increment(3, dest_inc_mode, dest_inc);
    parse_increment(0, src0_inc_mode, src0_inc);
    parse_increment(1, src1_inc_mode, src1_inc);
    parse_increment(2, src2_inc_mode, src2_inc);

    LOG_DISASM("{}", disasm_str);

    return true;
}

bool USSETranslatorVisitor::smbo(Imm1 nosched,
    Imm12 dest_offset,
    Imm12 src0_offset,
    Imm12 src1_offset,
    Imm12 src2_offset) {
    LOG_DISASM("{:016x}: SMBO {}, {}, {}, {}", m_instr, dest_offset, src0_offset, src1_offset, src2_offset);

    m_b.setDebugSourceLocation(m_recompiler.cur_pc, nullptr);

    auto parse_offset = [&](const int idx, Imm12 offset) {
        for (int i = 0; i < 17; i++) {
            repeat_increase[idx][i] = i + offset;
        }
    };
    parse_offset(3, dest_offset);
    parse_offset(0, src0_offset);
    parse_offset(1, src1_offset);
    parse_offset(2, src2_offset);

    return true;
}

bool USSETranslatorVisitor::kill(
    ShortPredicate pred) {
    LOG_DISASM("{:016x}: KILL {}", m_instr, disasm::s_predicate_str(pred));

    m_b.setDebugSourceLocation(m_recompiler.cur_pc, nullptr);
    m_b.makeStatementTerminator(spv::OpKill, "kill");

    return true;
}
