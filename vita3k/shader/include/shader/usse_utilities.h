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

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

struct FeatureState;

namespace shader::usse::utils {

struct SpirvUtilFunctions {
    spv::Id std_builtins{};
    std::map<DataType, spv::Function *> unpack_funcs;
    std::map<DataType, spv::Function *> pack_funcs;
    spv::Function *fetch_memory{ nullptr };
    spv::Function *unpack_fx10{ nullptr };
    spv::Function *pack_fx10{ nullptr };

    // buffer_address_vec[i][1] contains the buffer pointer with an array of vec_i and stride 16 bytes
    // 0 in the last index is for the read buffer, 1 is for the write buffer
    // this is technically not a function but is the best place to put it
    // buffer_address_vec[0] is for a packed float[] array
    spv::Id buffer_address_vec[5][2] = {};
};

spv::Id finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, spv::Id offset, const Imm4 dest_mask);
spv::Id load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset);
void store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, spv::Id source, std::uint8_t dest_mask, int off);

spv::Id unpack(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

spv::Id unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type);
spv::Id pack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id vec, const DataType source_type);

spv::Id fetch_memory(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, spv::Id addr);
void buffer_address_access(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, int dest_offset, spv::Id addr, uint32_t component_size, uint32_t nb_components, int buffer_idx = -1, bool is_buffer_store = false);

spv::Id make_vector_or_scalar_type(spv::Builder &b, spv::Id component, int size);

spv::Id unwrap_type(spv::Builder &b, spv::Id type);

spv::Id create_constant_vector_or_scalar(spv::Builder &b, spv::Id constant, int comp_count);
spv::Id convert_to_float(spv::Builder &b, const SpirvUtilFunctions &utils, spv::Id opr, DataType type, bool normal);
spv::Id convert_to_int(spv::Builder &b, const SpirvUtilFunctions &utils, spv::Id opr, DataType type, bool normal);

spv::Id add_uvec2_uint(spv::Builder &b, spv::Id vec, spv::Id to_add);

size_t dest_mask_to_comp_count(shader::usse::Imm4 dest_mask);

spv::Id create_access_chain(spv::Builder &b, const spv::StorageClass storage_class, const spv::Id base, const std::vector<spv::Id> &offsets);

template <typename T>
spv::Id make_uniform_vector_from_type(spv::Builder &b, spv::Id type, T val) {
    const int num_comp = b.getNumTypeComponents(type);
    spv::Id v_elem_type = (num_comp > 1) ? b.getContainedTypeId(type) : type;

    spv::Id cnst = spv::NoResult;

    if (b.isUintType(v_elem_type)) {
        cnst = b.makeUintConstant(val);
    } else if (b.isIntType(v_elem_type)) {
        cnst = b.makeIntConstant(val);
    } else {
        cnst = b.makeFloatConstant(val);
    }

    if (num_comp == 1) {
        return cnst;
    }

    std::vector<spv::Id> c_vecs(num_comp, cnst);
    spv::Id v0 = b.makeCompositeConstant(type, c_vecs);

    return v0;
}

template <typename F>
void make_for_loop(spv::Builder &b, spv::Id iterator, spv::Id initial_value_ite, spv::Id iterator_limit, F body) {
    auto blocks = b.makeNewLoop();
    b.createStore(initial_value_ite, iterator);
    b.createBranch(true, &blocks.head);

    b.setBuildPoint(&blocks.head);

    spv::Id compare_result = b.createBinOp(spv::OpSLessThan, b.makeBoolType(), b.createLoad(iterator, spv::NoPrecision), iterator_limit);

    b.createLoopMerge(&blocks.merge, &blocks.continue_target, spv::LoopControlMaskNone, {});
    b.createConditionalBranch(compare_result, &blocks.body, &blocks.merge);

    b.setBuildPoint(&blocks.body);
    body();

    // Increase i
    spv::Id add_to_me = b.createBinOp(spv::OpIAdd, b.makeIntegerType(32, true), b.createLoad(iterator, spv::NoPrecision), b.makeIntConstant(1));
    b.createStore(add_to_me, iterator);

    b.createBranch(true, &blocks.continue_target);

    b.setBuildPoint(&blocks.continue_target);
    b.createBranch(true, &blocks.head);

    b.setBuildPoint(&blocks.merge);
    b.closeLoop();
}
} // namespace shader::usse::utils
