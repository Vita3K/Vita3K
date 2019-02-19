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
#include <shader/usse_translator_types.h>

#include <SPIRV/SpvBuilder.h>
#include <boost/optional/optional.hpp>

#include <map>

using boost::optional;

namespace shader::usse {

// For debugging SPIR-V output
static uint32_t instr_idx = 0;

class USSETranslatorVisitor final {
public:
    using instruction_return_type = bool;

    spv::Id type_f32;
    spv::Id type_f32_v[5]; // Starts from 1 ([1] is vec 1)
    spv::Id const_f32[4];
    // spv::Id const_f32_v[5];  // Starts from 1 ([1] is vec 1)

    // SPIR-V inputs are read-only, so we need to write to these temporaries instead
    // TODO: Figure out actual PA count limit
    std::unordered_map<uint16_t, SpirvReg> pa_writeable;

    USSETranslatorVisitor() = delete;
    explicit USSETranslatorVisitor(spv::Builder &_b, const uint64_t &_instr, const SpirvShaderParameters &spirv_params, const SceGxmProgram &program)
        : m_b(_b)
        , m_instr(_instr)
        , m_spirv_params(spirv_params)
        , m_program(program) {
        // Build common type here, so builder won't have to look it up later
        type_f32 = m_b.makeFloatType(32);

        const_f32[3] = m_b.makeFpConstant(type_f32, 0.5f);
        const_f32[0] = m_b.makeFpConstant(type_f32, 0.0f);
        const_f32[1] = m_b.makeFpConstant(type_f32, 1.0f);
        const_f32[2] = m_b.makeFpConstant(type_f32, 2.0f);

        for (std::uint8_t i = 1; i < 5; i++) {
            type_f32_v[i] = m_b.makeVectorType(type_f32, i);
            //            const_f32_v[i] = m_b.makeCompositeConstant(type_f32_v[i],
            //                { const_f32[i - 1], const_f32[i - 1], const_f32[i - 1], const_f32[i - 1] });
        }
    }

private:
    //
    // Translation helpers
    //

#define BEGIN_REPEAT(repeat_count, repeat_offset_jump)          \
    const auto repeat_count_num = (uint8_t)repeat_count + 1;    \
    for (auto repeat_offset = 0;                                \
         repeat_offset < repeat_count_num * repeat_offset_jump; \
         repeat_offset += repeat_offset_jump) {
#define END_REPEAT() }

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
    bool get_spirv_reg(RegisterBank bank, std::uint32_t reg_offset, std::uint32_t shift_offset, SpirvReg &reg,
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
    spv::Id bridge(SpirvReg &src1, SpirvReg &src2, Swizzle4 swiz, const std::uint32_t shift_offset, const Imm4 dest_mask);

    /*
     * \brief Given an operand, load it and returns a SPIR-V vector with total components count equals to total bit set in
     *        write/dest mask
     * 
     * \returns A copy of given operand
    */
    spv::Id load(Operand &op, const Imm4 dest_mask, const std::uint8_t offset = 0, bool /* abs */ = false, bool /* neg */ = false);

    void store(Operand &dest, spv::Id source, std::uint8_t dest_mask = 0xFF, std::uint8_t off = 0);

    const SpirvVarRegBank &get_reg_bank(RegisterBank reg_bank) const;

    spv::Id swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle);

    // TODO: Separate file for translator helpers?
    static size_t dest_mask_to_comp_count(Imm4 dest_mask);

public:
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

    // Shader being translated
    const SceGxmProgram &m_program; // unused
};

} // namespace shader::usse