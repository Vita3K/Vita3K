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

#include <SPIRV/SpvBuilder.h>
#include <boost/optional/optional.hpp>

#include <array>
#include <map>

using boost::optional;

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

    spv::Function *f16_unpack_func;
    spv::Function *f16_pack_func;

    spv::Block *main_block;

    // Contains repeat increasement offset
    int repeat_increase[4][4];

    // SPIR-V inputs are read-only, so we need to write to these temporaries instead
    // TODO: Figure out actual PA count limit
    std::unordered_map<uint16_t, SpirvReg> pa_writeable;

    // Each SPIR-V reg will contains 4 SA
    std::array<SpirvReg, max_sa_registers / 4> sa_supplies;
    std::array<spv::Id, 4> predicates;

    void make_f16_unpack_func();
    void make_f16_pack_func();
    void do_texture_queries(const NonDependentTextureQueryCallInfos &texture_queries);

    spv::Id do_fetch_texture(const spv::Id tex, const spv::Id coord, const DataType dest_type);

    USSETranslatorVisitor() = delete;
    explicit USSETranslatorVisitor(spv::Builder &_b, USSERecompiler &_recompiler, const uint64_t &_instr, const SpirvShaderParameters &spirv_params, const NonDependentTextureQueryCallInfos &queries,
        bool is_secondary_program = false)
        : m_b(_b)
        , m_recompiler(_recompiler)
        , m_instr(_instr)
        , m_spirv_params(spirv_params)
        , m_second_program(is_secondary_program) {
        // Default increase mode
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                repeat_increase[i][j] = j;
            }
        }

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

        std::fill(predicates.begin(), predicates.end(), spv::NoResult);

        // Make utility functions
        make_f16_unpack_func();
        make_f16_pack_func();

        do_texture_queries(queries);
    }

private:
    //
    // Translation helpers
    //

#define BEGIN_REPEAT(repeat_count, jump)                     \
    const auto repeat_count_num = (uint8_t)repeat_count + 1; \
    const auto repeat_jump = jump;                           \
    for (auto current_repeat = 0; current_repeat < repeat_count_num; current_repeat++) {
#define END_REPEAT() }

#define GET_REPEAT(inst)                                                                           \
    const int dest_repeat_offset = get_repeat_offset(inst.opr.dest, current_repeat) * repeat_jump; \
    const int src0_repeat_offset = get_repeat_offset(inst.opr.src0, current_repeat) * repeat_jump; \
    const int src1_repeat_offset = get_repeat_offset(inst.opr.src1, current_repeat) * repeat_jump; \
    const int src2_repeat_offset = get_repeat_offset(inst.opr.src2, current_repeat) * repeat_jump

    const int get_repeat_offset(Operand &op, const std::uint8_t repeat_index) {
        return repeat_increase[op.index][repeat_index];
    }

    /**
     * \brief Get a SPIR-V variable corresponding to the given bank and register offset.
     * 
     * Beside finding the corresponding SPIR-V register bank and returning the associated SPIR-V variable, this function
     * does patching for edge cases, like getting a pa0 variable for storage.
     * 
     * The translator should use this function instead of getting the raw SPIR-V register bank using get_reg_bank, etc.
     * 
     * \returns True on success.
    */
    bool get_spirv_reg(RegisterBank bank, std::uint32_t reg_offset, int shift_offset, SpirvReg &reg,
        std::uint32_t &out_comp_offset, bool get_for_store);

    /**
     * \brief Create a new vector given two another vector and a shift offset.
     * 
     * This uses a method called bridging (connecting). For example, we have two vec4:
     * vec4 a1(x1, y1, z1, w1) and vec4(x2, y2, z2, w2)
     * 
     * When we call:
     *  bridge(a1, a2, SWIZZLE_CHANNEL_4(Z, X, Y, W), 2, 0b1111)
     * 
     * Function will first concats two vector and get components starting from offset 2
     * If the bit at current offset is set in write mask, this function will take the component at current offset
     * 
     *           x1   y1  [ z1   w1   x2   y2 ]  z2   w2
     *           ^    ^     ^
     *  Offset   0    1     2
     * 
     * Components in brackets are the one that will be taken to form vec4(z1, w1, x2, y2)
     * 
     * Then, it uses the swizzle to shuffle components.
     * 
     *                       SWIZZLE_CHANNEL4(Z, X, Y, W)
     * vec4(z1, w1, x2, y2) ==============================> vec4(x2, z1, w1, y2)
     *       ^   ^  ^   ^                                         ^   ^  ^   ^
     *       X   Y  Z   W                                         Z   X  Y   W
     * 
     * In situation like example:
     * 
     *   a1 = vec2(x1, y1), a2 = vec2(x2, y2), and offset = 1 (swizz = SWIZZLE_CHANNEL4(X, Y, Z, W)), write_mask = 0b1111
     * 
     * It will returns vec4(y1, x2, y2, y2)
     * 
     * \param src1 First vector in bridge operation.
     * \param src2 Second vector in bridge operation.
     * \param swiz Swizzle to shuffle components after getting the vec4.
     * \param shift_offset Offset in concats to get vec4
     * \param dest_mask Destination/write mask
     * 
     * \returns ID to a vec4 contains bridging results, or spv::NoResult if failed
     * 
    */
    spv::Id bridge(SpirvReg &src1, SpirvReg &src2, Swizzle4 swiz, const int shift_offset, const Imm4 dest_mask);

    /*
     * \brief Given an operand, load it and returns a SPIR-V vector with total components count equals to total bit set in
     *        write/dest mask
     * 
     * \returns A copy of given operand
    */
    spv::Id load(Operand &op, const Imm4 dest_mask, const int offset = 0);

    /**
     * \brief Unpack a vector/scalar as given component data type.
     * 
     * \param target Target vector/scalar. Component type must be f32.
     * \param type   Data type to unpack to. If the data is F32, target will be returned.
     * 
     * \returns A new vector with [total_comp_count(target) * 4 / size(type)] components
     */
    spv::Id unpack(spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
        const int shift_offset = 0);

    /**
     * \brief Unpack one scalar.
     */
    spv::Id unpack_one(spv::Id scalar, const DataType type);
    spv::Id pack_one(spv::Id vec, const DataType source_type);

    void store(Operand &dest, spv::Id source, std::uint8_t dest_mask = 0xFF, int off = 0);

    const SpirvVarRegBank *get_reg_bank(RegisterBank reg_bank) const;

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
    // Helpers
    //

    // In non-native frag shaders, GXM's ShaderPatcher API uses some specific PAs as the input color of its blending code
    // This function imitates that by write pa0 to fragment output color
    // TODO: Is this and our OpenGL blending code enough?
    // TOOD: Is pa0 correct or is there a GXP field or some other logic that determines the PA offfset (+size?) to use
    void emit_non_native_frag_output();

    //
    // Instructions
    //
    bool vmov(
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
        DataType move_data_type,
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

    bool vpck(
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
        Imm6 src1_n,
        Imm1 comp0_sel_bit1,
        Imm4 src2_n,
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
        Imm1 test_flag_2,
        Imm1 src1_ext,
        Imm1 src2_ext,
        Imm1 prec,
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

    bool nop();
    bool phas();

    bool spec(
        bool special,
        SpecialCategory category);

    bool set_predicate(const Imm2 idx, const spv::Id value);
    spv::Id load_predicate(const Imm2 idx, const bool neg = false);

private:
    // SPIR-V emitter
    spv::Builder &m_b;

    // Instruction word being translated
    const uint64_t &m_instr;

    // SPIR-V IDs
    const SpirvShaderParameters &m_spirv_params;

    USSERecompiler &m_recompiler;
};

using BlockCacheMap = std::map<shader::usse::USSEOffset, spv::Block *>;
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

    std::unordered_map<usse::USSEOffset, usse::USSEBlock> avail_blocks;

    explicit USSERecompiler(spv::Builder &b, const SpirvShaderParameters &parameters, const NonDependentTextureQueryCallInfos &queries);

    void reset(const std::uint64_t *inst, const std::size_t count);
    spv::Block *get_or_recompile_block(const usse::USSEBlock &block, spv::Block *custom = nullptr);
};

} // namespace shader::usse
