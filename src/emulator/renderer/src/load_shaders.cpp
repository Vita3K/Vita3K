#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <gxm/types.h>
#include <shadergen/functions.h>
#include <util/log.h>

#include <fstream>
#include <sstream>

using namespace glbinding;

namespace renderer {
static std::string load_shader(const char *hash, const char *extension, const char *base_path) {
    std::ostringstream path;
    path << base_path << "shaders/" << hash << "." << extension;

    std::ifstream is(path.str());
    if (is.fail()) {
        return std::string();
    }

    is.seekg(0, std::ios::end);
    const size_t size = is.tellg();
    is.seekg(0);

    std::string source(size, ' ');
    is.read(&source.front(), size);

    return source;
}

static void dump_missing_shader(const char *hash, const char *extension, const SceGxmProgram &program, const char *source) {
    // Dump missing shader GLSL.
    std::ostringstream glsl_path;
    glsl_path << hash << "." << extension;
    std::ofstream glsl_file(glsl_path.str());
    if (!glsl_file.fail()) {
        glsl_file << source;
        glsl_file.close();
    }

    // Dump missing shader binary.
    std::ostringstream gxp_path;
    gxp_path << hash << ".gxp";
    std::ofstream gxp(gxp_path.str(), std::ofstream::binary);
    if (!gxp.fail()) {
        gxp.write(reinterpret_cast<const char *>(&program), program.size);
        gxp.close();
    }
}

std::string load_fragment_shader(GLSLCache &cache, const SceGxmProgram &fragment_program, const char *base_path) {
    const Sha256Hash hash_bytes = sha256(&fragment_program, fragment_program.size);
    const GLSLCache::const_iterator cached = cache.find(hash_bytes);
    if (cached != cache.end()) {
        return cached->second;
    }

    const Sha256HashText hash_text = hex(hash_bytes);
    std::string source = load_shader(hash_text.data(), "frag", base_path);
    if (source.empty()) {
        LOG_ERROR("Missing fragment shader {}", hash_text.data());
        source = shadergen::spirv::generate_fragment_glsl(fragment_program);
        dump_missing_shader(hash_text.data(), "frag", fragment_program, source.c_str());
    }

    cache.emplace(hash_bytes, source);

    return source;
}

std::string load_vertex_shader(GLSLCache &cache, const SceGxmProgram &vertex_program, const char *base_path) {
    const Sha256Hash hash_bytes = sha256(&vertex_program, vertex_program.size);
    const GLSLCache::const_iterator cached = cache.find(hash_bytes);
    if (cached != cache.end()) {
        return cached->second;
    }

    const Sha256HashText hash_text = hex(hash_bytes);
    std::string source = load_shader(hash_text.data(), "vert", base_path);
    if (source.empty()) {
        LOG_ERROR("Missing vertex shader {}", hash_text.data());
        source = shadergen::spirv::generate_vertex_glsl(vertex_program);
        dump_missing_shader(hash_text.data(), "vert", vertex_program, source.c_str());
    }

    cache.emplace(hash_bytes, source);

    return source;
}
} // namespace renderer
