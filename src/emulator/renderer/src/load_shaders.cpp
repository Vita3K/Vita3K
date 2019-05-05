#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <util/fs.h>
#include <util/log.h>

#include <fstream>
#include <sstream>
#include <utility>

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

static void dump_missing_shader(const char *hash, const char *extension, const SceGxmProgram &program, const char *source, const char *spirv, const char *disasm,
    const char *base_path, const char *title_id) {
    fs::path shader_base_dir;
    shader_base_dir.append(base_path).append("shaderlog").append(title_id);

    if (!fs::exists(shader_base_dir))
        fs::create_directory(shader_base_dir);

    fs::path shader_base_path(shader_base_dir);
    shader_base_path /= hash;
    shader_base_path += std::string(".") + extension;

    // Dump missing shader GLSL.
    std::ofstream glsl_file(shader_base_path.string());
    if (!glsl_file.fail()) {
        glsl_file << source;
        glsl_file.close();
    }

    auto write_data_with_ext = [&](const char *ext, const char *data, const std::int64_t size) {
        // Dump missing shader binary.
        fs::path fpath(shader_base_path.string());
        fpath += ext;
        std::ofstream dfile(fpath.string(), (size == -1) ? 0 : std::ofstream::binary);

        if (!dfile.fail()) {
            if (size == -1) {
                // This is a normal string
                dfile << data;
            } else {
                dfile.write(data, size);
            }
            dfile.close();
        }
    };

    write_data_with_ext(".gxp", reinterpret_cast<const char*>(&program), program.size);
    write_data_with_ext(".dsm", disasm, -1);
    write_data_with_ext(".spt", spirv, -1);
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

        std::string spirv_dump;
        std::string disasm_dump;

        source = shader::convert_gxp_to_glsl(program, hash_text.data(), false, &spirv_dump, &disasm_dump);

        dump_missing_shader(hash_text.data(), shader_type_str, program, source.c_str(), spirv_dump.c_str(), disasm_dump.c_str(), 
            base_path, title_id);
    }

    cache.emplace(hash_bytes, source);

    return source;
}

} // namespace renderer
