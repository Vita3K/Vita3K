// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <shader/usse_constant_table.h>
#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <bitset>

using namespace shader;
using namespace usse;

void shader::usse::USSETranslatorVisitor::make_f16_unpack_func() {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_unpack_func_block;
    spv::Block *last_build_point = m_b.getBuildPoint();

    f16_unpack_func = m_b.makeFunctionEntry(spv::NoPrecision, type_f32_v[2], "f32_2_2xf16", { type_f32 }, decorations, &f16_unpack_func_block);
    spv::Id extracted = f16_unpack_func->getParamId(0);

    std::vector<spv::Id> cast_ops;
    cast_ops.push_back(extracted);

    extracted = m_b.createOp(spv::OpBitcast, type_ui32, cast_ops);
    extracted = m_b.createBuiltinCall(type_f32_v[2], std_builtins, GLSLstd450UnpackUnorm2x16, { extracted });

    m_b.makeReturn(false, extracted);
    m_b.setBuildPoint(last_build_point);
}

void shader::usse::USSETranslatorVisitor::make_f16_pack_func() {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *f16_pack_func_block;
    spv::Block *last_build_point = m_b.getBuildPoint();

    f16_pack_func = m_b.makeFunctionEntry(spv::NoPrecision, type_f32, "twoXf16_2_f32", { type_f32_v[2] }, decorations, &f16_pack_func_block);
    spv::Id extracted = f16_pack_func->getParamId(0);

    extracted = m_b.createBuiltinCall(type_ui32, std_builtins, GLSLstd450PackUnorm2x16, { extracted });

    std::vector<spv::Id> cast_ops;
    cast_ops.push_back(extracted);

    extracted = m_b.createOp(spv::OpBitcast, type_f32, cast_ops);

    m_b.makeReturn(false, extracted);
    m_b.setBuildPoint(last_build_point);
}

bool shader::usse::USSETranslatorVisitor::get_spirv_reg(usse::RegisterBank bank, std::uint32_t reg_offset, int shift_offset, SpirvReg &reg, std::uint32_t &out_comp_offset, bool get_for_store) {
    const std::uint32_t original_reg_offset = reg_offset;

    if (bank == usse::RegisterBank::FPINTERNAL) {
        // Each GPI register is 128 bits long = 16 bytes long
        // So we need to multiply the reg index with 16 in order to get the right offset
        reg_offset <<= 4;
    }

    const SpirvVarRegBank *spirv_bank = get_reg_bank(bank);

    if (!spirv_bank) {
        return false;
    }

    const auto writeable_idx = (reg_offset + shift_offset) >> 2;

    auto find_cached_reg = [&](RegisterCacheMap &cache, const std::size_t max = 256) -> int {
        if (reg_offset > max) {
            return -1;
        }

        RegisterCacheMap::iterator ite;

        if ((ite = cache.find(writeable_idx)) != cache.end()) {
            reg = ite->second;
            out_comp_offset = (reg_offset + shift_offset) % 4;
            return 1;
        }

        return 0;
    };

    auto create_supply_register = [&](SpirvReg &dest, const std::string &name, const bool do_copy) -> SpirvReg {
        const spv::Id new_writeable = m_b.createVariable(spv::StorageClassPrivate, type_f32_v[4], name.c_str());

        if (do_copy) {
            std::vector<SpirvReg> bridge_list;

            uint32_t temp_comp = 0;
            std::size_t collected_comp_count = 0;

            // Switch to main block. Do initialize there so things don't get complicated (like we already assign original register value
            // to a block, but another block haven't been initliazed yet).

            spv::Block *crr_bp = m_b.getBuildPoint();
            m_b.setBuildPoint(main_block);

            do {
                SpirvReg temp_reg;

                if (!spirv_bank->find_reg_at(static_cast<std::uint32_t>(writeable_idx * 4 + collected_comp_count), temp_reg, temp_comp)) {
                    temp_reg.size = 4;
                    temp_reg.type_id = type_f32_v[4];
                    temp_reg.var_id = const_f32_v0[4];
                }

                if (m_b.isArrayType(temp_reg.type_id)) {
                    const int size_per_element = temp_reg.size / m_b.getNumTypeComponents(temp_reg.type_id);

                    // Need to do a access chain to access the elements
                    temp_reg.var_id = m_b.createOp(spv::OpAccessChain, m_b.makePointer(spv::StorageClassPrivate, m_b.getContainedTypeId(temp_reg.type_id)),
                        { temp_reg.var_id, m_b.makeIntConstant(temp_comp / size_per_element) });
                    temp_reg.type_id = m_b.getContainedTypeId(temp_reg.type_id);
                    temp_reg.size = size_per_element;
                }

                bridge_list.push_back(temp_reg);
                collected_comp_count += temp_reg.size;
            } while (collected_comp_count < 4);

            SpirvReg bridge_result;
            bridge_result.type_id = type_f32_v[4];
            bridge_result.size = 4;

            if (bridge_list.size() == 1) {
                bridge_result.var_id = m_b.createLoad(bridge_list[0].var_id);
            } else {
                bridge_result.var_id = bridge(bridge_list[0], bridge_list[1], SWIZZLE_CHANNEL_4_DEFAULT, shift_offset, 0b1111);

                for (std::size_t i = 2; i < bridge_list.size(); i++) {
                    bridge_result.var_id = bridge(bridge_result, bridge_list[i], SWIZZLE_CHANNEL_4_DEFAULT, shift_offset, 0b1111);
                }
            }

            m_b.createStore(bridge_result.var_id, new_writeable);
            m_b.setBuildPoint(crr_bp);
        }

        dest.size = 4;
        dest.offset = writeable_idx << 2;
        dest.var_id = new_writeable;
        dest.type_id = type_f32_v[4];

        return dest;
    };

    switch (bank) {
    // If we are getting a PRIMATTR reg, we need to check whether a writeable one has already been created
    // If it is, get the writeable one, else proceed
    case usse::RegisterBank::PRIMATTR: {
        if (find_cached_reg(pa_writeable)) {
            return true;
        }

        break;
    }

    // Same for SECATTR
    case usse::RegisterBank::SECATTR: {
        const int result = find_cached_reg(sa_supplies, max_sa_registers);

        switch (result) {
        case 1: {
            return true;
        }

        case -1: {
            return false;
        }

        default: {
            break;
        }
        }

        break;
    }

    case usse::RegisterBank::TEMP: {
        if (find_cached_reg(r_supplies)) {
            return true;
        }

        break;
    }

    default: {
        break;
    }
    }

    auto create_new_reg_for_store = [&](RegisterCacheMap &cache, const char *prefix, const bool do_copy = true) {
        std::string new_name = prefix;
        new_name += std::to_string(writeable_idx << 2);

        reg = create_supply_register(reg, new_name, do_copy);
        cache[writeable_idx] = reg;

        out_comp_offset = (reg_offset + shift_offset) % 4;
    };

    bool result = spirv_bank->find_reg_at(reg_offset + shift_offset, reg, out_comp_offset);
    if (!result) {
        // Either if we get them for store or not, sometimes shader will still use SEC as temporary. Weird but ok.
        switch (bank) {
        case usse::RegisterBank::TEMP: {
            create_new_reg_for_store(r_supplies, "r", false);
            return true;
        }

        case usse::RegisterBank::SECATTR: {
            create_new_reg_for_store(sa_supplies, "sa", false);
            return true;
        }

        default: {
            break;
        }
        }

        return false;
    }

    if (m_b.isArrayType(reg.type_id)) {
        const int size_per_element = reg.size / m_b.getNumTypeComponents(reg.type_id);

        // Need to do a access chain to access the elements
        reg.var_id = m_b.createOp(spv::OpAccessChain, m_b.makePointer(spv::StorageClassPrivate, m_b.getContainedTypeId(reg.type_id)),
            { reg.var_id, m_b.makeIntConstant(out_comp_offset / size_per_element) });
        reg.type_id = m_b.getContainedTypeId(reg.type_id);

        out_comp_offset %= size_per_element;
    }

    if (get_for_store) {
        switch (bank) {
        case usse::RegisterBank::PRIMATTR: {
            create_new_reg_for_store(pa_writeable, "pa");
            break;
        }

        case usse::RegisterBank::SECATTR: {
            create_new_reg_for_store(sa_supplies, "sa");
            break;
        }

        default: {
            break;
        }
        }
    }

    return true;
}

spv::Id USSETranslatorVisitor::unpack_one(spv::Id scalar, const DataType type) {
    switch (type) {
    case DataType::F16: {
        return m_b.createFunctionCall(f16_unpack_func, { scalar });
    }

    default:
        break;
    }

    return spv::NoResult;
}

spv::Id USSETranslatorVisitor::pack_one(spv::Id vec, const DataType source_type) {
    switch (source_type) {
    case DataType::F16: {
        return m_b.createFunctionCall(f16_pack_func, { vec });
    }

    default:
        break;
    }

    return spv::NoResult;
}

spv::Id USSETranslatorVisitor::unpack(spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int shift_offset) {
    if (type == DataType::F32) {
        return target;
    }

    std::vector<spv::Id> unpack_results;

    const spv::Id target_type = m_b.getTypeId(target);
    const std::uint32_t target_comp_count = m_b.getNumTypeComponents(target_type);

    unpack_results.resize(target_comp_count);

    spv::Id unpacked_type = spv::NoResult;

    for (std::size_t i = 0; i < unpack_results.size(); i++) {
        spv::Id extracted = target;

        if (!m_b.isScalar(target) && !m_b.isConstant(target)) {
            std::vector<spv::Id> extract_ops;
            extract_ops.push_back(target);
            extract_ops.push_back(m_b.makeIntConstant(static_cast<int>(i)));

            if (target_comp_count > 1) {
                extracted = m_b.createOp(spv::OpVectorExtractDynamic, type_f32, extract_ops);
            }
        }

        unpack_results[i] = unpack_one(extracted, type);
        unpacked_type = m_b.getTypeId(unpack_results[i]);
    }

    SpirvReg reg_left{};
    SpirvReg reg_right{};

    reg_left.type_id = unpacked_type;
    reg_right.type_id = unpacked_type;

    reg_left.var_id = unpack_results[0];
    reg_right.var_id = unpack_results.size() == 1 ? unpack_results[0] : unpack_results[1];

    spv::Id last_shuffled = bridge(reg_left, reg_right, swizz, shift_offset, dest_mask);

    // It won't probably go up to two, but just in case ?
    for (std::size_t i = 1; i < unpack_results.size() - 1; i++) {
        reg_left.var_id = last_shuffled;
        reg_right.var_id = unpack_results[i];

        last_shuffled = bridge(reg_left, reg_right, swizz, shift_offset, dest_mask);
    }

    return last_shuffled;
}

spv::Id USSETranslatorVisitor::bridge(SpirvReg &src1, SpirvReg &src2, usse::Swizzle4 swiz, const int shift_offset, const Imm4 dest_mask) {
    // Queue keep track of components to be modified with given constant
    struct constant_queue_member {
        std::uint32_t index;
        spv::Id constant;
    };

    std::vector<constant_queue_member> constant_queue;

    std::vector<spv::Id> ops;
    ops.push_back(src1.var_id);
    ops.push_back(src2.var_id);

    const uint32_t src1_comp_count = m_b.getNumTypeComponents(src1.type_id);
    const uint32_t src2_comp_count = m_b.getNumTypeComponents(src2.type_id);
    const uint32_t total_comp_count = src1_comp_count + src2_comp_count;

    for (int i = 0; i < std::min(4, static_cast<int>(total_comp_count)); i++) {
        if (dest_mask & (1 << i)) {
            switch (swiz[i]) {
            case usse::SwizzleChannel::_X:
            case usse::SwizzleChannel::_Y:
            case usse::SwizzleChannel::_Z:
            case usse::SwizzleChannel::_W: {
                std::uint32_t swizz_off = (uint32_t)((int)swiz[i] + shift_offset);
                ops.push_back(std::min(swizz_off, total_comp_count - 1));

                break;
            }

            case usse::SwizzleChannel::_1:
            case usse::SwizzleChannel::_0:
            case usse::SwizzleChannel::_2:
            case usse::SwizzleChannel::_H: {
                ops.push_back((uint32_t)usse::SwizzleChannel::_X);
                constant_queue_member member;
                member.index = i;
                member.constant = const_f32[(uint32_t)swiz[i] - (uint32_t)usse::SwizzleChannel::_0];

                constant_queue.push_back(std::move(member));

                break;
            }

            default: {
                LOG_ERROR("Unsupport swizzle type");
                break;
            }
            }
        }
    }

    spv::Id shuff_type = spv::NoResult;

    // Get total swizzle actually use, by subtracting the ops by 2
    switch (ops.size() - 2) {
    case 2:
    case 3:
    case 4: {
        shuff_type = type_f32_v[ops.size() - 2];

        break;
    }
    case 1: {
        shuff_type = type_f32;
        break;
    }
    default:
        return spv::NoResult;
    }

    if (shuff_type == type_f32 && constant_queue.size() == 1) {
        return constant_queue[0].constant;
    }

    auto shuff_result = m_b.createOp(spv::OpVectorShuffle, shuff_type, ops);

    for (std::size_t i = 0; i < constant_queue.size(); i++) {
        const auto index = m_b.makeIntConstant(constant_queue[i].index);
        shuff_result = m_b.createOp(spv::OpVectorInsertDynamic, shuff_type,
            { shuff_result, constant_queue[i].constant, index });
    }

    return shuff_result;
}

spv::Id USSETranslatorVisitor::load(Operand &op, const Imm4 dest_mask, const int offset) {
    if (op.bank == RegisterBank::FPCONSTANT) {
        std::vector<spv::Id> consts;

        // Load constants. Ignore mask
        if (op.type == DataType::F32) {
            auto get_f32_from_bank = [&](const int num, const int bank) -> spv::Id {
                switch (bank) {
                case 0: {
                    return m_b.makeFloatConstant(*reinterpret_cast<const float *>(&usse::f32_constant_table_bank_0_raw[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]));
                }

                case 1: {
                    return m_b.makeFloatConstant(*reinterpret_cast<const float *>(&usse::f32_constant_table_bank_1_raw[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]));
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
                    return m_b.makeFloatConstant(
                        usse::f16_constant_table_bank0[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                case 1: {
                    return m_b.makeFloatConstant(
                        usse::f16_constant_table_bank1[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                case 2: {
                    return m_b.makeFloatConstant(
                        usse::f16_constant_table_bank2[op.num + static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::_X)]);
                }

                case 3: {
                    return m_b.makeFloatConstant(
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

        return m_b.makeCompositeConstant(type_f32_v[consts.size()], consts);
    }

    if (op.bank == RegisterBank::FPINTERNAL) {
        // Automatically F32
        op.type = DataType::F32;
    }

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);
    const std::size_t size_comp = get_data_type_size(op.type);

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
                comps.push_back(const_f32[(int)op.swizzle[i] - (int)SwizzleChannel::_0]);
            }
        }

        // Create a constant composite
        return m_b.makeCompositeConstant(type_f32_v[comps.size()], comps);
    }

    lowest_swizzle_bit = lowest_swizzle_bit * static_cast<int>(size_comp) / 4;
    highest_swizzle_bit = highest_swizzle_bit * static_cast<int>(size_comp) / 4;

    // TODO: Properly handle
    if (op.bank == RegisterBank::IMMEDIATE) {
        auto t = m_b.makeVectorType(type_f32, static_cast<int>(dest_comp_count_to_get));
        auto one = m_b.makeFpConstant(type_f32, 1.0);

        std::vector<spv::Id> ops;
        ops.resize(dest_comp_count_to_get);

        std::fill(ops.begin(), ops.end(), one);

        return m_b.makeCompositeConstant(t, ops);
    }

    // Composite a new vector
    SpirvReg reg_left{};
    std::uint32_t comp_offset{};

    // We need to get the left register, that will contains the lowest swizzle we needed.
    // For example: we need sa24.zw, while the register layouts like this:
    // sa24: float a;
    // sa25: float b;
    // sa26: float c;
    // sa27: float d;
    //
    // Four floats, not in the same vector. In order to build the foundation, to forms the expected result: vec2(c, d), we need
    // the base first, always be. So we need to first, get the register that contains our lowest swizzle channel Z. That is
    // the variable c.
    //
    // Swizzle like this: sa24.z1w1, constant swizzle channel will be ignored, so swizzle channel Z still be the lowest swizzle.
    // We will get the base by getting the register
    //
    // Another example: sa27.zwyz
    // Where
    // sa27: vec2 a;
    // sa28: vec3 b;
    //
    // The lowest swizzle channel is Y, so we will expects to get the variable a as the base first.
    if (!get_spirv_reg(op.bank, op.num, offset + lowest_swizzle_bit, reg_left, comp_offset, false)) {
        LOG_ERROR("Can't load register {}", disasm::operand_to_str(op, 0));
        return spv::NoResult;
    }

    std::uint32_t size_gotten = m_b.getNumTypeComponents(reg_left.type_id) - comp_offset;

    if ((dest_comp_count == 1) && size_comp == 4 && size_gotten == 1 && comp_offset == 0) {
        // We don't have to do any transformation.
        // Because:
        // The data type required is already 32-bit float, which is the data type we normally process.
        // There is only one channel, one float to get. No swizzling needed.
        return m_b.isConstant(reg_left.var_id) ? reg_left.var_id : m_b.createLoad(reg_left.var_id);
    }

    if (comp_offset == 0 && is_default(op.swizzle, static_cast<Imm4>(dest_comp_count)) && m_b.getNumTypeComponents(reg_left.type_id) == dest_comp_count_to_get) {
        spv::Id loaded = m_b.isConstant(reg_left.var_id) ? reg_left.var_id : m_b.createLoad(reg_left.var_id);

        if (size_comp == 4) {
            return loaded;
        } else {
            // Unpack
            return unpack(loaded, op.type, op.swizzle, dest_mask, 0);
        }
    }

    std::vector<SpirvReg> to_bridge;

    while (size_gotten < dest_comp_count_to_get) {
        SpirvReg another_one{};
        std::uint32_t temp_comp = 0;

        if (!get_spirv_reg(op.bank, op.num, offset + lowest_swizzle_bit + size_gotten, another_one, temp_comp, false)) {
            break;
        }

        to_bridge.push_back(another_one);
        size_gotten += another_one.size;
    }

    SpirvReg dummy;
    dummy.var_id = const_f32_v0[4];
    dummy.type_id = type_f32_v[4];
    dummy.size = 4;

    if (size_gotten < dest_comp_count_to_get) {
        LOG_ERROR("Can't load register {}, missing registers to bridge", disasm::operand_to_str(op, 0));
        return spv::NoResult;
    } else {
        // To bridge is empty. We need a place holder to shuffle the original up
        if (to_bridge.empty()) {
            to_bridge.push_back(dummy);
            size_gotten += 4;
        }
    }

    while (size_gotten < dest_comp_count) {
        // Add dummy vector. We need to at least make a complete vector with
        // dest_comp_count component
        //
        // Imagine this:
        // sa6.yxyx
        // with
        // sa6 = float a1
        // sa7 = float a2
        // sa8 = vec2 a3
        //
        // We already get the neccessary components: sa6 and sa7, these twos alone can form
        // the complete vector.
        //
        // But the bridge list is actually only have a2. Since we only get what we want, and bridge
        // forms a smaller or same size vector with the correct swizzle.
        //
        // Bridging these two will only result in a vec2(a1, a2)
        //
        // If we actually add a dummy vector (vec4) to bridge list, to fill the gap
        //
        // reg_left: a1, bridge_list: a2, dummy
        //
        // Then the bridge would result in a nice vec4
        //
        // First stage: a1, a2 => vec2(a2, a1)
        // Second stage: vec2(a2, a1), dummy => vec4(a2, a1, a2, a1)
        to_bridge.push_back(dummy);
        size_gotten += 4;
    }

    // For non-F32 type, we need to make a destination mask to extract neccessary components out
    // For example: sa6.xz with DataType = f16
    // Would result at least sa6 and sa7 to be extracted out, since sa6 contains f16 x and y, sa7 contains f16 z and w
    Imm4 extract_mask = dest_mask;
    Swizzle4 extract_swizz = op.swizzle;

    if (op.type != DataType::F32) {
        extract_mask = 0;

        for (int i = 0; i <= highest_swizzle_bit; i++) {
            // Build up an extract mask
            extract_mask |= (1 << i);
        }

        // Set default swizzle
        // We only need to extract, the unpack will do the swizzling job later.
        extract_swizz = SWIZZLE_CHANNEL_4_DEFAULT;
    }

    // We need to bridge
    spv::Id first_pass = spv::NoResult;

    // In return of the shift (that's lowest swizzle bit) that get us our favor register
    // Sometimes, things are quirky like this:
    // sa30.y where
    // sa27 = vec4(10, 20, 30, 40)
    // sa31 = vec4(50, 60, 70, 80)
    //
    // Of course, we need to shift 1 to get the register sa31, which is what we want. Unfortunately, the
    // comp offset unexpectedly owns 1 from the unexpected shift.
    // If we don't return the favor, the comp_offset will be 0, yet the swizzle still be Y, results in 60
    // By returning the 1 to the unexpected shift, we will get comp_offset = -1, swizzle is X, and results in 50

    first_pass = bridge(reg_left, to_bridge[0], extract_swizz, (int)comp_offset - lowest_swizzle_bit, extract_mask);

    SpirvReg first_pass_wrapper{};

    for (std::size_t i = 1; i < to_bridge.size(); i++) {
        first_pass_wrapper.type_id = m_b.getTypeId(first_pass);
        first_pass_wrapper.var_id = first_pass;

        first_pass = bridge(first_pass_wrapper, to_bridge[i], extract_swizz, (int)comp_offset - lowest_swizzle_bit, extract_mask);
    }

    if (size_comp != 4) {
        // Second pass: Do unpack
        // We already handle shift offset above, so now let's use 0
        first_pass = unpack(first_pass, op.type, op.swizzle, dest_mask, 0);
    }

    // Apply modifier flags
    if (op.flags & RegisterFlags::Negative) {
        // Negate the value
        first_pass = m_b.createBinOp(spv::OpFSub, type_f32_v[dest_comp_count], const_f32_v0[dest_comp_count], first_pass);
    }

    if (op.flags & RegisterFlags::Absolute) {
        // Absolute the result
        first_pass = m_b.createBuiltinCall(type_f32_v[dest_comp_count], std_builtins, GLSLstd450FAbs, { first_pass });
    }

    return first_pass;
}

void USSETranslatorVisitor::store(Operand &dest, spv::Id source, std::uint8_t dest_mask, int off) {
    // Composite a new vector
    SpirvReg dest_reg;
    std::uint32_t dest_comp_offset;

    if (source == spv::NoResult) {
        LOG_WARN("Source invalid");
        return;
    }

    if (!get_spirv_reg(dest.bank, dest.num, off, dest_reg, dest_comp_offset, true)) {
        LOG_ERROR("Can't find dest register {}", disasm::operand_to_str(dest, 0));
        return;
    }

    // If dest has default swizzle, is full-length (all dest component) and starts at a
    // register boundary, translate it to just a createStore
    const auto total_comp_source = static_cast<std::uint8_t>(m_b.getNumComponents(source));
    const auto total_comp_dest = static_cast<std::uint8_t>(m_b.getNumTypeComponents(dest_reg.type_id));

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);
    const std::size_t size_comp = get_data_type_size(dest.type);

    // Default load will get word as default component
    std::size_t comp_count_to_store = 0;
    bool already[4] = { false, false, false, false };

    for (int i = 0; i < 4; i++) {
        if (dest_mask & (1 << i) && (already[i * size_comp / 4] == false)) {
            comp_count_to_store++;
            already[i * size_comp / 4] = true;
        }
    }

    const bool needs_swizzle = !is_default(dest.swizzle, static_cast<Imm4>(dest_comp_count));

    // The source needs to fit in both the dest vector and the total write components must be equal to total source components
    const bool full_length = (total_comp_dest == total_comp_source) && (comp_count_to_store == total_comp_source);
    const bool starts_at_0 = (dest_comp_offset == 0);

    if (!needs_swizzle && full_length && starts_at_0) {
        m_b.createStore(source, dest_reg.var_id);
        return;
    }

    spv::Id result = spv::NoResult;

    // Else do the shifting/swizzling with OpVectorShuffle, if data type is f32
    if (dest.type == DataType::F32 || dest.bank == RegisterBank::FPINTERNAL) {
        if (total_comp_source == 1 && dest_comp_count == 1) {
            // Use OpInsertDynamic
            for (int i = 0; i < total_comp_dest; i++) {
                if (dest_mask & (1 << i)) {
                    result = m_b.createOp(spv::OpVectorInsertDynamic, dest_reg.type_id, { m_b.createLoad(dest_reg.var_id), source, m_b.makeIntConstant(i + dest_comp_offset % 4) });

                    break;
                }
            }
        } else {
            std::vector<spv::Id> ops;

            ops.push_back(source);
            ops.push_back(dest_reg.var_id);

            // Total comp = 2, limit mask scan to only x, y
            // Total comp = 3, limit mask scan to only x, y, z
            // So on..
            for (std::uint8_t i = 0; i < total_comp_dest; i++) {
                if (dest_mask & (1 << (dest_comp_offset + i) % 4)) {
                    ops.push_back((i + dest_comp_offset % 4) % 4);
                } else {
                    // Use original
                    ops.push_back(total_comp_source + i);
                }
            }

            result = m_b.createOp(spv::OpVectorShuffle, dest_reg.type_id, ops);
        }
    } else {
        // Extract each of the component
        // We could unpack all of them and do shuffle, but we want to avoid it as we can
        // Since bit operations are just costly
        std::vector<spv::Id> vars;
        std::unordered_map<std::uint32_t, spv::Id> cached_packed;
        int total_mask_on_so_far = 0;

        for (std::uint8_t i = 0; i < 4; i++) {
            std::uint32_t swizz_off = (dest_comp_offset + i) % 4;

            if (!(dest_mask & (1 << swizz_off))) {
                swizz_off = i;

                std::uint32_t extract_offset = (swizz_off % size_comp) % 4;
                swizz_off = swizz_off / static_cast<std::uint32_t>(size_comp);

                // Unpack the destination value
                if (cached_packed[swizz_off] == spv::NoResult) {
                    cached_packed[swizz_off] = dest_reg.var_id;

                    if (!m_b.isScalarType(dest_reg.type_id) && !m_b.isConstant(dest_reg.var_id)) {
                        cached_packed[swizz_off] = m_b.createOp(spv::OpVectorExtractDynamic, type_f32,
                            { dest_reg.var_id, m_b.makeIntConstant(swizz_off) });
                    }

                    cached_packed[swizz_off] = unpack_one(cached_packed[swizz_off], dest.type);
                }

                vars.push_back(m_b.createOp(spv::OpVectorExtractDynamic, type_f32,
                    { cached_packed[swizz_off], m_b.makeIntConstant(extract_offset) }));
            } else {
                if (m_b.isScalar(source) || m_b.isConstant(source) || m_b.getNumComponents(source) == 1) {
                    vars.push_back(source);
                } else {
                    vars.push_back(m_b.createOp(spv::OpVectorExtractDynamic, type_f32,
                        { source, m_b.makeIntConstant(total_mask_on_so_far++) }));
                }
            }
        }

        // Pack variables
        const int total_comp_per_pack = 4 / static_cast<int>(size_comp);
        std::vector<spv::Id> composite_ops;

        for (int i = 0; i < vars.size() - 1; i += total_comp_per_pack) {
            std::vector<spv::Id> ops;

            for (int j = 0; j < total_comp_per_pack; j++) {
                ops.push_back(vars[i + j]);
            }

            vars[i] = m_b.createCompositeConstruct(type_f32_v[total_comp_per_pack], ops);

            switch (dest.type) {
            case DataType::F16: {
                vars[i] = m_b.createFunctionCall(f16_pack_func, { vars[i] });
                break;
            }

            default: {
                LOG_ERROR("Can't handle packing unknown type!");
                break;
            }
            }

            composite_ops.push_back(vars[i]);
        }

        result = m_b.createCompositeConstruct(type_f32_v[composite_ops.size()], composite_ops);

        std::vector<spv::Id> shuffle_ops;
        shuffle_ops.push_back(result);
        shuffle_ops.push_back(dest_reg.var_id);

        for (int i = 0; i < composite_ops.size(); i++) {
            shuffle_ops.push_back(i);
        }

        // Shuffle so that we would get the best
        for (int i = static_cast<int>(composite_ops.size()); i < total_comp_dest; i++) {
            shuffle_ops.push_back(static_cast<int>(composite_ops.size() + i));
        }

        result = m_b.createOp(spv::OpVectorShuffle, dest_reg.type_id, shuffle_ops);
    }

    m_b.createStore(result, dest_reg.var_id);
}

const SpirvVarRegBank *USSETranslatorVisitor::get_reg_bank(RegisterBank reg_bank) const {
    switch (reg_bank) {
    case RegisterBank::PRIMATTR:
        return &m_spirv_params.ins;
    case RegisterBank::SECATTR:
        return &m_spirv_params.uniforms;
    case RegisterBank::OUTPUT:
        return &m_spirv_params.outs;
    case RegisterBank::TEMP:
        return &m_spirv_params.temps;
    case RegisterBank::FPINTERNAL:
        return &m_spirv_params.internals;
    }

    LOG_WARN("Reg bank {} unsupported", static_cast<uint8_t>(reg_bank));
    return nullptr;
}

spv::Id USSETranslatorVisitor::swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle) {
    switch (swizzle) {
    case SwizzleChannel::_X:
    case SwizzleChannel::_Y:
    case SwizzleChannel::_Z:
    case SwizzleChannel::_W:
        return m_b.createCompositeExtract(composite, type, static_cast<Imm4>(swizzle));

    // TODO: Implement these with OpCompositeExtract
    case SwizzleChannel::_0: break;
    case SwizzleChannel::_1: break;
    case SwizzleChannel::_2: break;

    case SwizzleChannel::_H: break;
    }

    LOG_WARN("Swizzle channel {} unsupported", static_cast<Imm4>(swizzle));
    return spv::NoResult;
}

size_t USSETranslatorVisitor::dest_mask_to_comp_count(Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    assert(bit_count <= 4 && bit_count > 0);
    return bit_count;
}

void USSETranslatorVisitor::emit_non_native_frag_output() {
    Operand pa0_operand;
    pa0_operand.bank = RegisterBank::PRIMATTR;
    pa0_operand.num = 0;
    pa0_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    pa0_operand.type = DataType::F16;

    Operand o0_operand;
    o0_operand.bank = RegisterBank::OUTPUT;
    o0_operand.num = 0;
    o0_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    spv::Id pa0_var = load(pa0_operand, 0xF, 0);
    store(o0_operand, pa0_var, 0xF, 0);
}

bool USSETranslatorVisitor::set_predicate(const Imm2 idx, const spv::Id value) {
    if (idx >= 4) {
        return false;
    }

    if (predicates[idx] == spv::NoResult) {
        const auto pred_name = fmt::format("p{}", idx);
        predicates[idx] = m_b.createVariable(spv::StorageClass::StorageClassPrivate, m_b.makeBoolType(), pred_name.c_str());
    }

    m_b.createStore(value, predicates[idx]);
    return true;
}

spv::Id USSETranslatorVisitor::load_predicate(const Imm2 idx, const bool neg) {
    if (idx >= 4) {
        return spv::NoResult;
    }

    if (predicates[idx] == spv::NoResult) {
        const auto pred_name = fmt::format("p{}", idx);
        predicates[idx] = m_b.createVariable(spv::StorageClass::StorageClassPrivate, m_b.makeBoolType(), pred_name.c_str());
    }

    spv::Id result = m_b.createLoad(predicates[idx]);

    if (neg) {
        std::vector<spv::Id> ops;
        ops.push_back(result);

        result = m_b.createOp(spv::OpLogicalNot, m_b.makeBoolType(), ops);
    }

    return result;
}
