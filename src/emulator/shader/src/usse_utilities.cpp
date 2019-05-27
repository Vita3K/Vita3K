#include <shader/usse_utilities.h>
#include <shader/usse_constant_table.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>

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
    std::vector<spv::Id> ops;

    // Create a f32 pointer type first
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id f32_pointer = b.makePointer(spv::StorageClassPrivate, type_f32);

    const auto first_comp_count = b.getNumComponents(first) ;

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

    LOG_WARN("Reg bank {} unsupported", static_cast<uint8_t>(reg_bank));
    return nullptr;
}

static spv::Function *make_f16_unpack_func(spv::Builder &b) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_unpack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    spv::Function *f16_unpack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32_v2, "f32_2_2xf16", { type_f32 },
        decorations, &f16_unpack_func_block);
    
    spv::Id extracted = f16_unpack_func->getParamId(0);

    std::vector<spv::Id> cast_ops;
    cast_ops.push_back(extracted);

    extracted = b.createOp(spv::OpBitcast, type_ui32, cast_ops);
    extracted = b.createBuiltinCall(type_f32_v2, b.import("GLSL.std.450"), GLSLstd450UnpackUnorm2x16, { extracted });

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_unpack_func;
}

static spv::Function *make_f16_pack_func(spv::Builder &b) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_pack_func_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Id type_ui32 = b.makeUintType(32);
    spv::Id type_f32 = b.makeFloatType(32);
    spv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    spv::Function *f16_pack_func = b.makeFunctionEntry(spv::NoPrecision, type_f32, "twoXf16_2_f32", { type_f32_v2 }, 
        decorations, &f16_pack_func_block);
    
    spv::Id extracted = f16_pack_func->getParamId(0);

    extracted = b.createBuiltinCall(type_ui32, b.import("GLSL.std.450"), GLSLstd450PackUnorm2x16, { extracted });

    std::vector<spv::Id> cast_ops;
    cast_ops.push_back(extracted);

    extracted = b.createOp(spv::OpBitcast, type_f32, cast_ops);

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_pack_func;
}

spv::Id shader::usse::utils::unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, spv::Id scalar, const DataType type) {
    switch (type) {
    case DataType::F16: {
        if (!utils.unpack_f16) {
            utils.unpack_f16 = make_f16_unpack_func(b);
        }

        return b.createFunctionCall(utils.unpack_f16, { scalar });
    }

    default:
        break;
    }

    return spv::NoResult;
}

spv::Id shader::usse::utils::pack_one(spv::Builder &b, SpirvUtilFunctions &utils, spv::Id vec, const DataType source_type) {
    switch (source_type) {
    case DataType::F16: {
        if (!utils.pack_f16) {
            utils.pack_f16 = make_f16_pack_func(b);
        }

        return b.createFunctionCall(utils.pack_f16, { vec });
    }

    default:
        break;
    }

    return spv::NoResult;
}

spv::Id shader::usse::utils::load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, Operand &op, const Imm4 dest_mask, const int shift_offset) {
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

        return b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(consts.size())), consts);
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
        return b.createLoad(b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, 
            b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(op.num) } ));
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
        return b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(comps.size())), comps);
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
    
    if (op.bank == RegisterBank::IMMEDIATE || !get_reg_bank(params, op.bank)) {
        if (dest_comp_count == 1) {
            if ((int)op.swizzle[0] >= (int)SwizzleChannel::_0) {
                return get_correspond_constant_with_channel(b, op.swizzle[0]);
            }
        }

        const auto comp_count = static_cast<int>(dest_comp_count);
        auto t = b.makeVectorType(type_f32, comp_count);
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

        return finalize(b, pass, pass, op.swizzle, shift_offset, dest_mask);
    }

    spv::Id first_pass = spv::NoResult;
    spv::Id connected_friend = spv::NoResult;
    spv::Id bank_base = *get_reg_bank(params, op.bank);
    spv::Id comp_type = b.makePointer(spv::StorageClassPrivate, b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base))));
    
    // Do an access chain
    first_pass = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((op.num + shift_offset) >> 2) });
    connected_friend = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((op.num + shift_offset + 3) >> 2) });

    first_pass = finalize(b, b.createLoad(first_pass), b.createLoad(connected_friend), extract_swizz, op.num + shift_offset, extract_mask);

    if (size_comp != 4) {
        // Second pass: Do unpack
        // We already handle shift offset above, so now let's use 0
        first_pass = unpack(b, utils, first_pass, op.type, op.swizzle, dest_mask, 0);
    }

    const bool is_unsigned_integer_dtype = is_unsigned_integer_data_type(op.type);
    const bool is_signed_integer_dtype = is_signed_integer_data_type(op.type);

    const bool is_integral = is_unsigned_integer_dtype || is_signed_integer_dtype;

    std::vector<spv::Id> ops;
    ops.push_back(first_pass);

    // Bitcast them to integer. Those flags assuming bits stores on those float registers are actually integer
    if (is_signed_integer_dtype) {
        first_pass = b.createOp(spv::OpBitcast, b.makeVectorType(b.makeIntType(32), static_cast<int>(dest_comp_count)), ops);
    } else if (is_unsigned_integer_dtype) {
        first_pass = b.createOp(spv::OpBitcast, b.makeVectorType(b.makeUintType(32), static_cast<int>(dest_comp_count)), ops);
    }

    spv::Id dest_type = b.makeVectorType(type_f32, static_cast<int>(dest_comp_count));

    // Apply modifier flags
    if (op.flags & RegisterFlags::Negative) {
        // Negate the value
        if (is_integral) {
            LOG_ERROR("Support on negative modifier is unavailable for integer!");
        } else {
            spv::Id v0 = spv::NoResult;
            std::vector<spv::Id> ops(dest_comp_count, b.makeFloatConstant(0.0f));

            first_pass = b.createBinOp(spv::OpFSub, dest_type, b.makeCompositeConstant(dest_type, ops), first_pass);
        }
    }

    if (op.flags & RegisterFlags::Absolute) {
        // Absolute the result
        if (is_integral) {
            LOG_ERROR("Support on abs modifier is unavailable for integer!");
        } else {
            first_pass = b.createBuiltinCall(dest_type, b.import("GLSL.std.450"), GLSLstd450FAbs, { first_pass });
        }
    }

    return first_pass;
}

spv::Id shader::usse::utils::unpack(spv::Builder &b, SpirvUtilFunctions &utils, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset) {
    if (type == DataType::F32) {
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

        unpack_results[i] = unpack_one(b, utils, extracted, type);
        unpacked_type = b.getTypeId(unpack_results[i]);
    }

    return finalize(b, unpack_results[0], unpack_results.size() > 1 ? unpack_results[1] : unpack_results[0],
        swizz, offset, dest_mask);
}

void shader::usse::utils::store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, Operand &dest,
    spv::Id source, std::uint8_t dest_mask, int off) {
    if (source == spv::NoResult) {
        LOG_WARN("Source invalid");
        return;
    }

    // Check for INDEX bank. INDEX bank are optimized to store an integer
    if (dest.bank == RegisterBank::INDEX || dest.bank == RegisterBank::PREDICATE) {
        spv::Id bank_base = *get_reg_bank(params, dest.bank);
        spv::Id var = b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, 
            b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(dest.num) });

        if (dest.bank == RegisterBank::INDEX) {
            if (!b.isIntType(source)) {
                std::vector<spv::Id> ops {source};
                source = b.createOp(spv::OpBitcast, b.makeIntType(32), ops);
            }
        }

        b.createStore(source, var);
        return;
    }

    if (!get_reg_bank(params, dest.bank)) {
        return;
    }

    // If dest has default swizzle, is full-length (all dest component) and starts at a
    // register boundary, translate it to just a createStore
    auto total_comp_source = static_cast<std::uint8_t>(b.getNumComponents(source));
    
    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);
    const std::size_t size_comp = get_data_type_size(dest.type);

    // If the source to store, is not in float form, bitcast it to float
    spv::Id source_elm_type_id = b.getTypeId(source);
    if (b.isVectorType(source_elm_type_id)) {
        source_elm_type_id = b.getContainedTypeId(source_elm_type_id);
    }

    spv::Id type_f32 = b.makeFloatType(32);

    if (b.isIntType(source_elm_type_id) || b.isUintType(source_elm_type_id)) {
        std::vector<spv::Id> ops {source};
        spv::Id bitcast_type = b.makeVectorType(type_f32, total_comp_source);

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

    case DataType::F32:
    case DataType::F16:
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

    spv::Id result = spv::NoResult;
    if (dest.type != DataType::F32) {
        std::vector<spv::Id> composites;

        // We need to pack source
        for (auto i = 0; i < static_cast<int>(total_comp_source); i += static_cast<int>(4 / size_comp)) {
            // Shuffle to get the type out
            std::vector<spv::Id> ops { source, source };
            for (auto j = 0; j < 4 / size_comp; j++) {
                ops.push_back(i + j);
            }

            spv::Id shuffled_type = b.makeVectorType(type_f32, static_cast<int>(ops.size() - 2));

            spv::Id shuffled = b.createOp(spv::OpVectorShuffle, shuffled_type, ops);
            shuffled = pack_one(b, utils, shuffled, dest.type);

            composites.push_back(shuffled);
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

    spv::Id bank_base = *get_reg_bank(params, dest.bank);

    // Element inside an array inside a pointer.
    spv::Id bank_base_elem_type = b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)));
    spv::Id comp_type = b.makePointer(spv::StorageClassPrivate, bank_base_elem_type);
    int insert_offset = dest.num + off;
    spv::Id elem = spv::NoResult;
    
    // Now we do store!
    if (total_comp_source == 1) {
        // We can do a single insert dynamic. We first need to get the position where we should insert
        for (auto i = 0; i < 4; i++) {
            if (dest_mask & (1 << i)) {
                insert_offset += i;
                break;
            }
        }

        elem = b.createOp(spv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(insert_offset >> 2) });
        spv::Id inserted = b.createOp(spv::OpVectorInsertDynamic, bank_base_elem_type, { b.createLoad(elem), 
            source, b.makeIntConstant(insert_offset % 4) });

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