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

static std::string generate_glsl(const SceGxmProgram &program) {
    GXM_PROFILE(__FUNCTION__);
    
    return "TODO";
}

static bool compile_shader(GLuint shader, const GLchar *source) {
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

bool compile_shader(GLuint shader, const SceGxmProgram *program, const char *base_path, ReportingState &reporting) {
    GXM_PROFILE(__FUNCTION__);
    
    const Sha256Hash hash_bytes = sha256(program, program->size);
    const std::array<char, 65> hash_text = hex(hash_bytes);
    
    std::ostringstream path;
    path << base_path << "shaders/" << hash_text.data() << ".glsl";
    
    std::ifstream is(path.str());
    if (is.fail()) {
        LOG_ERROR("Couldn't open '{}' for reading.", path.str());
        const std::string glsl = generate_glsl(*program);
        report_missing_shader(reporting, hash_text.data(), glsl.c_str());
        
        // Dump missing shader binary.
        std::ostringstream gxp_path;
        gxp_path << hash_text.data() << ".gxp";
        std::ofstream gxp(gxp_path.str(), std::ofstream::binary);
        if (!gxp.fail()) {
            gxp.write(reinterpret_cast<const char *>(program), program->size);
        }
        
        return false;
    }
    
    is.seekg(0, std::ios::end);
    const size_t size = is.tellg();
    is.seekg(0);
    
    std::string source(size, ' ');
    is.read(&source[0], size);
    
    return compile_shader(shader, source.c_str());
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

void bind_attribute_locations(GLuint gl_program, const SceGxmProgram *program) {
    GXM_PROFILE(__FUNCTION__);
    
    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);
    for (uint32_t i = 0; i < program->parameter_count; ++i) {
        const SceGxmProgramParameter *const parameter = &parameters[i];
        if (parameter->category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
            const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);
            const char *const parameter_name = reinterpret_cast<const char *>(parameter_bytes + parameter->name_offset);
            
            glBindAttribLocation(gl_program, parameter->resource_index, parameter_name);
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
