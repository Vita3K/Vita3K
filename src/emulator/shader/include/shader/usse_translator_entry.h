#pragma once

//
// Decoder/translator usage (exposed API)
//

struct SceGxmProgram;

namespace spv {
class Builder;
}

namespace shader {
namespace usse {
struct SpirvShaderParameters;

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const SpirvShaderParameters &parameters);

} // namespace usse
} // namespace shader
