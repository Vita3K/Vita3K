#pragma once

//
// Decoder/translator usage (exposed API)
//

struct SceGxmProgram;

namespace spv {
class Builder;
}

namespace shader {
struct SpirvShaderParameters;
namespace usse {

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const SpirvShaderParameters &parameters);

} // namespace usse
} // namespace shader
