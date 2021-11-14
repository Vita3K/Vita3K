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

#include <spirv_glsl.hpp>

#include <unordered_map>
#include <vector>

namespace spv {
class Builder;
}

namespace shader::usse {

using SpirvCode = std::vector<uint32_t>;
using SpirvVarRegBank = spv::Id;

struct SpirvUniformBufferInfo {
    std::uint32_t base;
    std::uint32_t size;
    std::uint32_t index_in_container;
};

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

    // Uniform buffer map contains layout info of a UBO inside the big SSBO.
    std::map<std::uint32_t, SpirvUniformBufferInfo> buffers;

    spv::Id buffer_container;
};

using Coord = std::pair<spv::Id, int>;

struct NonDependentTextureQueryCallInfo {
    spv::Id sampler;
    Coord coord;
    int coord_index = 0;

    std::uint32_t dest_offset = 0;
    int store_type = 0; ///< For sampling method later
};

using NonDependentTextureQueryCallInfos = std::vector<NonDependentTextureQueryCallInfo>;
using SamplerMap = std::unordered_map<std::uint32_t, spv::Id>;

} // namespace shader::usse
