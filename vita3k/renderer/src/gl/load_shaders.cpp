#include <renderer/functions.h>
#include <renderer/profile.h>

#include <renderer/gl/functions.h>

#include <gxm/types.h>
#include <renderer/types.h>
#include <shader/spirv_recompiler.h>
#include <util/fs.h>
#include <util/log.h>

#include <utility>

namespace renderer::gl {
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

std::string load_shader(const SceGxmProgram &program, const FeatureState &features, bool maskupdate, const char *base_path, const char *title_id) {
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    auto shader_type_to_str = [](SceGxmProgramType type) {
        return type == SceGxmProgramType::Vertex ? "vert" : type == SceGxmProgramType::Fragment ? "frag" : "unknown";
    };

    SceGxmProgramType program_type = program.get_type();
    const char *shader_type_str = shader_type_to_str(program_type);

    const Sha256HashText hash_text = hex(hash_bytes);

    std::string source = load_shader(hash_text.data(), shader_type_str, base_path);
    if (source.empty()) {
        LOG_INFO("Generating {} shader {}", shader_type_str, hash_text.data());

        std::string spirv_dump;
        std::string disasm_dump;

        const fs::path shader_base_dir{ fs::path("shaderlog") / title_id };
        if (!fs::exists(base_path / shader_base_dir))
            fs::create_directories(base_path / shader_base_dir);

        const auto shader_base_path = fs_utils::construct_file_name(base_path, shader_base_dir, hash_text.data(), ".gxp");

        // Dump gxp binary
        fs::ofstream of{ shader_base_path, fs::ofstream::binary };
        if (!of.fail()) {
            of.write(reinterpret_cast<const char *>(&program), program.size);
            of.close();
        }

        const auto write_data_with_ext = [&](const std::string &ext, const std::string &data) {
            fs::path out_path{ shader_base_path };
            out_path.replace_extension(ext);
            fs::ofstream of{ out_path };
            if (!of.fail()) {
                of << data;
                of.close();
            }
            return true;
        };

        source = shader::convert_gxp_to_glsl(program, hash_text.data(), features, maskupdate, false, write_data_with_ext);
    }

    return source;
}

} // namespace renderer::gl
