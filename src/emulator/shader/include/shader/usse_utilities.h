#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_types.h>
#include <shader/usse_translator_types.h>

namespace shader::usse::utils {

struct SpirvUtilFunctions {
    spv::Function *pack_f16 {nullptr};
    spv::Function *unpack_f16 {nullptr};
};

spv::Id finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, const int offset, const Imm4 dest_mask);
spv::Id load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, Operand op, const Imm4 dest_mask, const int shift_offset);
void store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, Operand dest, spv::Id source, std::uint8_t dest_mask, int off);

spv::Id unpack(spv::Builder &b, SpirvUtilFunctions &utils, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

spv::Id unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, spv::Id scalar, const DataType type);
spv::Id pack_one(spv::Builder &b, SpirvUtilFunctions &utils, spv::Id vec, const DataType source_type);

void single_cond_branch(spv::Builder &b, spv::Id cond, spv::Block *cond_satisfy_block, spv::Block *contigous_block);
void end_if(spv::Builder &b, spv::Block *merge_block);

}