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

#include <shader/usse_constant_table.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_utilities.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <features/state.h>

#include <bitset>

using namespace shader::usse;

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

spv::Id shader::usse::utils::finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, const int offset, const Imm4 dest_mask) {
    if (first == spv::NoResult || second == spv::NoResult) {
        return spv::NoResult;
    }

    std::vector<spv::Id> ops;

    spv::Id target_type = utils::unwrap_type(b, b.getTypeId(first));

    const auto first_comp_count = b.getNumComponents(first);

    if ((offset % 4 == 0) && is_default(swizz, 4) && dest_mask == 0b1111 && b.getNumComponents(first) == 4) {
        return first;
    }

    // Try to plant a composite construct
    for (auto i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            if ((int)swizz[i] >= (int)SwizzleChannel::C_0) {
                ops.push_back(get_correspond_constant_with_channel(b, swizz[i]));
            } else {
                int access_offset = offset % 4 + (int)swizz[i] - (int)SwizzleChannel::C_X;
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
            }
        }
    }

    if (ops.size() == 1) {
        return ops[0];
    }

    return b.createCompositeConstruct(b.makeVectorType(target_type, static_cast<int>(ops.size())), ops);
}

static size_t dest_mask_to_comp_count(shader::usse::Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    assert(bit_count <= 4 && bit_count > 0);
    return bit_count;
}

static const shader::usse::SpirvVarRegBank *get_reg_bank(const shader::usse::SpirvShaderParameters &params, shader::usse::RegisterBank reg_bank) {
    switch (reg_bank) {
    case shader::usse::RegisterBank::PRIMATTR:
        return &params.ins;
    case shader::usse::RegisterBank::SECATTR:
        return &params.uniforms;
    case shader::usse::RegisterBank::OUTPUT:
        return &params.outs;
    case shader::usse::RegisterBank::TEMP:
        return &params.temps;
    case shader::usse::RegisterBank::FPINTERNAL:
        return &params.internals;
    case shader::usse::RegisterBank::PREDICATE:
        return &params.predicates;
    case shader::usse::RegisterBank::INDEX:
        return &params.indexes;
    }

    // LOG_WARN("Reg bank {} unsupported", static_cast<uint8_t>(reg_bank));
    return nullptr;
}

// TODO: Not sure if this is right. Based on this article
// https://www.johndcook.com/blog/2018/04/15/eight-bit-floating-point/
// > Eight-bit IEEE-like float: The largest value would be 01011111 and have value: 4(1 – 2-5) = 31/8 = 3.3875.
static constexpr float MAX_FX8 = 3.3875f;

static spv::Function *make_fx8_unpack_func(spv::Builder &b, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *fx8_unpack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v4 = b.makeVectorType(type_f32, 4);
    spv::Id max_fx8_c = b.makeFloatConstant(MAX_FX8);

    spv::Function *fx8_unpack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32_v4, "unpack4xF8", { type_f32 },
        decorations, &fx8_unpack_func_block);

    spv::Id extracted = fx8_unpack_func->getParamId(0);

    // Cast to uint first
    extracted = b.createUnaryOp(spv::OpBitcast, type_ui32, extracted);
    extracted = b.createBuiltinCall(type_f32_v4, b.import("GLSL.std.450"), GLSLstd450UnpackSnorm4x8, { extracted });

    // Multiply them with max fx8
    extracted = b.createBinOp(spv::OpFMul, type_f32_v4, extracted, b.makeCompositeConstant(type_f32_v4, { max_fx8_c, max_fx8_c, max_fx8_c, max_fx8_c }));

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return fx8_unpack_func;
}

static spv::Function *make_fx8_pack_func(spv::Builder &b, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *fx8_pack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v4 = b.makeVectorType(type_f32, 4);
    spv::Id max_fx8_c = b.makeFloatConstant(MAX_FX8);

    spv::Function *fx8_pack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, "pack4xF8", { type_f32_v4 },
        decorations, &fx8_pack_func_block);

    spv::Id extracted = fx8_pack_func->getParamId(0);

    extracted = b.createBinOp(spv::OpFDiv, type_f32_v4, extracted, b.makeCompositeConstant(type_f32_v4, { max_fx8_c, max_fx8_c, max_fx8_c, max_fx8_c }));
    extracted = b.createBuiltinCall(type_ui32, b.import("GLSL.std.450"), GLSLstd450PackSnorm4x8, { extracted });
    extracted = b.createUnaryOp(spv::OpBitcast, type_f32, extracted);
    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return fx8_pack_func;
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
    }

    spv::Function *unpack_func = b.makeFunctionEntry(spv::NoPrecision, output_type, func_name.c_str(), { type_f32 },
        decorations, &unpack_func_block);
    spv::Id extracted = unpack_func->getParamId(0);

    extracted = b.createUnaryOp(spv::OpBitcast, is_signed ? type_i32 : type_ui32, extracted);

    std::vector<spv::Id> comps;
    std::vector<spv::Id> bias_comps;

    const auto comp_bits = 32 / comp_count;

    for (int i = 0; i < comp_count; ++i) {
        spv::Id comp;

        if (is_signed) {
            comp = b.createTriOp(spv::OpBitFieldSExtract, type_i32, extracted, b.makeIntConstant(comp_bits * i), b.makeIntConstant(comp_bits));
        } else {
            comp = b.createTriOp(spv::OpBitFieldUExtract, type_ui32, extracted, b.makeIntConstant(comp_bits * i), b.makeIntConstant(comp_bits));
        }

        comps.push_back(comp);
    }

    auto output = b.createCompositeConstruct(output_type, comps);

    if (is_signed) {
        // Sign extended them. Thanks kd-11 for method.
        spv::Id sign_check_vec_type = b.makeVectorType(b.makeBoolType(), comp_count);
        std::vector<std::uint32_t> constants(comp_count, b.makeIntConstant(1 << (comp_bits - 1)));
        std::vector<std::uint32_t> constant_bias(comp_count, b.makeIntConstant(1 << comp_bits));

        spv::Id sign_check_vec = b.createBinOp(spv::OpSLessThan, sign_check_vec_type, output,
            b.makeCompositeConstant(output_type, constants));
        spv::Id bias_vec = b.makeCompositeConstant(output_type, constant_bias);

        output = b.createTriOp(spv::OpSelect, output_type, sign_check_vec, output, b.createBinOp(spv::OpISub, output_type, output, bias_vec));
    }

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
    }

    spv::Function *pack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, func_name.c_str(), { input_type },
        decorations, &pack_func_block);

    spv::Id extracted = pack_func->getParamId(0);
    const int comp_bits = 32 / comp_count;

    auto output = b.makeUintConstant(0);
    for (int i = 0; i < comp_count; ++i) {
        auto comp = b.createBinOp(spv::OpVectorExtractDynamic, type_ui32, extracted, b.makeIntConstant(i));
        output = b.createOp(spv::OpBitFieldInsert, type_ui32, { output, comp, b.makeIntConstant(comp_bits * i), b.makeIntConstant(comp_bits - (is_signed ? 1 : 0)) });

        if (is_signed) {
            auto sign_bit = b.createBinOp(spv::OpShiftRightLogical, type_ui32, comp, b.makeIntConstant(31));
            output = b.createOp(spv::OpBitFieldInsert, type_ui32, { output, sign_bit, b.makeIntConstant(comp_bits * i + comp_bits - 1), b.makeIntConstant(1) });
        }
    }

    output = b.createUnaryOp(spv::OpBitcast, type_f32, output);

    b.makeReturn(false, output);
    b.setBuildPoint(last_build_point);

    return pack_func;
}

static spv::Function *make_f16_unpack_func(spv::Builder &b, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_unpack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    spv::Function *f16_unpack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32_v2, "unpack2xF16", { type_f32 },
        decorations, &f16_unpack_func_block);

    spv::Id extracted = f16_unpack_func->getParamId(0);

    extracted = b.createUnaryOp(spv::OpBitcast, type_ui32, extracted);
    extracted = b.createBuiltinCall(type_f32_v2, b.import("GLSL.std.450"), GLSLstd450UnpackHalf2x16, { extracted });

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_unpack_func;
}

static spv::Function *make_f16_pack_func(spv::Builder &b, const FeatureState &features) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_pack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    spv::Function *f16_pack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, "pack2xF16", { type_f32_v2 },
        decorations, &f16_pack_func_block);

    spv::Id extracted = f16_pack_func->getParamId(0);

    // use packHalf2x16
    extracted = b.createBuiltinCall(type_ui32, b.import("GLSL.std.450"), GLSLstd450PackHalf2x16, { extracted });
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

    spv::Function *fetch_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, func_name.c_str(), { type_i32 },
        {}, &func_block);

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

    spv::Id src = b.createLoad(b.createAccessChain(spv::StorageClassStorageBuffer, buffer_container, { b.makeIntConstant(info.index_in_container), base_vector, base_offset }));

    spv::Id friend_offset = b.createBinOp(spv::OpIAdd, type_i32, base_offset, one_cst);
    spv::Id friend_vector = b.createBinOp(spv::OpIAdd, type_i32, base_vector, b.createBinOp(spv::OpSDiv, type_i32, friend_offset, b.makeIntConstant(4)));

    friend_offset = b.createBinOp(spv::OpSRem, type_i32, friend_offset, four_cst);

    spv::Id src_friend = b.createLoad(b.createAccessChain(spv::StorageClassStorageBuffer, buffer_container, { b.makeIntConstant(info.index_in_container), friend_vector, friend_offset }));
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
    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_i32 = b.makeIntType(32);
    spv::Id type_bool = b.makeBoolType();

    spv::Block *func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Function *fetch_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, "fetchMemory", { type_i32 },
        {}, &func_block);
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

spv::Id shader::usse::utils::fetch_memory(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, spv::Id addr) {
    if (!utils.fetch_memory) {
        utils.fetch_memory = make_fetch_memory_func(b, params);
    }

    return b.createFunctionCall(utils.fetch_memory, { addr });
}

spv::Id shader::usse::utils::unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type) {
    switch (type) {
    case DataType::INT8:
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::INT16: {
        if (utils.unpack_funcs.find(type) == utils.unpack_funcs.end()) {
            utils.unpack_funcs[type] = make_unpack_func(b, features, type);
        }
        return b.createFunctionCall(utils.unpack_funcs.at(type), { scalar });
    }
    case DataType::F16: {
        if (utils.unpack_funcs.find(type) == utils.unpack_funcs.end()) {
            utils.unpack_funcs[type] = make_f16_unpack_func(b, features);
        }
        return b.createFunctionCall(utils.unpack_funcs.at(type), { scalar });
    }
    // TODO: Not really FX8?
    case DataType::C10: {
        if (!utils.unpack_fx8) {
            utils.unpack_fx8 = make_fx8_unpack_func(b, features);
        }

        return b.createFunctionCall(utils.unpack_fx8, { scalar });
    }
    default: {
        LOG_ERROR("Unsupported unpack type: {}", log_hex(type));
        break;
    }
    }

    return spv::NoResult;
}

spv::Id shader::usse::utils::pack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id vec, const DataType source_type) {
    switch (source_type) {
    case DataType::INT8:
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::INT16: {
        if (utils.pack_funcs.find(source_type) == utils.pack_funcs.end()) {
            utils.pack_funcs[source_type] = make_pack_func(b, features, source_type);
        }
        return b.createFunctionCall(utils.pack_funcs.at(source_type), { vec });
    }
    case DataType::F16: {
        if (utils.pack_funcs.find(source_type) == utils.pack_funcs.end()) {
            utils.pack_funcs[source_type] = make_f16_pack_func(b, features);
        }
        return b.createFunctionCall(utils.pack_funcs.at(source_type), { vec });
    }
    // TODO: Not really FX8?
    case DataType::C10: {
        if (!utils.pack_fx8) {
            utils.pack_fx8 = make_fx8_pack_func(b, features);
        }

        return b.createFunctionCall(utils.pack_fx8, { vec });
    }

    default: {
        LOG_ERROR("Unsupported pack type: {}", log_hex(source_type));
        break;
    }
    }

    return spv::NoResult;
}

static spv::Id apply_modifiers(spv::Builder &b, const shader::usse::RegisterFlags flags, spv::Id val) {
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

        result = b.createBuiltinCall(dest_type, b.import("GLSL.std.450"), GLSLstd450FAbs, { result });
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

spv::Id shader::usse::utils::load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset) {
    spv::Id type_f32 = b.makeFloatType(32);

    if (op.bank == RegisterBank::FPCONSTANT) {
        const bool integral = (op.type == DataType::UINT32) || (op.type == DataType::UINT16);
        std::vector<spv::Id> consts;

        auto handle_unexpect_swizzle = [&](const SwizzleChannel ch, const bool integral) {
#define GEN_CONSTANT(cnst)                                           \
    if (integral)                                                    \
        return b.makeUintConstant(static_cast<std::uint32_t>(cnst)); \
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

        // Load constants. Ignore mask
        if ((op.type == DataType::F32) || (op.type == DataType::UINT32)) {
            auto get_f32_from_bank = [&](const int num) -> spv::Id {
                int swizz_val = static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::C_X);
                std::uint32_t value = 0;

                switch (swizz_val >> 1) {
                case 0: {
                    value = usse::f32_constant_table_bank_0_raw[op.num + (swizz_val & 1)];
                    break;
                }

                case 1: {
                    value = usse::f32_constant_table_bank_1_raw[op.num + (swizz_val & 1)];
                    break;
                }

                default:
                    return handle_unexpect_swizzle(op.swizzle[num], integral);
                }

                if (integral)
                    return b.makeUintConstant(value);

                return b.makeFloatConstant(*reinterpret_cast<const float *>(&value));
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    consts.push_back(get_f32_from_bank(i));
                }
            }
        } else if ((op.type == DataType::F16) || (op.type == DataType::UINT16)) {
            auto get_f16_from_bank = [&](const int num) -> spv::Id {
                const int swizz_val = static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::C_X);
                float value = 0;

                switch (swizz_val & 3) {
                case 0: {
                    value = usse::f16_constant_table_bank0[op.num];
                    break;
                }

                case 1: {
                    value = usse::f16_constant_table_bank1[op.num];
                    break;
                }

                case 2: {
                    value = usse::f16_constant_table_bank2[op.num];
                    break;
                }

                case 3: {
                    value = usse::f16_constant_table_bank3[op.num];
                    break;
                }

                default:
                    return handle_unexpect_swizzle(op.swizzle[num], integral);
                }

                if (integral) {
                    return b.makeUintConstant(*reinterpret_cast<const std::uint32_t *>(&value));
                }

                return b.makeFloatConstant(value);
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    consts.push_back(get_f16_from_bank(i));
                }
            }
        }

        if (consts.size() == 1) {
            return apply_modifiers(b, op.flags, consts[0]);
        } else {
            spv::Id result = b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(consts.size())), consts);
            return apply_modifiers(b, op.flags, result);
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
        return b.createLoad(b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(op.num) }));
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
                return constant;
            }

            std::vector<spv::Id> ops(dest_comp_count, constant);
            spv::Id pass = b.makeCompositeConstant(b.makeVectorType(b.getTypeId(constant), static_cast<int>(dest_comp_count)), ops);

            pass = finalize(b, pass, pass, op.swizzle, shift_offset, dest_mask);
            return apply_modifiers(b, op.flags, pass);
        }
    }

    spv::Id idx_in_arr_1 = spv::NoResult;
    spv::Id idx_in_arr_2 = spv::NoResult;

    if (op.bank == RegisterBank::INDEXED1 || op.bank == RegisterBank::INDEXED2) {
        // Decode the info. Usually the number of bits in a INDEXED number is 7.
        // TODO: Fix the assumption
        const Imm2 bank_enc = (op.num >> 5) & 0b11;
        const Imm5 add_off = op.num & 0b11111;

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
        spv::Id idx_reg_val = b.createLoad(b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, type_i32), { params.indexes, b.makeIntConstant(idx_off) }));

        spv::Id real_idx = b.createBinOp(spv::OpIAdd, type_i32, b.createBinOp(spv::OpIMul, type_i32, idx_reg_val, b.makeIntConstant(2)), b.makeIntConstant(add_off));

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
            return apply_modifiers(b, op.flags, comps[0]);
        }

        else {
            return apply_modifiers(b, op.flags, b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(comps.size())), comps));
        }
    }

    // For non-F32 and non-I32 type, we need to make a destination mask to extract neccessary F32 components out
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

    first_pass = finalize(b, b.createLoad(first_pass), b.createLoad(connected_friend), extract_swizz,
        op.num + shift_offset, extract_mask);

    if (first_pass == spv::NoResult) {
        return first_pass;
    }

    if (size_comp != 4) {
        // Second pass: Do unpack
        first_pass = unpack(b, utils, features, first_pass, op.type, op.swizzle, dest_mask, 0);
    } else if (op.type == DataType::INT32) {
        first_pass = b.createUnaryOp(spv::OpBitcast, b.makeVectorType(b.makeIntType(32), static_cast<int>(dest_comp_count)), first_pass);
    } else if (op.type == DataType::UINT32) {
        first_pass = b.createUnaryOp(spv::OpBitcast, b.makeVectorType(b.makeUintType(32), static_cast<int>(dest_comp_count)), first_pass);
    }

    if (first_pass == spv::NoResult) {
        return first_pass;
    }

    return apply_modifiers(b, op.flags, first_pass);
}

spv::Id shader::usse::utils::unpack(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset) {
    if (type == DataType::F32 || target == spv::NoResult) {
        return target;
    }

    spv::Id type_f32 = b.makeFloatType(32);
    std::vector<spv::Id> unpack_results;

    const spv::Id target_type = b.getTypeId(target);
    const std::uint32_t target_comp_count = b.getNumTypeComponents(target_type);

    unpack_results.resize(target_comp_count);

    spv::Id unpacked_type = spv::NoResult;

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
        unpacked_type = b.getTypeId(unpack_results[i]);
    }

    return finalize(b, unpack_results[0], unpack_results.size() > 1 ? unpack_results[1] : unpack_results[0],
        swizz, offset, dest_mask);
}

void shader::usse::utils::store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest,
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

    const std::size_t size_comp = get_data_type_size(dest.type);

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
    nearest_swizz_on = (int)((nearest_swizz_on / num_comp_in_float) * num_comp_in_float);

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
                        elem = b.createOp(spv::OpVectorExtractDynamic, vec_comp_type, { b.createLoad(elem), b.makeIntConstant(actual_offset_start_to_store % 4) });

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
        spv::Id inserted = b.createOp(spv::OpVectorInsertDynamic, bank_base_elem_type, { b.createLoad(elem), source, b.makeIntConstant(insert_offset % 4) });

        b.createStore(inserted, elem);
        return;
    }

    if (total_comp_source == 4 && dest_mask == 0b1111 && insert_offset % 4 == 0) {
        // Store directly
        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(insert_offset >> 2) });
        b.createStore(source, elem);
        return;
    }

    std::vector<spv::Id> ops;

    // The provided source are stored in continous order, however the dest mask may not be the same
    const int total_elem_to_copy_first_vec = std::min<int>(4 - (insert_offset % 4), total_comp_source);
    std::uint32_t dest_comp_stored_so_far = 0;

    elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset) >> 2) });

    ops.push_back(b.createLoad(elem));
    ops.push_back(source);

    for (auto i = 0; i < insert_offset % 4; i++) {
        ops.push_back(i);
    }

    for (auto i = insert_offset % 4; i < 4; i++) {
        if ((dest_comp_stored_so_far < total_comp_source) && (dest_mask & (1 << (i - (insert_offset % 4))))) {
            ops.push_back(4 + (dest_comp_stored_so_far++));
        } else {
            ops.push_back(i);
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
        ops.push_back(b.createLoad(elem));
        ops.push_back(source);

        // Start taking value that specified in the mask
        for (auto i = 0; i < total_elem_left; i++) {
            if ((dest_comp_stored_so_far < total_comp_source) && (dest_mask & (1 << (total_elem_to_copy_first_vec + i)))) {
                ops.push_back(4 + (dest_comp_stored_so_far++));
            } else {
                ops.push_back(i);
            }
        }

        for (auto i = total_elem_left; i < 4; i++) {
            ops.push_back(i);
        }

        spv::Id shuffled = b.createOp(spv::OpVectorShuffle, bank_base_elem_type, ops);
        b.createStore(shuffled, elem);
    }
}

spv::Id shader::usse::utils::make_vector_or_scalar_type(spv::Builder &b, spv::Id component, int size) {
    if (size == 1) {
        return component;
    }
    return b.makeVectorType(component, size);
}

spv::Id shader::usse::utils::unwrap_type(spv::Builder &b, spv::Id type) {
    if (b.isVectorType(type)) {
        return b.getContainedTypeId(type);
    }
    return type;
}

// will break in 32-bit host
static float get_int_normalize_constants(DataType type) {
    switch (type) {
    case DataType::UINT8:
    case DataType::INT8:
        return 255.0f;
    case DataType::UINT16:
    case DataType::INT16:
        return 65535.0f;
    case DataType::UINT32:
    case DataType::INT32:
        return 4294967295.0f;
    default:
        assert(false);
    }
}

static spv::Id create_constant_vector_or_scalar(spv::Builder &b, spv::Id constant, int comp_count) {
    if (comp_count == 1) {
        return constant;
    }
    std::vector<spv::Id> oprs;
    for (int i = 0; i < comp_count; ++i) {
        oprs.push_back(constant);
    }
    return b.createCompositeConstruct(b.makeVectorType(b.getTypeId(constant), comp_count), oprs);
}

spv::Id shader::usse::utils::convert_to_float(spv::Builder &b, spv::Id opr, DataType type, bool normal) {
    auto spv_type = unwrap_type(b, b.getTypeId(opr));
    int comp_count = b.isVector(opr) ? 1 : b.getNumComponents(opr);
    auto target_type = b.isVector(opr) ? b.makeVectorType(b.makeFloatType(32), b.getNumComponents(opr)) : b.makeFloatType(32);
    assert(b.isIntType(spv_type) || b.isUintType(spv_type));

    if (b.isIntType(spv_type)) {
        opr = b.createUnaryOp(spv::OpConvertSToF, target_type, opr);
    } else {
        opr = b.createUnaryOp(spv::OpConvertUToF, target_type, opr);
    }

    if (normal) {
        const auto constant = get_int_normalize_constants(type);
        const auto normalizer = b.makeFloatConstant(constant);
        const auto normalizer_vec = create_constant_vector_or_scalar(b, normalizer, comp_count);

        opr = b.createBinOp(spv::OpFDiv, target_type, opr, normalizer_vec);
    }
    return opr;
}

spv::Id shader::usse::utils::convert_to_int(spv::Builder &b, spv::Id opr, DataType type, bool normal) {
    auto opr_type = b.getTypeId(opr);
    auto spv_type = unwrap_type(b, b.getTypeId(opr));
    assert(b.isFloatType(spv_type));

    int comp_count = b.isVector(opr) ? 1 : b.getNumComponents(opr);
    bool is_uint = is_unsigned_integer_data_type(type);
    auto target_comp_type = is_uint ? b.makeUintType(32) : b.makeIntType(32);
    auto target_type = b.isVector(opr) ? b.makeVectorType(target_comp_type, b.getNumComponents(opr)) : target_comp_type;

    if (normal) {
        const auto constant = get_int_normalize_constants(type);
        const auto normalizer = b.makeFloatConstant(constant);
        const auto normalizer_vec = create_constant_vector_or_scalar(b, normalizer, comp_count);
        const auto zero_vec = create_constant_vector_or_scalar(b, b.makeFloatConstant(0.0), comp_count);
        const auto one_vec = create_constant_vector_or_scalar(b, b.makeFloatConstant(1.0), comp_count);

        opr = b.createBuiltinCall(opr_type, b.import("GLSL.std.450"), GLSLstd450FClamp, { opr, zero_vec, one_vec });
        opr = b.createBinOp(spv::OpFMul, opr_type, opr, normalizer_vec);
    }

    if (!is_uint) {
        opr = b.createUnaryOp(spv::OpConvertFToS, target_type, opr);
    } else {
        opr = b.createUnaryOp(spv::OpConvertFToU, target_type, opr);
    }

    return opr;
}
