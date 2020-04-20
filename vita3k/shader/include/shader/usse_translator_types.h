#pragma once

#include <spirv_glsl.hpp>

#include <unordered_map>
#include <vector>

namespace spv {
class Builder;
}

namespace shader::usse {

using SpirvCode = std::vector<uint32_t>;
using SpirvVarRegBank = spv::Id;

struct SpirvShaderParameters {
    // Mapped to 'pa' (primary attribute) USSE registers
    // for vertex: vertex inputs (vertex attributes)
    // for fragment: fragment inputs (linkage from vertex stage)
    SpirvVarRegBank ins;

    // Mapped to 'sa' (secondary attribute) USSE registers
    SpirvVarRegBank uniforms;

    // Mapped to 'r' (temporary) USSE registers
    SpirvVarRegBank temps;

    // Mapped to 'i' (internal) usse registers
    SpirvVarRegBank internals;

    // Mapped to 'o' (output) USSE registers
    // for vertex: vertex outputs (linkage into fragment stage)
    // for fragment: fragment outputs (color outputs)
    SpirvVarRegBank outs;

    // Mapped to 'idx' (index) USSE registers
    // Use for indexing bank registers dynamically
    SpirvVarRegBank indexes;

    // Mapped to 'p' (predicates) USSE registers
    // Store boolean, use for conditional execution.
    SpirvVarRegBank predicates;

    // Sampler map. Since all banks are a flat array, sampler must be in an explicit bank.
    std::unordered_map<std::uint32_t, spv::Id> samplers;

    std::unordered_map<std::uint32_t, spv::Id> buffers;
};

using Coord = std::pair<spv::Id, int>;

struct NonDependentTextureQueryCallInfo {
    spv::Id sampler;
    Coord coord;
    int coord_index;

    std::uint32_t dest_offset;
    int store_type; ///< For sampling method later
};

using NonDependentTextureQueryCallInfos = std::vector<NonDependentTextureQueryCallInfo>;
using SamplerMap = std::unordered_map<std::uint32_t, spv::Id>;

} // namespace shader::usse
