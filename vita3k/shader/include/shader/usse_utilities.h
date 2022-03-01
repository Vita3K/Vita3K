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

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

#include <gxm/types.h>

struct FeatureState;

namespace shader::usse::utils {

struct SpirvUtilFunctions {
    std::map<DataType, spv::Function *> unpack_funcs;
    std::map<DataType, spv::Function *> pack_funcs;
    spv::Function *fetch_memory{ nullptr };
    spv::Function *pack_fx8{ nullptr };
    spv::Function *unpack_fx8{ nullptr };
};

spv::Id finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, const int offset, const Imm4 dest_mask);
spv::Id load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset);
void store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, spv::Id source, std::uint8_t dest_mask, int off);

spv::Id unpack(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

spv::Id unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type);
spv::Id pack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id vec, const DataType source_type);

spv::Id fetch_memory(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, spv::Id addr);

spv::Id make_vector_or_scalar_type(spv::Builder &b, spv::Id component, int size);

spv::Id unwrap_type(spv::Builder &b, spv::Id type);

spv::Id convert_to_float(spv::Builder &b, spv::Id opr, DataType type, bool normal);
spv::Id convert_to_int(spv::Builder &b, spv::Id opr, DataType type, bool normal);

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
    b.createBranch(&blocks.head);

    b.setBuildPoint(&blocks.head);

    spv::Id compare_result = b.createBinOp(spv::OpSLessThan, b.makeBoolType(), b.createLoad(iterator), iterator_limit);

    b.createLoopMerge(&blocks.merge, &blocks.continue_target, spv::LoopControlMaskNone, {});
    b.createConditionalBranch(compare_result, &blocks.body, &blocks.merge);

    b.setBuildPoint(&blocks.body);
    body();

    // Increase i
    spv::Id add_to_me = b.createBinOp(spv::OpIAdd, b.makeIntegerType(32, true), b.createLoad(iterator), b.makeIntConstant(1));
    b.createStore(add_to_me, iterator);

    b.createBranch(&blocks.continue_target);

    b.setBuildPoint(&blocks.continue_target);
    b.createBranch(&blocks.head);

    b.setBuildPoint(&blocks.merge);
    b.closeLoop();
}
} // namespace shader::usse::utils