#include <gxm/functions.h>

#include <crypto/hash.h>
#include <gxm/types.h>
#include <reporting/functions.h>
#include <util/log.h>

#include <glbinding/AbstractFunction.h>
#include <glbinding/FunctionCall.h>
#include <microprofile.h>

#include <fstream>
#include <sstream>

#define GXM_PROFILE(name) MICROPROFILE_SCOPEI("GXM", name, MP_BLUE)

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

static bool compile_glsl(GLuint shader, const GLchar *source) {
    GXM_PROFILE(__FUNCTION__);
    
    const GLint length = static_cast<GLint>(strlen(source));
    glShaderSource(shader, 1, &source, &length);
    
    glCompileShader(shader);
    
    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    
    if (log_length > 0) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, &log.front());
        
        LOG_ERROR("{}", log.front());
    }
    
    GLboolean is_compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    assert(is_compiled != GL_FALSE);
    
    return is_compiled != GL_FALSE;
}

typedef std::function<std::string(const SceGxmProgram &program)> GenerateGLSL;

static bool compile_shader(GLuint shader, const SceGxmProgram &program, const char *base_path, const GenerateGLSL &generate_glsl, ReportingState &reporting) {
    GXM_PROFILE(__FUNCTION__);
    
    const Sha256Hash hash_bytes = sha256(&program, program.size);
    const std::array<char, 65> hash_text = hex(hash_bytes);
    
    std::ostringstream path;
    path << base_path << "shaders/" << hash_text.data() << ".glsl";
    
    std::ifstream is(path.str());
    std::string source;
    if (is.fail()) {
        LOG_ERROR("Couldn't open shader '{}' for reading.", path.str());
        source = generate_glsl(program);
        report_missing_shader(reporting, hash_text.data(), source.c_str());
        
        // Dump missing shader GLSL.
        std::ostringstream glsl_path;
        glsl_path << hash_text.data() << ".glsl";
        std::ofstream glsl_file(glsl_path.str());
        if (!glsl_file.fail()) {
            glsl_file << source;
            glsl_file.close();
        }
        
        // Dump missing shader binary.
        std::ostringstream gxp_path;
        gxp_path << hash_text.data() << ".gxp";
        std::ofstream gxp(gxp_path.str(), std::ofstream::binary);
        if (!gxp.fail()) {
            gxp.write(reinterpret_cast<const char *>(&program), program.size);
            gxp.close();
        }
    } else {
        is.seekg(0, std::ios::end);
        const size_t size = is.tellg();
        is.seekg(0);
        
        source.resize(size, ' ');
        is.read(&source[0], size);
    }
    
    return compile_glsl(shader, source.c_str());
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

bool compile_fragment_shader(GLuint shader, const SceGxmProgram &program, const char *base_path, ReportingState &reporting) {
    GXM_PROFILE(__FUNCTION__);
    return compile_shader(shader, program, base_path, generate_fragment_glsl, reporting);
}

bool compile_vertex_shader(GLuint shader, const SceGxmProgram &program, const char *base_path, ReportingState &reporting) {
    GXM_PROFILE(__FUNCTION__);
    return compile_shader(shader, program, base_path, generate_vertex_glsl, reporting);
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

void bind_attribute_locations(GLuint gl_program, const SceGxmProgram &program) {
    GXM_PROFILE(__FUNCTION__);
    
    const SceGxmProgramParameter *const parameters = program_parameters(program);
    for (uint32_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
            glBindAttribLocation(gl_program, parameter.resource_index, parameter_name(parameter));
        }
    }
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
