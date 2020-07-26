#include <shader/usse_constant_table.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_utilities.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <features/state.h>

#include <bitset>

static spv::Id get_correspond_constant_with_channel(spv::Builder &b, shader::usse::SwizzleChannel swizz) {
    switch (swizz) {
    case shader::usse::SwizzleChannel::_0: {
        return b.makeFloatConstant(0.0f);
    }

    case shader::usse::SwizzleChannel::_1: {
        return b.makeFloatConstant(1.0f);
    }

    case shader::usse::SwizzleChannel::_2: {
        return b.makeFloatConstant(2.0f);
    }

    case shader::usse::SwizzleChannel::_H: {
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

    // Create a f32 pointer type first
    spv::Id type_f32 = b.makeFloatType(32);

    const auto first_comp_count = b.getNumComponents(first);

    if ((offset % 4 == 0) && is_default(swizz, 4) && dest_mask == 0b1111 && b.getNumComponents(first) == 4) {
        return first;
    }

    // Try to plant a composite construct
    for (auto i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            if ((int)swizz[i] >= (int)SwizzleChannel::_0) {
                ops.push_back(get_correspond_constant_with_channel(b, swizz[i]));
            } else {
                int access_offset = offset % 4 + (int)swizz[i] - (int)SwizzleChannel::_X;
                spv::Id access_base = first;

                if (access_offset >= first_comp_count) {
                    access_offset -= first_comp_count;
                    access_base = second;
                }

                if (!b.isScalar(access_base)) {
                    ops.push_back(b.createOp(spv::OpVectorExtractDynamic, type_f32, { access_base, b.makeIntConstant(access_offset) }));
                } else {
                    ops.push_back(access_base);
                }
            }
        }
    }

    if (ops.size() == 1) {
        return ops[0];
    }

    return b.createCompositeConstruct(b.makeVectorType(type_f32, static_cast<int>(ops.size())), ops);
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

spv::Id shader::usse::utils::unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type) {
    // TODO: doing this for uint8 is probably not right
    switch (type) {
    case DataType::F16: {
        if (!utils.unpack_f16) {
            utils.unpack_f16 = make_f16_unpack_func(b, features);
        }

        return b.createFunctionCall(utils.unpack_f16, { scalar });
    }
    case DataType::UINT8:
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
    case DataType::F16: {
        if (!utils.pack_f16) {
            utils.pack_f16 = make_f16_pack_func(b, features);
        }

        return b.createFunctionCall(utils.pack_f16, { vec });
    }
    case DataType::UINT8:
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

    if (flags & shader::usse::RegisterFlags::Absolute) {
        // Absolute the result
        if (is_uint) {
            // It's already > 0, what do you expect more
            return result;
        }

        result = b.createBuiltinCall(dest_type, b.import("GLSL.std.450"), GLSLstd450FAbs, { result });
    }

    return result;
}

spv::Id shader::usse::utils::load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset) {
    spv::Id type_f32 = b.makeFloatType(32);

    if (op.bank == RegisterBank::FPCONSTANT) {
        std::vector<spv::Id> consts;

        // Load constants. Ignore mask
        if (op.type == DataType::F32) {
            auto get_f32_from_bank = [&](const int num, const int bank) -> spv::Id {
                switch (bank) {
                case 0: {
                    return b.makeFloatConstant(*reinterpret_cast<const float *>(&usse::f32_constant_table_bank_0_raw[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]));
                }

                case 1: {
                    return b.makeFloatConstant(*reinterpret_cast<const float *>(&usse::f32_constant_table_bank_1_raw[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]));
                }

                default:
                    break;
                }

                return spv::NoResult;
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    if (i == 1 && op.index == 0) {
                        consts.push_back(get_f32_from_bank(i, 1));
                    } else {
                        consts.push_back(get_f32_from_bank(i, 0));
                    }
                }
            }
        } else if (op.type == DataType::F16) {
            auto get_f16_from_bank = [&](const int num, const int bank) -> spv::Id {
                switch (bank) {
                case 0: {
                    return b.makeFloatConstant(
                        usse::f16_constant_table_bank0[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                case 1: {
                    return b.makeFloatConstant(
                        usse::f16_constant_table_bank1[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                case 2: {
                    return b.makeFloatConstant(
                        usse::f16_constant_table_bank2[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                case 3: {
                    return b.makeFloatConstant(
                        usse::f16_constant_table_bank3[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                default:
                    break;
                }

                return spv::NoResult;
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    if (i >= 1 && op.index == 0) {
                        consts.push_back(get_f16_from_bank(i, i));
                    } else {
                        consts.push_back(get_f16_from_bank(i, 0));
                    }
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
        }

        default:
            break;
        }

        op.num <<= 2;
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
                if ((int)op.swizzle[0] >= (int)SwizzleChannel::_0) {
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

    // Default load will get word as default component
    std::size_t dest_comp_count_to_get = 0;
    bool already[4] = { false, false, false, false };
    int lowest_swizzle_bit = 999;
    int highest_swizzle_bit = -1;

    for (int i = 0; i < 4; i++) {
        const int swizzle_bit = (int)op.swizzle[i] - (int)SwizzleChannel::_X;

        if (dest_mask & (1 << i) && swizzle_bit <= 3 && (already[swizzle_bit * size_comp / 4] == false)) {
            dest_comp_count_to_get++;
            already[swizzle_bit * size_comp / 4] = true;

            lowest_swizzle_bit = std::min(lowest_swizzle_bit, swizzle_bit);
            highest_swizzle_bit = std::max(highest_swizzle_bit, swizzle_bit);
        }
    }

    if (lowest_swizzle_bit == 999 || highest_swizzle_bit == -1) {
        // This is the default value of the lowest swizzle channel, which means that all of the loadable ones
        // are constant swizzle channel. Iterates through all of them and build a composite constant
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

    lowest_swizzle_bit = lowest_swizzle_bit * static_cast<int>(size_comp) / 4;
    highest_swizzle_bit = highest_swizzle_bit * static_cast<int>(size_comp) / 4;

    // For non-F32 and non-I32 type, we need to make a destination mask to extract neccessary components out
    // For example: sa6.xz with DataType = f16
    // Would result at least sa6 and sa7 to be extracted out, since sa6 contains f16 x and y, sa7 contains f16 z and w
    Imm4 extract_mask = dest_mask;
    Swizzle4 extract_swizz = op.swizzle;

    if (size_comp != 4) {
        extract_mask = 0;

        for (int i = 0; i <= highest_swizzle_bit; i++) {
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

    first_pass = finalize(b, b.createLoad(first_pass), b.createLoad(connected_friend), extract_swizz, op.num + shift_offset, extract_mask);

    if (first_pass == spv::NoResult) {
        return first_pass;
    }

    if (size_comp != 4) {
        // Second pass: Do unpack
        // We already handle shift offset above, so now let's use 0
        first_pass = unpack(b, utils, features, first_pass, op.type, op.swizzle, dest_mask, 0);
    }

    if (first_pass == spv::NoResult) {
        return first_pass;
    }

    const bool is_unsigned_integer_dtype = is_unsigned_integer_data_type(op.type);
    const bool is_signed_integer_dtype = is_signed_integer_data_type(op.type);

    // Bitcast them to integer. Those flags assuming bits stores on those float registers are actually integer
    if (is_signed_integer_dtype) {
        first_pass = b.createUnaryOp(spv::OpBitcast, utils::make_vector_or_scalar_type(b, b.makeIntType(32), static_cast<int>(dest_comp_count)), first_pass);
    } else if (is_unsigned_integer_dtype) {
        first_pass = b.createUnaryOp(spv::OpBitcast, utils::make_vector_or_scalar_type(b, b.makeUintType(32), static_cast<int>(dest_comp_count)), first_pass);
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

    // If the source to store, is not in float form, bitcast it to float
    spv::Id source_elm_type_id = b.getTypeId(source);
    if (b.isVectorType(source_elm_type_id)) {
        source_elm_type_id = b.getContainedTypeId(source_elm_type_id);
    }

    spv::Id type_f32 = b.makeFloatType(32);

    if (b.isIntType(source_elm_type_id) || b.isUintType(source_elm_type_id)) {
        std::vector<spv::Id> ops{ source };
        spv::Id bitcast_type = utils::make_vector_or_scalar_type(b, type_f32, total_comp_source);

        source = b.createOp(spv::OpBitcast, bitcast_type, ops);
    }

    // Since we have bitcasted non-float type to float, we might as well do type transformation with dest store type
    switch (dest.type) {
    case DataType::INT16:
    case DataType::UINT16: {
        dest.type = DataType::F16;
        break;
    }

    case DataType::INT32:
    case DataType::UINT32: {
        dest.type = DataType::F32;
        break;
    }

    case DataType::INT8:
    case DataType::UINT8: {
        dest.type = DataType::C10;
        break;
    }

    case DataType::F32:
    case DataType::F16:
    case DataType::C10:
        break;

    default: {
        LOG_ERROR("Unsupported source type {}", static_cast<int>(dest.type));
        return;
    }
    }

    if (dest.bank == RegisterBank::FPINTERNAL) {
        dest.type = DataType::F32;
        dest.num <<= 2;
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

    // Floor down to nearest size comp
    nearest_swizz_on = (int)((nearest_swizz_on) / (4 / size_comp) * (4 / size_comp));

    if (dest.type != DataType::F32) {
        std::vector<spv::Id> composites;

        // We need to pack source
        for (auto i = 0; i < static_cast<int>(total_comp_source); i += static_cast<int>(4 / size_comp)) {
            // Shuffle to get the type out
            std::vector<spv::Id> ops;
            for (auto j = 0; j < 4 / size_comp; j++) {
                if (dest_mask & (1 << (nearest_swizz_on + i + j))) {
                    if (b.isScalar(source) || total_comp_source == 1) {
                        ops.push_back(source);
                    } else {
                        ops.push_back(b.createOp(spv::OpVectorExtractDynamic, b.makeFloatType(32), { source, b.makeIntConstant(std::min(i + j, (int)total_comp_source - 1)) }));
                    }
                } else {
                    if (elem == spv::NoResult) {
                        // Replace it
                        const int actual_offset_start_to_store = (int)(insert_offset + (i + nearest_swizz_on) / size_comp);
                        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(actual_offset_start_to_store >> 2) });
                        elem = b.createOp(spv::OpVectorExtractDynamic, b.makeFloatType(32), { b.createLoad(elem), b.makeIntConstant(actual_offset_start_to_store % 4) });

                        // Extract to f16
                        elem = unpack_one(b, utils, features, elem, dest.type);
                    }

                    ops.push_back(b.createOp(spv::OpVectorExtractDynamic, b.makeFloatType(32), { elem, b.makeIntConstant(j) }));
                }
            }

            spv::Id result_type = utils::make_vector_or_scalar_type(b, type_f32, (int)ops.size());
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

    // Store with traditional toture method
    if ((insert_offset % 4) + total_comp_source > 4) {
        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset + 3) >> 2) });
        const auto total_comp_left_to_copy = total_comp_source - (4 - (insert_offset % 4));
        const auto copied = 4 - insert_offset % 4;

        // Do an access chain
        ops.push_back(b.createLoad(elem));
        ops.push_back(source);

        for (auto i = 0; i < total_comp_left_to_copy; i++) {
            ops.push_back(4 + copied + i);
        }

        for (auto i = total_comp_left_to_copy; i < 4; i++) {
            ops.push_back(i);
        }

        spv::Id shuffled = b.createOp(spv::OpVectorShuffle, bank_base_elem_type, ops);
        b.createStore(shuffled, elem);
    }

    // Store the rest
    ops.clear();
    elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset) >> 2) });

    ops.push_back(b.createLoad(elem));
    ops.push_back(source);

    const auto start_offset_from_vec4 = insert_offset % 4;

    for (auto i = 0; i < start_offset_from_vec4; i++) {
        ops.push_back(i);
    }

    for (auto i = 0; i < std::min((int)(4 - start_offset_from_vec4), (int)total_comp_source); i++) {
        ops.push_back(4 + i);
    }

    for (auto i = std::min(start_offset_from_vec4 + total_comp_source, 4); i < 4; i++) {
        ops.push_back(i);
    }

    spv::Id shuffled = b.createOp(spv::OpVectorShuffle, b.makeVectorType(type_f32, 4), ops);
    b.createStore(shuffled, elem);
}

spv::Id shader::usse::utils::make_uniform_vector_from_type(spv::Builder &b, spv::Id type, int val) {
    const int num_comp = b.getNumTypeComponents(type);
    spv::Id v_elem_type = (num_comp > 1) ? b.getContainedTypeId(type) : type;

    spv::Id cnst = spv::NoResult;

    if (b.isUintType(v_elem_type)) {
        cnst = b.makeUintConstant(val);
    } else if (b.isIntType(v_elem_type)) {
        cnst = b.makeIntConstant(val);
    } else {
        cnst = b.makeFloatConstant(static_cast<float>(val));
    }

    if (num_comp == 1) {
        return cnst;
    }

    std::vector<spv::Id> c_vecs(num_comp, cnst);
    spv::Id v0 = b.makeCompositeConstant(type, c_vecs);

    return v0;
}

spv::Id shader::usse::utils::make_vector_or_scalar_type(spv::Builder &b, spv::Id component, int size) {
    if (size == 1) {
        return component;
    }
    return b.makeVectorType(component, size);
}
