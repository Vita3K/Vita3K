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

#include <shader/spirv_recompiler.h>
#include <shader/usse_constant_table.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_utilities.h>

#include <util/bit_cast.h>
#include <util/float_to_half.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <features/state.h>

#include <bitset>

namespace shader::usse::utils {

static spv::Id get_correspond_constant_with_channel(spv::Builder &b, shader::usse::SwizzleChannel swizz) {
    switch (swizz) {
    case shader::usse::SwizzleChannel::C_0: {
        return b.makeFloatConstant(0.0f);
    }

    case shader::usse::SwizzleChannel::C_1: {
        return b.makeFloatConstant(1.0f);
    }

    case shader::usse::SwizzleChannel::C_2: {
        return b.makeFloatConstant(2.0f);
    }

    case shader::usse::SwizzleChannel::C_H: {
        return b.makeFloatConstant(0.5f);
    }

    default:
        break;
    }

    return spv::NoResult;
}

spv::Id finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, spv::Id offset, const Imm4 dest_mask) {
    if (first == spv::NoResult || second == spv::NoResult) {
        return spv::NoResult;
    }

    std::vector<spv::Id> ops;

    spv::Id target_type = utils::unwrap_type(b, b.getTypeId(first));

    const auto first_comp_count = b.getNumComponents(first);

    const bool offset_is_const = b.isConstant(offset);
    const int offset_value = offset_is_const ? b.getConstantScalar(offset) : 0;
    if (offset_is_const && (offset_value % 4 == 0) && is_default(swizz, 4) && dest_mask == 0b1111 && b.getNumComponents(first) == 4) {
        return first;
    }

    const spv::Id i32 = b.makeIntType(32);
    // if offset is not constant, threshold to be considered in the first base
    spv::Id first_base_threshold = 0;
    if (!offset_is_const) {
        // threshold is 4 - offset % 4
        spv::Id off_mod_4 = b.createBinOp(spv::OpBitwiseAnd, i32, offset, b.makeIntConstant(3));
        first_base_threshold = b.createBinOp(spv::OpISub, i32, b.makeIntConstant(4), off_mod_4);
    }

    // Try to plant a composite construct
    for (auto i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            if ((int)swizz[i] >= (int)SwizzleChannel::C_0) {
                ops.push_back(get_correspond_constant_with_channel(b, swizz[i]));
            } else if (offset_is_const) {
                int access_offset = offset_value % 4 + (int)swizz[i] - (int)SwizzleChannel::C_X;
                spv::Id access_base = first;

                if (access_offset >= first_comp_count) {
                    access_offset -= first_comp_count;
                    access_base = second;
                }

                if (!b.isScalar(access_base)) {
                    ops.push_back(b.createOp(spv::OpVectorExtractDynamic, target_type, { access_base, b.makeIntConstant(access_offset) }));
                } else {
                    ops.push_back(access_base);
                }
            } else {
                if (first_comp_count != 4) {
                    LOG_ERROR("Unhandled non-const offset without a vec4 entry");
                    return spv::NoResult;
                }
                // do exactly as above, but in spirv
                spv::Id delta = b.makeIntConstant((int)swizz[i] - (int)SwizzleChannel::C_X);
                spv::Id access_offset = b.createBinOp(spv::OpIAdd, i32, offset, delta);
                access_offset = b.createBinOp(spv::OpBitwiseAnd, i32, access_offset, b.makeIntConstant(3));

                spv::Id first_base = b.createOp(spv::OpVectorExtractDynamic, target_type, { first, access_offset });
                spv::Id second_base = b.createOp(spv::OpVectorExtractDynamic, target_type, { second, access_offset });
                // select one if (int)swizz[i] - (int)SwizzleChannel::C_X < 4 - offset % 4
                spv::Id cond = b.createBinOp(spv::OpSLessThan, b.makeBoolType(), delta, first_base_threshold);
                ops.push_back(b.createOp(spv::OpSelect, target_type, { cond, first_base, second_base }));
            }
        }
    }

    if (ops.size() == 1) {
        return ops[0];
    }

    return b.createCompositeConstruct(b.makeVectorType(target_type, static_cast<int>(ops.size())), ops);
}

size_t dest_mask_to_comp_count(shader::usse::Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    assert(bit_count <= 4 && bit_count > 0);
    return bit_count;
}

spv::Id create_access_chain(spv::Builder &b, const spv::StorageClass storage_class, const spv::Id base, const std::vector<spv::Id> &offsets) {
    spv::Builder::AccessChain access_chain{};
    access_chain.base = base;
    access_chain.indexChain = offsets;
    b.setAccessChain(access_chain);
    return b.createAccessChain(storage_class, base, offsets);
}

static const SpirvVarRegBank *get_reg_bank(const shader::usse::SpirvShaderParameters &params, shader::usse::RegisterBank reg_bank) {
    switch (reg_bank) {
    case RegisterBank::PRIMATTR:
        return &params.ins;
    case RegisterBank::SECATTR:
        return &params.uniforms;
    case RegisterBank::OUTPUT:
        return &params.outs;
    case RegisterBank::TEMP:
        return &params.temps;
    case RegisterBank::FPINTERNAL:
        return &params.internals;
    case RegisterBank::PREDICATE:
        return &params.predicates;
    case RegisterBank::INDEX:
        return &params.indexes;
    default:
        // LOG_WARN("Reg bank {} unsupported", static_cast<uint8_t>(reg_bank));
        return nullptr;
    }
}

static spv::Function *make_fx10_unpack_func(spv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *fx10_unpack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_i32 = b.makeIntType(32);
    spv::Id ivec3 = b.makeVectorType(type_i32, 3);
    spv::Id uvec3 = b.makeVectorType(b.makeUintType(32), 3);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v3 = b.makeVectorType(type_f32, 3);

    spv::Function *fx10_unpack_func = b.makeFunctionEntry(
        spv::NoPrecision, type_f32_v3, "unpack3xFX10", spv::LinkageTypeMax, { type_f32 },
        decorations, &fx10_unpack_func_block);
    b.setupFunctionDebugInfo(fx10_unpack_func, "unpack3xFX10", { type_f32 }, { "to_unpack" });
    fx10_unpack_func->setReturnPrecision(spv::DecorationRelaxedPrecision);

    spv::Id extracted = fx10_unpack_func->getParamId(0);

    // Cast to int first
    extracted = b.createUnaryOp(spv::OpBitcast, type_i32, extracted);
    spv::Id vec = b.createCompositeConstruct(ivec3, { extracted, extracted, extracted });

    // vec = vec >> uvec3(0,10,20);
    // note: note entirely sure, I really hope the layout is the same as in a 32-bit little-endian integer
    const spv::Id shift_amount = b.makeCompositeConstant(uvec3, { b.makeUintConstant(0), b.makeUintConstant(10), b.makeUintConstant(20) });
    vec = b.createBinOp(spv::OpShiftRightLogical, ivec3, vec, shift_amount);

    // sign-extend the 10-bit integer:
    // vec <<= 22 (logical)
    // vec >>= 22 (arithmetic)
    spv::Id extend_amount = b.makeUintConstant(22);
    extend_amount = b.makeCompositeConstant(uvec3, { extend_amount, extend_amount, extend_amount });
    vec = b.createBinOp(spv::OpShiftLeftLogical, ivec3, vec, extend_amount);
    vec = b.createBinOp(spv::OpShiftRightArithmetic, ivec3, vec, extend_amount);

    // normalize it
    vec = convert_to_float(b, utils, vec, DataType::C10, true);

    b.makeReturn(false, vec);
    b.setBuildPoint(last_build_point);

    return fx10_unpack_func;
}

static spv::Function *make_fx10_pack_func(spv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *fx10_pack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    // Basic types
    spv::Id type_i32 = b.makeIntType(32);
    spv::Id type_u32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);

    // Vector types of 4 components
    spv::Id vec4_u32 = b.makeVectorType(type_u32, 4);
    spv::Id vec4_i32 = b.makeVectorType(type_i32, 4);
    spv::Id vec4_f32 = b.makeVectorType(type_f32, 4);

    // Create function entry: float pack4xFX10(vec4 to_pack)
    spv::Function *fx10_pack_func = b.makeFunctionEntry(
        spv::NoPrecision, type_f32, "pack4xFX10", spv::LinkageTypeMax, { vec4_f32 },
        decorations, &fx10_pack_func_block);
    b.setupFunctionDebugInfo(fx10_pack_func, "pack4xFX10", { type_f32 }, { "to_pack" });

    // Get function parameter id (input vector)
    spv::Id extracted = fx10_pack_func->getParamId(0);

    // Clamp input float vector to range [-2.0, 2.0]
    // This ensures values fit in signed 10-bit FX10 format range
    spv::Id min_val = create_constant_vector_or_scalar(b, b.makeFloatConstant(-2.f), 4);
    spv::Id max_val = create_constant_vector_or_scalar(b, b.makeFloatConstant(2.f), 4);
    spv::Id clamped = b.createBuiltinCall(vec4_f32, utils.std_builtins, GLSLstd450FClamp, { extracted, min_val, max_val });

    // Convert clamped float vector to signed 10-bit integer vector (normalized)
    spv::Id int_vec = convert_to_int(b, utils, clamped, DataType::C10, true);

    // Bitcast int vector to unsigned vector for safe bitwise operations
    spv::Id int_vec_u = b.createUnaryOp(spv::OpBitcast, vec4_u32, int_vec);

    // Create 10-bit mask (0x3FF) for each vector component
    spv::Id mask_10bits = b.makeCompositeConstant(vec4_u32,
        { b.makeUintConstant(0x3FF), b.makeUintConstant(0x3FF), b.makeUintConstant(0x3FF), b.makeUintConstant(0x3FF) });

    // Mask out only the lowest 10 bits for each component
    int_vec_u = b.createBinOp(spv::OpBitwiseAnd, vec4_u32, int_vec_u, mask_10bits);

    // Shift each component by (0, 10, 20, 30) bits to pack into a single 32-bit uint
    spv::Id shifts = b.makeCompositeConstant(vec4_u32,
        { b.makeUintConstant(0), b.makeUintConstant(10), b.makeUintConstant(20), b.makeUintConstant(30) });
    int_vec_u = b.createBinOp(spv::OpShiftLeftLogical, vec4_u32, int_vec_u, shifts);

    // Combine all 4 components into a single uint using bitwise OR
    spv::Id packed = b.createCompositeExtract(int_vec_u, type_u32, 0);
    for (int i = 1; i < 4; ++i) {
        spv::Id comp = b.createCompositeExtract(int_vec_u, type_u32, i);
        packed = b.createBinOp(spv::OpBitwiseOr, type_u32, packed, comp);
    }

    // Bitcast the packed uint into a float (bitwise equivalent)
    packed = b.createUnaryOp(spv::OpBitcast, type_f32, packed);

    // Return the packed float
    b.makeReturn(false, packed);

    // Restore previous build point
    b.setBuildPoint(last_build_point);

    return fx10_pack_func;
}

static spv::Function *make_unpack_func(spv::Builder &b, const FeatureState &features, DataType source_type) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *unpack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_i32 = b.makeIntType(32);
    spv::Id type_ui32 = b.makeUintType(32);

    std::string func_name;
    spv::Id output_type;
    int comp_count;
    bool is_signed;

    switch (source_type) {
    case DataType::UINT16: {
        func_name = "unpack2xU16";
        output_type = b.makeVectorType(type_ui32, 2);
        comp_count = 2;
        is_signed = false;
        break;
    }
    case DataType::INT16: {
        func_name = "unpack2xS16";
        output_type = b.makeVectorType(type_i32, 2);
        comp_count = 2;
        is_signed = true;
        break;
    }
    case DataType::UINT8: {
        func_name = "unpack4xU8";
        output_type = b.makeVectorType(type_ui32, 4);
        comp_count = 4;
        is_signed = false;
        break;
    }
    case DataType::INT8: {
        func_name = "unpack4xS8";
        output_type = b.makeVectorType(type_i32, 4);
        comp_count = 4;
        is_signed = true;
        break;
    }
    default:
        assert(false);
        return nullptr;
    }

    spv::Function *unpack_func = b.makeFunctionEntry(
        spv::NoPrecision, output_type, func_name.c_str(), spv::LinkageTypeMax, { type_f32 },
        decorations, &unpack_func_block);
    b.setupFunctionDebugInfo(unpack_func, func_name.c_str(), { type_f32 }, { "to_unpack" });
    unpack_func->setReturnPrecision(spv::DecorationRelaxedPrecision);
    spv::Id extracted = unpack_func->getParamId(0);

    const spv::Id result_type = is_signed ? type_i32 : type_ui32;
    extracted = b.createUnaryOp(spv::OpBitcast, result_type, extracted);

    const auto comp_bits = 32 / comp_count;
    spv::Id comp_bits_val = b.makeUintConstant(comp_bits);

    std::vector<spv::Id> comps;
    for (int i = 0; i < comp_count; ++i) {
        const spv::Op op = is_signed ? spv::OpBitFieldSExtract : spv::OpBitFieldUExtract;
        spv::Id comp = b.createTriOp(op, result_type, extracted, b.makeUintConstant(comp_bits * i), comp_bits_val);

        comps.push_back(comp);
    }

    auto output = b.createCompositeConstruct(output_type, comps);

    b.makeReturn(false, output);
    b.setBuildPoint(last_build_point);

    return unpack_func;
}

static spv::Function *make_pack_func(spv::Builder &b, const FeatureState &features, DataType source_type) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *pack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_i32 = b.makeIntType(32);
    spv::Id type_f32 = b.makeFloatType(32);

    std::string func_name;
    spv::Id input_type;
    int comp_count;
    bool is_signed;

    switch (source_type) {
    case DataType::UINT16: {
        func_name = "pack2xU16";
        input_type = b.makeVectorType(type_ui32, 2);
        comp_count = 2;
        is_signed = false;
        break;
    }
    case DataType::INT16: {
        func_name = "pack2xS16";
        input_type = b.makeVectorType(type_i32, 2);
        comp_count = 2;
        is_signed = true;
        break;
    }
    case DataType::UINT8: {
        func_name = "pack4xU8";
        input_type = b.makeVectorType(type_ui32, 4);
        comp_count = 4;
        is_signed = false;
        break;
    }
    case DataType::INT8: {
        func_name = "pack4xS8";
        input_type = b.makeVectorType(type_i32, 4);
        comp_count = 4;
        is_signed = true;
        break;
    }
    default:
        assert(false);
        return nullptr;
    }

    spv::Function *pack_func = b.makeFunctionEntry(
        spv::NoPrecision, type_f32, func_name.c_str(), spv::LinkageTypeMax, { input_type },
        decorations, &pack_func_block);
    b.setupFunctionDebugInfo(pack_func, func_name.c_str(), { input_type }, { "to_pack" });

    pack_func->addParamPrecision(0, spv::DecorationRelaxedPrecision);
    spv::Id extracted = pack_func->getParamId(0);
    const int comp_bits = 32 / comp_count;

    const spv::Id comp_type = b.getContainedTypeId(input_type);

    spv::Id output = b.makeUintConstant(0);
    for (int i = 0; i < comp_count; ++i) {
        spv::Id comp = b.createBinOp(spv::OpVectorExtractDynamic, comp_type, extracted, b.makeIntConstant(i));

        if (is_signed)
            comp = b.createUnaryOp(spv::OpBitcast, type_ui32, comp);

        output = b.createOp(spv::OpBitFieldInsert, type_ui32, { output, comp, b.makeIntConstant(comp_bits * i), b.makeIntConstant(comp_bits) });
    }

    output = b.createUnaryOp(spv::OpBitcast, type_f32, output);

    b.makeReturn(false, output);
    b.setBuildPoint(last_build_point);

    return pack_func;
}

static spv::Function *make_f16_unpack_func(spv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_unpack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    spv::Function *f16_unpack_func = b.makeFunctionEntry(
        spv::NoPrecision, type_f32_v2, "unpack2xF16", spv::LinkageTypeMax, { type_f32 },
        decorations, &f16_unpack_func_block);
    b.setupFunctionDebugInfo(f16_unpack_func, "unpack2xF16", { type_f32 }, { "to_unpack" });
    f16_unpack_func->setReturnPrecision(spv::DecorationRelaxedPrecision);

    spv::Id extracted = f16_unpack_func->getParamId(0);

    extracted = b.createUnaryOp(spv::OpBitcast, type_ui32, extracted);
    extracted = b.createBuiltinCall(type_f32_v2, utils.std_builtins, GLSLstd450UnpackHalf2x16, { extracted });

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_unpack_func;
}

static spv::Function *make_f16_pack_func(spv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_pack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    spv::Function *f16_pack_func = b.makeFunctionEntry(
        spv::NoPrecision, type_f32, "pack2xF16", spv::LinkageTypeMax, { type_f32_v2 },
        decorations, &f16_pack_func_block);
    b.setupFunctionDebugInfo(f16_pack_func, "pack2xF16", { type_f32_v2 }, { "to_pack" });

    f16_pack_func->addParamPrecision(0, spv::DecorationRelaxedPrecision);
    spv::Id extracted = f16_pack_func->getParamId(0);

    // use packHalf2x16
    extracted = b.createBuiltinCall(type_ui32, utils.std_builtins, GLSLstd450PackHalf2x16, { extracted });
    extracted = b.createUnaryOp(spv::OpBitcast, type_f32, extracted);

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_pack_func;
}

static spv::Function *make_fetch_memory_func_for_array(spv::Builder &b, spv::Id buffer_container, const SpirvUniformBufferInfo &info, const int buffer_index) {
    // The address can be unaligned, so we load two words around address / 4 and combine them.
    // | = address
    // s = memory[address/4] (source)
    // f = memory[address/4+1] (friend)
    // sss|(sfff)f
    // Data inside () is what we want.

    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_i32 = b.makeIntType(32);
    spv::Block *func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    const std::string func_name = fmt::format("fetchMemoryForBuffer{}Base{}", buffer_index, info.base);

    spv::Function *fetch_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, func_name.c_str(), spv::LinkageTypeMax, { type_i32 },
        {}, &func_block);
    b.setupFunctionDebugInfo(fetch_func, func_name.c_str(), { type_i32 }, { "addr" });

    spv::Id sixteen_cst = b.makeIntConstant(16);
    spv::Id eight_cst = b.makeIntConstant(8);
    spv::Id four_cst = b.makeIntConstant(4);
    spv::Id one_cst = b.makeIntConstant(1);
    spv::Id zero_cst = b.makeIntConstant(0);

    spv::Id addr = fetch_func->getParamId(0);
    spv::Id base_vector = b.createBinOp(spv::OpSDiv, type_i32, addr, sixteen_cst);
    spv::Id base_left = b.createBinOp(spv::OpSRem, type_i32, addr, sixteen_cst);
    spv::Id base_offset = b.createBinOp(spv::OpSDiv, type_i32, base_left, four_cst);
    spv::Id rem = b.createBinOp(spv::OpSRem, type_i32, base_left, four_cst);
    spv::Id rem_inv = b.createBinOp(spv::OpISub, type_i32, four_cst, rem);

    // If int was shifted by more than 32 bits in nvidia glsl, the pipeline crashes.
    // rem_inv_overflow is the flag used to make sure >> 32 is not executed.
    spv::Id rem_inv_overflow = b.createBinOp(spv::OpIEqual, b.makeBoolType(), rem_inv, four_cst);
    rem_inv = b.createTriOp(spv::OpSelect, type_i32, rem_inv_overflow, zero_cst, rem_inv);

    spv::Id rem_in_bits = b.createBinOp(spv::OpIMul, type_i32, rem, eight_cst);
    spv::Id rem_inv_in_bits = b.createBinOp(spv::OpIMul, type_i32, rem_inv, eight_cst);

    spv::Id src = b.createLoad(utils::create_access_chain(b, spv::StorageClassStorageBuffer, buffer_container, { b.makeIntConstant(info.index_in_container), base_vector, base_offset }), spv::NoPrecision);

    spv::Id friend_offset = b.createBinOp(spv::OpIAdd, type_i32, base_offset, one_cst);
    spv::Id friend_vector = b.createBinOp(spv::OpIAdd, type_i32, base_vector, b.createBinOp(spv::OpSDiv, type_i32, friend_offset, b.makeIntConstant(4)));

    friend_offset = b.createBinOp(spv::OpSRem, type_i32, friend_offset, four_cst);

    spv::Id src_friend = b.createLoad(utils::create_access_chain(b, spv::StorageClassStorageBuffer, buffer_container, { b.makeIntConstant(info.index_in_container), friend_vector, friend_offset }), spv::NoPrecision);
    spv::Id src_casted = b.createUnaryOp(spv::OpBitcast, type_ui32, src);
    spv::Id src_friend_casted = b.createUnaryOp(spv::OpBitcast, type_ui32, src_friend);

    spv::Id high_part = b.createBinOp(spv::OpShiftLeftLogical, type_ui32, src_casted, rem_in_bits);
    spv::Id low_part = b.createBinOp(spv::OpShiftRightLogical, type_ui32, src_friend_casted, rem_inv_in_bits);
    low_part = b.createTriOp(spv::OpSelect, type_ui32, rem_inv_overflow, b.makeUintConstant(0), low_part);

    spv::Id output = b.createBinOp(spv::OpBitwiseOr, type_ui32, high_part, low_part);
    spv::Id output_casted = b.createUnaryOp(spv::OpBitcast, type_f32, output);

    b.makeReturn(false, output_casted);
    b.setBuildPoint(last_build_point);

    return fetch_func;
}

static spv::Function *make_fetch_memory_func(spv::Builder &b, const SpirvShaderParameters &params) {
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_i32 = b.makeIntType(32);
    spv::Id type_bool = b.makeBoolType();

    spv::Block *func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Function *fetch_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, "fetchMemory", spv::LinkageTypeMax, { type_i32 },
        {}, &func_block);
    b.setupFunctionDebugInfo(fetch_func, "fetchMemory", { type_i32 }, { "addr" });

    spv::Id addr = fetch_func->getParamId(0);

    std::stack<std::unique_ptr<spv::Builder::If>> fetch_stacks;

    for (auto &[index, buffer_info] : params.buffers) {
        if (!fetch_stacks.empty()) {
            fetch_stacks.top()->makeBeginElse();
        }

        const spv::Id range_begin = b.makeIntConstant(buffer_info.base);
        const spv::Id range_end = b.makeIntConstant(buffer_info.base + buffer_info.size);

        spv::Id need1 = b.createBinOp(spv::OpSGreaterThanEqual, type_bool, addr, range_begin);
        spv::Id need2 = b.createBinOp(spv::OpSLessThan, type_bool, addr, range_end);

        spv::Id need_final = b.createBinOp(spv::OpLogicalAnd, type_bool, need1, need2);

        fetch_stacks.push(std::make_unique<spv::Builder::If>(need_final, spv::SelectionControlMaskNone, b));

        spv::Id subtracted_base = b.createBinOp(spv::OpISub, type_i32, addr, range_begin);
        spv::Function *access_func = make_fetch_memory_func_for_array(b, params.buffer_container, buffer_info, index);

        b.makeReturn(false, b.createFunctionCall(access_func, { subtracted_base }));
    }

    while (!fetch_stacks.empty()) {
        std::unique_ptr<spv::Builder::If> fetch_if = std::move(fetch_stacks.top());
        fetch_if->makeEndIf();

        fetch_stacks.pop();
    }

    b.makeReturn(false, b.makeFloatConstant(0.0f));
    b.setBuildPoint(last_build_point);

    return fetch_func;
}

spv::Id fetch_memory(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, spv::Id addr) {
    if (!utils.fetch_memory) {
        utils.fetch_memory = make_fetch_memory_func(b, params);
    }

    return b.createFunctionCall(utils.fetch_memory, { addr });
}

static spv::Id make_or_get_buffer_ptr(spv::Builder &b, shader::usse::utils::SpirvUtilFunctions &utils, int nb_components, int stride = 16, bool is_write = false) {
    const int buffer_utils_idx = (stride == 4) ? 0 : nb_components;

    if (utils.buffer_address_vec[buffer_utils_idx][is_write])
        return utils.buffer_address_vec[buffer_utils_idx][is_write];

    const spv::Id f32 = b.makeFloatType(32);
    const spv::Id vec = shader::usse::utils::make_vector_or_scalar_type(b, f32, nb_components);
    const spv::Id runtime_array = b.makeRuntimeArray(vec);
    // always a stride of 16, even if the array size is less
    b.addDecoration(runtime_array, spv::DecorationArrayStride, stride);
    const spv::Id buffer_data = b.makeStructType({ runtime_array }, fmt::format("buffer_ptr{}_s{}", nb_components, stride).c_str());
    b.addDecoration(buffer_data, spv::DecorationBlock);
    b.addMemberName(buffer_data, 0, "data");
    // non-writable for the time being
    if (is_write)
        b.addMemberDecoration(buffer_data, 0, spv::DecorationNonReadable);
    else
        b.addMemberDecoration(buffer_data, 0, spv::DecorationNonWritable);
    b.addMemberDecoration(buffer_data, 0, spv::DecorationOffset, 0);

    utils.buffer_address_vec[buffer_utils_idx][is_write] = b.makePointer(spv::StorageClassPhysicalStorageBuffer, buffer_data);
    return utils.buffer_address_vec[buffer_utils_idx][is_write];
}

void buffer_address_access(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, int dest_offset, spv::Id addr, uint32_t component_size, uint32_t nb_components, int buffer_idx, bool is_buffer_store) {
    const spv::Id i32 = b.makeIntType(32);
    const spv::Id zero = b.makeIntConstant(0);

    spv::Id buffer_idx_val;
    if (buffer_idx == -1) {
        // buffer index is in the upper 4 bits of addr
        buffer_idx_val = b.createBinOp(spv::OpShiftRightLogical, i32, addr, b.makeIntConstant(28));
        // remove the buffer index bits from the address
        addr = b.createBinOp(spv::OpBitwiseAnd, i32, addr, b.makeIntConstant((1 << 28) - 1));
    } else {
        buffer_idx_val = b.makeIntConstant(buffer_idx);
    }

    spv::Id buffer_address = utils::create_access_chain(b, spv::StorageClassUniform, params.render_info_id, { b.makeIntConstant(params.buffer_addresses_id), buffer_idx_val });
    buffer_address = b.createLoad(buffer_address, spv::NoPrecision);
    // add the offset from the base address
    buffer_address = add_uvec2_uint(b, buffer_address, addr);

    if (component_size == sizeof(uint32_t)) {
        int buffer_idx_vec4 = 0;
        if (nb_components >= 4) {
            // first copy them 4 by 4 (using the fact that we can do 4-byte aligned reads)
            const spv::Id buffer_container = make_or_get_buffer_ptr(b, utils, 4, 16, is_buffer_store);
            const spv::Id buffer_address_vec4 = b.createUnaryOp(spv::OpBitcast, buffer_container, buffer_address);
            while (nb_components >= 4) {
                spv::Id accessed = utils::create_access_chain(b, spv::StorageClassPhysicalStorageBuffer, buffer_address_vec4, { zero, b.makeIntConstant(buffer_idx_vec4) });

                if (is_buffer_store) {
                    spv::Id data = load(b, params, utils, features, dest, 0b1111, dest_offset);
                    b.createStore(data, accessed, spv::MemoryAccessAlignedMask, spv::ScopeMax, 4);
                } else {
                    accessed = b.createLoad(accessed, spv::NoPrecision, spv::MemoryAccessAlignedMask, spv::ScopeMax, 4);
                    store(b, params, utils, features, dest, accessed, 0b1111, dest_offset);
                }

                dest.num += 4;
                nb_components -= 4;
                buffer_idx_vec4++;
            }
        }

        assert(nb_components < 4);
        if (nb_components > 0) {
            // do one last load for the at most 3 last components
            const spv::Id buffer_container = make_or_get_buffer_ptr(b, utils, nb_components, 16, is_buffer_store);
            const spv::Id buffer_address_vec = b.createUnaryOp(spv::OpBitcast, buffer_container, buffer_address);

            spv::Id accessed = utils::create_access_chain(b, spv::StorageClassPhysicalStorageBuffer, buffer_address_vec, { zero, b.makeIntConstant(buffer_idx_vec4) });

            if (is_buffer_store) {
                spv::Id data = load(b, params, utils, features, dest, (1 << nb_components) - 1, dest_offset);
                b.createStore(data, accessed, spv::MemoryAccessAlignedMask, spv::ScopeMax, 4);
            } else {
                accessed = b.createLoad(accessed, spv::NoPrecision, spv::MemoryAccessAlignedMask, spv::ScopeMax, 4);
                store(b, params, utils, features, dest, accessed, (1 << nb_components) - 1, dest_offset);
            }
        }
    } else {
        if (is_buffer_store) {
            LOG_ERROR("non-32 bit buffer store is not implemented! Please report it to the devs.");
            return;
        }

        // less optimized
        // TODO: if the gpu supports it, load it as a u16vec4 / u8vec4
        const spv::Id buffer_container = make_or_get_buffer_ptr(b, utils, 1, 4);
        // pack the component by groups of 4 (except possible the last ones) when storing them
        std::vector<spv::Id> loaded_components;

        for (uint32_t component_idx = 0; component_idx < nb_components; component_idx++) {
            spv::Id component_addr = add_uvec2_uint(b, buffer_address, b.makeUintConstant(component_idx * component_size));
            // we must make it 4-byte aligned
            spv::Id addr_low_bits = b.createCompositeExtract(component_addr, i32, 0);
            spv::Id alignment = b.createBinOp(spv::OpBitwiseAnd, i32, addr_low_bits, b.makeIntConstant(0b11));
            addr_low_bits = b.createBinOp(spv::OpBitwiseAnd, i32, addr_low_bits, b.makeIntConstant(~0b11));
            component_addr = b.createCompositeInsert(addr_low_bits, component_addr, b.getTypeId(component_addr), 0);

            // now we can finally load it
            component_addr = b.createUnaryOp(spv::OpBitcast, buffer_container, component_addr);
            spv::Id loaded = utils::create_access_chain(b, spv::StorageClassPhysicalStorageBuffer, component_addr, { zero, zero });
            loaded = b.createLoad(loaded, spv::NoPrecision, spv::MemoryAccessAlignedMask, spv::ScopeMax, 4);

            // now keep only the interesting 8/16 bits
            loaded = b.createUnaryOp(spv::OpBitcast, i32, loaded);
            spv::Id shift = b.createBinOp(spv::OpShiftLeftLogical, i32, alignment, b.makeIntConstant(3)); // 1 byte = 8 bits
            loaded = b.createOp(spv::OpBitFieldSExtract, i32, { loaded, shift, b.makeIntConstant(component_size * 8) });

            loaded_components.push_back(loaded);

            if (loaded_components.size() == 4 || component_idx == nb_components - 1) {
                spv::Id component_vec;
                if (loaded_components.size() == 1) {
                    component_vec = loaded_components[0];
                } else {
                    component_vec = b.createCompositeConstruct(b.makeVectorType(i32, loaded_components.size()), loaded_components);
                }
                store(b, params, utils, features, dest, component_vec, (1 << loaded_components.size()) - 1, dest_offset);

                dest.num += component_size;
                loaded_components.clear();
            }
        }
    }
}

spv::Id unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type) {
    switch (type) {
    case DataType::INT8:
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::INT16: {
        auto iter = utils.unpack_funcs.find(type);
        if (iter == utils.unpack_funcs.end()) {
            iter = utils.unpack_funcs.emplace(type, make_unpack_func(b, features, type)).first;
        }
        return b.createFunctionCall(iter->second, { scalar });
    }
    case DataType::F16: {
        auto iter = utils.unpack_funcs.find(type);
        if (iter == utils.unpack_funcs.end()) {
            iter = utils.unpack_funcs.emplace(type, make_f16_unpack_func(b, utils, features)).first;
        }
        return b.createFunctionCall(iter->second, { scalar });
    }
    case DataType::C10: {
        if (!utils.unpack_fx10) {
            utils.unpack_fx10 = make_fx10_unpack_func(b, utils, features);
        }

        return b.createFunctionCall(utils.unpack_fx10, { scalar });
    }
    default: {
        LOG_ERROR("Unsupported unpack type: 0x{:0X}", fmt::underlying(type));
        break;
    }
    }

    return spv::NoResult;
}

spv::Id pack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id vec, const DataType source_type) {
    switch (source_type) {
    case DataType::INT8:
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::INT16: {
        auto iter = utils.pack_funcs.find(source_type);
        if (iter == utils.pack_funcs.end()) {
            iter = utils.pack_funcs.emplace(source_type, make_pack_func(b, features, source_type)).first;
        }
        return b.createFunctionCall(iter->second, { vec });
    }
    case DataType::C10: {
        if (!utils.pack_fx10) {
            utils.pack_fx10 = make_fx10_pack_func(b, utils, features);
        }
        return b.createFunctionCall(utils.pack_fx10, { vec });
    }
    case DataType::F16: {
        auto iter = utils.pack_funcs.find(source_type);
        if (iter == utils.pack_funcs.end()) {
            iter = utils.pack_funcs.emplace(source_type, make_f16_pack_func(b, utils, features)).first;
        }
        return b.createFunctionCall(iter->second, { vec });
    }

    default: {
        LOG_ERROR("Unsupported pack type: 0x{:0X}", fmt::underlying(source_type));
        break;
    }
    }

    return spv::NoResult;
}

static spv::Id apply_modifiers(spv::Builder &b, const SpirvUtilFunctions &utils, const shader::usse::RegisterFlags flags, spv::Id val) {
    spv::Id contained_type = b.getTypeId(val);

    if (!b.isScalarType(contained_type)) {
        contained_type = b.getContainedTypeId(contained_type);
    }

    const bool is_int = b.isIntType(contained_type);
    const bool is_uint = b.isUintType(contained_type);

    const int num_comp = b.getNumComponents(val);
    spv::Id dest_type = b.getTypeId(val);

    spv::Id result = val;

    if (flags & shader::usse::RegisterFlags::Absolute) {
        // Absolute the result
        if (is_uint) {
            // It's already > 0, what do you expect more
            return result;
        }

        result = b.createBuiltinCall(dest_type, utils.std_builtins, GLSLstd450FAbs, { result });
    }

    // Apply modifier flags
    if (flags & shader::usse::RegisterFlags::Negative) {
        // Negate the value
        spv::Id c0 = spv::NoResult;
        spv::Op sub_op = spv::OpAny;

        if (is_int) {
            c0 = b.makeIntConstant(0);
            sub_op = spv::OpISub;
        } else if (is_uint) {
            c0 = b.makeUintConstant(0);
            sub_op = spv::OpISub;
        } else {
            c0 = b.makeFloatConstant(0.0f);
            sub_op = spv::OpFSub;
        }

        std::vector<spv::Id> ops(num_comp, c0);
        result = b.createBinOp(sub_op, dest_type, (num_comp == 1) ? c0 : b.makeCompositeConstant(dest_type, ops), result);
    }

    return result;
}

spv::Id load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset) {
    spv::Id type_f32 = b.makeFloatType(32);

    if (op.bank == RegisterBank::FPCONSTANT) {
        const bool integral_unsigned = (op.type == DataType::UINT32) || (op.type == DataType::UINT16);
        const bool integral_signed = (op.type == DataType::INT32) || (op.type == DataType::INT16);
        std::vector<spv::Id> consts;

        auto handle_unexpect_swizzle = [&](const SwizzleChannel ch) {
#define GEN_CONSTANT(cnst)                                           \
    if (integral_unsigned)                                           \
        return b.makeUintConstant(static_cast<std::uint32_t>(cnst)); \
    else if (integral_signed)                                        \
        return b.makeIntConstant(static_cast<std::uint32_t>(cnst));  \
    else                                                             \
        return b.makeFloatConstant(static_cast<float>(cnst))
            switch (ch) {
            case SwizzleChannel::C_0:
                GEN_CONSTANT(0);
                break;
            case SwizzleChannel::C_1:
                GEN_CONSTANT(1);
                break;
            case SwizzleChannel::C_2:
                GEN_CONSTANT(2);
                break;
            case SwizzleChannel::C_H:
                GEN_CONSTANT(0.5f);
                break;
            default: break;
            }

            return spv::NoResult;

#undef GEN_CONSTANT
        };

        // https://wiki.henkaku.xyz/vita/SGX543#Constants
        // Load constants. Ignore mask
        if ((op.type == DataType::F32) || (op.type == DataType::UINT32) || (op.type == DataType::INT32)) {
            auto get_f32_from_bank = [&](const int num) -> spv::Id {
                int swizz_val = static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::C_X);
                if (swizz_val >= 4)
                    return handle_unexpect_swizzle(op.swizzle[num]);

                uint32_t value = 0;
                // bank 1 is only used for channel 1
                if (swizz_val == 1) {
                    value = usse::f32_constant_table_bank_1_raw[op.num];
                } else {
                    value = usse::f32_constant_table_bank_0_raw[op.num];
                }

                if (integral_unsigned)
                    return b.makeUintConstant(value);
                else if (integral_signed)
                    return b.makeIntConstant(value);
                else
                    return b.makeFloatConstant(std::bit_cast<float>(value));
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    consts.push_back(get_f32_from_bank(i));
                }
            }
        } else if ((op.type == DataType::F16) || (op.type == DataType::UINT16) || (op.type == DataType::INT16)) {
            auto get_f16_from_bank = [&](const int num) -> spv::Id {
                const int swizz_val = static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::C_X);
                if (swizz_val >= 4)
                    return handle_unexpect_swizzle(op.swizzle[num]);

                float value = 0.f;
                switch (swizz_val) {
                case 1:
                    value = usse::f16_constant_table_bank1[op.num];
                    break;

                case 2:
                    value = usse::f16_constant_table_bank2[op.num];
                    break;

                case 3:
                    value = usse::f16_constant_table_bank3[op.num];
                    break;

                default:
                    value = usse::f16_constant_table_bank0[op.num];
                    break;
                }

                if (integral_unsigned || integral_signed) {
                    uint16_t value_int = util::encode_flt16(value);
                    if (integral_unsigned)
                        return b.makeUintConstant(value_int);
                    else
                        return b.makeIntConstant(static_cast<int16_t>(value));
                } else {
                    return b.makeFloatConstant(value);
                }
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    consts.push_back(get_f16_from_bank(i));
                }
            }
        }

        if (consts.size() == 1) {
            return apply_modifiers(b, utils, op.flags, consts[0]);
        } else {
            spv::Id result = b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(consts.size())), consts);
            return apply_modifiers(b, utils, op.flags, result);
        }
    }

    if (op.bank == RegisterBank::FPINTERNAL) {
        // Automatically F32
        switch (op.type) {
        case DataType::F16: {
            op.type = DataType::F32;
            break;
        }

        case DataType::INT16:
        case DataType::INT8: {
            op.type = DataType::INT32;
            break;
        }

        case DataType::UINT16:
        case DataType::UINT8: {
            op.type = DataType::UINT32;
            break;
        }

        default:
            break;
        }

        op.num <<= 2;
        shift_offset <<= 2;
    }

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);
    const std::size_t size_comp = get_data_type_size(op.type);

    if (op.bank == RegisterBank::PREDICATE || op.bank == RegisterBank::INDEX) {
        spv::Id bank_base = *get_reg_bank(params, op.bank);
        if (op.bank == RegisterBank::INDEX) {
            op.num -= 1;
        }
        spv::Id result = b.createLoad(b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(op.num) }), spv::NoPrecision);

        if (!is_float_data_type(op.type) && size_comp < sizeof(int32_t)) {
            spv::Id mask;
            spv::Id type;
            switch (op.type) {
            case DataType::UINT8:
                mask = b.makeUintConstant(0xFF);
                type = b.makeUintType(32);
                break;
            case DataType::INT8:
                mask = b.makeIntConstant(0xFF);
                type = b.makeIntType(32);
                break;
            case DataType::UINT16:
                mask = b.makeUintConstant(0xFFFF);
                type = b.makeUintType(32);
                break;
            default: // DataType::UINT16
                mask = b.makeIntConstant(0xFFFF);
                type = b.makeIntType(32);
                break;
            }
            result = b.createBinOp(spv::OpBitwiseAnd, type, result, mask);
        }

        return result;
    }

    if (op.bank == RegisterBank::IMMEDIATE || !get_reg_bank(params, op.bank)) {
        if (op.bank != RegisterBank::INDEXED1 && op.bank != RegisterBank::INDEXED2) {
            if (dest_comp_count == 1) {
                if ((int)op.swizzle[0] >= (int)SwizzleChannel::C_0) {
                    return get_correspond_constant_with_channel(b, op.swizzle[0]);
                }
            }

            spv::Id constant = spv::NoResult;

            const int imm = (op.bank == RegisterBank::IMMEDIATE) ? op.num : 0;

            if (is_unsigned_integer_data_type(op.type)) {
                constant = b.makeUintConstant(imm);
            } else if (is_signed_integer_data_type(op.type)) {
                constant = b.makeIntConstant(imm);
            } else {
                constant = b.makeFloatConstant(static_cast<float>(imm));
            }

            if (dest_comp_count == 1) {
                return apply_modifiers(b, utils, op.flags, constant);
            }

            std::vector<spv::Id> ops(dest_comp_count, constant);
            spv::Id pass = b.makeCompositeConstant(b.makeVectorType(b.getTypeId(constant), static_cast<int>(dest_comp_count)), ops);

            pass = finalize(b, pass, pass, op.swizzle, b.makeIntConstant(shift_offset), dest_mask);
            return apply_modifiers(b, utils, op.flags, pass);
        }
    }

    spv::Id idx_in_arr_1 = spv::NoResult;
    spv::Id idx_in_arr_2 = spv::NoResult;

    spv::Id finalize_offset = b.makeIntConstant(op.num + shift_offset);
    if (op.bank == RegisterBank::INDEXED1 || op.bank == RegisterBank::INDEXED2) {
        // Decode the info. Usually the number of bits in a INDEXED number is 7.
        // TODO: Fix the assumption
        const Imm2 bank_enc = (op.num >> 5) & 0b11;
        const Imm5 add_off = (op.num & 0b11111) + shift_offset;

        const std::int8_t idx_off = (int)op.bank - (int)RegisterBank::INDEXED1;

        switch (bank_enc) {
        case 0: {
            op.bank = RegisterBank::TEMP;
            break;
        }

        case 1: {
            op.bank = RegisterBank::OUTPUT;
            break;
        }

        case 2: {
            op.bank = RegisterBank::PRIMATTR;
            break;
        }

        case 3: {
            op.bank = RegisterBank::SECATTR;
            break;
        }
        }

        spv::Id type_i32 = b.makeIntType(32);

        // Calculate the "at" offset.
        spv::Id idx_reg_val = b.createLoad(b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, type_i32), { params.indexes, b.makeIntConstant(idx_off) }), spv::NoPrecision);

        spv::Id real_idx = b.createBinOp(spv::OpIAdd, type_i32, b.createBinOp(spv::OpIMul, type_i32, idx_reg_val, b.makeIntConstant(2)), b.makeIntConstant(add_off));
        finalize_offset = real_idx;

        idx_in_arr_1 = b.createBinOp(spv::OpSDiv, type_i32, real_idx, b.makeIntConstant(4));
        idx_in_arr_2 = b.createBinOp(spv::OpSDiv, type_i32, b.createBinOp(spv::OpIAdd, type_i32, real_idx, b.makeIntConstant(3)),
            b.makeIntConstant(4));
    }

    const int num_comp_in_single_float = static_cast<int>(4 / size_comp);

    // In here we calculate the highest/lowest offset of component that got written.
    // Starting from the nearest X component.
    int lowest_dest_write_offset = 999; ///< Lowest offset of the component to write.
    int highest_dest_write_offset = -1; ///< Highest offset of the component to write.

    for (int i = 0; i < 4; i++) {
        if (static_cast<int>(op.swizzle[i]) >= static_cast<int>(SwizzleChannel::C_0)) {
            continue;
        }

        const int swizzle_bit = static_cast<int>(op.swizzle[i]) - static_cast<int>(SwizzleChannel::C_X);

        if (dest_mask & (1 << i)) {
            lowest_dest_write_offset = std::min(lowest_dest_write_offset, swizzle_bit);
            highest_dest_write_offset = std::max(highest_dest_write_offset, swizzle_bit);
        }
    }

    if (lowest_dest_write_offset == 999 || highest_dest_write_offset == -1) {
        // This is the default value of the lowest swizzle channel.
        // Which means that no mask is on, all of the loadable ones, are constant swizzle channel.
        // Iterates through all of them and build a composite constant
        std::vector<spv::Id> comps;

        for (int i = 0; i < 4; i++) {
            if (dest_mask & (1 << i)) {
                comps.push_back(get_correspond_constant_with_channel(b, op.swizzle[i]));
            }
        }
        // Create a constant composite
        if (comps.size() == 1) {
            return apply_modifiers(b, utils, op.flags, comps[0]);
        }

        else {
            return apply_modifiers(b, utils, op.flags, b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(comps.size())), comps));
        }
    }

    // For non-F32 and non-I32 type, we need to make a destination mask to extract necessary F32 components out
    // For example: sa6.xz with DataType = f16
    // Would result at least sa6 and sa7 to be extracted out, since sa6 contains f16 x and y, sa7 contains f16 z and w
    Imm4 extract_mask = dest_mask;
    Swizzle4 extract_swizz = op.swizzle;

    if (size_comp != 4) {
        extract_mask = 0;

        for (int i = lowest_dest_write_offset / num_comp_in_single_float; i <= highest_dest_write_offset / num_comp_in_single_float; i++) {
            // Build up an extract mask
            extract_mask |= (1 << i);
        }

        // Set default swizzle
        // We only need to extract, the unpack will do the swizzling job later.
        extract_swizz = SWIZZLE_CHANNEL_4_DEFAULT;
    }

    spv::Id first_pass = spv::NoResult;
    spv::Id connected_friend = spv::NoResult;
    spv::Id bank_base = *get_reg_bank(params, op.bank);
    spv::Id comp_type = b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)));
    comp_type = b.makePointer(spv::StorageClassPrivate, comp_type);

    // May access two float in the arrays to get U8 or F16 components
    // Need to divide it with number of source components that each float can hold
    if (idx_in_arr_1 == spv::NoResult) {
        idx_in_arr_1 = b.makeIntConstant((op.num + shift_offset) >> 2);
    }

    if (idx_in_arr_2 == spv::NoResult) {
        idx_in_arr_2 = b.makeIntConstant((op.num + shift_offset + 3) >> 2);
    }

    std::vector<spv::Id> first_pass_operands;
    std::vector<spv::Id> second_pass_operands;

    first_pass_operands.push_back(bank_base);
    second_pass_operands.push_back(bank_base);

    first_pass_operands.push_back(idx_in_arr_1);
    second_pass_operands.push_back(idx_in_arr_2);

    // Do an access chain
    first_pass = b.createOp(spv::OpAccessChain, comp_type, first_pass_operands);
    connected_friend = b.createOp(spv::OpAccessChain, comp_type, second_pass_operands);

    first_pass = finalize(b, b.createLoad(first_pass, spv::NoPrecision), b.createLoad(connected_friend, spv::NoPrecision), extract_swizz,
        finalize_offset, extract_mask);

    if (first_pass == spv::NoResult) {
        return first_pass;
    }

    if (size_comp != 4) {
        // Second pass: Do unpack
        first_pass = unpack(b, utils, features, first_pass, op.type, op.swizzle, dest_mask, 0);
    } else if (op.type == DataType::INT32) {
        first_pass = b.createUnaryOp(spv::OpBitcast, make_vector_or_scalar_type(b, b.makeIntType(32), static_cast<int>(dest_comp_count)), first_pass);
    } else if (op.type == DataType::UINT32) {
        first_pass = b.createUnaryOp(spv::OpBitcast, make_vector_or_scalar_type(b, b.makeUintType(32), static_cast<int>(dest_comp_count)), first_pass);
    }

    if (first_pass == spv::NoResult) {
        return first_pass;
    }

    return apply_modifiers(b, utils, op.flags, first_pass);
}

spv::Id unpack(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset) {
    if (type == DataType::F32 || target == spv::NoResult) {
        return target;
    }

    spv::Id type_f32 = b.makeFloatType(32);
    std::vector<spv::Id> unpack_results;

    const spv::Id target_type = b.getTypeId(target);
    const std::uint32_t target_comp_count = b.getNumTypeComponents(target_type);

    unpack_results.resize(target_comp_count);

    for (std::size_t i = 0; i < unpack_results.size(); i++) {
        spv::Id extracted = target;

        if (!b.isScalar(target) && !b.isConstant(target)) {
            std::vector<spv::Id> extract_ops;
            extract_ops.push_back(target);
            extract_ops.push_back(b.makeIntConstant(static_cast<int>(i)));

            if (target_comp_count > 1) {
                extracted = b.createOp(spv::OpVectorExtractDynamic, type_f32, extract_ops);
            }
        }

        unpack_results[i] = unpack_one(b, utils, features, extracted, type);
    }

    return finalize(b, unpack_results[0], unpack_results.size() > 1 ? unpack_results[1] : unpack_results[0],
        swizz, b.makeIntConstant(offset), dest_mask);
}

void store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest,
    spv::Id source, std::uint8_t dest_mask, int off) {
    if (source == spv::NoResult) {
        LOG_WARN("Source invalid");
        return;
    }

    // Check for INDEX bank. INDEX bank are optimized to store an integer
    if (dest.bank == RegisterBank::INDEX || dest.bank == RegisterBank::PREDICATE) {
        spv::Id bank_base = *get_reg_bank(params, dest.bank);

        if (dest.bank == RegisterBank::INDEX) {
            dest.num -= 1;

            if (!b.isIntType(source)) {
                std::vector<spv::Id> ops{ source };
                source = b.createOp(spv::OpBitcast, b.makeIntType(32), ops);
            }

            // if dest is a 8 or 16 bits integer
            // Todo: keep the content of the upper bits of idx (not done right now)
            if (!is_float_data_type(dest.type) && get_data_type_size(dest.type) < 4) {
                spv::Id mask = (get_data_type_size(dest.type) == 1)
                    ? b.makeIntConstant(0xFF)
                    : b.makeIntConstant(0xFFFF);
                source = b.createBinOp(spv::OpBitwiseAnd, b.makeIntType(32), source, mask);
            }
        }

        spv::Id var = b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(dest.num) });

        b.createStore(source, var);
        return;
    }

    if (!get_reg_bank(params, dest.bank)) {
        return;
    }

    // If dest has default swizzle, is full-length (all dest component) and starts at a
    // register boundary, translate it to just a createStore
    auto total_comp_source = static_cast<std::uint8_t>(b.getNumComponents(source));

    spv::Id source_elm_type_id = b.getTypeId(source);
    if (b.isVectorType(source_elm_type_id)) {
        source_elm_type_id = b.getContainedTypeId(source_elm_type_id);
    }

    if (is_float_data_type(dest.type) && (b.isIntType(source_elm_type_id) || b.isUintType(source_elm_type_id))) {
        LOG_CRITICAL("Trying to store float to int storage");
        return;
    }

    if (!is_float_data_type(dest.type) && !b.isIntType(source_elm_type_id) && !b.isUintType(source_elm_type_id)) {
        LOG_CRITICAL("Trying to store int to float storage");
        return;
    }

    spv::Id type_f32 = b.makeFloatType(32);

    // FPINTERNAL bank can't pack, always store 32-bit. So bitcast
    if (((dest.bank == RegisterBank::FPINTERNAL) && is_integer_data_type(dest.type))
        || ((dest.bank != RegisterBank::FPINTERNAL) && ((dest.type == DataType::UINT32) || (dest.type == DataType::INT32)))) {
        std::vector<spv::Id> ops{ source };
        spv::Id bitcast_type = utils::make_vector_or_scalar_type(b, type_f32, total_comp_source);

        source = b.createOp(spv::OpBitcast, bitcast_type, ops);
        dest.type = DataType::F32;
    }

    if (dest.bank == RegisterBank::FPINTERNAL) {
        dest.type = DataType::F32;
        dest.num <<= 2;
        off <<= 2;
    }

    const std::size_t size_comp = get_data_type_size(dest.type);

    spv::Id bank_base = *get_reg_bank(params, dest.bank);

    // Element inside an array inside a pointer.
    spv::Id bank_base_elem_type = b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)));
    spv::Id comp_type = b.makePointer(spv::StorageClassPrivate, bank_base_elem_type);
    int insert_offset = dest.num + off;
    spv::Id elem = spv::NoResult;

    // Check the nearest avail swizzle
    int nearest_swizz_on = 0;

    for (int i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            nearest_swizz_on = i;
            break;
        }
    }

    // Floor down to nearest component that a float can hold. We originally want to optimize it to store from the first offset in float unit that writes the data.
    // But for unit size smaller than float, we have to start from the beginning in float unit.
    const int num_comp_in_float = static_cast<int>(4 / size_comp);
    nearest_swizz_on = nearest_swizz_on / num_comp_in_float * num_comp_in_float;

    if (dest.type != DataType::F32) {
        std::vector<spv::Id> composites;
        spv::Id vec_comp_type = utils::unwrap_type(b, b.getTypeId(source));
        int source_value_taken_count = 0;

        // We need to pack source
        for (auto i = 0; i < 4 - nearest_swizz_on; i += num_comp_in_float) {
            // Shuffle to get the type out
            std::vector<spv::Id> ops;
            for (auto j = 0; j < num_comp_in_float; j++) {
                if (dest_mask & (1 << (nearest_swizz_on + i + j))) {
                    if (b.isScalar(source) || total_comp_source == 1) {
                        ops.push_back(source);
                    } else {
                        ops.push_back(b.createOp(spv::OpVectorExtractDynamic, vec_comp_type, { source, b.makeIntConstant(std::min(source_value_taken_count++, (int)total_comp_source - 1)) }));
                    }
                } else {
                    if (elem == spv::NoResult) {
                        // Replace it
                        const int actual_offset_start_to_store = insert_offset + (i + nearest_swizz_on) / num_comp_in_float;
                        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(actual_offset_start_to_store >> 2) });
                        elem = b.createOp(spv::OpVectorExtractDynamic, b.makeFloatType(32), { b.createLoad(elem, spv::NoPrecision), b.makeIntConstant(actual_offset_start_to_store % 4) });

                        // Extract to f16
                        elem = unpack_one(b, utils, features, elem, dest.type);
                    }

                    ops.push_back(b.createOp(spv::OpVectorExtractDynamic, vec_comp_type, { elem, b.makeIntConstant(j) }));
                }
            }

            spv::Id result_type = utils::make_vector_or_scalar_type(b, vec_comp_type, (int)ops.size());
            spv::Id result = ops.size() == 1 ? ops[0] : b.createCompositeConstruct(result_type, ops);
            result = pack_one(b, utils, features, result, dest.type);

            composites.push_back(result);

            elem = spv::NoResult;
        }

        if (composites.size() == 1) {
            // A single package
            source = composites[0];
            total_comp_source = 1;
        } else {
            spv::Id final_composite_type = b.makeVectorType(type_f32, static_cast<int>(composites.size()));

            // Composite a new vector
            source = b.createCompositeConstruct(final_composite_type, composites);
            total_comp_source = static_cast<std::uint8_t>(composites.size());
        }

        // Replace it with a full mask, since we have already pre-process this into storable F32
        dest_mask = 0b1111;
    }

    // Now we do store!
    if (total_comp_source == 1) {
        insert_offset += (int)(nearest_swizz_on / (4 / size_comp));
        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(insert_offset >> 2) });
        spv::Id inserted = b.createOp(spv::OpVectorInsertDynamic, bank_base_elem_type, { b.createLoad(elem, spv::NoPrecision), source, b.makeIntConstant(insert_offset % 4) });

        b.createStore(inserted, elem);
        return;
    }

    if (total_comp_source == 4 && dest_mask == 0b1111 && insert_offset % 4 == 0) {
        // Store directly
        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(insert_offset >> 2) });
        b.createStore(source, elem);
        return;
    }

    std::vector<spv::IdImmediate> ops;

    // The provided source are stored in continuous order, however the dest mask may not be the same
    const int total_elem_to_copy_first_vec = std::min<int>(4 - (insert_offset % 4), total_comp_source);
    std::uint32_t dest_comp_stored_so_far = 0;

    elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset) >> 2) });

    ops.emplace_back(true, b.createLoad(elem, spv::NoPrecision));
    ops.emplace_back(true, source);

    for (auto i = 0; i < insert_offset % 4; i++) {
        ops.emplace_back(false, i);
    }

    for (auto i = insert_offset % 4; i < 4; i++) {
        if ((dest_comp_stored_so_far < total_comp_source) && (dest_mask & (1 << (i - (insert_offset % 4))))) {
            ops.emplace_back(false, 4 + (dest_comp_stored_so_far++));
        } else {
            ops.emplace_back(false, i);
        }
    }

    spv::Id shuffled = b.createOp(spv::OpVectorShuffle, b.makeVectorType(type_f32, 4), ops);
    b.createStore(shuffled, elem);

    // Check if there's leftover to be stored to next vec4 element.
    if ((insert_offset % 4) + total_comp_source > 4) {
        ops.clear();
        const int total_elem_left = ((insert_offset % 4) + total_comp_source) - 4;

        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset + 3) >> 2) });

        // Do an access chain
        ops.emplace_back(true, b.createLoad(elem, spv::NoPrecision));
        ops.emplace_back(true, source);

        // Start taking value that specified in the mask
        for (auto i = 0; i < total_elem_left; i++) {
            if ((dest_comp_stored_so_far < total_comp_source) && (dest_mask & (1 << (total_elem_to_copy_first_vec + i)))) {
                ops.emplace_back(false, 4 + (dest_comp_stored_so_far++));
            } else {
                ops.emplace_back(false, i);
            }
        }

        for (auto i = total_elem_left; i < 4; i++) {
            ops.emplace_back(false, i);
        }

        spv::Id shuffled = b.createOp(spv::OpVectorShuffle, bank_base_elem_type, ops);
        b.createStore(shuffled, elem);
    }
}

spv::Id make_vector_or_scalar_type(spv::Builder &b, spv::Id component, int size) {
    if (size == 1) {
        return component;
    }
    return b.makeVectorType(component, size);
}

spv::Id unwrap_type(spv::Builder &b, spv::Id type) {
    if (b.isVectorType(type)) {
        return b.getContainedTypeId(type);
    }
    return type;
}

static float get_int_normalize_range_constants(DataType type) {
    switch (type) {
    case DataType::UINT8:
        return 255.0f;
    case DataType::INT8:
        return 127.0f;
    case DataType::C10:
        // signed 10-bit, with a range of [-2, 2]
        return 255.0f;
    case DataType::UINT16:
        return 65535.0f;
    case DataType::INT16:
        return 32767.0f;
    case DataType::UINT32:
        return 4294967295.0f;
    case DataType::INT32:
        return 2147483647.0f;
    default:
        assert(false);
        return 0.0f;
    }
}

spv::Id create_constant_vector_or_scalar(spv::Builder &b, spv::Id constant, int comp_count) {
    if (comp_count == 1) {
        return constant;
    }
    std::vector<spv::Id> oprs(comp_count, constant);
    return b.createCompositeConstruct(b.makeVectorType(b.getTypeId(constant), comp_count), oprs);
}

spv::Id convert_to_float(spv::Builder &b, const SpirvUtilFunctions &utils, spv::Id opr, DataType type, bool normal) {
    const auto spv_type = unwrap_type(b, b.getTypeId(opr));
    const auto comp_count = b.isVector(opr) ? b.getNumComponents(opr) : 1;
    const auto target_type = b.isVector(opr) ? b.makeVectorType(b.makeFloatType(32), comp_count) : b.makeFloatType(32);
    assert(b.isIntType(spv_type) || b.isUintType(spv_type));

    const auto is_sint = b.isIntType(spv_type);

    if (is_sint) {
        opr = b.createUnaryOp(spv::OpConvertSToF, target_type, opr);
    } else {
        opr = b.createUnaryOp(spv::OpConvertUToF, target_type, opr);
    }

    if (normal) {
        const float normalizer = b.makeFloatConstant(get_int_normalize_range_constants(type));
        const spv::Id normalizer_vec = create_constant_vector_or_scalar(b, normalizer, comp_count);

        opr = b.createBinOp(spv::OpFDiv, target_type, opr, normalizer_vec);
        if (is_sint) {
            // opr = max(-1.0f, opr) (or -2.0f for fx10)
            float lower_bound = type == DataType::C10 ? -2.f : -1.f;
            const spv::Id minus1 = create_constant_vector_or_scalar(b, b.makeFloatConstant(lower_bound), comp_count);
            opr = b.createBuiltinCall(target_type, utils.std_builtins, GLSLstd450FMax, { opr, minus1 });
        }
    }
    return opr;
}

spv::Id convert_to_int(spv::Builder &b, const SpirvUtilFunctions &utils, spv::Id opr, DataType type, bool normal) {
    const auto opr_type = b.getTypeId(opr);
    assert(b.isFloatType(unwrap_type(b, b.getTypeId(opr))));

    const auto comp_count = b.isVector(opr) ? b.getNumComponents(opr) : 1;
    const auto is_uint = is_unsigned_integer_data_type(type);
    const auto target_comp_type = is_uint ? b.makeUintType(32) : b.makeIntType(32);
    const auto target_type = b.isVector(opr) ? b.makeVectorType(target_comp_type, comp_count) : target_comp_type;

    if (normal) {
        const float constant_range = get_int_normalize_range_constants(type);
        const spv::Id normalizer = b.makeFloatConstant(constant_range);
        const auto normalizer_vec = create_constant_vector_or_scalar(b, normalizer, comp_count);
        const bool is_fx10 = type == DataType::C10; // fx10 range is [-2,2]
        const auto range_begin_vec = create_constant_vector_or_scalar(b, b.makeFloatConstant(is_uint ? 0.f : (is_fx10 ? -2.f : -1.f)), comp_count);
        const auto range_end_vec = create_constant_vector_or_scalar(b, b.makeFloatConstant(is_fx10 ? 2.f : 1.f), comp_count);

        // opr = round(clamp(opr * norm), -1, 1)
        opr = b.createBuiltinCall(opr_type, utils.std_builtins, GLSLstd450FClamp, { opr, range_begin_vec, range_end_vec });
        opr = b.createBinOp(spv::OpFMul, opr_type, opr, normalizer_vec);
        opr = b.createBuiltinCall(opr_type, utils.std_builtins, GLSLstd450Round, { opr });
    }

    if (!is_uint) {
        opr = b.createUnaryOp(spv::OpConvertFToS, target_type, opr);
    } else {
        opr = b.createUnaryOp(spv::OpConvertFToU, target_type, opr);
    }

    return opr;
}

spv::Id add_uvec2_uint(spv::Builder &b, spv::Id vec, spv::Id to_add) {
    if (b.isConstant(to_add) && b.getConstantScalar(to_add) == 0)
        return vec;

    const spv::Id u32 = b.makeUintType(32);
    const spv::Id uvec2 = b.makeVectorType(u32, 2);
    const spv::Id add_result_type = b.makeStructResultType(u32, u32);

    if (!b.isUintType(b.getTypeId(to_add)))
        // convert i32 to u32
        to_add = b.createUnaryOp(spv::OpBitcast, u32, to_add);

    // add to_add to the lower part of vec then add the carry to the upper part of vec
    // something like this
    // uint carry;
    // vec.x = uaddCarry(vec.x, to_add, carry);
    // vec.y += carry;
    spv::Id lower = b.createCompositeExtract(vec, u32, 0);
    spv::Id lower_add = b.createBinOp(spv::OpIAddCarry, add_result_type, lower, to_add);
    spv::Id carry = b.createCompositeExtract(lower_add, u32, 1);
    spv::Id upper = b.createCompositeExtract(vec, u32, 1);
    upper = b.createBinOp(spv::OpIAdd, u32, upper, carry);
    lower = b.createCompositeExtract(lower_add, u32, 0);
    return b.createCompositeConstruct(uvec2, { lower, upper });
}

} // namespace shader::usse::utils
