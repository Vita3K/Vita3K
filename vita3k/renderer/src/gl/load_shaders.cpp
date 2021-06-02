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
static bool load_shader(const char *hash, const char *extension, const char *base_path, const char *title_id, char **destination, std::size_t &size_read) {
    const auto shader_path = fs_utils::construct_file_name(base_path, (fs::path("shaders") / title_id).string().c_str(), hash, extension);
    fs::ifstream is(shader_path, fs::ifstream::binary);
    if (!is) {
        return false;
    }

    is.seekg(0, fs::ifstream::end);
    size_read = is.tellg();
    is.seekg(0);

    if (size_read == 0) {
        return false;
    }

    if (destination == nullptr) {
        return true;
    }

    is.read(*destination, size_read);
    return true;
}

static const Sha256HashText get_shader_hash(const SceGxmProgram &program) {
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    return hex(hash_bytes);
}

template <typename R, typename F>
R load_shader_generic(F genfunc, const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id, const char *shader_type_str) {
    const Sha256HashText hash_text = get_shader_hash(program);

    std::size_t read_size = 0;
    R source;

    if (load_shader(hash_text.data(), shader_type_str, base_path, title_id, nullptr, read_size)) {
        source.resize((read_size + sizeof(typename R::value_type) - 1) / sizeof(typename R::value_type));

        char *dest_pointer = reinterpret_cast<char *>(source.data());
        load_shader(hash_text.data(), shader_type_str, base_path, title_id, &dest_pointer, read_size);
    }

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

        source = genfunc(program, hash_text.data(), features, hint_attributes, maskupdate, false, write_data_with_ext);
    }

    return source;
}

std::string load_glsl_shader(const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id) {
    SceGxmProgramType program_type = program.get_type();

    auto shader_type_to_str = [](SceGxmProgramType type) {
        return (type == SceGxmProgramType::Vertex) ? "vert" : ((type == SceGxmProgramType::Fragment) ? "frag" : "unknown");
    };

    const char *shader_type_str = shader_type_to_str(program_type);
    return load_shader_generic<std::string>(shader::convert_gxp_to_glsl, program, features, hint_attributes, maskupdate, base_path, title_id, shader_type_str);
}

std::vector<std::uint32_t> load_spirv_shader(const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id) {
    return load_shader_generic<std::vector<std::uint32_t>>(shader::convert_gxp_to_spirv, program, features, hint_attributes, maskupdate, base_path, title_id, "spv");
}

} // namespace renderer::gl
