#pragma once

//
// Decoder/translator usage (exposed API)
//

#include <vector>

struct SceGxmProgram;

namespace spv {
class Builder;
}

namespace shader {
namespace usse {
struct SpirvShaderParameters;
struct NonDependentTextureQueryCallInfo;

using NonDependentTextureQueryCallInfos = std::vector<NonDependentTextureQueryCallInfo>;

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const SpirvShaderParameters &parameters, const NonDependentTextureQueryCallInfos &queries);

} // namespace usse
} // namespace shader
