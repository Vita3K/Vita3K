#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

struct FeatureState;

namespace shader::usse::utils {

struct SpirvUtilFunctions {
    spv::Function *pack_f16{ nullptr };
    spv::Function *unpack_f16{ nullptr };
};

spv::Id finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, const int offset, const Imm4 dest_mask);
spv::Id load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset);
void store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, spv::Id source, std::uint8_t dest_mask, int off);

spv::Id unpack(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

spv::Id unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type);
spv::Id pack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id vec, const DataType source_type);

spv::Id make_uniform_vector_from_type(spv::Builder &b, spv::Id type, int val);

} // namespace shader::usse::utils