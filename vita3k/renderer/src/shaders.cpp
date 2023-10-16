// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <renderer/profile.h>

#include <renderer/vulkan/state.h>

#include <gxm/types.h>
#include <renderer/state.h>
#include <renderer/types.h>
#include <shader/spirv_recompiler.h>
#include <util/fs.h>
#include <util/log.h>

#include <utility>

namespace renderer {

bool get_shaders_cache_hashs(State &renderer) {
    const auto shaders_path{ fs::path(renderer.cache_path) / "shaders" / renderer.title_id / renderer.self_name };
    const std::string hash_file_name = fmt::format("hashs-{}.dat", (renderer.current_backend == Backend::OpenGL) ? "gl" : "vk");

    fs::ifstream shaders_hashs(shaders_path / hash_file_name, std::ios::in | std::ios::binary);
    if (!shaders_hashs.is_open())
        return false;

    renderer.shaders_cache_hashs.clear();
    // Read size of hashes list
    size_t size;
    shaders_hashs.read((char *)&size, sizeof(size));

    // Check version of cache and device id
    uint32_t versionInFile;
    shaders_hashs.read((char *)&versionInFile, sizeof(uint32_t));
    uint32_t features_mask;
    shaders_hashs.read((char *)&features_mask, sizeof(uint32_t));
    if (versionInFile != shader::CURRENT_VERSION || features_mask != renderer.get_features_mask()) {
        shaders_hashs.close();
        fs::remove_all(shaders_path);
        fs::remove_all(fs::path(renderer.log_path) / "shaderlog" / renderer.title_id / renderer.self_name);
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
    const auto shaders_path{ fs::path(renderer.cache_path) / "shaders" / renderer.title_id / renderer.self_name };
    if (!fs::exists(shaders_path))
        fs::create_directory(shaders_path);
    std::string hash_file_name = fmt::format("hashs-{}.dat", (renderer.current_backend == Backend::OpenGL) ? "gl" : "vk");
    fs::ofstream shaders_hashs(shaders_path / hash_file_name, std::ios::out | std::ios::binary);

    if (shaders_hashs.is_open()) {
        // Write Size of shaders cache hashes list
        const auto size = shaders_cache_hashs.size();
        shaders_hashs.write((char *)&size, sizeof(size));

        // Write version of cache
        const uint32_t versionInFile = shader::CURRENT_VERSION;
        shaders_hashs.write((char *)&versionInFile, sizeof(uint32_t));
        const uint32_t features_mask = renderer.get_features_mask();
        shaders_hashs.write((char *)&features_mask, sizeof(uint32_t));

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

static bool load_shader(const char *hash, const char *extension, const char *cache_path, const char *title_id, const char *self_name, char **destination, std::size_t &size_read) {
    const auto shader_path = fs_utils::construct_file_name(cache_path, (fs::path("shaders") / title_id / self_name).string().c_str(), hash, extension);
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

static const Sha256Hash get_shader_hash(const SceGxmProgram &program) {
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    return hash_bytes;
}

template <typename R>
R load_shader_generic(const char *hash_text, const char *cache_path, const char *title_id, const char *self_name, const char *shader_type_str) {
    std::size_t read_size = 0;
    R source;

    if (load_shader(hash_text, shader_type_str, cache_path, title_id, self_name, nullptr, read_size)) {
        source.resize((read_size + sizeof(typename R::value_type) - 1) / sizeof(typename R::value_type));

        char *dest_pointer = reinterpret_cast<char *>(source.data());
        load_shader(hash_text, shader_type_str, cache_path, title_id, self_name, &dest_pointer, read_size);
    }

    return source;
}

shader::GeneratedShader load_shader_generic(shader::Target target, const SceGxmProgram &program, const FeatureState &features, const shader::Hints &hints, bool maskupdate, const char *cache_path, const char *title_id, const char *self_name, const char *shader_type_str, const std::string &shader_version, bool shader_cache) {
    // TODO: no need to recompute the hash here
    const std::string hash_text = hex_string(get_shader_hash(program));
    // Set Shader Hash with Version
    const std::string hash_hex_ver = shader_version + "-" + static_cast<std::string>(hash_text.data());

    if (shader_cache) {
        if (target == shader::Target::GLSLOpenGL) {
            std::string source = load_shader_generic<std::string>(hash_hex_ver.c_str(), cache_path, title_id, self_name, shader_type_str);
            if (!source.empty()) {
                return { source, std::vector<uint32_t>() };
            }
        } else {
            std::vector<uint32_t> source = load_shader_generic<std::vector<uint32_t>>(hash_hex_ver.c_str(), cache_path, title_id, self_name, shader_type_str);
            if (!source.empty())
                return { "", source };
        }
    }

    LOG_INFO("Generating {} shader {}", shader_type_str, hash_text.data());

    std::string spirv_dump;
    std::string disasm_dump;

    const fs::path shader_base_dir{ fs::path("shaderlog") / title_id / self_name };
    if (!fs::exists(cache_path / shader_base_dir))
        fs::create_directories(cache_path / shader_base_dir);

    auto shader_cache_path = fs_utils::construct_file_name(cache_path, shader_base_dir, hash_hex_ver.c_str(), ".gxp");

    // Dump gxp binary
    fs::ofstream of{ shader_cache_path, fs::ofstream::binary };
    if (!of.fail()) {
        of.write(reinterpret_cast<const char *>(&program), program.size);
        of.close();
    }

    const auto write_data_with_ext = [&](const std::string &ext, const std::string &data) {
        fs::path out_path{ shader_cache_path };
        out_path.replace_extension(ext);
        fs::ofstream of{ out_path };
        if (!of.fail()) {
            of << data;
            of.close();
        }
        return true;
    };

    shader::GeneratedShader source = shader::convert_gxp(program, hash_text.data(), features, target, hints, maskupdate, false, write_data_with_ext);

    // Copy shader generate to shaders cache
    const auto shaders_cache_path = fs::path("shaders") / title_id / self_name;
    if (!fs::exists(cache_path / shaders_cache_path))
        fs::create_directories(cache_path / shaders_cache_path);

    if (target == shader::Target::GLSLOpenGL) {
        shader_cache_path.replace_extension(shader_type_str);
        if (fs::exists(shader_cache_path)) {
            try {
                const auto shader_dst_path = fs_utils::construct_file_name(cache_path, shaders_cache_path, hash_hex_ver.c_str(), shader_type_str);
                fs::copy_file(shader_cache_path, shader_dst_path, fs::copy_options::overwrite_existing);
                fs::remove(shader_cache_path);
            } catch (std::exception &e) {
                LOG_ERROR("Failed to moved shaders file: \n{}", e.what());
            }
        }
    } else {
        const auto shader_dst_path = fs_utils::construct_file_name(cache_path, shaders_cache_path, hash_hex_ver.c_str(), "spv");
        fs::ofstream of{ shader_dst_path, fs::ofstream::binary };
        if (!of.fail()) {
            of.write(reinterpret_cast<const char *>(source.spirv.data()), sizeof(uint32_t) * source.spirv.size());
            of.close();
        }
    }

    return source;
}

std::string load_glsl_shader(const SceGxmProgram &program, const FeatureState &features, const shader::Hints &hints, bool maskupdate, const char *cache_path, const char *title_id, const char *self_name, const std::string &shader_version, bool shader_cache) {
    SceGxmProgramType program_type = program.get_type();

    auto shader_type_to_str = [](SceGxmProgramType type) {
        return (type == SceGxmProgramType::Vertex) ? "vert" : ((type == SceGxmProgramType::Fragment) ? "frag" : "unknown");
    };

    const char *shader_type_str = shader_type_to_str(program_type);
    return load_shader_generic(shader::Target::GLSLOpenGL, program, features, hints, maskupdate, cache_path, title_id, self_name, shader_type_str, shader_version, shader_cache).glsl;
}

std::vector<uint32_t> load_spirv_shader(const SceGxmProgram &program, const FeatureState &features, bool is_vulkan, const shader::Hints &hints, bool maskupdate, const char *cache_path, const char *title_id, const char *self_name, const std::string &shader_version, bool shader_cache) {
    const shader::Target target = is_vulkan ? shader::Target::SpirVVulkan : shader::Target::SpirVOpenGL;
    auto shader_type_to_str = [](SceGxmProgramType type) {
        return (type == SceGxmProgramType::Vertex) ? "vert.spv.txt" : ((type == SceGxmProgramType::Fragment) ? "frag.spv.txt" : "unknown.spv.txt");
    };
    const char *shader_type_str = shader_type_to_str(program.get_type());

    return load_shader_generic(target, program, features, hints, maskupdate, cache_path, title_id, self_name, shader_type_str, shader_version, shader_cache).spirv;
}

std::string pre_load_shader_glsl(const char *hash_text, const char *shader_type_str, const char *cache_path, const char *title_id, const char *self_name) {
    return load_shader_generic<std::string>(hash_text, cache_path, title_id, self_name, shader_type_str);
}

std::vector<uint32_t> pre_load_shader_spirv(const char *hash_text, const char *shader_type_str, const char *cache_path, const char *title_id, const char *self_name) {
    return load_shader_generic<std::vector<uint32_t>>(hash_text, cache_path, title_id, self_name, shader_type_str);
}

} // namespace renderer
