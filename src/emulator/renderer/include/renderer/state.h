#pragma once

#include <crypto/hash.h>
#include <renderer/types.h>

#include <map>
#include <string>

namespace renderer {

struct State {
    GLSLCache fragment_glsl_cache;
    GLSLCache vertex_glsl_cache;

    GXPPtrMap gxp_ptr_map;
};

} // namespace renderer
