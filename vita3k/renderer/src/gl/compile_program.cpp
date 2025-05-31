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

#include <renderer/profile.h>
#include <renderer/shaders.h>
#include <renderer/types.h>

#include <renderer/gl/state.h>
#include <renderer/gl/types.h>

#include <util/log.h>

#include <shader/spirv_recompiler.h>

#include <iomanip>
#include <vector>

namespace renderer::gl {
static SharedGLObject compile_glsl(GLenum type, const std::string &source) {
    R_PROFILE(__func__);

    SharedGLObject shader = std::make_shared<GLObject>();
    if (!shader->init(glCreateShader(type), glDeleteShader)) {
        return SharedGLObject();
    }

    const GLchar *source_glchar = source.c_str();
    const GLint length = static_cast<GLint>(source.length());
    glShaderSource(shader->get(), 1, &source_glchar, &length);

    glCompileShader(shader->get());

    GLint log_length = 0;
    glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);

    // Intel driver returns an info log length of at least 1 even if it is empty.
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());

        LOG_ERROR("{}", log.data());
    }

    GLint is_compiled = GL_FALSE;
    glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
    assert(is_compiled != GL_FALSE);
    if (!is_compiled) {
        return SharedGLObject();
    }

    return shader;
}

static SharedGLObject compile_spirv(GLenum type, const std::vector<std::uint32_t> &source) {
    R_PROFILE(__func__);

    SharedGLObject shader = std::make_shared<GLObject>();
    if (!shader->init(glCreateShader(type), glDeleteShader)) {
        return SharedGLObject();
    }

    const GLchar *source_glchar = reinterpret_cast<const GLchar *>(source.data());
    const GLint length = static_cast<GLint>(source.size() * sizeof(std::uint32_t));

    GLuint need_compile[1] = { shader->get() };
    const char *shader_entry = (type == GL_VERTEX_SHADER) ? "main_vs" : "main_fs";

    glShaderBinary(1, need_compile, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, source_glchar, length);
    glSpecializeShaderARB(need_compile[0], shader_entry, 0, nullptr, nullptr);

    GLint log_length = 0;
    glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);

    // Intel driver returns an info log length of at least 1 even if it is empty.
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());

        LOG_ERROR("{}", log.data());
    }

    GLint is_compiled = GL_FALSE;
    glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
    assert(is_compiled != GL_FALSE);
    if (!is_compiled) {
        return SharedGLObject();
    }

    return shader;
}

static std::string convert_hash_to_hex(const Sha256Hash &hash) {
    std::string str;
    str.reserve(hash.size() * 2);

    for (size_t i = 0; i < hash.size(); ++i) {
        fmt::format_to(std::back_inserter(str), "{:02x}", hash[i]);
    }

    return str;
}

static SharedGLObject compile_program(ProgramCache &program_cache, const SharedGLObject &frag_shader, const SharedGLObject &vert_shader, const ProgramHashes &hashes) {
    SharedGLObject program = std::make_shared<GLObject>();
    if (!program->init(glCreateProgram(), glDeleteProgram)) {
        return SharedGLObject();
    }

    glAttachShader(program->get(), frag_shader->get());
    glAttachShader(program->get(), vert_shader->get());
    glLinkProgram(program->get());

    GLint log_length = 0;
    glGetProgramiv(program->get(), GL_INFO_LOG_LENGTH, &log_length);

    // Intel driver returns an info log length of at least 1 even if it is empty.
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetProgramInfoLog(program->get(), log_length, nullptr, log.data());

        LOG_ERROR("{}\n", log.data());
    }

    GLint is_linked = GL_FALSE;
    glGetProgramiv(program->get(), GL_LINK_STATUS, &is_linked);
    assert(is_linked != GL_FALSE);
    if (is_linked == GL_FALSE) {
        return SharedGLObject();
    }

    glDetachShader(program->get(), frag_shader->get());
    glDetachShader(program->get(), vert_shader->get());

    program_cache.emplace(hashes, program);

    return program;
}

static SharedGLObject compile_shader(const fs::path &shader_cache_path, const std::string &shader_version, const std::string &hash_hex,
    const char *type_str, const GLenum type, ShaderCache &cache, const Sha256Hash &hash) {
    // Set Shader version with hash

    // Load Shader
    const auto shader_name = shader_cache_path / fmt::format("{}-{}.{}", shader_version, hash_hex, type_str);
    const std::string shader = pre_load_shader_glsl(shader_name);
    if (shader.empty()) {
        LOG_WARN("{} shader is empty or not found:\n{}", type_str, hash_hex);
        return SharedGLObject();
    }

    // Compile Shader
    SharedGLObject obj = compile_glsl(type, shader);
    if (!obj) {
        LOG_CRITICAL("Error in compile {} shader:\n{}", type_str, hash_hex);
        return SharedGLObject();
    }

    // Push shader Compiled
    cache.emplace(hash, obj);
    LOG_INFO("{} shader compiled: {}", type_str, hash_hex);

    return obj;
}

static std::vector<ShadersHash>::iterator get_shaders_hash_index(std::vector<ShadersHash> &shaders_cache_hashs, const Sha256Hash &frag_hash, const Sha256Hash &vert_hash) {
    const auto shader_hash_index = std::find_if(shaders_cache_hashs.begin(), shaders_cache_hashs.end(), [&](const ShadersHash &h) {
        return (h.frag == frag_hash) && (h.vert == vert_hash);
    });

    return shader_hash_index;
}

void pre_compile_program(GLState &renderer, const ShadersHash &hash) {
    if (fs::exists(renderer.shaders_path) && !fs::is_empty(renderer.shaders_path)) {
        // Compile Fragment Shader
        const auto frag_hash_hex = convert_hash_to_hex(hash.frag);
        const SharedGLObject frag_shader = compile_shader(renderer.shaders_path, renderer.shader_version,
            frag_hash_hex, "frag", GL_FRAGMENT_SHADER, renderer.fragment_shader_cache, hash.frag);
        if (!frag_shader) {
            return;
        }

        // Compile Vertex Shader
        const auto vert_hash_hex = convert_hash_to_hex(hash.vert);
        const SharedGLObject vert_shader = compile_shader(renderer.shaders_path, renderer.shader_version,
            vert_hash_hex, "vert", GL_VERTEX_SHADER, renderer.vertex_shader_cache, hash.vert);
        if (!vert_shader) {
            return;
        }

        // Compile Program
        const ProgramHashes hashes(hash.frag, hash.vert);
        compile_program(renderer.program_cache, frag_shader, vert_shader, hashes);
        renderer.programs_count_pre_compiled++;
        LOG_INFO("Program Compiled {}/{}", renderer.programs_count_pre_compiled, renderer.shaders_cache_hashs.size());
    }
}

static SharedGLObject get_or_compile_shader(const SceGxmProgram *program, const FeatureState &features, const Sha256Hash &hash,
    ShaderCache &cache, const GLenum type, const shader::Hints &hints, bool shader_cache, bool spirv, bool maskupdate, const fs::path &shader_cache_path, const fs::path &shader_log_path, const std::string &shader_version, uint32_t &shaders_count_compiled) {
    const auto cached = cache.find(hash);
    if (cached == cache.end()) {
        SharedGLObject obj = nullptr;

        // Need to compile new one and add it to cache
        if (features.spirv_shader && spirv) {
            obj = compile_spirv(type, load_spirv_shader(*program, features, false, hints, maskupdate, shader_cache_path, shader_log_path, shader_version + "spv", shader_cache));
        } else {
            obj = compile_glsl(type, load_glsl_shader(*program, features, hints, maskupdate, shader_cache_path, shader_log_path, shader_version, shader_cache));
        }

        cache.emplace(hash, obj);

        shaders_count_compiled++;

        return obj;
    }

    return cached->second;
}

SharedGLObject compile_program(GLState &renderer, GLContext &context, const GxmRecordState &state, const FeatureState &features, const MemState &mem,
    bool shader_cache, bool spirv, bool maskupdate) {
    R_PROFILE(__func__);

    assert(state.fragment_program);
    assert(state.vertex_program);

    const SceGxmVertexProgram &vertex_program_gxm = *state.vertex_program.get(mem);
    const SceGxmFragmentProgram &fragment_program_gxm = *state.fragment_program.get(mem);

    const GLFragmentProgram &fragment_program = *reinterpret_cast<GLFragmentProgram *>(
        fragment_program_gxm.renderer_data.get());

    const GLVertexProgram &vertex_program = *reinterpret_cast<GLVertexProgram *>(
        vertex_program_gxm.renderer_data.get());

    const ProgramHashes hashes(fragment_program.hash, vertex_program.hash);

    // First pass, trying to find the program, since link is costly
    const ProgramCache::const_iterator cached = renderer.program_cache.find(hashes);
    if (cached != renderer.program_cache.end()) {
        return cached->second;
    }

    // No... It doesn't exist. Now we try to find each object. If it doesn't exist then we can kind
    // of compile it again.

    // update the hints
    context.shader_hints.color_format = state.color_surface.colorFormat;
    context.shader_hints.attributes = &vertex_program_gxm.attributes;

    const SharedGLObject fragment_shader = get_or_compile_shader(fragment_program_gxm.program.get(mem), features, fragment_program.hash, renderer.fragment_shader_cache,
        GL_FRAGMENT_SHADER, context.shader_hints, shader_cache, spirv, maskupdate, renderer.shaders_path, renderer.shaders_log_path, renderer.shader_version, renderer.shaders_count_compiled);

    if (!fragment_shader) {
        LOG_CRITICAL("Error in get/compile fragment vertex shader:\n{}", hex_string(fragment_program.hash));
        return SharedGLObject();
    }

    const SharedGLObject vertex_shader = get_or_compile_shader(vertex_program_gxm.program.get(mem), features, vertex_program.hash, renderer.vertex_shader_cache,
        GL_VERTEX_SHADER, context.shader_hints, shader_cache, spirv, maskupdate, renderer.shaders_path, renderer.shaders_log_path, renderer.shader_version, renderer.shaders_count_compiled);

    if (!vertex_shader) {
        LOG_CRITICAL("Error in get/compiled vertex shader:\n{}", hex_string(vertex_program.hash));
        return SharedGLObject();
    }

    SharedGLObject program = compile_program(renderer.program_cache, fragment_shader, vertex_shader, hashes);

    // Save shader cache haches
    const auto shader_cache_hash_index = get_shaders_hash_index(renderer.shaders_cache_hashs, fragment_program.hash, vertex_program.hash);
    if (shader_cache_hash_index == renderer.shaders_cache_hashs.end()) {
        renderer.shaders_cache_hashs.push_back({ fragment_program.hash, vertex_program.hash });
        save_shaders_cache_hashs(renderer, renderer.shaders_cache_hashs);
    }

    return program;
}
} // namespace renderer::gl
