// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <gxm/types.h>
#include <util/log.h>

#include <shader/spirv_recompiler.h>

#include <gxm/functions.h>
#include <vector>

namespace renderer::gl {
static SharedGLObject compile_glsl(GLenum type, const std::string &source) {
    R_PROFILE(__func__);

    const SharedGLObject shader = std::make_shared<GLObject>();
    if (!shader->init(glCreateShader(type), glDeleteShader)) {
        return SharedGLObject();
    }

    const GLchar *source_glchar = static_cast<const GLchar *>(source.c_str());
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

    const SharedGLObject shader = std::make_shared<GLObject>();
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

static SharedGLObject get_or_compile_shader(const SceGxmProgram *program, const FeatureState &features, const std::string &hash,
    ShaderCache &cache, const GLenum type, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool spirv, bool maskupdate, const char *base_path, const char *title_id, uint32_t &shaders_count_compiled) {
    const auto cached = cache.find(hash);
    if (cached == cache.end()) {
        SharedGLObject obj = nullptr;

        // Need to compile new one and add it to cache
        if (features.spirv_shader && spirv) {
            obj = compile_spirv(type, load_spirv_shader(*program, features, hint_attributes, maskupdate, base_path, title_id));
        } else {
            obj = compile_glsl(type, load_glsl_shader(*program, features, hint_attributes, maskupdate, base_path, title_id));
        }

        cache.emplace(hash, obj);

        shaders_count_compiled++;

        return obj;
    }

    return cached->second;
}

SharedGLObject compile_program(ProgramCache &program_cache, ShaderCache &vertex_cache, ShaderCache &fragment_cache,
    const GxmRecordState &state, const FeatureState &features, const MemState &mem, bool spirv, bool maskupdate, const char *base_path, const char *title_id, uint32_t &shaders_count_compiled) {
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
    const ProgramCache::const_iterator cached = program_cache.find(hashes);
    if (cached != program_cache.end()) {
        return cached->second;
    }

    // No... It doesn't exist. Now we try to find each object. If it doesn't exist then we can kind
    // of compile it again.
    const SharedGLObject fragment_shader = get_or_compile_shader(fragment_program_gxm.program.get(mem),
        features, fragment_program.hash, fragment_cache, GL_FRAGMENT_SHADER, nullptr, spirv, maskupdate, base_path, title_id, shaders_count_compiled);

    if (!fragment_shader) {
        LOG_CRITICAL("Error in get/compile fragment vertex shader:\n{}", vertex_program.hash);
        return SharedGLObject();
    }

    const SharedGLObject vertex_shader = get_or_compile_shader(vertex_program_gxm.program.get(mem),
        features, vertex_program.hash, vertex_cache, GL_VERTEX_SHADER, &vertex_program_gxm.attributes, spirv, maskupdate, base_path, title_id, shaders_count_compiled);

    if (!vertex_shader) {
        LOG_CRITICAL("Error in get/compiled vertex shader:\n{}", vertex_program.hash);
        return SharedGLObject();
    }

    const SharedGLObject program = std::make_shared<GLObject>();
    if (!program->init(glCreateProgram(), glDeleteProgram)) {
        return SharedGLObject();
    }

    glAttachShader(program->get(), fragment_shader->get());
    glAttachShader(program->get(), vertex_shader->get());
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

    glDetachShader(program->get(), fragment_shader->get());
    glDetachShader(program->get(), vertex_shader->get());

    program_cache.emplace(hashes, program);

    return program;
}
} // namespace renderer::gl
