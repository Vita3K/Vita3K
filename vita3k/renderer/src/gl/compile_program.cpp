#include <renderer/profile.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <gxm/types.h>
#include <util/log.h>

#include <gxm/functions.h>
#include <vector>

namespace renderer {
namespace gl {
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

static void bind_attribute_locations(GLuint gl_program, const GLVertexProgram &program) {
    R_PROFILE(__func__);

    for (const AttributeLocations::value_type &binding : program.attribute_locations) {
        glBindAttribLocation(gl_program, binding.first / sizeof(uint32_t), binding.second.c_str());
    }
}

static void bind_uniform_block_locations(GLuint gl_program, const SceGxmProgram &program) {
    std::string name_type = (program.is_vertex() ? "vert" : "frag");

    for (uint16_t buffer_index = 0; buffer_index < SCE_GXM_REAL_MAX_UNIFORM_BUFFER; buffer_index++) {
        const SceGxmProgramParameterContainer *container = gxp::get_container_by_index(program, buffer_index);

        if (!container)
            continue;

        uint32_t binding = buffer_index + 1;
        std::string buffer_name;

        if (buffer_index == 14) {
            buffer_name = fmt::format("{}_defaultUniformBuffer", name_type);
            binding = 0;
        } else {
            buffer_name = fmt::format("{}_buffer{}", name_type, binding);
        }

        if (program.is_fragment())
            binding += SCE_GXM_REAL_MAX_UNIFORM_BUFFER;

        GLuint index = glGetUniformBlockIndex(gl_program, buffer_name.c_str());

        if (index == GL_INVALID_INDEX) {
            LOG_ERROR("Missing buffer {} for container index {}.", buffer_name, buffer_index);
            continue;
        }

        glUniformBlockBinding(gl_program, index, binding);
    }
}

static SharedGLObject get_or_compile_shader(const SceGxmProgram *program, const FeatureState &features, const std::string &hash,
    ShaderCache &cache, const GLenum type, const char *base_path, const char *title_id) {
    const auto cached = cache.find(hash);
    if (cached == cache.end()) {
        // Need to compile new one and add it to cache
        auto obj = compile_glsl(type, load_shader(*program, features, base_path, title_id));
        cache.emplace(hash, obj);

        return obj;
    }

    return cached->second;
}

SharedGLObject compile_program(ProgramCache &program_cache, ShaderCache &vertex_cache, ShaderCache &fragment_cache,
    const GxmContextState &state, const FeatureState &features, const MemState &mem, const char *base_path, const char *title_id) {
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
        features, fragment_program.hash, fragment_cache, GL_FRAGMENT_SHADER, base_path, title_id);

    if (!fragment_shader) {
        LOG_CRITICAL("Error in get/compile fragment vertex shader:\n{}", vertex_program.hash);
        return SharedGLObject();
    }

    const SharedGLObject vertex_shader = get_or_compile_shader(vertex_program_gxm.program.get(mem),
        features, vertex_program.hash, vertex_cache, GL_VERTEX_SHADER, base_path, title_id);

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

    bind_attribute_locations(program->get(), vertex_program);

    glLinkProgram(program->get());

    if (!features.use_shader_binding) {
        bind_uniform_block_locations(program->get(), *vertex_program_gxm.program.get(mem));
        bind_uniform_block_locations(program->get(), *fragment_program_gxm.program.get(mem));
    }

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
} // namespace gl
} // namespace renderer
