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

#include <shader/spirv/translator.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/decoder_helpers.h>
#include <shader/disasm.h>
#include <shader/types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;

static spv::Id postprocess_dot_result_for_store(spv::Builder &b, spv::Id dot_result, shader::usse::Imm4 mask) {
    const std::size_t comp_count = dest_mask_to_comp_count(mask);
    if (comp_count == 1) {
        return dot_result;
    }

    spv::Id type_result = b.makeVectorType(b.getTypeId(dot_result), comp_count);
    std::vector<spv::Id> comps(comp_count, dot_result);

    return b.createCompositeConstruct(type_result, comps);
}

bool USSETranslatorVisitorSpirv::vmad(
    ExtVecPredicate pred,
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
    Imm6 src1_n) {
    if (!USSETranslatorVisitor::vmad(pred, skipinv, gpi1_swiz_ext, opcode2, dest_use_bank_ext, end, src1_bank_ext, repeat_mode, gpi0_abs, repeat_count, nosched, write_mask, src1_neg, src1_abs,
            gpi1_neg, gpi1_abs, gpi0_swiz_ext, dest_bank, src1_bank, gpi0_n, dest_n, gpi0_swiz, gpi1_swiz, gpi1_n, gpi0_neg, src1_swiz_ext, src1_swiz, src1_n)) {
        return false;
    }

    m_b.setLine(m_recompiler.cur_pc);

    set_repeat_multiplier(2, 2, 2, 4);

    // Write mask is a 4-bit immidiate
    // If a bit is one, a swizzle is active
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, repeat_mode);

    // SRC1 and SRC2 here is actually GPI0 and GPI1.
    spv::Id vsrc0 = load(decoded_inst.opr.src0, write_mask, src0_repeat_offset);
    spv::Id vsrc1 = load(decoded_inst.opr.src1, write_mask, src1_repeat_offset);
    spv::Id vsrc2 = load(decoded_inst.opr.src2, write_mask, src2_repeat_offset);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_ERROR("Source not loaded (vsrc0: {}, vsrc1: {}, vsrc2: {})", vsrc0, vsrc1, vsrc2);
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(decoded_inst.opr.dest, add_result, write_mask, 0);
    END_REPEAT()

    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitorSpirv::vmad2(
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
    Imm6 src2_n) {
    if (!USSETranslatorVisitor::vmad2(dat_fmt, pred, skipinv, src0_swiz_bits2, syncstart, src0_abs, src1_bank_ext,
            src2_bank_ext, src2_swiz, src1_swiz_bit2, nosched, dest_mask, src1_mod, src2_mod, src0_bank, dest_bank,
            src1_bank, src2_bank, dest_n, src1_swiz_bits01, src0_swiz_bits01, src0_n, src1_n, src2_n)) {
        return false;
    }

    m_b.setLine(m_recompiler.cur_pc);

    // Translate the instruction
    spv::Id vsrc0 = load(decoded_inst.opr.src0, decoded_dest_mask, 0);
    spv::Id vsrc1 = load(decoded_inst.opr.src1, decoded_dest_mask, 0);
    spv::Id vsrc2 = load(decoded_inst.opr.src2, decoded_dest_mask, 0);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_ERROR("Source not loaded (vsrc0: {}, vsrc1: {}, vsrc2: {})", vsrc0, vsrc1, vsrc2);
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(decoded_inst.opr.dest, add_result, decoded_dest_mask, 0);

    return true;
}

bool USSETranslatorVisitorSpirv::vdp(
    ExtVecPredicate pred,
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
    Imm6 src1_n) {
    if (!USSETranslatorVisitor::vdp(pred, skipinv, clip_plane_enable, opcode2, dest_use_bank_ext, end, src1_bank_ext,
            repeat_mode, gpi0_abs, repeat_count, nosched, write_mask, src1_neg, src1_abs, clip_plane_n, dest_bank,
            src1_bank, gpi0_n, dest_n, gpi0_swiz, src1_swiz_w, src1_swiz_z, src1_swiz_y, src1_swiz_x, src1_n)) {
        return false;
    }

    m_b.setLine(m_recompiler.cur_pc);

    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    // Decoding done
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, repeat_mode)

    spv::Id lhs = load(decoded_inst.opr.src1, type == 1 ? 0b0111 : 0b1111, src1_repeat_offset);
    spv::Id rhs = load(decoded_inst.opr.src2, type == 1 ? 0b0111 : 0b1111, src2_repeat_offset);

    if (lhs == spv::NoResult || rhs == spv::NoResult) {
        LOG_ERROR("Source not loaded (lhs: {}, rhs: {})", lhs, rhs);
        return false;
    }

    spv::Id result = m_b.createBinOp(spv::OpDot, type_f32, lhs, rhs);
    result = postprocess_dot_result_for_store(m_b, result, write_mask);
    store(decoded_inst.opr.dest, result, write_mask, 0);

    // Rotate write mask
    write_mask = (write_mask << 1) | (write_mask >> 3);
    END_REPEAT()

    return true;
}

spv::Id USSETranslatorVisitorSpirv::do_alu_op(Instruction &inst, const Imm4 source_mask, const Imm4 possible_dest_mask) {
    spv::Id vsrc1 = load(inst.opr.src1, source_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, source_mask, 0);
    std::vector<spv::Id> ids;
    ids.push_back(vsrc1);

    if (vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_WARN("Could not find a src register");
        return spv::NoResult;
    }

    spv::Id result = spv::NoResult;
    spv::Id source_type = m_b.getTypeId(vsrc1);

    switch (inst.opcode) {
    case Opcode::AND: {
        result = m_b.createBinOp(spv::OpBitwiseAnd, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::OR: {
        if (m_b.getOpCode(vsrc2) == spv::Op::OpConstant && m_b.getConstantScalar(vsrc2) == 0) {
            result = vsrc1;
        } else {
            result = m_b.createBinOp(spv::OpBitwiseOr, source_type, vsrc1, vsrc2);
        }
        break;
    }

    case Opcode::XOR: {
        result = m_b.createBinOp(spv::OpBitwiseXor, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::SHL: {
        result = m_b.createBinOp(spv::OpShiftLeftLogical, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::SHR: {
        result = m_b.createBinOp(spv::OpShiftRightLogical, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::ASR: {
        result = m_b.createBinOp(spv::OpShiftRightArithmetic, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::VDSX:
    case Opcode::VF16DSX:
        result = m_b.createOp(spv::OpDPdx, source_type, ids);
        break;

    case Opcode::VDSY:
    case Opcode::VF16DSY:
        result = m_b.createOp(spv::OpDPdy, source_type, ids);
        break;

    case Opcode::VADD:
    case Opcode::VF16ADD: {
        result = m_b.createBinOp(spv::OpFAdd, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::VMUL:
    case Opcode::VF16MUL: {
        result = m_b.createBinOp(spv::OpFMul, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::VMIN:
    case Opcode::VF16MIN: {
        result = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450FMin, { vsrc1, vsrc2 });
        break;
    }

    case Opcode::VMAX:
    case Opcode::VF16MAX: {
        result = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450FMax, { vsrc1, vsrc2 });
        break;
    }

    case Opcode::VFRC:
    case Opcode::VF16FRC: {
        // Dest = Source1 - Floor(Source2)
        // If two source are identical, let's use the fractional function
        if (inst.opr.src1.is_same(inst.opr.src2, source_mask)) {
            result = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450Fract, { vsrc1 });
        } else {
            // We need to floor source 2
            spv::Id source2_floored = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450Floor, { vsrc2 });
            // Then subtract source 1 with the floored source 2. TADA!
            result = m_b.createBinOp(spv::OpFSub, source_type, vsrc1, source2_floored);
        }

        break;
    }

    case Opcode::VDP:
    case Opcode::VF16DP: {
        result = m_b.createBinOp(spv::OpDot, m_b.makeFloatType(32), vsrc1, vsrc2);
        result = postprocess_dot_result_for_store(m_b, result, possible_dest_mask);
        break;
    }

    case Opcode::FPSUB8:
    case Opcode::ISUBU16:
    case Opcode::ISUB16:
    case Opcode::ISUBU32:
    case Opcode::ISUB32:
        result = m_b.createBinOp(spv::OpISub, m_b.makeIntType(32), vsrc1, vsrc2);
        break;

    default: {
        LOG_ERROR("Unimplemented ALU opcode instruction: {}", disasm::opcode_str(inst.opcode));
        break;
    }
    }

    return result;
}

bool USSETranslatorVisitorSpirv::v32nmad(
    ExtVecPredicate pred,
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
    if (!USSETranslatorVisitor::v32nmad(pred, skipinv, src1_swiz_10_11, syncstart, dest_bank_ext, src1_swiz_9,
            src1_bank_ext, src2_bank_ext, src2_swiz, nosched, dest_mask, src1_mod, src2_mod, src1_swiz_7_8,
            dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_n, src1_swiz_0_6, op2, src1_n, src2_n)) {
        return false;
    }

    // Recompile
    m_b.setLine(m_recompiler.cur_pc);
    spv::Id result = do_alu_op(decoded_inst, decoded_source_mask, dest_mask);

    if (result != spv::NoResult) {
        store(decoded_inst.opr.dest, result, dest_mask, 0);
    }

    return true;
}

bool USSETranslatorVisitorSpirv::vcomp(
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
    Imm4 write_mask) {
    if (!USSETranslatorVisitor::vcomp(pred, skipinv, dest_type, syncstart, dest_bank_ext, end, src1_bank_ext,
            repeat_count, nosched, op2, src_type, src1_mod, src_comp, dest_bank, src1_bank, dest_n, src1_n,
            write_mask)) {
        return false;
    }

    m_b.setLine(m_recompiler.cur_pc);

    // TODO: Log
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    spv::Id result = load(decoded_inst.opr.src1, decoded_source_mask, src1_repeat_offset);

    if (result == spv::NoResult) {
        LOG_ERROR("Result not loaded");
        return false;
    }

    switch (decoded_inst.opcode) {
    case Opcode::VRCP: {
        // We have to manually divide by 1
        const int num_comp = m_b.getNumComponents(result);
        const spv::Id one_const = m_b.makeFloatConstant(1.0f);
        spv::Id one_v = spv::NoResult;

        if (num_comp == 1) {
            one_v = one_const;
        } else {
            std::vector<spv::Id> ones;
            ones.insert(ones.begin(), num_comp, one_const);

            one_v = m_b.makeCompositeConstant(type_f32_v[num_comp], ones);
        }

        result = m_b.createBinOp(spv::OpFDiv, m_b.getTypeId(result), one_v, result);
        break;
    }

    case Opcode::VRSQ: {
        // Inverse squareroot. Call a builtin this case.
        result = m_b.createBuiltinCall(m_b.getTypeId(result), std_builtins, GLSLstd450InverseSqrt, { result });
        break;
    }

    case Opcode::VLOG: {
        // src0 = e^y => return y
        result = m_b.createBuiltinCall(m_b.getTypeId(result), std_builtins, GLSLstd450Log, { result });
        break;
    }

    case Opcode::VEXP: {
        // y = e^src0 => return y
        result = m_b.createBuiltinCall(m_b.getTypeId(result), std_builtins, GLSLstd450Exp, { result });
        break;
    }

    default:
        break;
    }

    store(decoded_inst.opr.dest, result, write_mask, dest_repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitorSpirv::sop2(
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
    Imm7 src2_n) {
    static auto selector_zero = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                    const spv::Id src2_alpha) {
        return utils::make_uniform_vector_from_type(b, type, 0);
    };

    static auto selector_src1_color = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        return src1_color;
    };

    static auto selector_src2_color = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        return src2_color;
    };

    static auto selector_src1_alpha = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        if (!b.isScalarType(type) || b.getNumTypeComponents(type) > 1) {
            // We must do a composite construct
            return b.createCompositeConstruct(type, { src1_alpha, src1_alpha, src1_alpha });
        }

        return src1_alpha;
    };

    static auto selector_src2_alpha = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        if (!b.isScalarType(type) || b.getNumTypeComponents(type) > 1) {
            // We must do a composite construct
            return b.createCompositeConstruct(type, { src2_alpha, src2_alpha, src2_alpha });
        }

        return src2_alpha;
    };

    // This opcode always operates on C10.
    static Opcode operations[] = {
        Opcode::FADD,
        Opcode::FSUB,
        Opcode::FMIN,
        Opcode::FMAX
    };

    using SelectorFunc = std::function<spv::Id(spv::Builder &, const spv::Id, const spv::Id, const spv::Id,
        const spv::Id, const spv::Id)>;

    static SelectorFunc color_selector_1[] = {
        selector_zero,
        selector_src1_color,
        selector_src2_color,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr, // source alpha saturated.
        nullptr // source alpha scale
    };

    static SelectorFunc color_selector_2[] = {
        selector_zero,
        selector_src1_color,
        selector_src2_color,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr, // source alpha saturated.
        nullptr // zero source 2 minus half.
    };

    static SelectorFunc alpha_selector_1[] = {
        selector_zero,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr // source 2 scale.
    };

    static SelectorFunc alpha_selector_2[] = {
        selector_zero,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr // zero source 2 minus half.
    };

    Instruction inst;
    inst.opcode = Opcode::SOP2;

    // Never double register num
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);

    inst.opr.src1.type = DataType::UINT8;
    inst.opr.src2.type = DataType::UINT8;
    inst.opr.dest.type = DataType::UINT8;

    if (cop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid color opcode: {}", (int)cop);
        return true;
    }

    if (aop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid alpha opcode: {}", (int)aop);
        return true;
    }

    Opcode color_op = operations[cop];
    Opcode alpha_op = operations[aop];

    if (csel1 >= sizeof(color_selector_1) / sizeof(SelectorFunc) || csel2 >= sizeof(color_selector_2) / sizeof(SelectorFunc) || asel1 >= sizeof(alpha_selector_1) / sizeof(SelectorFunc) || asel2 >= sizeof(alpha_selector_2) / sizeof(SelectorFunc)) {
        LOG_ERROR("Unknown color/alpha selector (csel1: {}, csel2: {}, asel1: {}, asel2: {}", csel1, csel2,
            asel1, asel2);
        return true;
    }

    // Lookup color selector
    SelectorFunc color_selector_1_func = color_selector_1[csel1];
    SelectorFunc color_selector_2_func = color_selector_2[csel2];
    SelectorFunc alpha_selector_1_func = alpha_selector_1[asel1];
    SelectorFunc alpha_selector_2_func = alpha_selector_2[asel2];

    if (!color_selector_1_func || !color_selector_2_func || !alpha_selector_1_func || !alpha_selector_2_func) {
        LOG_ERROR("Unimplemented color/alpha selector (csel1: {}, csel2: {}, asel1: {}, asel2: {}", csel1, csel2,
            asel1, asel2);
        return true;
    }

    auto apply_opcode = [&](Opcode op, spv::Id type, spv::Id lhs, spv::Id rhs) {
        switch (op) {
        case Opcode::FADD: {
            return m_b.createBinOp(spv::OpFAdd, type, lhs, rhs);
        }

        case Opcode::FSUB: {
            return m_b.createBinOp(spv::OpFSub, type, lhs, rhs);
        }

        case Opcode::FMIN:
        case Opcode::VMIN: {
            return m_b.createBuiltinCall(type, std_builtins, GLSLstd450FMin, { lhs, rhs });
        }

        case Opcode::FMAX:
        case Opcode::VMAX: {
            return m_b.createBuiltinCall(type, std_builtins, GLSLstd450FMax, { lhs, rhs });
        }

        default: {
            LOG_ERROR("Unsuppported sop opcode: {}", disasm::opcode_str(op));
            break;
        }
        }

        return spv::NoResult;
    };

    spv::Id uniform_1_alpha = spv::NoResult;
    spv::Id uniform_1_color = spv::NoResult;

    auto apply_complement_modifiers = [&](spv::Id &target, Imm1 enable, spv::Id type, const bool is_rgb) {
        if (enable == 1) {
            if (is_rgb) {
                if (!uniform_1_color)
                    uniform_1_color = utils::make_uniform_vector_from_type(m_b, type, 1);

                target = m_b.createBinOp(spv::OpFSub, type, uniform_1_color, target);
            } else {
                if (!uniform_1_alpha)
                    uniform_1_alpha = m_b.makeFloatConstant(1.0f);

                target = m_b.createBinOp(spv::OpFSub, type, uniform_1_alpha, target);
            }
        }
    };

    BEGIN_REPEAT(count)
    GET_REPEAT(inst, RepeatMode::SLMSI)

    spv::Id src1_color = load(inst.opr.src1, 0b0111, src1_repeat_offset);
    spv::Id src2_color = load(inst.opr.src2, 0b0111, src2_repeat_offset);
    spv::Id src1_alpha = load(inst.opr.src1, 0b1000, src1_repeat_offset);
    spv::Id src2_alpha = load(inst.opr.src2, 0b1000, src2_repeat_offset);

    src1_color = utils::convert_to_float(m_b, src1_color, DataType::UINT8, true);
    src2_color = utils::convert_to_float(m_b, src2_color, DataType::UINT8, true);
    src1_alpha = utils::convert_to_float(m_b, src1_alpha, DataType::UINT8, true);
    src2_alpha = utils::convert_to_float(m_b, src2_alpha, DataType::UINT8, true);

    spv::Id src_color_type = m_b.getTypeId(src1_color);
    spv::Id src_alpha_type = m_b.getTypeId(src1_alpha);

    // Apply source flag.
    // Complement is the vector that when added with source creates a nice vec4(1.0).
    // Since the whole opcode is around stencil and blending.
    apply_complement_modifiers(src1_color, src1_mod, src_color_type, true);
    apply_complement_modifiers(src1_alpha, src1_mod, src_alpha_type, false);

    // Apply color selector
    spv::Id factored_rgb_lhs = color_selector_1_func(m_b, src_color_type, src1_color, src2_color, src1_alpha, src2_alpha);
    spv::Id factored_rgb_rhs = color_selector_2_func(m_b, src_color_type, src1_color, src2_color, src1_alpha, src2_alpha);

    spv::Id factored_a_lhs = alpha_selector_1_func(m_b, src_alpha_type, src1_color, src2_color, src1_alpha, src2_alpha);
    spv::Id factored_a_rhs = alpha_selector_2_func(m_b, src_alpha_type, src1_color, src2_color, src1_alpha, src2_alpha);

    // Apply modifiers
    // BREAKDOWN BREAKDOWN
    apply_complement_modifiers(factored_rgb_lhs, cmod1, src_color_type, true);
    apply_complement_modifiers(factored_rgb_rhs, cmod2, src_color_type, true);
    apply_complement_modifiers(factored_a_lhs, amod1, src_alpha_type, false);
    apply_complement_modifiers(factored_a_rhs, amod2, src_alpha_type, false);

    // Factor them with source
    factored_rgb_lhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_rgb_lhs, src1_color);
    factored_rgb_rhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_rgb_rhs, src2_color);

    factored_a_lhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_a_lhs, src1_alpha);
    factored_a_rhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_a_rhs, src2_alpha);

    auto color_res = apply_opcode(color_op, src_color_type, factored_rgb_lhs, factored_rgb_rhs);
    auto alpha_res = apply_opcode(alpha_op, src_alpha_type, factored_a_lhs, factored_a_rhs);

    color_res = utils::convert_to_int(m_b, color_res, DataType::UINT8, true);
    alpha_res = utils::convert_to_int(m_b, alpha_res, DataType::UINT8, true);

    // Final result. Do binary operation and then store
    store(inst.opr.dest, color_res, 0b0111, dest_repeat_offset);
    store(inst.opr.dest, alpha_res, 0b1000, dest_repeat_offset);

    // TODO log correctly
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(color_op));
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(alpha_op));

    END_REPEAT()

    return true;
}

bool shader::usse::USSETranslatorVisitorSpirv::sop2m(Imm2 pred,
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
    Imm7 src2num) {
    static auto selector_zero = [](spv::Builder &b, const spv::Id type, const spv::Id src1, const spv::Id src2) {
        return utils::make_uniform_vector_from_type(b, type, 0);
    };

    static auto selector_src1_color = [](spv::Builder &b, const spv::Id type, const spv::Id src1, const spv::Id src2) {
        return src1;
    };

    static auto selector_src2_color = [](spv::Builder &b, const spv::Id type, const spv::Id src1, const spv::Id src2) {
        return src2;
    };

    static auto selector_src1_alpha = [](spv::Builder &b, const spv::Id type, const spv::Id src1, const spv::Id src2) {
        return b.createOp(spv::OpVectorShuffle, type, { src1, src1, 3, 3, 3, 3 });
    };

    static auto selector_src2_alpha = [](spv::Builder &b, spv::Id type, const spv::Id src1, const spv::Id src2) {
        return b.createOp(spv::OpVectorShuffle, type, { src2, src2, 3, 3, 3, 3 });
    };

    // This opcode always operates on C10.
    static Opcode operations[] = {
        Opcode::FADD,
        Opcode::FSUB,
        Opcode::FMIN,
        Opcode::FMAX
    };

    using SelectorFunc = std::function<spv::Id(spv::Builder &, const spv::Id, const spv::Id, const spv::Id)>;

    static SelectorFunc selector[] = {
        selector_zero,
        nullptr, // source alpha saturated.
        selector_src1_color,
        selector_src1_alpha,
        nullptr,
        nullptr,
        selector_src2_color,
        selector_src2_alpha
    };

    Instruction inst;
    inst.opcode = Opcode::SOP2WM;

    // Never double register num
    inst.opr.src1 = decode_src12(inst.opr.src1, src1num, src1bank, src1bankext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2num, src2bank, src2bankext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, destnum, destbank, destbankext, false, 7, m_second_program);

    inst.opr.src1.type = DataType::UINT8;
    inst.opr.src2.type = DataType::UINT8;
    inst.opr.dest.type = DataType::UINT8;

    if (cop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid color opcode: {}", (int)cop);
        return true;
    }

    if (aop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid alpha opcode: {}", (int)aop);
        return true;
    }

    Opcode color_op = operations[cop];
    Opcode alpha_op = operations[aop];

    // Lookup color selector
    SelectorFunc operation_1_lhs_selector_func = selector[sel1];
    SelectorFunc operation_2_lhs_selector_func = selector[sel2];

    if (!operation_1_lhs_selector_func || !operation_2_lhs_selector_func) {
        LOG_ERROR("Unimplemented operation selector (sel1: {}, sel2: {}", sel1, sel2);
        return true;
    }

    auto apply_opcode = [&](Opcode op, spv::Id type, spv::Id lhs, spv::Id rhs) {
        switch (op) {
        case Opcode::FADD: {
            return m_b.createBinOp(spv::OpFAdd, type, lhs, rhs);
        }

        case Opcode::FSUB: {
            return m_b.createBinOp(spv::OpFSub, type, lhs, rhs);
        }

        case Opcode::FMIN:
        case Opcode::VMIN: {
            return m_b.createBuiltinCall(type, std_builtins, GLSLstd450FMin, { lhs, rhs });
        }

        case Opcode::FMAX:
        case Opcode::VMAX: {
            return m_b.createBuiltinCall(type, std_builtins, GLSLstd450FMax, { lhs, rhs });
        }

        default: {
            LOG_ERROR("Unsuppported sop opcode: {}", disasm::opcode_str(op));
            break;
        }
        }

        return spv::NoResult;
    };

    spv::Id uniform_1 = spv::NoResult;

    auto apply_complement_modifiers = [&](spv::Id &target, Imm1 enable, spv::Id type) {
        if (enable == 1) {
            if (!uniform_1)
                uniform_1 = utils::make_uniform_vector_from_type(m_b, type, 1);

            target = m_b.createBinOp(spv::OpFSub, type, uniform_1, target);
        }
    };

    spv::Id src1 = load(inst.opr.src1, 0b1111, 0);
    spv::Id src2 = load(inst.opr.src2, 0b1111, 0);

    src1 = utils::convert_to_float(m_b, src1, DataType::UINT8, true);
    src2 = utils::convert_to_float(m_b, src2, DataType::UINT8, true);

    spv::Id src_type = m_b.getTypeId(src1);

    // Apply selector
    spv::Id operation_1 = operation_1_lhs_selector_func(m_b, src_type, src1, src2);
    spv::Id operation_2 = operation_2_lhs_selector_func(m_b, src_type, src1, src2);

    // Apply modifiers
    apply_complement_modifiers(operation_1, mod1, src_type);
    apply_complement_modifiers(operation_2, mod2, src_type);

    // Factor them with source
    operation_1 = m_b.createBinOp(spv::OpFMul, src_type, operation_1, src1);
    operation_2 = m_b.createBinOp(spv::OpFMul, src_type, operation_2, src2);

    spv::Id result = apply_opcode(color_op, src_type, operation_1, operation_2);
    spv::Id alpha_type = m_b.makeFloatType(32);

    // Alpha is at the first bit, so reverse that
    wmask = ((wmask & 0b1110) >> 1) | ((wmask & 1) << 3);

    if (wmask & 0b1000) {
        // Alpha is written, so calculate and also store
        const spv::Id alpha_index = m_b.makeIntConstant(3);
        spv::Id a1 = m_b.createBinOp(spv::OpVectorExtractDynamic, alpha_type, operation_1, alpha_index);
        spv::Id a2 = m_b.createBinOp(spv::OpVectorExtractDynamic, alpha_type, operation_2, alpha_index);

        result = m_b.createTriOp(spv::OpVectorInsertDynamic, src_type, result, apply_opcode(alpha_op, alpha_type, a1, a2), alpha_index);
    }

    result = utils::convert_to_int(m_b, result, DataType::UINT8, true);

    // Final result. Do binary operation and then store
    store(inst.opr.dest, result, wmask, 0);

    // TODO log correctly
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(color_op));
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(alpha_op));

    return true;
}

bool shader::usse::USSETranslatorVisitorSpirv::sop3(Imm2 pred,
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
    Imm7 src2n) {
    LOG_DISASM("Unimplmenet Opcode: sop3");
    return true;
}

bool USSETranslatorVisitorSpirv::vdual(
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
    Imm7 unified_store_slot_num) {
    if (!USSETranslatorVisitor::vdual(comp_count_type, gpi1_neg, sv_pred, skipinv, dual_op1_ext_vec3_or_has_w_vec4,
            type_f16, gpi1_swizz_ext, unified_store_swizz, unified_store_neg, dual_op1, dual_op2_ext, prim_ustore,
            gpi0_swizz, gpi1_swizz, prim_dest_bank, unified_store_slot_bank, prim_dest_num_gpi_case,
            prim_dest_num, dual_op2, src_config, gpi2_slot_num_bit_1, gpi2_slot_num_bit_0_or_unified_store_abs,
            gpi1_slot_num, gpi0_slot_num, write_mask_non_gpi, unified_store_slot_num)) {
        return false;
    }

    auto do_dual_op = [&](Instruction &inst, Imm4 write_mask_source) {
        spv::Id result;
        switch (inst.opcode) {
        case Opcode::VMOV: {
            result = load(inst.opr.src0, write_mask_source);
            break;
        }
        case Opcode::FADD:
        case Opcode::VADD: {
            const spv::Id first = load(inst.opr.src0, write_mask_source);
            const spv::Id second = load(inst.opr.src1, write_mask_source);
            result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(first), first, second);
            break;
        }
        case Opcode::FRCP: {
            const spv::Id source = load(inst.opr.src0, write_mask_source);
            const int num_comp = m_b.getNumComponents(source);
            const spv::Id one_const = m_b.makeFloatConstant(1.0f);
            spv::Id one_v = spv::NoResult;

            if (num_comp == 1) {
                one_v = one_const;
            } else {
                std::vector<spv::Id> ones;
                ones.insert(ones.begin(), num_comp, one_const);

                one_v = m_b.makeCompositeConstant(type_f32_v[num_comp], ones);
            }

            result = m_b.createBinOp(spv::OpFDiv, m_b.getTypeId(source), one_v, source);
            break;
        }
        case Opcode::FRSQ: {
            const spv::Id source = load(inst.opr.src0, write_mask_source);
            result = m_b.createBuiltinCall(m_b.getTypeId(source), std_builtins, GLSLstd450InverseSqrt, { source });
            break;
        }
        case Opcode::FMUL:
        case Opcode::VMUL: {
            const spv::Id first = load(inst.opr.src0, write_mask_source);
            const spv::Id second = load(inst.opr.src1, write_mask_source);
            result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(first), first, second);
            break;
        }
        case Opcode::VDP: {
            const spv::Id first = load(inst.opr.src0, write_mask_source);
            const spv::Id second = load(inst.opr.src1, write_mask_source);
            result = m_b.createBinOp(spv::OpDot, type_f32, first, second);
            break;
        }
        case Opcode::FEXP: {
            const spv::Id source = load(inst.opr.src0, write_mask_source);
            result = m_b.createBuiltinCall(m_b.getTypeId(source), std_builtins, GLSLstd450Exp, { source });
            break;
        }
        case Opcode::FLOG: {
            const spv::Id source = load(inst.opr.src0, write_mask_source);
            result = m_b.createBuiltinCall(m_b.getTypeId(source), std_builtins, GLSLstd450Log, { source });
            break;
        }
        case Opcode::VSSQ: {
            const spv::Id source = load(inst.opr.src0, write_mask_source);
            result = m_b.createBinOp(spv::OpDot, type_f32, source, source);
            break;
        }
        case Opcode::FMAD:
        case Opcode::VMAD: {
            const spv::Id first = load(inst.opr.src0, write_mask_source);
            const spv::Id second = load(inst.opr.src1, write_mask_source);
            const spv::Id third = load(inst.opr.src2, write_mask_source);
            const spv::Id type = m_b.getTypeId(first);
            result = m_b.createBinOp(spv::OpFMul, type, first, second);
            result = m_b.createBinOp(spv::OpFAdd, type, result, third);
            break;
        }
        default:
            LOG_ERROR("Missing implementation for DUAL {}.", disasm::opcode_str(inst.opcode));
            return spv::NoResult;
        }

        return result;
    };

    spv::Id op1_result = do_dual_op(decoded_inst, decoded_source_mask);
    if (op1_result == spv::NoResult)
        return false;
    spv::Id op2_result = do_dual_op(decoded_inst_2, decoded_source_mask_2);
    if (op2_result == spv::NoResult)
        return false;

    // Dual instructions are async. To simulate that we store after both instructions are completed.
    store(decoded_inst.opr.dest, op1_result, decoded_dest_mask);
    store(decoded_inst_2.opr.dest, op2_result, decoded_dest_mask_2);

    return true;
}
