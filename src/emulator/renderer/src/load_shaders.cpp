#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <util/fs.h>
#include <util/log.h>

#include <utility>

using namespace glbinding;

namespace renderer {
static std::string load_shader(const char *hash, const char *extension, const char *base_path) {
    const auto shader_path = fs_utils::construct_file_name(base_path, "shaders", hash, extension);
    fs::ifstream is(shader_path, fs::ifstream::binary);
    if (!is) {
        return std::string();
    }

    is.seekg(0, fs::ifstream::end);
    const size_t size = is.tellg();
    is.seekg(0);

    std::string source(size, ' ');
    is.read(&source.front(), size);

    return source;
}

static void dump_missing_shader(const char *hash, const char *extension, const SceGxmProgram &program, const char *source, const char *base_path, const char *title_id) {
    const fs::path shader_base_dir{ fs::path("shaderlog") / title_id };
    if (!fs::exists(base_path / shader_base_dir))
        fs::create_directories(base_path / shader_base_dir);

    const auto shader_base_path = fs_utils::construct_file_name(base_path, shader_base_dir, hash, extension);

    // Dump missing shader GLSL.
    fs::ofstream glsl_file(shader_base_path);
    if (glsl_file) {
        glsl_file << source;
        glsl_file.close();
    }

    // Dump missing shader binary.
    fs::path gxp_path{ shader_base_path };
    gxp_path.replace_extension(".gxp");
    fs::ofstream gxp_file(gxp_path, fs::ofstream::binary);
    if (gxp_file) {
        gxp_file.write(reinterpret_cast<const char *>(&program), program.size);
        gxp_file.close();
    }
}

std::string load_shader(GLSLCache &cache, const SceGxmProgram &program, const char *base_path, const char *title_id) {
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    const GLSLCache::const_iterator cached = cache.find(hash_bytes);
    if (cached != cache.end()) {
        return cached->second;
    }

    auto shader_type_to_str = [](emu::SceGxmProgramType type) {
        return type == emu::Vertex ? "vert" : type == emu::Fragment ? "frag" : "unknown";
    };

    emu::SceGxmProgramType program_type = program.get_type();
    const char *shader_type_str = shader_type_to_str(program_type);

    const Sha256HashText hash_text = hex(hash_bytes);

    std::string source = load_shader(hash_text.data(), shader_type_str, base_path);
    if (source.empty()) {
        LOG_INFO("Generating {} shader {}", shader_type_str, hash_text.data());

        source = shader::convert_gxp_to_glsl(program, hash_text.data());

        dump_missing_shader(hash_text.data(), shader_type_str, program, source.c_str(), base_path, title_id);
    }

    cache.emplace(hash_bytes, source);

    return source;
}

} // namespace renderer
