#include <renderer/functions.h>
#include <renderer/profile.h>

#include <renderer/gl/functions.h>

#include <gxm/types.h>
#include <renderer/types.h>
#include <shader/spirv_recompiler.h>
#include <util/fs.h>
#include <util/log.h>

#include <fstream>
#include <utility>

namespace renderer::gl {
static std::string load_shader(const char *hash, const char *extension, const char *base_path) {
    const auto shader_path = fs_utils::construct_file_name(base_path, "shaders", hash, extension);
    std::ifstream is(shader_path, std::ifstream::binary);
    if (!is) {
        return std::string();
    }

    is.seekg(0, std::ifstream::end);
    const size_t size = is.tellg();
    is.seekg(0);

    std::string source(size, ' ');
    is.read(&source.front(), size);

    return source;
}

static void dump_missing_shader(const char *hash, const char *extension, const SceGxmProgram &program, const char *source, const char *spirv,
    const char *disasm, const char *base_path, const char *title_id) {
    const fs::path shader_base_dir{ fs::path("shaderlog") / title_id };
    if (!fs::exists(base_path / shader_base_dir))
        fs::create_directories(base_path / shader_base_dir);

    const auto shader_base_path = fs_utils::construct_file_name(base_path, shader_base_dir, hash, extension);

    // Dump missing shader GLSL.
    std::ofstream glsl_file(shader_base_path);
    if (glsl_file) {
        glsl_file << source;
        glsl_file.close();
    }

    const auto write_data_with_ext = [&](const char *ext, const char *data, const std::int64_t size) {
        // Dump missing shader binary.
        fs::path out_path{ shader_base_path };
        out_path.replace_extension(ext);

        if (size == -1) {
            std::ofstream of{ out_path };
            if (!of.fail()) {
                of << data; // This is a normal string
                of.close();
            }
        } else {
            std::ofstream of{ out_path, std::ofstream::binary };
            if (!of.fail()) {
                of.write(data, size);
                of.close();
            }
        }
    };

    write_data_with_ext(".gxp", reinterpret_cast<const char *>(&program), program.size);
    write_data_with_ext(".dsm", disasm, -1);
    write_data_with_ext(".spt", spirv, -1);
}

std::string load_shader(const SceGxmProgram &program, const FeatureState &features, const char *base_path, const char *title_id) {
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    auto shader_type_to_str = [](SceGxmProgramType type) {
        return type == Vertex ? "vert" : type == Fragment ? "frag" : "unknown";
    };

    SceGxmProgramType program_type = program.get_type();
    const char *shader_type_str = shader_type_to_str(program_type);

    const Sha256HashText hash_text = hex(hash_bytes);

    std::string source = load_shader(hash_text.data(), shader_type_str, base_path);
    if (source.empty()) {
        LOG_INFO("Generating {} shader {}", shader_type_str, hash_text.data());

        std::string spirv_dump;
        std::string disasm_dump;

        source = shader::convert_gxp_to_glsl(program, hash_text.data(), features, false, &spirv_dump, &disasm_dump);

        dump_missing_shader(hash_text.data(), shader_type_str, program, source.c_str(), spirv_dump.c_str(), disasm_dump.c_str(),
            base_path, title_id);
    }

    return source;
}

} // namespace renderer::gl
