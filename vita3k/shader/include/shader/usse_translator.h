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

#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_utilities.h>
#include <util/preprocessor.h>
#include <util/optional.h>

#include <SPIRV/SpvBuilder.h>

#include <array>
#include <map>

struct FeatureState;

namespace shader::usse {

// For debugging SPIR-V output
static uint32_t instr_idx = 0;
constexpr std::size_t max_sa_registers = 128;

struct USSERecompiler;

class USSETranslatorVisitor final {
public:
    using instruction_return_type = bool;

    spv::Id std_builtins;

    spv::Id type_f32;
    spv::Id type_ui32;
    spv::Id type_f32_v[5]; // Starts from 1 ([1] is vec 1)
    spv::Id const_f32[4];

    spv::Id const_f32_v0[5];

    utils::SpirvUtilFunctions m_util_funcs;

    spv::Block *main_block;
    spv::Id out;

    // Contains repeat increasement offset
    int repeat_increase[4][4];

    void do_texture_queries(const NonDependentTextureQueryCallInfos &texture_queries);
    spv::Id do_fetch_texture(const spv::Id tex, const Coord &coord, const DataType dest_type);

    USSETranslatorVisitor() = delete;
    explicit USSETranslatorVisitor(spv::Builder &_b, USSERecompiler &_recompiler, const SceGxmProgram &program, const FeatureState &features,
        utils::SpirvUtilFunctions &utils, const uint64_t &_instr, const SpirvShaderParameters &spirv_params, const NonDependentTextureQueryCallInfos &queries,
        bool is_secondary_program = false)
        : m_util_funcs(utils)
        , m_second_program(is_secondary_program)
        , m_b(_b)
        , m_instr(_instr)
        , m_spirv_params(spirv_params)
        , m_recompiler(_recompiler)
        , m_program(program)
        , m_features(features) {
        // Default increase mode
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                repeat_increase[i][j] = j;
            }
        }

        out = spv::NoResult;

        // Set main block
        main_block = m_b.getBuildPoint();

        // Import GLSL.std.450
        std_builtins = m_b.import("GLSL.std.450");

        // Build common type here, so builder won't have to look it up later
        type_f32 = m_b.makeFloatType(32);
        type_ui32 = m_b.makeUintType(32);

        const_f32[3] = m_b.makeFloatConstant(0.5f);
        const_f32[0] = m_b.makeFloatConstant(0.0f);
        const_f32[1] = m_b.makeFloatConstant(1.0f);
        const_f32[2] = m_b.makeFloatConstant(2.0f);

        for (std::uint8_t i = 1; i < 5; i++) {
            type_f32_v[i] = m_b.makeVectorType(type_f32, i);

            std::vector<spv::Id> consts;

            for (std::uint8_t j = 1; j < i + 1; j++) {
                consts.push_back(const_f32[0]);
            }

            const_f32_v0[i] = m_b.makeCompositeConstant(type_f32_v[i], consts);
        }

        do_texture_queries(queries);
    }

    /*
     * \brief Given an operand, load it and returns a SPIR-V vector with total components count equals to total bit set in
     *        write/dest mask
     *
     * \returns A copy of given operand
    */
    spv::Id load(Operand op, const Imm4 dest_mask, int shift_offset = 0);

private:
    //
    // Translation helpers
    //
#define BEGIN_REPEAT_COMPLEX(repeat_count, dest_jump, src_jump) \
    const auto repeat_count_num = (uint8_t)repeat_count + 1;    \
    constexpr auto dest_repeat_jump = dest_jump;                \
    constexpr auto src_repeat_jump = src_jump;                  \
    for (auto current_repeat = 0; current_repeat < repeat_count_num; current_repeat++) {
#define BEGIN_REPEAT(repeat_count, jump) BEGIN_REPEAT_COMPLEX(repeat_count, jump, jump)
#define END_REPEAT() }

#define GET_REPEAT(inst)                                                                          \
    int dest_repeat_offset = get_repeat_offset(inst.opr.dest, current_repeat) * dest_repeat_jump; \
    int src0_repeat_offset = get_repeat_offset(inst.opr.src0, current_repeat) * src_repeat_jump;  \
    int src1_repeat_offset = get_repeat_offset(inst.opr.src1, current_repeat) * src_repeat_jump;  \
    int src2_repeat_offset = get_repeat_offset(inst.opr.src2, current_repeat) * src_repeat_jump;  \
    if (inst.opr.dest.bank == RegisterBank::FPINTERNAL)                                           \
        dest_repeat_offset /= dest_repeat_jump;                                                   \
    if (inst.opr.src0.bank == RegisterBank::FPINTERNAL)                                           \
        src0_repeat_offset /= src_repeat_jump;                                                    \
    if (inst.opr.src1.bank == RegisterBank::FPINTERNAL)                                           \
        src1_repeat_offset /= src_repeat_jump;                                                    \
    if (inst.opr.src2.bank == RegisterBank::FPINTERNAL)                                           \
        src2_repeat_offset /= src_repeat_jump;

    const int get_repeat_offset(Operand &op, const std::uint8_t repeat_index) {
        return repeat_increase[op.index][repeat_index];
    }

    void store(Operand dest, spv::Id source, std::uint8_t dest_mask = 0xFF, int shift_offset = 0);
    spv::Id swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle);

    // TODO: Separate file for translator helpers?
    static size_t dest_mask_to_comp_count(Imm4 dest_mask);

    bool m_second_program{ false };

    spv::Id do_alu_op(Instruction &inst, const Imm4 dest_mask);

public:
    void set_secondary_program(const bool is_it) {
        m_second_program = is_it;
    }

    bool is_translating_secondary_program() {
        return m_second_program;
    }

    //
    // Instructions
    //
    bool vmov(
        ExtPredicate pred,
        bool skipinv,
        Imm1 test_bit_2,
        Imm1 src0_comp_sel,
        bool syncstart,
        Imm1 dest_bank_ext,
        Imm1 end_or_src0_bank_ext,
        Imm1 src1_bank_ext,
        Imm1 src2_bank_ext,
        MoveType move_type,
        RepeatCount repeat_count,
        bool nosched,
        DataType move_data_type,
        Imm1 test_bit_1,
        Imm4 src0_swiz,
        Imm1 src0_bank_sel,
        Imm2 dest_bank_sel,
        Imm2 src1_bank_sel,
        Imm2 src2_bank_sel,
        Imm4 dest_mask,
        Imm6 dest_n,
        Imm6 src0_n,
        Imm6 src1_n,
        Imm6 src2_n);

    bool vmad(
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
        Imm6 src0_n);

    bool vmad2(
        Imm1 dat_fmt,
        Imm2 pred,
        Imm1 skipinv,
        Imm1 src0_swiz_bits2,
        Imm1 syncstart,
        Imm1 src0_abs,
        Imm1 src1_bank_ext,
        Imm1 src2_bank_ext,
        Imm3 src2_swiz,
        Imm1 src1_swiz_bit2,
        Imm1 nosched,
        Imm4 dest_mask,
        Imm2 src1_mod,
        Imm2 src2_mod,
        Imm1 src0_bank,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm6 dest_n,
        Imm2 src1_swiz_bits01,
        Imm2 src0_swiz_bits01,
        Imm6 src0_n,
        Imm6 src1_n,
        Imm6 src2_n);

    bool vnmad32(
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
        Imm6 src2_n);

    bool vnmad16(
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
        Imm6 src2_n);

    bool sop2(
        Imm2 pred,
        Imm1 cmod1,
        Imm1 skipinv,
        Imm1 nosched,
        Imm2 asel1,
        Imm1 dest_bank_ext,
        Imm1 end,
        Imm1 src1_bank_ext,
        Imm1 src2_bank_ext,
        Imm1 cmod2,
        Imm3 count,
        Imm1 amod1,
        Imm2 asel2,
        Imm3 csel1,
        Imm3 csel2,
        Imm1 amod2,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm1 src1_mod,
        Imm2 cop,
        Imm2 aop,
        Imm1 asrc1_mod,
        Imm1 dest_mod,
        Imm7 src1_n,
        Imm7 src2_n);

    bool vpck(
        ExtPredicate pred,
        bool skipinv,
        bool nosched,
        Imm1 unknown,
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
        Imm2 src2_bank_sel,
        Imm7 dest_n,
        Imm2 comp_sel_3,
        Imm1 scale,
        Imm2 comp_sel_1,
        Imm2 comp_sel_2,
        Imm6 src1_n,
        Imm1 comp0_sel_bit1,
        Imm6 src2_n,
        Imm1 comp_sel_0_bit0);

    bool vbw(
        Imm3 op1,
        ExtPredicate pred,
        Imm1 skipinv,
        Imm1 nosched,
        Imm1 repeat_count,
        Imm1 sync_start,
        Imm1 dest_ext,
        Imm1 end,
        Imm1 src1_ext,
        Imm1 src2_ext,
        Imm4 mask_count,
        Imm1 src2_invert,
        Imm5 src2_rot,
        Imm2 src2_exth,
        Imm1 op2,
        Imm1 bitwise_partial,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm7 src2_sel,
        Imm7 src1_n,
        Imm7 src2_n);

    bool vcomp(
        ExtPredicate pred,
        bool skipinv,
        Imm2 dest_type,
        bool syncstart,
        bool dest_bank_ext,
        bool end,
        bool src1_bank_ext,
        RepeatCount repeat_count,
        bool nosched,
        Imm2 op2,
        Imm2 src_type,
        Imm2 src1_mod,
        Imm2 src_comp,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm7 dest_n,
        Imm7 src1_n,
        Imm4 write_mask);

    bool vdp(
        ExtPredicate pred,
        Imm1 skipinv,
        bool clip_plane_enable,
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
        Imm3 clip_plane_n,
        Imm2 dest_bank,
        Imm2 src0_bank,
        Imm2 gpi0_n,
        Imm6 dest_n,
        Imm4 gpi0_swiz,
        Imm3 src0_swiz_w,
        Imm3 src0_swiz_z,
        Imm3 src0_swiz_y,
        Imm3 src0_swiz_x,
        Imm6 src0_n);

    bool br(
        ExtPredicate pred,
        Imm1 syncend,
        bool exception,
        bool pwait,
        Imm1 sync_ext,
        bool nosched,
        bool br_monitor,
        bool save_link,
        Imm1 br_type,
        Imm1 any_inst,
        Imm1 all_inst,
        std::uint32_t br_off);

    bool smp(
        ExtPredicate pred,
        Imm1 skipinv,
        Imm1 nosched,
        Imm1 syncstart,
        Imm1 minpack,
        Imm1 src0_ext,
        Imm1 src1_ext,
        Imm1 src2_ext,
        Imm2 fconv_type,
        Imm2 mask_count,
        Imm2 dim,
        Imm2 lod_mode,
        bool dest_use_pa,
        Imm2 sb_mode,
        Imm2 src0_type,
        Imm1 src0_bank,
        Imm2 drc_sel,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm7 src0_n,
        Imm7 src1_n,
        Imm7 src2_n);

    bool vtst(
        ExtPredicate pred,
        Imm1 skipinv,
        Imm1 onceonly,
        Imm1 syncstart,
        Imm1 dest_ext,
        Imm1 src1_neg,
        Imm1 src1_ext,
        Imm1 src2_ext,
        Imm1 prec,
        Imm1 src2_vscomp,
        RepeatCount rpt_count,
        Imm2 sign_test,
        Imm2 zero_test,
        Imm1 test_crcomb_and,
        Imm3 chan_cc,
        Imm2 pdst_n,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm1 test_wben,
        Imm2 alu_sel,
        Imm4 alu_op,
        Imm7 src1_n,
        Imm7 src2_n);

    bool vtstmsk(
        Imm3 pred,
        Imm1 skipinv,
        Imm1 onceonly,
        Imm1 syncstart,
        Imm1 dest_ext,
        Imm1 test_flag_2,
        Imm1 src1_ext,
        Imm1 src2_ext,
        Imm1 prec,
        Imm1 src2_vscomp,
        RepeatCount rpt_count,
        Imm2 sign_test,
        Imm2 zero_test,
        Imm1 test_crcomb_and,
        Imm2 tst_mask_type,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm1 test_wben,
        Imm2 alu_sel,
        Imm4 alu_op,
        Imm7 src1_n,
        Imm7 src2_n);

    bool smlsi(
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
        Imm8 src2_inc);

    bool vdual(
        Imm1 comp_count_type,
        Imm1 gpi1_neg,
        Imm2 sv_pred,
        Imm1 skipinv,
        Imm1 dual_op1_ext_vec3_or_has_w_vec4,
        bool type_f16,
        Imm1 gpi1_swizz_ext,
        Imm4 unified_store_swizz,
        Imm1 unified_store_neg,
        Imm3 dual_op1,
        Imm1 dual_op2_ext,
        bool prim_ustore,
        Imm4 gpi0_swizz,
        Imm4 gpi1_swizz,
        Imm2 prim_dest_bank,
        Imm2 unified_store_slot_bank,
        Imm2 prim_dest_num_gpi_case,
        Imm7 prim_dest_num,
        Imm3 dual_op2,
        Imm2 src_config,
        Imm1 gpi2_slot_num_bit_1,
        Imm1 gpi2_slot_num_bit_0_or_unified_store_abs,
        Imm2 gpi1_slot_num,
        Imm2 gpi0_slot_num,
        Imm3 write_mask_non_gpi,
        Imm7 unified_store_slot_num);

    bool vldst(
        Imm2 op1,
        ExtPredicate pred,
        Imm1 skipinv,
        Imm1 nosched,
        Imm1 moe_expand,
        Imm1 sync_start,
        Imm1 cache_ext,
        Imm1 src0_bank_ext,
        Imm1 src1_bank_ext,
        Imm1 src2_bank_ext,
        Imm4 mask_count,
        Imm2 addr_mode,
        Imm2 mode,
        Imm1 dest_bank_primattr,
        Imm1 range_enable,
        Imm2 data_type,
        Imm1 increment_or_decrement,
        Imm1 src0_bank,
        Imm1 cache_by_pass12,
        Imm1 drc_sel,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm7 src0_n,
        Imm7 src1_n,
        Imm7 src2_n);

    bool nop();
    bool phas();

    bool spec(
        bool special,
        SpecialCategory category);

private:
    // SPIR-V emitter
    spv::Builder &m_b;

    // Instruction word being translated
    const uint64_t &m_instr;

    // SPIR-V IDs
    const SpirvShaderParameters &m_spirv_params;

    USSERecompiler &m_recompiler;

    const SceGxmProgram &m_program;

    const FeatureState &m_features;
};

using BlockCacheMap = std::map<shader::usse::USSEOffset, spv::Function *>;
constexpr int sgx543_pc_bits = 20;

struct USSERecompiler final {
    BlockCacheMap cache;
    const std::uint64_t *inst;
    std::size_t count;
    spv::Builder &b;
    USSETranslatorVisitor visitor;
    std::uint64_t cur_instr;
    usse::USSEOffset cur_pc;

    const SceGxmProgram *program;
    spv::Function *end_hook_func;

    std::unordered_map<usse::USSEOffset, usse::USSEBlock> avail_blocks;

    explicit USSERecompiler(spv::Builder &b, const SceGxmProgram &program, const FeatureState &features,
        const SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils, spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &queries);

    void reset(const std::uint64_t *inst, const std::size_t count);
    spv::Function *get_or_recompile_block(const usse::USSEBlock &block);
};

} // namespace shader::usse
