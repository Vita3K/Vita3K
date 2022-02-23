// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

bool get_shaders_cache_hashs(GLState &renderer, const char *base_path, const char *title_id, const char *self_name) {
    const auto shaders_path{ fs::path(base_path) / "cache/shaders" / title_id / self_name };
    fs::ifstream shaders_hashs(shaders_path / "hashs.dat", std::ios::in | std::ios::binary);
    if (shaders_hashs.is_open()) {
        renderer.shaders_cache_hashs.clear();
        // Read size of hashes list
        size_t size;
        shaders_hashs.read((char *)&size, sizeof(size));

        // Check version of cache
        uint32_t versionInFile;
        shaders_hashs.read((char *)&versionInFile, sizeof(uint32_t));
        if (versionInFile != shader::CURRENT_VERSION) {
            shaders_hashs.close();
            fs::remove_all(shaders_path);
            fs::remove_all(fs::path(base_path) / "shaderlog" / title_id / self_name);
            LOG_WARN("Current version of cache: {}, is outdated, recreate it.", versionInFile);
            return false;
        }

        // Read Hashs info value
        for (size_t a = 0; a < size; a++) {
            auto read = [&shaders_hashs]() {
                size_t size;

                shaders_hashs.read((char *)&size, sizeof(size));

                std::vector<char> buffer(size);
                shaders_hashs.read(buffer.data(), size);

                return std::string(buffer.begin(), buffer.end());
            };

            ShadersHash hash;
            hash.frag = read();
            hash.vert = read();

            renderer.shaders_cache_hashs.push_back({ hash.frag, hash.vert });
        }

        shaders_hashs.close();
    }

    return !renderer.shaders_cache_hashs.empty();
}

static bool load_shader(const char *hash, const char *extension, const char *base_path, const char *title_id, const char *self_name, char **destination, std::size_t &size_read) {
    const auto shader_path = fs_utils::construct_file_name(base_path, (fs::path("cache/shaders") / title_id / self_name).string().c_str(), hash, extension);
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

template <typename R>
R load_shader_generic(const char *hash_text, const char *base_path, const char *title_id, const char *self_name, const char *shader_type_str) {
    std::size_t read_size = 0;
    R source;

    if (load_shader(hash_text, shader_type_str, base_path, title_id, self_name, nullptr, read_size)) {
        source.resize((read_size + sizeof(typename R::value_type) - 1) / sizeof(typename R::value_type));

        char *dest_pointer = reinterpret_cast<char *>(source.data());
        load_shader(hash_text, shader_type_str, base_path, title_id, self_name, &dest_pointer, read_size);
    }

    return source;
}

template <typename R, typename F>
R load_shader_generic(F genfunc, const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id, const char *self_name, const char *shader_type_str, const std::string &shader_version, bool shader_cache) {
    const Sha256HashText hash_text = get_shader_hash(program);
    // Set Shader Hash with Version
    const std::string hash_hex_ver = shader_version + "-" + static_cast<std::string>(hash_text.data());

    // Todo, spirv shader no can load any shader, is totaly broken
    R source = shader_cache ? load_shader_generic<R>(hash_hex_ver.c_str(), base_path, title_id, self_name, shader_type_str) : R{};

    if (source.empty()) {
        LOG_INFO("Generating {} shader {}", shader_type_str, hash_text.data());

        std::string spirv_dump;
        std::string disasm_dump;

        const fs::path shader_base_dir{ fs::path("shaderlog") / title_id / self_name };
        if (!fs::exists(base_path / shader_base_dir))
            fs::create_directories(base_path / shader_base_dir);

        auto shader_base_path = fs_utils::construct_file_name(base_path, shader_base_dir, hash_hex_ver.c_str(), ".gxp");

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

        // Copy shader generate to shaders cache
        shader_base_path.replace_extension(shader_type_str);
        if (fs::exists(shader_base_path)) {
            try {
                const auto shaders_cache_path = fs::path("cache/shaders") / title_id / self_name;
                if (!fs::exists(base_path / shaders_cache_path))
                    fs::create_directories(base_path / shaders_cache_path);

                const auto shader_dst_path = fs_utils::construct_file_name(base_path, shaders_cache_path, hash_hex_ver.c_str(), shader_type_str);
                fs::copy_file(shader_base_path, shader_dst_path, fs::copy_option::overwrite_if_exists);
                fs::remove(shader_base_path);
            } catch (std::exception &e) {
                LOG_ERROR("Failed to moved shaders file: \n{}", e.what());
            }
        }
    }

    return source;
}

std::string load_glsl_shader(const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id, const char *self_name, const std::string &shader_version, bool shader_cache) {
    SceGxmProgramType program_type = program.get_type();

    auto shader_type_to_str = [](SceGxmProgramType type) {
        return (type == SceGxmProgramType::Vertex) ? "vert" : ((type == SceGxmProgramType::Fragment) ? "frag" : "unknown");
    };

    const char *shader_type_str = shader_type_to_str(program_type);
    return load_shader_generic<std::string>(shader::convert_gxp_to_glsl, program, features, hint_attributes, maskupdate, base_path, title_id, self_name, shader_type_str, shader_version, shader_cache);
}

std::vector<std::uint32_t> load_spirv_shader(const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id, const char *self_name) {
    return load_shader_generic<std::vector<std::uint32_t>>(shader::convert_gxp_to_spirv, program, features, hint_attributes, maskupdate, base_path, title_id, self_name, "spv", "v0", false);
}

std::string pre_load_glsl_shader(const char *hash_text, const char *shader_type_str, const char *base_path, const char *title_id, const char *self_name) {
    return load_shader_generic<std::string>(hash_text, base_path, title_id, self_name, shader_type_str);
}

} // namespace renderer::gl
