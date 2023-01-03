// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <shader/usse_types.h>

#include <map>
#include <unordered_map>
#include <vector>

namespace spv {
class Builder;
typedef unsigned int Id;
} // namespace spv

namespace shader::usse {

using SpirvCode = std::vector<uint32_t>;
using SpirvVarRegBank = spv::Id;

struct SpirvUniformBufferInfo {
    std::uint32_t base;
    std::uint32_t size;
    std::uint32_t index_in_container;
};

struct SamplerInfo {
    spv::Id id;
    DataType component_type;
    uint8_t component_count;
};
using SamplerMap = std::unordered_map<std::uint32_t, SamplerInfo>;

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
    SamplerMap samplers;

    // Uniform buffer map contains layout info of a UBO inside the big SSBO.
    // only used if memory mapping is not enabled
    std::map<std::uint32_t, SpirvUniformBufferInfo> buffers;

    // when not using buffer device address, contains the storage buffer type
    spv::Id buffer_container;

    spv::Id render_info_id;
};

using Coord = std::pair<spv::Id, int>;

struct NonDependentTextureQueryCallInfo {
    spv::Id sampler;
    Coord coord;
    int coord_index = 0;
    int sampler_index = 0;

    std::uint32_t dest_offset = 0;
    int store_type = 0; ///< For sampling method later
    int prod_pos = -1;

    DataType component_type;
    uint8_t component_count;
};

using NonDependentTextureQueryCallInfos = std::vector<NonDependentTextureQueryCallInfo>;

} // namespace shader::usse
