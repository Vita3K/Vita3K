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

//
// Decoder/translator usage (exposed API)
//

#include <vector>

struct SceGxmProgram;
struct FeatureState;

namespace spv {
class Builder;
class Function;
} // namespace spv

namespace shader {
namespace usse {
struct SpirvShaderParameters;
struct NonDependentTextureQueryCallInfo;

namespace utils {
struct SpirvUtilFunctions;
}

using NonDependentTextureQueryCallInfos = std::vector<NonDependentTextureQueryCallInfo>;

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const FeatureState &features, const SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils,
    spv::Function *begin_hook_func, spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &queries);

} // namespace usse
} // namespace shader
