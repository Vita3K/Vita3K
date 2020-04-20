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
    spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &queries);

} // namespace usse
} // namespace shader
