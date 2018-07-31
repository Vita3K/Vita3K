#pragma once

#include <crypto/hash.h>

#include <map>
#include <string>

namespace renderer {
typedef std::map<Sha256Hash, std::string> GLSLCache;

struct State {
    GLSLCache fragment_glsl_cache;
    GLSLCache vertex_glsl_cache;
};
} // namespace renderer
