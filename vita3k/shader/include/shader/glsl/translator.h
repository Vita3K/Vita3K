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

#include <gxm/types.h>
#include <shader/glsl/recompiler.h>
#include <shader/program_analyzer.h>
#include <shader/translator.h>
#include <shader/translator_types.h>

#include <array>
#include <map>

struct FeatureState;

namespace shader::usse {
struct USSERecompiler;
}

namespace shader::usse::glsl {
class CodeWriter;
struct ShaderVariables;
struct ProgramState;

class USSETranslatorVisitorGLSL : public USSETranslatorVisitor {
public:
    void do_texture_queries(const NonDependentTextureQueries &texture_queries);

    USSETranslatorVisitorGLSL() = delete;
    explicit USSETranslatorVisitorGLSL(USSERecompiler &_recompiler, ProgramState &program, CodeWriter &writer,
        ShaderVariables &variables, const FeatureState &features, const uint64_t &_instr,
        bool is_secondary_program = false)
        : USSETranslatorVisitor(_recompiler, program.actual_program, features, _instr, is_secondary_program)
        , writer(writer)
        , variables(variables)
        , program_state(program) {
        do_texture_queries(program.non_dependent_queries);
    }

public:
    // Instructions start
    bool vmad2(Imm1 dat_fmt,
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
        Imm6 src2_n) override;

    bool v32nmad(ExtVecPredicate pred,
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
        Imm6 src2_n) override;

    bool vmad(ExtVecPredicate pred,
        Imm1 skipinv,
        Imm1 gpi1_swiz_ext,
        Imm1 opcode2,
        Imm1 dest_use_bank_ext,
        Imm1 end,
        Imm1 src1_bank_ext,
        RepeatMode repeat_mode,
        Imm1 gpi0_abs,
        RepeatCount repeat_count,
        bool nosched,
        Imm4 write_mask,
        Imm1 src1_neg,
        Imm1 src1_abs,
        Imm1 gpi1_neg,
        Imm1 gpi1_abs,
        Imm1 gpi0_swiz_ext,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 gpi0_n,
        Imm6 dest_n,
        Imm4 gpi0_swiz,
        Imm4 gpi1_swiz,
        Imm2 gpi1_n,
        Imm1 gpi0_neg,
        Imm1 src1_swiz_ext,
        Imm4 src1_swiz,
        Imm6 src1_n) override;

    bool vdp(ExtVecPredicate pred,
        Imm1 skipinv,
        bool clip_plane_enable,
        Imm1 opcode2,
        Imm1 dest_use_bank_ext,
        Imm1 end,
        Imm1 src1_bank_ext,
        RepeatMode repeat_mode,
        Imm1 gpi0_abs,
        RepeatCount repeat_count,
        bool nosched,
        Imm4 write_mask,
        Imm1 src1_neg,
        Imm1 src1_abs,
        Imm3 clip_plane_n,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 gpi0_n,
        Imm6 dest_n,
        Imm4 gpi0_swiz,
        Imm3 src1_swiz_w,
        Imm3 src1_swiz_z,
        Imm3 src1_swiz_y,
        Imm3 src1_swiz_x,
        Imm6 src1_n) override;

    bool vdual(Imm1 comp_count_type,
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
        Imm7 unified_store_slot_num) override;

    bool vcomp(ExtPredicate pred,
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
        Imm4 write_mask) override;

    bool vmov(ExtPredicate pred,
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
        Imm6 src2_n) override;

    bool vpck(ExtPredicate pred,
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
        Imm1 comp_sel_0_bit0) override;

    bool vtst(ExtPredicate pred,
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
        Imm7 src2_n) override;

    bool vtstmsk(ExtPredicate pred,
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
        Imm7 src2_n) override;

    bool vbw(Imm3 op1,
        ExtPredicate pred,
        Imm1 skipinv,
        Imm1 nosched,
        bool repeat_sel,
        Imm1 sync_start,
        Imm1 dest_ext,
        Imm1 end,
        Imm1 src1_ext,
        Imm1 src2_ext,
        RepeatCount repeat_count,
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
        Imm7 src2_n) override;

    bool sop2(Imm2 pred,
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
        Imm7 src2_n) override;

    bool sop2m(Imm2 pred,
        Imm1 mod1,
        Imm1 skipinv,
        Imm1 nosched,
        Imm2 cop,
        Imm1 destbankext,
        Imm1 end,
        Imm1 src1bankext,
        Imm1 src2bankext,
        Imm1 mod2,
        Imm4 wmask,
        Imm2 aop,
        Imm3 sel1,
        Imm3 sel2,
        Imm2 destbank,
        Imm2 src1bank,
        Imm2 src2bank,
        Imm7 destnum,
        Imm7 src1num,
        Imm7 src2num) override;

    bool sop3(Imm2 pred,
        Imm1 mod1,
        Imm1 skipinv,
        Imm1 nosched,
        Imm2 cop,
        Imm1 destbext,
        Imm1 end,
        Imm1 src1bext,
        Imm1 src2bext,
        Imm1 mod2,
        Imm4 wrmask,
        Imm2 aop,
        Imm3 sel1,
        Imm3 sel2,
        Imm1 src0bank,
        Imm2 destbank,
        Imm2 src1bank,
        Imm2 src2bank,
        Imm7 destn,
        Imm7 src0n,
        Imm7 src1n,
        Imm7 src2n) override;

    bool i8mad(Imm2 pred,
        Imm1 cmod1,
        Imm1 skipinv,
        Imm1 nosched,
        Imm2 csel0,
        Imm1 dest_bank_ext,
        Imm1 end,
        Imm1 src1_bank_ext,
        Imm1 src2_bank_ext,
        Imm1 cmod2,
        RepeatCount repeat_count,
        Imm1 saturated,
        Imm1 cmod0,
        Imm1 asel0,
        Imm1 amod2,
        Imm1 amod1,
        Imm1 amod0,
        Imm1 csel1,
        Imm1 csel2,
        Imm1 src0_neg,
        Imm1 src0_bank,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_num,
        Imm7 src0_num,
        Imm7 src1_num,
        Imm7 src2_num) override;

    bool i16mad(ShortPredicate pred,
        Imm1 abs,
        bool skipinv,
        bool nosched,
        Imm1 src2_neg,
        Imm1 sel1h_upper8,
        Imm1 dest_bank_ext,
        Imm1 end,
        Imm1 src1_bank_ext,
        Imm1 src2_bank_ext,
        RepeatCount repeat_count,
        Imm2 mode,
        Imm2 src2_format,
        Imm2 src1_format,
        Imm1 sel2h_upper8,
        Imm2 or_shift,
        Imm1 src0_bank,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm7 src0_n,
        Imm7 src1_n,
        Imm7 src2_n) override;

    bool i32mad(ShortPredicate pred,
        Imm1 src0_high,
        Imm1 nosched,
        Imm1 src1_high,
        Imm1 src2_high,
        bool dest_bank_ext,
        Imm1 end,
        bool src1_bank_ext,
        bool src2_bank_ext,
        RepeatCount repeat_count,
        bool is_signed,
        bool is_sat,
        Imm2 src2_type,
        Imm1 src0_bank,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm7 src0_n,
        Imm7 src1_n,
        Imm7 src2_n) override;

    bool i32mad2(ExtPredicate pred,
        Imm1 nosched,
        Imm2 sn,
        bool dest_bank_ext,
        Imm1 end,
        bool src1_bank_ext,
        bool src2_bank_ext,
        bool src0_bank_ext,
        Imm3 count,
        bool is_signed,
        Imm1 negative_src1,
        Imm1 negative_src2,
        Imm1 src0_bank,
        Imm2 dest_bank,
        Imm2 src1_bank,
        Imm2 src2_bank,
        Imm7 dest_n,
        Imm7 src0_n,
        Imm7 src1_n,
        Imm7 src2_n) override;

    bool smp(ExtPredicate pred,
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
        Imm7 src2_n) override;

    bool kill(ShortPredicate pred) override;

    bool limm(bool skipinv,
        bool nosched,
        bool dest_bank_ext,
        bool end,
        Imm6 imm_value_bits26to31,
        ExtPredicate pred,
        Imm5 imm_value_bits21to25,
        Imm2 dest_bank,
        Imm7 dest_num,
        Imm21 imm_value_first_21bits) override;

    bool vldst(Imm2 op1,
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
        Imm7 src2_n) override;
    // Instructions end

    std::string do_alu_op(Instruction &inst, const Imm4 source_mask, const Imm4 possible_dest_mask);

private:
    CodeWriter &writer;
    ShaderVariables &variables;
    ProgramState &program_state;

    std::string vtst_impl(Instruction inst, ExtPredicate pred, int zero_test, int sign_test, Imm4 load_mask, bool mask);
    std::string do_fetch_texture(const std::string tex, std::string coord_name, const DataType dest_type, const int lod_mode,
        const std::string extra1, const std::string extra2);
};

struct USSERecompilerGLSL : public USSERecompiler {
    std::uint32_t cond_stack = 0;
    bool cond_decled[3]{ false, false, false };

    CodeWriter &writer;
    ShaderVariables &variables;

    explicit USSERecompilerGLSL(ProgramState &program_state, CodeWriter &writer, ShaderVariables &variables, const FeatureState &features);

    void compile_break_node(const usse::USSEBreakNode &node) override;
    void compile_continue_node(const usse::USSEContinueNode &node) override;
    void compile_conditional_node(const usse::USSEConditionalNode &cond) override;
    void compile_loop_node(const usse::USSELoopNode &loop) override;
    void compile_code_node(const usse::USSECodeNode &code) override;

    std::string get_condition(std::uint8_t num, bool do_neg);
    void compile_to_function();

    void begin_condition(const int cond) override;
    void end_condition() override;
};

} // namespace shader::usse::glsl
