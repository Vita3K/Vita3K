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

#include <shader/usse_translator.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>
#include <boost/optional.hpp>
#include <spdlog/fmt/fmt.h>
#include <spirv.hpp>

#include <bitset>
#include <iostream>
#include <tuple>

// TODO: Remove
#include "disassemble.h"

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

void shader::usse::USSETranslatorVisitor::do_texture_queries(const NonDependentTextureQueryCallInfos &texture_queries) {
    for (auto &texture_query : texture_queries) {
        const bool query_f16 = texture_query.store_type == static_cast<int>(DataType::F16);

        auto image_sample = m_b.createOp(spv::OpImageSampleImplicitLod,
            query_f16 ? type_f32_v[4] : m_b.getTypeId(texture_query.dest), // destination result type
            { texture_query.sampler, texture_query.coord } // sampler and texture coordinates
        );

        if (query_f16) {
            // Pack them
            spv::Id pack1 = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { image_sample, image_sample, 0, 1 });
            pack1 = pack_one(pack1, DataType::F16);

            spv::Id pack2 = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { image_sample, image_sample, 2, 3 });
            pack2 = pack_one(pack2, DataType::F16);

            image_sample = m_b.createCompositeConstruct(type_f32_v[2], { pack1, pack2 });
        }

        m_b.createStore(image_sample, texture_query.dest);
    }
}

bool shader::usse::USSETranslatorVisitor::get_spirv_reg(usse::RegisterBank bank, std::uint32_t reg_offset, std::uint32_t shift_offset, SpirvReg &reg, std::uint32_t &out_comp_offset, bool get_for_store) {
    const std::uint32_t original_reg_offset = reg_offset;

    if (bank == usse::RegisterBank::FPINTERNAL) {
        // Each GPI register is 128 bits long = 16 bytes long
        // So we need to multiply the reg index with 16 in order to get the right offset
        reg_offset *= 16;
    }

    const auto writeable_idx = (reg_offset + shift_offset) / 4;

    // If we are getting a PRIMATTR reg, we need to check whether a writeable one has already been created
    // If it is, get the writeable one, else proceed
    if (bank == usse::RegisterBank::PRIMATTR) {
        decltype(pa_writeable)::iterator pa;
        if ((pa = pa_writeable.find(writeable_idx)) != pa_writeable.end()) {
            reg = pa->second;
            return true;
        }
    }

    // Again, for SECATTR, there are situation where SECATTR registers
    if (bank == usse::RegisterBank::SECATTR) {
        if (reg_offset > 128) {
            return false;
        }

        if (sa_supplies[writeable_idx].var_id != spv::NoResult) {
            reg = sa_supplies[writeable_idx];
            return true;
        }
    }

    const SpirvVarRegBank *spirv_bank = get_reg_bank(bank);

    auto create_supply_register = [&](SpirvReg base, const std::string &name) -> SpirvReg {
        const spv::Id new_writeable = m_b.createVariable(spv::StorageClassPrivate, type_f32_v[4], name.c_str());

        SpirvReg org1 = base;
        SpirvReg org2;

        uint32_t temp_comp = 0;

        if (!spirv_bank->find_reg_at(reg_offset + shift_offset + m_b.getNumTypeComponents(org1.type_id), org2, temp_comp)) {
            org2 = org1;
        }

        m_b.createStore(bridge(org1, org2, SWIZZLE_CHANNEL_4_DEFAULT, shift_offset, 0b1111), new_writeable);

        base.var_id = new_writeable;
        base.type_id = type_f32_v[4];

        return base;
    };

    bool result = spirv_bank->find_reg_at(reg_offset + shift_offset, reg, out_comp_offset);
    if (!result) {
        // Either if we get them for store or not, sometimes shader will still use SEC as temporary. Weird but ok.
        if (bank == usse::RegisterBank::SECATTR) {
            reg.offset = writeable_idx * 4;

            const std::string new_sa_supply_name = fmt::format("sa{}_temp", std::to_string(writeable_idx * 4));
            reg.type_id = type_f32_v[4];
            reg.var_id = m_b.createVariable(spv::StorageClassPrivate, reg.type_id, new_sa_supply_name.c_str());
            reg.size = 4;

            sa_supplies[writeable_idx] = reg;
            out_comp_offset = (reg_offset + shift_offset) % 4;

            return true;
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

    if (bank == usse::RegisterBank::PRIMATTR && get_for_store) {
        if (pa_writeable.count(writeable_idx) == 0) {
            const std::string new_pa_writeable_name = fmt::format("pa{}_temp", std::to_string(((reg_offset + shift_offset) / 4) * 4));
            reg = create_supply_register(reg, new_pa_writeable_name);
            pa_writeable[writeable_idx] = reg;
        }
    }

    // Either if we get them for store or not, sometimes shader will still use SEC as temporary. Weird but ok.
    if (bank == usse::RegisterBank::SECATTR && get_for_store) {
        const std::string new_sa_supply_name = fmt::format("sa{}_temp", std::to_string(((reg_offset + shift_offset) / 4) * 4));
        reg = create_supply_register(reg, new_sa_supply_name);
        sa_supplies[writeable_idx] = reg;
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
    const std::uint32_t shift_offset) {
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

    if (unpack_results.size() == 1) {
        return unpack_results[0];
    }

    SpirvReg reg_left{};
    SpirvReg reg_right{};

    reg_left.type_id = unpacked_type;
    reg_right.type_id = unpacked_type;

    reg_left.var_id = unpack_results[0];
    reg_right.var_id = unpack_results[1];

    spv::Id last_shuffled = bridge(reg_left, reg_right, swizz, shift_offset, dest_mask);

    // It won't probably go up to two, but just in case ?
    for (std::size_t i = 2; i < unpack_results.size() - 2; i++) {
        reg_left.var_id = last_shuffled;
        reg_right.var_id = unpack_results[i];

        last_shuffled = bridge(reg_left, reg_right, swizz, shift_offset, dest_mask);
    }

    return last_shuffled;
}

spv::Id USSETranslatorVisitor::bridge(SpirvReg &src1, SpirvReg &src2, usse::Swizzle4 swiz, const std::uint32_t shift_offset, const Imm4 dest_mask,
    const std::size_t size_comp) {
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

    std::unordered_map<std::uint32_t, bool> already;

    for (int i = 0; i < std::min(4, static_cast<int>(total_comp_count)); i++) {
        if (dest_mask & (1 << i)) {
            switch (swiz[i]) {
            case usse::SwizzleChannel::_X:
            case usse::SwizzleChannel::_Y:
            case usse::SwizzleChannel::_Z:
            case usse::SwizzleChannel::_W: {
                std::uint32_t swizz_off = (uint32_t)swiz[i] + shift_offset;

                if (size_comp != 4) {
                    swizz_off = swizz_off / static_cast<std::uint32_t>(size_comp);
                }

                if (!already[swizz_off]) {
                    already[swizz_off] = true;
                    ops.push_back(std::min(swizz_off, total_comp_count));
                }

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

spv::Id USSETranslatorVisitor::load(Operand &op, const Imm4 dest_mask, const std::uint8_t offset, bool, bool) {
    /*
    // Optimization: Check for constant swizzle and emit it right away
    for (std::uint8_t i = 0; i < 4; i++) {
        USSE::SwizzleChannel channel = static_cast<USSE::SwizzleChannel>(
            static_cast<std::uint32_t>(USSE::SwizzleChannel::_0) + i);

        if (op.swizzle == USSE::Swizzle4{ channel, channel, channel, channel }) {
            return spv_v4const[i];
        }
    }
    */
    if (op.bank == RegisterBank::FPINTERNAL) {
        // Automatically F32
        op.type = DataType::F32;
    }

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);
    const std::size_t size_comp = get_data_type_size(op.type);

    // Default load will get word as default component
    std::size_t dest_comp_count_to_get = 0;
    bool already[4] = { false, false, false, false };

    for (int i = 0; i < 4; i++) {
        if (dest_mask & (1 << i) && (already[i * size_comp / 4] == false)) {
            dest_comp_count_to_get++;
            already[i * size_comp / 4] = true;
        }
    }

    // TODO: Properly handle
    if (op.bank == RegisterBank::FPCONSTANT || op.bank == RegisterBank::IMMEDIATE) {
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

    if (!get_spirv_reg(op.bank, op.num, offset, reg_left, comp_offset, false)) {
        LOG_ERROR("Can't load register {}", disasm::operand_to_str(op, 0));
        return spv::NoResult;
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

    std::uint32_t size_gotten = m_b.getNumTypeComponents(reg_left.type_id) - comp_offset;

    std::vector<SpirvReg> to_bridge;

    while (size_gotten < dest_comp_count_to_get) {
        SpirvReg another_one{};
        std::uint32_t temp_comp = 0;

        if (!get_spirv_reg(op.bank, op.num, offset + size_gotten, another_one, temp_comp, false)) {
            break;
        }

        to_bridge.push_back(another_one);
        size_gotten += another_one.size;
    }

    if (size_gotten < dest_comp_count_to_get) {
        LOG_ERROR("Can't load register {}, missing registers to bridge", disasm::operand_to_str(op, 0));
        return spv::NoResult;
    } else {
        // To bridge is empty. We need a place holder to shuffle the original up
        if (to_bridge.empty()) {
            to_bridge.push_back(reg_left);
        }
    }

    // We need to bridge
    spv::Id first_pass = spv::NoResult;
    first_pass = bridge(reg_left, to_bridge[0], op.swizzle, comp_offset, dest_mask, size_comp);

    SpirvReg first_pass_wrapper{};

    for (std::size_t i = 1; i < to_bridge.size(); i++) {
        first_pass_wrapper.type_id = m_b.getTypeId(first_pass);
        first_pass_wrapper.var_id = first_pass;

        first_pass = bridge(first_pass_wrapper, to_bridge[i], op.swizzle, comp_offset, dest_mask, size_comp);
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

void USSETranslatorVisitor::store(Operand &dest, spv::Id source, std::uint8_t dest_mask, std::uint8_t off) {
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
                if (m_b.isScalar(source) || m_b.isConstant(source)) {
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

bool USSETranslatorVisitor::vmov(
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
    Imm6 src2_n) {
    Instruction inst{};

    static const Opcode tb_decode_vmov[] = {
        Opcode::VMOV,
        Opcode::VMOVC,
        Opcode::VMOVCU8,
        Opcode::INVALID,
    };

    inst.opcode = tb_decode_vmov[(Imm3)move_type];
    dest_mask = decode_write_mask(dest_mask, move_data_type == DataType::F16);

    std::string disasm_str = fmt::format("{:016x}: {}{}.{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), disasm::data_type_str(move_data_type));

    // TODO: dest mask
    // TODO: flags
    // TODO: test type

    const bool is_double_regs = move_data_type == DataType::C10 || move_data_type == DataType::F16 || move_data_type == DataType::F32;
    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);

    // Decode operands
    uint8_t reg_bits = is_double_regs ? 7 : 6;

    inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, is_double_regs, reg_bits, m_second_program);
    inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, is_double_regs, reg_bits, m_second_program);

    // Velocity uses a vec4 table, non-extended, so i assumes type=vec4, extended=false
    inst.opr.src1.swizzle = decode_vec34_swizzle(src0_swiz, false, 2);

    inst.opr.src1.type = move_data_type;
    inst.opr.dest.type = move_data_type;

    // TODO: adjust dest mask if needed

    if (is_conditional) {
        LOG_WARN("Conditional vmov instructions unsupported");
        return false;
    }

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    // if (is_conditional) {
    //     inst.operands.src0 = decode_src0(src0_n, src0_bank_sel, end_or_src0_bank_ext, is_double_regs);
    //     inst.operands.src2 = decode_src12(src2_n, src2_bank_sel, src2_bank_ext, is_double_regs);
    // }

    disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask));
    LOG_DISASM(disasm_str);

    // Recompile

    m_b.setLine(usse::instr_idx);

    BEGIN_REPEAT(repeat_count, 2)
    spv::Id source = load(inst.opr.src1, dest_mask, repeat_offset);
    store(inst.opr.dest, source, dest_mask, repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::vmad(
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
    Imm6 src0_n) {
    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), "VMAD");

    Instruction inst{};

    // Is this VMAD3 or VMAD4, op2 = 0 => vec3
    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    // Double regs always true for src0, dest
    inst.opr.src0 = decode_src12(src0_n, src0_bank, src0_bank_ext, true, 7, m_second_program);
    inst.opr.dest = decode_dest(dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    // GPI0 and GPI1, setup!
    inst.opr.src1.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src1.num = gpi0_n;

    inst.opr.src2.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src2.num = gpi1_n;

    // Swizzleee
    if (type == 1) {
        inst.opr.dest.swizzle[3] = usse::SwizzleChannel::_X;
    }

    inst.opr.src0.swizzle = decode_vec34_swizzle(src0_swiz, src0_swiz_ext, type);
    inst.opr.src1.swizzle = decode_vec34_swizzle(gpi0_swiz, gpi0_swiz_ext, type);
    inst.opr.src2.swizzle = decode_vec34_swizzle(gpi1_swiz, gpi1_swiz_ext, type);

    disasm_str += fmt::format(" {} {} {} {}", disasm::operand_to_str(inst.opr.dest, write_mask), disasm::operand_to_str(inst.opr.src0, write_mask), disasm::operand_to_str(inst.opr.src1, write_mask), disasm::operand_to_str(inst.opr.src2, write_mask));

    LOG_DISASM("{}", disasm_str);
    m_b.setLine(usse::instr_idx);

    // Write mask is a 4-bit immidiate
    // If a bit is one, a swizzle is active
    BEGIN_REPEAT(repeat_count, 2)
    spv::Id vsrc0 = load(inst.opr.src0, write_mask, repeat_offset, src0_abs, src0_neg);
    spv::Id vsrc1 = load(inst.opr.src1, write_mask, repeat_offset, gpi0_abs, gpi0_neg);
    spv::Id vsrc2 = load(inst.opr.src2, write_mask, repeat_offset, gpi1_abs, gpi1_neg);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(inst.opr.dest, add_result, write_mask, repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::vnmad32(
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
    Imm6 src2_n) {
    Instruction inst{};

    static const Opcode tb_decode_vop_f32[] = {
        Opcode::VMUL,
        Opcode::VADD,
        Opcode::VFRC,
        Opcode::VDSX,
        Opcode::VDSY,
        Opcode::VMIN,
        Opcode::VMAX,
        Opcode::VDP,
    };
    static const Opcode tb_decode_vop_f16[] = {
        Opcode::VF16MUL,
        Opcode::VF16ADD,
        Opcode::VF16FRC,
        Opcode::VF16DSX,
        Opcode::VF16DSY,
        Opcode::VF16MIN,
        Opcode::VF16MAX,
        Opcode::VF16DP,
    };
    Opcode opcode;
    const bool is_32_bit = m_instr & (1ull << 59);
    if (is_32_bit)
        opcode = tb_decode_vop_f32[op2];
    else
        opcode = tb_decode_vop_f16[op2];

    // Decode operands
    // TODO: modifiers

    inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, true, 7, m_second_program);
    inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, true, 7, m_second_program);
    inst.opr.src1.flags = decode_modifier(src1_mod);

    inst.opr.src2 = decode_src12(src2_n, src2_bank_sel, src2_bank_ext, true, 7, m_second_program);

    if (src2_mod == 1) {
        inst.opr.src2.flags = RegisterFlags::Absolute;
    }

    inst.opr.dest.type = is_32_bit ? DataType::F32 : DataType::F16;
    inst.opr.src1.type = inst.opr.dest.type;
    inst.opr.src2.type = inst.opr.dest.type;

    const auto src1_swizzle_enc = src1_swiz_0_6 | src1_swiz_7_8 << 7 | src1_swiz_9 << 9 | src1_swiz_10_11 << 10;
    inst.opr.src1.swizzle = decode_swizzle4(src1_swizzle_enc);

    static const Swizzle4 tb_decode_src2_swizzle[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, Z),
        SWIZZLE_CHANNEL_4(X, X, Y, Z),
        SWIZZLE_CHANNEL_4(X, Y, X, Y),
        SWIZZLE_CHANNEL_4(X, Y, W, Z),
        SWIZZLE_CHANNEL_4(Z, X, Y, W),
        SWIZZLE_CHANNEL_4(Z, W, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, X, Z),
        SWIZZLE_CHANNEL_4(X, X, Y, Y),
        SWIZZLE_CHANNEL_4(X, Z, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, 1),
    };

    inst.opr.src2.swizzle = tb_decode_src2_swizzle[src2_swiz];

    // TODO: source modifiers

    auto dest_comp_count = dest_mask_to_comp_count(dest_mask);

    LOG_DISASM("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(opcode), disasm::operand_to_str(inst.opr.dest, dest_mask),
        disasm::operand_to_str(inst.opr.src1, dest_mask), disasm::operand_to_str(inst.opr.src2, dest_mask));

    // Recompile

    m_b.setLine(usse::instr_idx);

    spv::Id vsrc1 = load(inst.opr.src1, dest_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, dest_mask, 0);

    if (vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_WARN("Could not find a src register");
        return false;
    }

    spv::Id result = spv::NoResult;

    switch (opcode) {
    case Opcode::VADD:
    case Opcode::VF16ADD: {
        result = m_b.createBinOp(spv::OpFAdd, type_f32_v[dest_comp_count], vsrc1, vsrc2);
        break;
    }

    case Opcode::VMUL:
    case Opcode::VF16MUL: {
        result = m_b.createBinOp(spv::OpFMul, type_f32_v[dest_comp_count], vsrc1, vsrc2);
        break;
    }

    case Opcode::VMIN:
    case Opcode::VF16MIN: {
        result = m_b.createBuiltinCall(m_b.getTypeId(vsrc1), std_builtins, GLSLstd450FMin, { vsrc1, vsrc2 });
        break;
    }

    case Opcode::VMAX:
    case Opcode::VF16MAX: {
        result = m_b.createBuiltinCall(m_b.getTypeId(vsrc1), std_builtins, GLSLstd450FMax, { vsrc1, vsrc2 });
        break;
    }

    case Opcode::VFRC:
    case Opcode::VF16FRC: {
        // Dest = Source1 - Floor(Source2)
        // If two source are identical, let's use the fractional function
        if (inst.opr.src1.is_same(inst.opr.src2, dest_mask)) {
            result = m_b.createBuiltinCall(m_b.getTypeId(vsrc1), std_builtins, GLSLstd450Fract, { vsrc1 });
        } else {
            // We need to floor source 2
            spv::Id source2_floored = m_b.createBuiltinCall(m_b.getTypeId(vsrc2), std_builtins, GLSLstd450Floor, { vsrc2 });
            // Then subtract source 1 with the floored source 2. TADA!
            result = m_b.createBinOp(spv::OpFSub, m_b.getTypeId(vsrc1), vsrc1, source2_floored);
        }

        break;
    }

    default: {
        LOG_ERROR("Unimplemented vnmad instruction: {}", disasm::opcode_str(opcode));
        break;
    }
    }

    if (result != spv::NoResult) {
        store(inst.opr.dest, result, dest_mask, 0);
    }

    return true;
}

bool USSETranslatorVisitor::vnmad16(
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
    Imm6 src2_n) {
    // TODO
    return vnmad32(pred, skipinv, src1_swiz_10_11, syncstart, dest_bank_ext, src1_swiz_9, src1_bank_ext, src2_bank_ext, src2_swiz, nosched, dest_mask, src1_mod, src2_mod, src1_swiz_7_8, dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_n, src1_swiz_0_6, op2, src1_n, src2_n);
}

bool USSETranslatorVisitor::vpck(
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
    Imm5 src1_n,
    Imm1 comp0_sel_bit1,
    Imm4 src2_n,
    Imm1 comp_sel_0_bit0) {
    Instruction inst{};

    // TODO: There are some combinations that are invalid.
    const DataType dest_data_type_table[] = {
        DataType::UINT8,
        DataType::INT8,
        DataType::O8,
        DataType::UINT16,
        DataType::INT16,
        DataType::F16,
        DataType::F32,
        DataType::C10
    };

    const DataType src_data_type_table[] = {
        DataType::UINT8,
        DataType::INT8,
        DataType::O8,
        DataType::UINT16,
        DataType::INT16,
        DataType::F16,
        DataType::F32,
        DataType::C10
    };

    const Opcode op_table[][static_cast<int>(DataType::TOTAL_TYPE)] = {
        { Opcode::VPCKU8U8,
            Opcode::VPCKU8S8,
            Opcode::VPCKU8O8,
            Opcode::VPCKU8U16,
            Opcode::VPCKU8S16,
            Opcode::VPCKU8F16,
            Opcode::VPCKU8F32,
            Opcode::INVALID },
        { Opcode::VPCKS8U8,
            Opcode::VPCKS8S8,
            Opcode::VPCKS8O8,
            Opcode::VPCKS8U16,
            Opcode::VPCKS8S16,
            Opcode::VPCKS8F16,
            Opcode::VPCKS8F32,
            Opcode::INVALID },
        { Opcode::VPCKO8U8,
            Opcode::VPCKO8S8,
            Opcode::VPCKO8O8,
            Opcode::VPCKO8U16,
            Opcode::VPCKO8S16,
            Opcode::VPCKO8F16,
            Opcode::VPCKO8F32,
            Opcode::INVALID },
        { Opcode::VPCKU16U8,
            Opcode::VPCKU16S8,
            Opcode::VPCKU16O8,
            Opcode::VPCKU16U16,
            Opcode::VPCKU16S16,
            Opcode::VPCKU16F16,
            Opcode::VPCKU16F32,
            Opcode::INVALID },
        { Opcode::VPCKS16U8,
            Opcode::VPCKS16S8,
            Opcode::VPCKS16O8,
            Opcode::VPCKS16U16,
            Opcode::VPCKS16S16,
            Opcode::VPCKS16F16,
            Opcode::VPCKS16F32,
            Opcode::INVALID },
        { Opcode::VPCKF16U8,
            Opcode::VPCKF16S8,
            Opcode::VPCKF16O8,
            Opcode::VPCKF16U16,
            Opcode::VPCKF16S16,
            Opcode::VPCKF16F16,
            Opcode::VPCKF16F32,
            Opcode::VPCKF16C10 },
        { Opcode::VPCKF32U8,
            Opcode::VPCKF32S8,
            Opcode::VPCKF32O8,
            Opcode::VPCKF32U16,
            Opcode::VPCKF32S16,
            Opcode::VPCKF32F16,
            Opcode::VPCKF32F32,
            Opcode::VPCKF32C10 },
        { Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::VPCKC10F16,
            Opcode::VPCKC10F32,
            Opcode::VPCKC10C10 }
    };

    inst.opcode = op_table[dest_fmt][src_fmt];

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));

    inst.opr.dest = decode_dest(dest_n, dest_bank_sel, dest_bank_ext, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(src1_n, src1_bank_sel, src1_bank_ext, true, 7, m_second_program);

    inst.opr.dest.type = dest_data_type_table[dest_fmt];
    inst.opr.src1.type = src_data_type_table[src_fmt];

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    Imm2 comp_sel_0 = comp_sel_0_bit0;

    if (inst.opr.src1.type == DataType::F32)
        comp_sel_0 |= (comp0_sel_bit1 & 1) << 1;
    else
        comp_sel_0 |= (src2_n & 1) << 1;

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_CAST(comp_sel_0, comp_sel_1, comp_sel_2, comp_sel_3);

    disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask));
    LOG_DISASM(disasm_str);

    // Recompile

    m_b.setLine(usse::instr_idx);

    BEGIN_REPEAT(repeat_count, 2)
    spv::Id source = load(inst.opr.src1, dest_mask, repeat_offset);
    store(inst.opr.dest, source, dest_mask, repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::phas() {
    usse::instr_idx--;
    LOG_DISASM("{:016x}: PHAS", m_instr);
    return true;
}

bool USSETranslatorVisitor::spec(
    bool special,
    SpecialCategory category) {
    usse::instr_idx--;
    LOG_DISASM("{:016x}: SPEC category: {}, special: {}", m_instr, (uint8_t)category, special);
    return true;
}
