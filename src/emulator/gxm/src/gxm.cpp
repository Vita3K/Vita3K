#include <gxm/functions.h>

#include <crypto/hash.h>
#include <gxm/types.h>
#include <util/log.h>

#include <glbinding/AbstractFunction.h>
#include <glbinding/FunctionCall.h>
#include <microprofile.h>

#include <cstring> // memcmp
#include <fstream>
#include <sstream>

#define GXM_PROFILE(name) MICROPROFILE_SCOPEI("GXM", name, MP_BLUE)

static std::string load_shader(const char *hash, const char *base_path) {
    std::ostringstream path;
    path << base_path << "shaders/" << hash << ".glsl";
    
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

static const SceGxmProgramParameter *program_parameters(const SceGxmProgram &program) {
    return reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program.parameters_offset) + program.parameters_offset);
}

static const char *parameter_name(const SceGxmProgramParameter &parameter) {
    const uint8_t *const bytes = reinterpret_cast<const uint8_t *>(&parameter);
    return reinterpret_cast<const char *>(bytes + parameter.name_offset);
}

static const char *scalar_type(SceGxmParameterType type) {
    switch (type) {
        case SCE_GXM_PARAMETER_TYPE_F32: return "float";
        case SCE_GXM_PARAMETER_TYPE_U32: return "uint";
        case SCE_GXM_PARAMETER_TYPE_S32: return "int";
    }
    
    return "?";
}

static const char *vector_prefix(SceGxmParameterType type) {
    switch (type) {
        case SCE_GXM_PARAMETER_TYPE_F32: return "";
        case SCE_GXM_PARAMETER_TYPE_U32: return "u";
        case SCE_GXM_PARAMETER_TYPE_S32: return "i";
    }
    
    return "?";
}

static void output_scalar_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count == 1);
    
    glsl << scalar_type(static_cast<SceGxmParameterType>(parameter.type)) << " " << parameter_name(parameter);
    if (parameter.array_size != 1) {
        glsl << "[" << parameter.array_size << "]";
    }
}

static void output_vector_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count >= 2);
    assert(parameter.component_count <= 4);
    
    glsl << vector_prefix(static_cast<SceGxmParameterType>(parameter.type)) << "vec" << parameter.component_count << " " << parameter_name(parameter);
    if (parameter.array_size != 1) {
        glsl << "[" << parameter.array_size << "]";
    }
}

static void output_matrix_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count >= 2);
    assert(parameter.array_size >= 2);
    assert(parameter.array_size <= 4);
    
    glsl << vector_prefix(static_cast<SceGxmParameterType>(parameter.type)) << "mat";
    if (parameter.component_count == parameter.array_size) {
        glsl << parameter.component_count;
    } else {
        glsl << parameter.component_count << "x" << parameter.array_size;
    }
    glsl << " " << parameter_name(parameter);
}

static void output_glsl_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    if (parameter.component_count >= 2) {
        if ((parameter.array_size >= 2) &&
            (parameter.array_size <= 4)) {
            output_matrix_decl(glsl, parameter);
        } else {
            output_vector_decl(glsl, parameter);
        }
    } else {
        output_scalar_decl(glsl, parameter);
    }
}

static void output_glsl_parameters(std::ostream &glsl, const SceGxmProgram &program) {
    if (program.parameter_count > 0) {
        glsl << "\n";
    }
    
    const SceGxmProgramParameter *const parameters = program_parameters(program);
    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        switch (static_cast<SceGxmParameterCategory>(parameter.category)) {
            case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE:
                glsl << "attribute ";
                output_glsl_decl(glsl, parameter);
                break;
            case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
                glsl << "uniform ";
                output_glsl_decl(glsl, parameter);
                break;
            case SCE_GXM_PARAMETER_CATEGORY_SAMPLER:
                assert(parameter.component_count == 4);
                glsl << "uniform sampler2D " << parameter_name(parameter);
                break;
            case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE:
                assert(parameter.component_count == 0);
                glsl << "auxiliary_surface";
                break;
            case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER:
                assert(parameter.component_count == 0);
                glsl << "uniform_buffer";
                break;
        }
        glsl << ";\n";
    }
}

static std::string generate_fragment_glsl(const SceGxmProgram &program) {
    GXM_PROFILE(__FUNCTION__);
    
    std::ostringstream glsl;
    glsl << "// Fragment shader.\n";
    glsl << "#version 120\n";
    output_glsl_parameters(glsl, program);
    glsl << "\n";
    glsl << "void main() {\n";
    glsl << "    gl_FragColor = vec4(1, 0, 1, 1);\n";
    glsl << "}\n";
    
    return glsl.str();
}

static std::string generate_vertex_glsl(const SceGxmProgram &program) {
    GXM_PROFILE(__FUNCTION__);
    
    std::ostringstream glsl;
    glsl << "// Vertex shader.\n";
    glsl << "#version 120\n";
    output_glsl_parameters(glsl, program);
    glsl << "\n";
    glsl << "void main() {\n";
    glsl << "    gl_Position = vec4(0, 0, 0, 1);\n";
    glsl << "}\n";
    
    return glsl.str();
}

static void dump_missing_shader(const char *hash, const SceGxmProgram &program, const char *source) {
    // Dump missing shader GLSL.
    std::ostringstream glsl_path;
    glsl_path << hash << ".glsl";
    std::ofstream glsl_file(glsl_path.str());
    if (!glsl_file.fail()) {
        glsl_file << source;
        glsl_file.close();
    }
    
    // Dump missing shader binary.
    std::ostringstream gxp_path;
    gxp_path << hash << ".gxp";
    std::ofstream gxp(gxp_path.str(), std::ofstream::binary);
    if (!gxp.fail()) {
        gxp.write(reinterpret_cast<const char *>(&program), program.size);
        gxp.close();
    }
}

static SharedGLObject compile_glsl(GLenum type, const GLchar *source) {
    GXM_PROFILE(__FUNCTION__);
    
    const SharedGLObject shader = std::make_shared<GLObject>();
    if (!shader->init(glCreateShader(type), glDeleteShader)) {
        return SharedGLObject();
    }
    
    const GLint length = static_cast<GLint>(strlen(source));
    glShaderSource(shader->get(), 1, &source, &length);
    
    glCompileShader(shader->get());
    
    GLint log_length = 0;
    glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);
    
    if (log_length > 0) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());
        
        LOG_ERROR("{}", log.data());
    }
    
    GLboolean is_compiled = GL_FALSE;
    glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
    assert(is_compiled != GL_FALSE);
    if (!is_compiled) {
        return SharedGLObject();
    }
    
    return shader;
}

static void bind_attribute_locations(GLuint gl_program, const SceGxmVertexProgram &program) {
    GXM_PROFILE(__FUNCTION__);
    
    for (const AttributeLocations::value_type &binding : program.attribute_locations) {
        glBindAttribLocation(gl_program, binding.first, binding.second.c_str());
    }
}

template <class T>
void uniform_4(GLint location, GLsizei count, const T *value);

template <>
void uniform_4<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
    glUniform4fv(location, count, value);
}

template <class T>
void uniform_matrix_4(GLint location, GLsizei count, GLboolean, const T *value);

template <>
void uniform_matrix_4<GLfloat>(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glUniformMatrix4fv(location, count, transpose, value);
}

template <class T>
void set_uniform(GLint location, size_t component_count, GLsizei array_size, const T *value) {
    switch (component_count) {
        case 4:
            switch (array_size) {
                case 4:
                    uniform_matrix_4<T>(location, 1, GL_FALSE, value);
                    break;
                default:
                    uniform_4<T>(location, array_size, value);
                    break;
            }
            break;
            
        default:
            LOG_WARN("Unhandled uniform component count {}.", component_count);
            break;
    }
}

static void set_uniforms(GLuint gl_program, const UniformBuffers &uniform_buffers, const SceGxmProgram &gxm_program, const MemState &mem) {
    GXM_PROFILE(__FUNCTION__);
    
    const SceGxmProgramParameter *const parameters = program_parameters(gxm_program);
    for (size_t i = 0; i < gxm_program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
            continue;
        }
        
        const char *const name = parameter_name(parameter);
        const GLint location = glGetUniformLocation(gl_program, name);
        if (location < 0) {
            LOG_WARN("Uniform parameter {} not found in current OpenGL program.", name);
            continue;
        }
        
        const SceGxmParameterType type = static_cast<SceGxmParameterType>(parameter.type);
        const Ptr<const void> uniform_buffer = uniform_buffers[parameter.container_index];
        if (!uniform_buffer) {
            LOG_WARN("Uniform buffer {} not set for parameter {}.", parameter.container_index, name);
            continue;
        }
        
        const uint8_t *const base = static_cast<const uint8_t *>(uniform_buffer.get(mem));
        const GLfloat *const src = reinterpret_cast<const GLfloat *>(base + parameter.resource_index * 4); // TODO What offset?
        switch (type) {
            case SCE_GXM_PARAMETER_TYPE_F32:
                set_uniform<GLfloat>(location, parameter.component_count, parameter.array_size, src);
                break;
            default:
                LOG_WARN("Type {} not handled for uniform parameter {}.", type, name);
                break;
        }
    }
}

static bool operator<(const SceGxmRegisteredProgram &a, const SceGxmRegisteredProgram &b) {
    return a.program < b.program;
}

static bool operator<(const emu::SceGxmBlendInfo &a, const emu::SceGxmBlendInfo &b) {
    return memcmp(&a, &b, sizeof(a)) < 0;
}

void before_callback(const glbinding::FunctionCall &fn) {
#if MICROPROFILE_ENABLED
    const MicroProfileToken token = MicroProfileGetToken("OpenGL", fn.function->name(), MP_CYAN, MicroProfileTokenTypeCpu);
    MICROPROFILE_ENTER_TOKEN(token);
#endif // MICROPROFILE_ENABLED
}

void after_callback(const glbinding::FunctionCall &fn) {
    MICROPROFILE_LEAVE();
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
        std::stringstream gl_error;
        gl_error << error;
        LOG_ERROR("OpenGL: {} set error {}.", fn.function->name(), gl_error.str());
        assert(false);
    }
}

std::string get_fragment_glsl(SceGxmShaderPatcher &shader_patcher, const SceGxmProgram &fragment_program, const char *base_path) {
    const Sha256Hash hash_bytes = sha256(&fragment_program, fragment_program.size);
    const GLSLCache::const_iterator cached = shader_patcher.fragment_glsl_cache.find(hash_bytes);
    if (cached != shader_patcher.fragment_glsl_cache.end()) {
        return cached->second;
    }
    
    const std::array<char, 65> hash_text = hex(hash_bytes);
    std::string source = load_shader(hash_text.data(), base_path);
    if (source.empty()) {
        source = generate_fragment_glsl(fragment_program);
        dump_missing_shader(hash_text.data(), fragment_program, source.c_str());
    }
    
    shader_patcher.fragment_glsl_cache.emplace(hash_bytes, source);
    
    return source;
}

std::string get_vertex_glsl(SceGxmShaderPatcher &shader_patcher, const SceGxmProgram &vertex_program, const char *base_path) {
    const Sha256Hash hash_bytes = sha256(&vertex_program, vertex_program.size);
    const GLSLCache::const_iterator cached = shader_patcher.vertex_glsl_cache.find(hash_bytes);
    if (cached != shader_patcher.vertex_glsl_cache.end()) {
        return cached->second;
    }
    
    const std::array<char, 65> hash_text = hex(hash_bytes);
    std::string source = load_shader(hash_text.data(), base_path);
    if (source.empty()) {
        source = generate_vertex_glsl(vertex_program);
        dump_missing_shader(hash_text.data(), vertex_program, source.c_str());
    }
    
    shader_patcher.vertex_glsl_cache.emplace(hash_bytes, source);
    
    return source;
}

AttributeLocations attribute_locations(const SceGxmProgram &vertex_program) {
    AttributeLocations locations;
    
    const SceGxmProgramParameter *const parameters = program_parameters(vertex_program);
    for (uint32_t i = 0; i < vertex_program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
            locations.emplace(parameter.resource_index, parameter_name(parameter));
        }
    }
    
    return locations;
}

SharedGLObject get_program(SceGxmContext &context, const MemState &mem) {
    GXM_PROFILE(__FUNCTION__);
    
    assert(context.fragment_program);
    assert(context.vertex_program);
    
    const SceGxmFragmentProgram &fragment_program = *context.fragment_program.get(mem);
    const SceGxmVertexProgram &vertex_program = *context.vertex_program.get(mem);
    const ProgramGLSLs glsls(fragment_program.glsl, vertex_program.glsl);
    const ProgramCache::const_iterator cached = context.program_cache.find(glsls);
    if (cached != context.program_cache.end()) {
        return cached->second;
    }
    
    const SharedGLObject fragment_shader = compile_glsl(GL_FRAGMENT_SHADER, fragment_program.glsl.c_str());
    if (!fragment_shader) {
        return SharedGLObject();
    }
    
    const SharedGLObject vertex_shader = compile_glsl(GL_VERTEX_SHADER, vertex_program.glsl.c_str());
    if (!vertex_shader) {
        return SharedGLObject();
    }
    
    const SharedGLObject program = std::make_shared<GLObject>();
    if (!program->init(glCreateProgram(), &glDeleteProgram)) {
        return SharedGLObject();
    }
    
    glAttachShader(program->get(), fragment_shader->get());
    glAttachShader(program->get(), vertex_shader->get());
    
    bind_attribute_locations(program->get(), vertex_program);
    
    glLinkProgram(program->get());
    
    GLint log_length = 0;
    glGetProgramiv(program->get(), GL_INFO_LOG_LENGTH, &log_length);
    
    if (log_length > 0) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetProgramInfoLog(program->get(), log_length, nullptr, log.data());
        
        LOG_ERROR("{}", log.data());
    }
    
    GLboolean is_linked = GL_FALSE;
    glGetProgramiv(program->get(), GL_LINK_STATUS, &is_linked);
    assert(is_linked != GL_FALSE);
    if (is_linked == GL_FALSE) {
        return SharedGLObject();
    }
    
    glDetachShader(program->get(), fragment_shader->get());
    glDetachShader(program->get(), vertex_shader->get());
    
    context.program_cache.emplace(glsls, program);
    
    return program;
}

GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format) {
    GXM_PROFILE(__FUNCTION__);
    
    switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_U8:
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
            return GL_UNSIGNED_BYTE;
        case SCE_GXM_ATTRIBUTE_FORMAT_S8:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
            return GL_BYTE;
        case SCE_GXM_ATTRIBUTE_FORMAT_U16:
        case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
            return GL_UNSIGNED_SHORT;
        case SCE_GXM_ATTRIBUTE_FORMAT_S16:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
            return GL_SHORT;
        case SCE_GXM_ATTRIBUTE_FORMAT_F16:
            return GL_HALF_FLOAT;
        case SCE_GXM_ATTRIBUTE_FORMAT_F32:
            return GL_FLOAT;
            
        default:
            assert(!"Unhandled format.");
            return GL_UNSIGNED_BYTE;
    }
}

bool attribute_format_normalised(SceGxmAttributeFormat format) {
    GXM_PROFILE(__FUNCTION__);
    
    switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
            return true;
        default:
            return false;
    }
}

void set_uniforms(GLuint program, const SceGxmContext &context, const MemState &mem) {
    GXM_PROFILE(__FUNCTION__);
    
    assert(context.fragment_program);
    assert(context.fragment_program.get(mem)->program);
    assert(context.vertex_program);
    assert(context.vertex_program.get(mem)->program);
    
    set_uniforms(program, context.fragment_uniform_buffers, *context.fragment_program.get(mem)->program.get(mem), mem);
    set_uniforms(program, context.vertex_uniform_buffers, *context.vertex_program.get(mem)->program.get(mem), mem);
}

void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels) {
    GXM_PROFILE(__FUNCTION__);
    
    uint32_t *row1 = &pixels[0];
    uint32_t *row2 = &pixels[(height - 1) * stride_in_pixels];
    
    while (row1 < row2) {
        std::swap_ranges(&row1[0], &row1[width], &row2[0]);
        row1 += stride_in_pixels;
        row2 -= stride_in_pixels;
    }
}

GLenum translate_blend_func(SceGxmBlendFunc src) {
    GXM_PROFILE(__FUNCTION__);
    
    switch (src) {
        case SCE_GXM_BLEND_FUNC_NONE:
            return GL_FUNC_ADD; // TODO Disable blending? Warn?
        case SCE_GXM_BLEND_FUNC_ADD:
            return GL_FUNC_ADD;
        case SCE_GXM_BLEND_FUNC_SUBTRACT:
            return GL_FUNC_SUBTRACT;
        case SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT:
            return GL_FUNC_REVERSE_SUBTRACT;
    }
    
    return GL_FUNC_ADD;
}

GLenum translate_blend_factor(SceGxmBlendFactor src) {
    GXM_PROFILE(__FUNCTION__);
    
    switch (src) {
        case SCE_GXM_BLEND_FACTOR_ZERO:
            return GL_ZERO;
        case SCE_GXM_BLEND_FACTOR_ONE:
            return GL_ONE;
        case SCE_GXM_BLEND_FACTOR_SRC_COLOR:
            return GL_SRC_COLOR;
        case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
            return GL_ONE_MINUS_SRC_COLOR;
        case SCE_GXM_BLEND_FACTOR_SRC_ALPHA:
            return GL_SRC_ALPHA;
        case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case SCE_GXM_BLEND_FACTOR_DST_COLOR:
            return GL_DST_COLOR;
        case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
            return GL_ONE_MINUS_DST_COLOR;
        case SCE_GXM_BLEND_FACTOR_DST_ALPHA:
            return GL_DST_ALPHA;
        case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
            return GL_ONE_MINUS_DST_ALPHA;
        case SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE:
            return GL_SRC_ALPHA_SATURATE;
        case SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE:
            return GL_DST_ALPHA; // TODO Not supported.
    }
    
    return GL_ONE;
}

GLenum translate_internal_format(SceGxmTextureFormat src) {
    GXM_PROFILE(__FUNCTION__);
    
    switch (src) {
        case SCE_GXM_TEXTURE_FORMAT_P8_ABGR:
            return GL_RGBA;
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
            return GL_RGBA8;
        case SCE_GXM_TEXTURE_FORMAT_U8_R111:
            return GL_INTENSITY8;
        default:
            return GL_RGBA8; // TODO Warn.
    }
}

GLenum translate_format(SceGxmTextureFormat src) {
    GXM_PROFILE(__FUNCTION__);
    
    switch (src) {
        case SCE_GXM_TEXTURE_FORMAT_P8_ABGR:
            return GL_COLOR_INDEX;
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
            return GL_RGBA;
        case SCE_GXM_TEXTURE_FORMAT_U8_R111:
            return GL_RED;
        default:
            return GL_RGBA; // TODO Warn.
    }
}

GLenum translate_primitive(SceGxmPrimitiveType primType){
    GXM_PROFILE(__FUNCTION__);
    
    switch (primType){
        case SCE_GXM_PRIMITIVE_TRIANGLES:
            return GL_TRIANGLES;
        case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
            return GL_TRIANGLE_STRIP;
        case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
            return GL_TRIANGLE_FAN;
        case SCE_GXM_PRIMITIVE_LINES:
            return GL_LINES;
        case SCE_GXM_PRIMITIVE_POINTS:
            return GL_POINTS;
        case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES: // Todo: Implement this
            return GL_TRIANGLES;
    }
    return GL_TRIANGLES;
}

bool operator<(const FragmentProgramCacheKey& a, const FragmentProgramCacheKey &b) {
    if (a.fragment_program < b.fragment_program) {
        return true;
    }
    if (b.fragment_program < a.fragment_program) {
        return false;
    }
    if (a.blend_info < b.blend_info) {
        return true;
    }
    if (b.blend_info < a.blend_info) {
        return false;
    }
    return a.vertex_program < b.vertex_program;
}
