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

#include <shader/usse_translator.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

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
    std::string disasm_str = "{:016x}: SMLSI ";

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
            // Parse value as immidiate
            for (int i = 0; i < 4; i++) {
                repeat_increase[idx][i] = i * static_cast<std::int8_t>(inc_value);
            }

            disasm_str += fmt::format(" inc.{} ", static_cast<std::int8_t>(inc_value));
        }
    };

    parse_increment(3, dest_inc_mode, dest_inc);
    parse_increment(0, src0_inc_mode, src0_inc);
    parse_increment(1, src1_inc_mode, src1_inc);
    parse_increment(2, src2_inc_mode, src2_inc);

    LOG_DISASM(disasm_str, m_instr);

    return true;
}

bool USSETranslatorVisitor::kill(
    ShortPredicate pred) {
    LOG_DISASM("{:016x}: KILL {}", m_instr, disasm::s_predicate_str(pred));

    auto cur_pc = m_recompiler.cur_pc;

    spv::Function *no_kill_block = m_recompiler.get_or_recompile_block(m_recompiler.avail_blocks[cur_pc + 1]);

    m_b.setLine(m_recompiler.cur_pc);

    if (pred == ShortPredicate::NONE) {
        m_b.createFunctionCall(no_kill_block, {});
    } else {
        spv::Id pred_v = spv::NoResult;

        Operand pred_opr{};
        pred_opr.bank = RegisterBank::PREDICATE;

        bool do_neg = false;

        if (pred >= ShortPredicate::P0 && pred <= ShortPredicate::P1) {
            pred_opr.num = static_cast<int>(pred) - static_cast<int>(ShortPredicate::P0);
        } else {
            pred_opr.num = static_cast<int>(pred) - static_cast<int>(ShortPredicate::NEGP0);
            do_neg = true;
        }

        pred_v = load(pred_opr, 0b0001);

        if (pred_v == spv::NoResult) {
            LOG_ERROR("Pred not loaded");
            return false;
        }

        if (do_neg) {
            std::vector<spv::Id> ops{ pred_v };
            pred_v = m_b.createOp(spv::OpLogicalNot, m_b.makeBoolType(), ops);
        }

        spv::Function *continous_block = m_recompiler.get_or_recompile_block(m_recompiler.avail_blocks[cur_pc + 1]);
        spv::Builder::If cond_builder(pred_v, spv::SelectionControlMaskNone, m_b);
        m_b.createFunctionCall(no_kill_block, {});
        cond_builder.makeBeginElse();
        m_b.makeDiscard();
        cond_builder.makeEndIf();
    }

    return true;
}