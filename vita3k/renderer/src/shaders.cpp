// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <renderer/shaders.h>

#include <renderer/vulkan/state.h>

#include <gxm/types.h>
#include <renderer/state.h>
#include <renderer/types.h>
#include <shader/spirv_recompiler.h>
#include <util/fs.h>
#include <util/log.h>

#include <string>
#include <vector>

namespace renderer {

bool get_shaders_cache_hashs(State &renderer) {
    const std::string hash_file_name = fmt::format("hashs-{}.dat", (renderer.current_backend == Backend::OpenGL) ? "gl" : "vk");

    fs::ifstream shaders_hashs(renderer.shaders_path / hash_file_name, std::ios::in | std::ios::binary);
    if (!shaders_hashs.is_open())
        return false;

    renderer.shaders_cache_hashs.clear();
    // Read size of hashes list
    size_t size;
    shaders_hashs.read((char *)&size, sizeof(size));

    // Check version of cache
    uint32_t versionInFile;
    shaders_hashs.read((char *)&versionInFile, sizeof(uint32_t));
    uint32_t features_mask;
    shaders_hashs.read((char *)&features_mask, sizeof(uint32_t));
    if (versionInFile != shader::CURRENT_VERSION || features_mask != renderer.get_features_mask()) {
        shaders_hashs.close();
        fs::remove_all(renderer.shaders_path);
        fs::remove_all(renderer.shaders_log_path);
        if (versionInFile != shader::CURRENT_VERSION)
            LOG_WARN("Current version of cache: {}, is outdated, recreate it.", versionInFile);
        else
            LOG_WARN("Incompatible GPU features enabled, recreating shader cache");
        return false;
    }

    if (renderer.current_backend == Backend::Vulkan) {
        // Read the pipeline cache
        dynamic_cast<vulkan::VKState &>(renderer).pipeline_cache.read_pipeline_cache();
    }

    // Read Hashs info value
    for (size_t a = 0; a < size; a++) {
        auto read = [&shaders_hashs]() {
            Sha256Hash hash;

            shaders_hashs.read(reinterpret_cast<char *>(hash.data()), sizeof(Sha256Hash));

            return hash;
        };

        ShadersHash hash;
        hash.frag = read();
        hash.vert = read();

        renderer.shaders_cache_hashs.push_back({ hash.frag, hash.vert });
    }

    shaders_hashs.close();

    return !renderer.shaders_cache_hashs.empty();
}

void save_shaders_cache_hashs(State &renderer, std::vector<ShadersHash> &shaders_cache_hashs) {
    fs::create_directories(renderer.shaders_path);
    std::string hash_file_name = fmt::format("hashs-{}.dat", (renderer.current_backend == Backend::OpenGL) ? "gl" : "vk");
    fs::ofstream shaders_hashs(renderer.shaders_path / hash_file_name, std::ios::out | std::ios::binary);

    if (shaders_hashs.is_open()) {
        // Write Size of shaders cache hashes list
        const auto size = shaders_cache_hashs.size();
        shaders_hashs.write((const char *)&size, sizeof(size));

        // Write version of cache
        const uint32_t versionInFile = shader::CURRENT_VERSION;
        shaders_hashs.write((const char *)&versionInFile, sizeof(uint32_t));
        const uint32_t features_mask = renderer.get_features_mask();
        shaders_hashs.write((const char *)&features_mask, sizeof(uint32_t));

        // Write shader hash list
        for (const auto &hash : shaders_cache_hashs) {
            auto write = [&shaders_hashs](const Sha256Hash &hash) {
                shaders_hashs.write(reinterpret_cast<const char *>(hash.data()), sizeof(Sha256Hash));
            };

            write(hash.frag);
            write(hash.vert);
        }
        shaders_hashs.close();
    }
}

static bool load_shader(const fs::path &shader_name, char **destination, std::size_t &size_read) {
    fs::ifstream is(shader_name, fs::ifstream::binary);
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

static Sha256Hash get_shader_hash(const SceGxmProgram &program) {
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    return hash_bytes;
}

template <typename R>
static R load_shader_generic(const fs::path &shader_path) {
    std::size_t read_size = 0;
    R source;

    if (load_shader(shader_path, nullptr, read_size)) {
        source.resize((read_size + sizeof(typename R::value_type) - 1) / sizeof(typename R::value_type));

        char *dest_pointer = reinterpret_cast<char *>(source.data());
        load_shader(shader_path, &dest_pointer, read_size);
    }

    return source;
}

static shader::GeneratedShader load_shader_generic(shader::Target target, const SceGxmProgram &program, const FeatureState &features, const shader::Hints &hints, bool maskupdate, const fs::path &shader_cache_path, const fs::path &shaderlog_path, const char *shader_type_str, const std::string &shader_version, bool shader_cache) {
    // TODO: no need to recompute the hash here
    const std::string hash_text = hex_string(get_shader_hash(program));
    // Set Shader Hash with Version
    const std::string hash_hex_ver = fmt::format("{}-{}", shader_version, hash_text);
    const auto get_shader_path = [&](const char *ext) {
        return shader_cache_path / fmt::format("{}.{}", hash_hex_ver, ext);
    };
    const auto get_shaderlog_path = [&](const char *ext) {
        return shaderlog_path / fmt::format("{}.{}", hash_hex_ver, ext);
    };

    const auto shader_path = get_shader_path(shader_type_str);
    if (shader_cache) {
        if (target == shader::Target::GLSLOpenGL) {
            std::string source = load_shader_generic<std::string>(shader_path);
            if (!source.empty()) {
                return { source, std::vector<uint32_t>() };
            }
        } else {
            std::vector<uint32_t> source = load_shader_generic<std::vector<uint32_t>>(get_shader_path("spv"));
            if (!source.empty())
                return { "", source };
        }
    }

    LOG_INFO("Generating {} shader {}", shader_type_str, hash_text);

    fs::create_directories(shaderlog_path);

    auto shader_log_path = get_shaderlog_path("gxp");

    // Dump gxp binary
    fs_utils::dump_data(shader_log_path, &program, program.size);
    const auto write_data_with_ext = [&](const std::string &ext, const std::string &data) {
        fs::path out_path;
        if (ext == shader_type_str) {
            out_path = shader_path;
        } else {
            out_path = shader_log_path;
            out_path.replace_extension(ext);
        }
        fs_utils::dump_data(out_path, data.c_str(), data.size());
        return true;
    };

    shader::GeneratedShader source = shader::convert_gxp(program, hash_text, features, target, hints, maskupdate, false, write_data_with_ext);

    // Copy shader generate to shaders cache
    const auto shaders_cache_path = fs::path(shader_cache_path);
    fs::create_directories(shaders_cache_path);

    if (target != shader::Target::GLSLOpenGL) {
        const auto shader_dst_path = get_shader_path("spv");
        fs_utils::dump_data(shader_dst_path, source.spirv.data(), sizeof(uint32_t) * source.spirv.size());
    }

    return source;
}

std::string load_glsl_shader(const SceGxmProgram &program, const FeatureState &features, const shader::Hints &hints, bool maskupdate, const fs::path &shader_cache_path, const fs::path &shader_log_path, const std::string &shader_version, bool shader_cache) {
    SceGxmProgramType program_type = program.get_type();

    auto shader_type_to_str = [](SceGxmProgramType type) {
        return (type == SceGxmProgramType::Vertex) ? "vert" : ((type == SceGxmProgramType::Fragment) ? "frag" : "unknown");
    };

    const char *shader_type_str = shader_type_to_str(program_type);

    return load_shader_generic(shader::Target::GLSLOpenGL, program, features, hints, maskupdate, shader_cache_path, shader_log_path, shader_type_str, shader_version, shader_cache).glsl;
}

std::vector<uint32_t> load_spirv_shader(const SceGxmProgram &program, const FeatureState &features, bool is_vulkan, const shader::Hints &hints, bool maskupdate, const fs::path &shader_cache_path, const fs::path &shader_log_path, const std::string &shader_version, bool shader_cache) {
    const shader::Target target = is_vulkan ? shader::Target::SpirVVulkan : shader::Target::SpirVOpenGL;
    auto shader_type_to_str = [](SceGxmProgramType type) {
        return (type == SceGxmProgramType::Vertex) ? "vert.spv.txt" : ((type == SceGxmProgramType::Fragment) ? "frag.spv.txt" : "unknown.spv.txt");
    };
    const char *shader_type_str = shader_type_to_str(program.get_type());

    return load_shader_generic(target, program, features, hints, maskupdate, shader_cache_path, shader_log_path, shader_type_str, shader_version, shader_cache).spirv;
}

std::string pre_load_shader_glsl(const fs::path &shader_path) {
    return load_shader_generic<std::string>(shader_path);
}

std::vector<uint32_t> pre_load_shader_spirv(const fs::path &shader_path) {
    return load_shader_generic<std::vector<uint32_t>>(shader_path);
}

} // namespace renderer
