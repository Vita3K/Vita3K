#include <gxm/functions.h>

#include <crypto/hash.h>
#include <gxm/types.h>
#include <util/log.h>

#include <glbinding/AbstractFunction.h>
#include <glbinding/FunctionCall.h>
#include <microprofile.h>

#include <algorithm>
#include <cstring> // memcmp
#include <fstream>
#include <sstream>

#define GXM_PROFILE(name) MICROPROFILE_SCOPEI("GXM", name, MP_BLUE)

static std::string load_shader(const char *hash, const char *extension, const char *base_path) {
    std::ostringstream path;
    path << base_path << "shaders/" << hash << "." << extension;

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

/**
* \brief Returns raw parameter name from GXP
*        Therefore, if parameter belongs in a struct, includes it in the form of "struct_name.field_name"
*/
static std::string parameter_name_raw(const SceGxmProgramParameter &parameter) {
    const uint8_t *const bytes = reinterpret_cast<const uint8_t *>(&parameter);
    return reinterpret_cast<const char *>(bytes + parameter.name_offset);
}

/**
 * \brief If parameter belongs in a struct, returns the struct field name only
 */
static std::string parameter_name(const SceGxmProgramParameter &parameter) {
    auto full_name = parameter_name_raw(parameter);
    auto struct_idx = full_name.find('.');
    bool is_struct_field = struct_idx != std::string::npos;

    if (is_struct_field)
        return full_name.substr(struct_idx + 1, full_name.length());
    else
        return full_name;
}

static SceGxmParameterType parameter_type(const SceGxmProgramParameter &parameter) {
    return static_cast<SceGxmParameterType>(
        static_cast<uint16_t>(parameter.type));
}

static void log_parameter(const SceGxmProgramParameter &parameter) {
    std::string type;
    switch (parameter.category) {
    case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE:
        type = "Vertex attribute";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
        type = "Uniform";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_SAMPLER:
        type = "Sampler";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE:
        type = "Auxiliary surface";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER:
        type = "Uniform buffer";
        break;
    default:
        type = "Other type";
        break;
    }
    LOG_DEBUG("{}: name:{:s} type:{:d} component_count:{:x} container_index:{:x}",
        type, parameter_name_raw(parameter), parameter.type, parameter.component_count, parameter.container_index);
}

static const char *scalar_type(SceGxmParameterType type) {
    switch (type) {
    case SCE_GXM_PARAMETER_TYPE_F16:
    case SCE_GXM_PARAMETER_TYPE_F32:
        return "float";
    case SCE_GXM_PARAMETER_TYPE_U8:
    case SCE_GXM_PARAMETER_TYPE_U16:
    case SCE_GXM_PARAMETER_TYPE_U32:
        return "uint";
    case SCE_GXM_PARAMETER_TYPE_S8:
    case SCE_GXM_PARAMETER_TYPE_S16:
    case SCE_GXM_PARAMETER_TYPE_S32:
        return "int";
    default: {
        LOG_ERROR("Unsupported parameter type {:#x} used in shader.", type);
    }

        return "?";
    }
}

static const char *vector_prefix(SceGxmParameterType type) {
    switch (type) {
    case SCE_GXM_PARAMETER_TYPE_F32:
        return "";
    case SCE_GXM_PARAMETER_TYPE_U32:
        return "u";
    case SCE_GXM_PARAMETER_TYPE_S32:
        return "i";
    default:
        LOG_ERROR("Unsupported parameter type {:#x} used in shader.", type);
    }
    return "?";
}

static void output_uniform_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count == 4);
    glsl << " sampler2D " << parameter_name_raw(parameter);
}

static void output_scalar_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count == 1);

    glsl << scalar_type(parameter_type(parameter)) << " " << parameter_name(parameter);
    if (parameter.array_size > 1) {
        glsl << "[" << parameter.array_size << "]";
    }
}

static void output_vector_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count > 1);
    assert(parameter.component_count <= 4);

    const auto vector = vector_prefix(parameter_type(parameter));

    glsl << vector << "vec" << parameter.component_count << " " << parameter_name(parameter);
    if (parameter.array_size > 1) {
        glsl << "[" << parameter.array_size << "]";
    }
}

static void output_matrix_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
    assert(parameter.component_count > 1);
    assert(parameter.array_size >= 2);
    assert(parameter.array_size <= 4);

    glsl << vector_prefix(parameter_type(parameter)) << "mat";
    if (parameter.component_count == parameter.array_size) {
        glsl << parameter.component_count;
    } else {
        glsl << parameter.component_count << "x" << parameter.array_size;
    }
    glsl << " " << parameter_name(parameter);
}

void output_struct_decl_begin(std::ostream &glsl, const std::string &qualifier) {
    if (!qualifier.empty())
        glsl << qualifier << " ";
    glsl << "struct {\n";
}

void output_struct_decl_end(std::ostream &glsl, const std::string &struct_name) {
    glsl << "} " << struct_name;
    glsl << ";\n";
}

static void output_glsl_decl(std::ostream &glsl, std::string &cur_struct_decl, const SceGxmProgramParameter &parameter, const std::string &qualifier, bool is_sampler = false) {
    const std::string param_name_raw = parameter_name_raw(parameter);
    const auto struct_idx = param_name_raw.find('.');
    const bool is_struct_field = struct_idx != std::string::npos;
    const std::string struct_name = is_struct_field ? param_name_raw.substr(0, struct_idx) : ""; // empty if not a struct
    const bool struct_started = is_struct_field && cur_struct_decl != struct_name; // previous param was no struct, or a different struct
    const bool struct_ended = !is_struct_field && !cur_struct_decl.empty(); // previous param was a struct but this one isn't

    if (struct_started)
        output_struct_decl_begin(glsl, qualifier);

    if (struct_ended)
        output_struct_decl_end(glsl, cur_struct_decl);

    // Indent if struct field
    if (is_struct_field)
        glsl << "   ";

    // No qualifier if struct field. They're only present once in the struct declaration.
    if (!is_struct_field)
        glsl << qualifier << " ";

    // TODO: Should be using param type here
    if (is_sampler) {
        output_uniform_decl(glsl, parameter);
    } else if (parameter.component_count > 1) {
        if (parameter.array_size > 1 && parameter.array_size <= 4) {
            output_matrix_decl(glsl, parameter);
        } else {
            output_vector_decl(glsl, parameter);
        }
    } else {
        output_scalar_decl(glsl, parameter);
    }

    glsl << ";\n";

    cur_struct_decl = struct_name;
}

static void output_glsl_parameters(std::ostream &glsl, const SceGxmProgram &program) {
    if (program.parameter_count > 0) {
        glsl << "\n";
    }

    // Keeps track of current struct declaration
    std::string cur_struct_decl;

    const SceGxmProgramParameter *const parameters = program_parameters(program);
    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        log_parameter(parameter);

        switch (parameter.category) {
        case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE: {
            output_glsl_decl(glsl, cur_struct_decl, parameter, "in");
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM: {
            output_glsl_decl(glsl, cur_struct_decl, parameter, "uniform");
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_SAMPLER: {
            output_glsl_decl(glsl, cur_struct_decl, parameter, "uniform ", true);
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE: {
            assert(parameter.component_count == 0);
            LOG_CRITICAL("auxiliary_surface used in shader");
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER: {
            assert(parameter.component_count == 0);
            LOG_CRITICAL("uniform_buffer used in shader");
            break;
        }
        default: {
            LOG_CRITICAL("Unknown parameter type used in shader.");
            break;
        }
        }
    }

    if (!cur_struct_decl.empty()) {
        output_struct_decl_end(glsl, cur_struct_decl);
    }
}

static std::string generate_fragment_glsl(const SceGxmProgram &program) {
    GXM_PROFILE(__FUNCTION__);

    std::ostringstream glsl;
    glsl << "// Fragment shader.\n";
    glsl << "#version 410\n";
    output_glsl_parameters(glsl, program);
    glsl << "\n";
    glsl << "void main() {\n";
    glsl << "    gl_FragColor = vec4(0, 1, 0, 1);\n";
    glsl << "}\n";

    return glsl.str();
}

static std::string generate_vertex_glsl(const SceGxmProgram &program) {
    GXM_PROFILE(__FUNCTION__);

    std::ostringstream glsl;
    glsl << "// Vertex shader.\n";
    glsl << "#version 410\n";
    output_glsl_parameters(glsl, program);
    glsl << "\n";
    glsl << "void main() {\n";
    glsl << "    gl_Position = vec4(0, 0, 0, 1);\n";
    glsl << "}\n";

    return glsl.str();
}

static void dump_missing_shader(const char *hash, const char *extension, const SceGxmProgram &program, const char *source) {
    // Dump missing shader GLSL.
    std::ostringstream glsl_path;
    glsl_path << hash << "." << extension;
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

static SharedGLObject compile_glsl(GLenum type, const std::string &source) {
    GXM_PROFILE(__FUNCTION__);

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
        glBindAttribLocation(gl_program, binding.first / sizeof(uint32_t), binding.second.c_str());
    }
}

template <class T>
void uniform_4(GLint location, GLsizei count, const T *value);

template <>
void uniform_4<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
    glUniform4fv(location, count, value);
}

template <class T>
void uniform_1(GLint location, GLsizei count, const T *value);

template <>
void uniform_1<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
    glUniform1fv(location, count, value);
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
    case 1:
        switch (array_size) {
        case 1:
            uniform_1<T>(location, array_size, value);
            break;
        }
        break;
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

        auto name = parameter_name(parameter);
        const GLint location = glGetUniformLocation(gl_program, name.c_str());
        if (location < 0) {
            LOG_WARN("Uniform parameter {} not found in current OpenGL program.", name);
            continue;
        }

        const SceGxmParameterType type = static_cast<SceGxmParameterType>(static_cast<uint16_t>(parameter.type));
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
    std::string source = load_shader(hash_text.data(), "frag", base_path);
    if (source.empty()) {
        LOG_ERROR("Missing fragment shader {}", hash_text.data());
        source = generate_fragment_glsl(fragment_program);
        dump_missing_shader(hash_text.data(), "frag", fragment_program, source.c_str());
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
    std::string source = load_shader(hash_text.data(), "vert", base_path);
    if (source.empty()) {
        LOG_ERROR("Missing vertex shader {}", hash_text.data());
        source = generate_vertex_glsl(vertex_program);
        dump_missing_shader(hash_text.data(), "vert", vertex_program, source.c_str());
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
            locations.emplace(parameter.resource_index, parameter_name_raw(parameter));
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

    const SharedGLObject fragment_shader = compile_glsl(GL_FRAGMENT_SHADER, fragment_program.glsl);
    if (!fragment_shader) {
        LOG_CRITICAL("Error in compiled fragment shader:\n{}", fragment_program.glsl);
        return SharedGLObject();
    }
    const SharedGLObject vertex_shader = compile_glsl(GL_VERTEX_SHADER, vertex_program.glsl.c_str());
    if (!vertex_shader) {
        LOG_CRITICAL("Error in compiled vertex shader:\n{}", vertex_program.glsl);
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

    default: {
        LOG_ERROR("Unsupported attribute format {:#x}", format);
    }
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
    assert(context.vertex_program);

    const SceGxmFragmentProgram &fragment_program = *context.fragment_program.get(mem);
    const SceGxmVertexProgram &vertex_program = *context.vertex_program.get(mem);
    assert(fragment_program.program);
    assert(vertex_program.program);

    set_uniforms(program, context.fragment_uniform_buffers, *fragment_program.program.get(mem), mem);
    set_uniforms(program, context.vertex_uniform_buffers, *vertex_program.program.get(mem), mem);
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
    case SCE_GXM_BLEND_FUNC_MIN:
        return GL_MIN;
    case SCE_GXM_BLEND_FUNC_MAX:
        return GL_MAX;
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

namespace texture {

    SceGxmTextureFormat get_format(const SceGxmTexture *texture) {
        return static_cast<SceGxmTextureFormat>(
            texture->base_format << 24 | texture->format0 << 31 | texture->swizzle_format << 12);
    }

    unsigned int get_width(const SceGxmTexture *texture) {
        if (texture->type << 29 != SCE_GXM_TEXTURE_SWIZZLED && texture->type << 29 != SCE_GXM_TEXTURE_TILED) {
            return texture->width + 1;
        }
        return 1 << (texture->width & 0xF);
    }

    unsigned int get_height(const SceGxmTexture *texture) {
        if (texture->type << 29 != SCE_GXM_TEXTURE_SWIZZLED && texture->type << 29 != SCE_GXM_TEXTURE_TILED) {
            return texture->height + 1;
        }
        return 1 << (texture->height & 0xF);
    }

    SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src) {
        return static_cast<SceGxmTextureBaseFormat>(src & 0xFF000000);
    }

    std::uint32_t get_swizzle(SceGxmTextureFormat src) {
        return static_cast<SceGxmTextureBaseFormat>(src & 0x0000F000);
    }

    bool is_paletted_format(SceGxmTextureFormat src) {
        const auto base_format = get_base_format(src);

        return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4;
    }

    GLenum translate_internal_format(SceGxmTextureFormat src) {
        GXM_PROFILE(__FUNCTION__);

        if (is_paletted_format(src)) {
            return GL_RGBA;
        }

        switch (src) {
        case SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB:
        case SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR:
            return GL_RGB565;
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR:
            return GL_RGBA8;
        case SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR:
            return GL_RGBA4;
        case SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR:
            return GL_RGB5_A1;
        case SCE_GXM_TEXTURE_FORMAT_U8_R111: // TODO: this is inaccurate
        case SCE_GXM_TEXTURE_FORMAT_U8_111R: // TODO: this is inaccurate
        case SCE_GXM_TEXTURE_FORMAT_U8_1RRR: // TODO: this is inaccurate
            return GL_R8;
        default:
            return GL_RGBA8;
        }
    }

    GLenum translate_format(SceGxmTextureFormat src) {
        GXM_PROFILE(__FUNCTION__);

        if (is_paletted_format(src)) {
            return GL_RGBA;
        }

        switch (src) {
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
        case SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR:
        case SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR:
            return GL_RGBA;
        case SCE_GXM_TEXTURE_FORMAT_U8_R111: // TODO: this is inaccurate
            return GL_RED;
        case SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB:
        case SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR:
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR:
            return GL_RGB;
        case SCE_GXM_TEXTURE_FORMAT_U8_111R: // TODO: this is inaccurate
            return GL_ALPHA;
        case SCE_GXM_TEXTURE_FORMAT_U8_1RRR: // TODO: this is inaccurate
            return GL_RED;
        default:
            return GL_RGBA;
        }
    }

    GLenum translate_wrap_mode(SceGxmTextureAddrMode src) {
        GXM_PROFILE(__FUNCTION__);

        switch (src) {
        case SCE_GXM_TEXTURE_ADDR_REPEAT:
            return GL_REPEAT;
        case SCE_GXM_TEXTURE_ADDR_CLAMP:
            return GL_CLAMP_TO_EDGE;
        case SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP:
            return GL_CLAMP_TO_EDGE; // FIXME: Not supported in OpenGL 4.1 core.
        case SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER:
            return GL_REPEAT; // FIXME: Is this correct?
        case SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER:
            return GL_CLAMP_TO_BORDER;
        case SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER:
            return GL_CLAMP_TO_BORDER; // FIXME: Is this correct?
        case SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER:
            return GL_CLAMP_TO_BORDER; // FIXME: Is this correct?
        default:
            LOG_WARN("Unsupported texture wrap mode translated: {:#08X}", src);
            return GL_CLAMP_TO_EDGE;
        }
    }

    GLenum translate_minmag_filter(SceGxmTextureFilter src) {
        GXM_PROFILE(__FUNCTION__);

        switch (src) {
        case SCE_GXM_TEXTURE_FILTER_POINT:
            return GL_NEAREST;
        case SCE_GXM_TEXTURE_FILTER_LINEAR:
            return GL_LINEAR;
        default:
            LOG_WARN("Unsupported texture min/mag filter translated: {:#08X}", src);
            return GL_LINEAR;
        }
    }

    GLenum translate_type(SceGxmTextureFormat format) {
        const auto base_format = texture::get_base_format(format);
        switch (base_format) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
            return GL_UNSIGNED_SHORT_4_4_4_4;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
            return GL_UNSIGNED_BYTE;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
            return GL_UNSIGNED_SHORT_5_6_5;
        default: {
            LOG_WARN("Unsupported translate type: {:#08X}", format);
            return GL_UNSIGNED_BYTE;
        }
        }
    }

} // namespace texture

GLenum translate_primitive(SceGxmPrimitiveType primType) {
    GXM_PROFILE(__FUNCTION__);

    switch (primType) {
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
        LOG_WARN("Unsupported primitive: SCE_GXM_PRIMITIVE_TRIANGLE_EDGES, using GL_TRIANGLES instead");
        return GL_TRIANGLES;
    }
    return GL_TRIANGLES;
}

GLenum translate_stencil_func(SceGxmStencilFunc stencil_func) {
    GXM_PROFILE(__FUNCTION__);

    switch (stencil_func) {
    case SCE_GXM_STENCIL_FUNC_NEVER:
        return GL_NEVER;
    case SCE_GXM_STENCIL_FUNC_LESS:
        return GL_LESS;
    case SCE_GXM_STENCIL_FUNC_EQUAL:
        return GL_EQUAL;
    case SCE_GXM_STENCIL_FUNC_LESS_EQUAL:
        return GL_LEQUAL;
    case SCE_GXM_STENCIL_FUNC_GREATER:
        return GL_GREATER;
    case SCE_GXM_STENCIL_FUNC_NOT_EQUAL:
        return GL_NOTEQUAL;
    case SCE_GXM_STENCIL_FUNC_GREATER_EQUAL:
        return GL_GEQUAL;
    case SCE_GXM_STENCIL_FUNC_ALWAYS:
        return GL_ALWAYS;
    }

    return GL_ALWAYS;
}

GLenum translate_stencil_op(SceGxmStencilOp stencil_op) {
    GXM_PROFILE(__FUNCTION__);

    switch (stencil_op) {
    case SCE_GXM_STENCIL_OP_KEEP:
        return GL_KEEP;
    case SCE_GXM_STENCIL_OP_ZERO:
        return GL_ZERO;
    case SCE_GXM_STENCIL_OP_REPLACE:
        return GL_REPLACE;
    case SCE_GXM_STENCIL_OP_INCR:
        return GL_INCR;
    case SCE_GXM_STENCIL_OP_DECR:
        return GL_DECR;
    case SCE_GXM_STENCIL_OP_INVERT:
        return GL_INVERT;
    case SCE_GXM_STENCIL_OP_INCR_WRAP:
        return GL_INCR_WRAP;
    case SCE_GXM_STENCIL_OP_DECR_WRAP:
        return GL_DECR_WRAP;
    }

    return GL_KEEP;
}

GLenum translate_depth_func(SceGxmDepthFunc depth_func) {
    GXM_PROFILE(__FUNCTION__);

    switch (depth_func) {
    case SCE_GXM_DEPTH_FUNC_NEVER:
        return GL_NEVER;
    case SCE_GXM_DEPTH_FUNC_LESS:
        return GL_LESS;
    case SCE_GXM_DEPTH_FUNC_EQUAL:
        return GL_EQUAL;
    case SCE_GXM_DEPTH_FUNC_LESS_EQUAL:
        return GL_LEQUAL;
    case SCE_GXM_DEPTH_FUNC_GREATER:
        return GL_GREATER;
    case SCE_GXM_DEPTH_FUNC_NOT_EQUAL:
        return GL_NOTEQUAL;
    case SCE_GXM_DEPTH_FUNC_GREATER_EQUAL:
        return GL_GEQUAL;
    case SCE_GXM_DEPTH_FUNC_ALWAYS:
        return GL_ALWAYS;
    }

    return GL_ALWAYS;
}

bool operator<(const FragmentProgramCacheKey &a, const FragmentProgramCacheKey &b) {
    if (a.fragment_program < b.fragment_program) {
        return true;
    }
    if (b.fragment_program < a.fragment_program) {
        return false;
    }
    return b.blend_info < a.blend_info;
}
