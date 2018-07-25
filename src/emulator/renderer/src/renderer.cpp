#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <renderer/state.h>
#include <renderer/types.h>

#include <glutil/gl.h>
#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <SDL_video.h>
#include <glbinding/Binding.h>

#include <cassert>
#include <fstream>
#include <sstream>

using namespace glbinding;

namespace renderer {
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

    static SceGxmParameterType parameter_type(const SceGxmProgramParameter &parameter) {
        return static_cast<SceGxmParameterType>(static_cast<uint16_t>(parameter.type));
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
        LOG_DEBUG("{}: name:{:s} type:{:d} component_count:{} container_index:{}",
            type, gxm::parameter_name_raw(parameter), parameter.type, log_hex(uint8_t(parameter.component_count)), log_hex(uint8_t(parameter.container_index)));
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
            LOG_ERROR("Unsupported parameter type {} used in shader.", log_hex(type));
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
            LOG_ERROR("Unsupported parameter type {} used in shader.", log_hex(type));
        }
        return "?";
    }

    static void output_sampler_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
        glsl << "sampler2D " << gxm::parameter_name_raw(parameter);
    }

    static void output_scalar_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
        assert(parameter.component_count == 1);

        glsl << scalar_type(parameter_type(parameter)) << " " << gxm::parameter_name(parameter);
        if (parameter.array_size > 1) {
            glsl << "[" << parameter.array_size << "]";
        }
    }

    static void output_vector_decl(std::ostream &glsl, const SceGxmProgramParameter &parameter) {
        assert(parameter.component_count > 1);
        assert(parameter.component_count <= 4);

        const auto vector = vector_prefix(parameter_type(parameter));

        glsl << vector << "vec" << parameter.component_count << " " << gxm::parameter_name(parameter);
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
        glsl << " " << gxm::parameter_name(parameter);
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
        const std::string param_name_raw = gxm::parameter_name_raw(parameter);
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
            // samplers are special because they can't be inside structs
            output_sampler_decl(glsl, parameter);
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

        const SceGxmProgramParameter *const parameters = gxm::program_parameters(program);
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
                output_glsl_decl(glsl, cur_struct_decl, parameter, "uniform", true);
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
        R_PROFILE(__func__);

        std::ostringstream glsl;
        glsl << "// Fragment shader.\n";
        glsl << "#version 410\n";
        output_glsl_parameters(glsl, program);
        glsl << "\n";
        glsl << "out vec4 fragColor;\n";
        glsl << "\n";
        glsl << "void main() {\n";
        glsl << "    fragColor = vec4(0, 1, 0, 1);\n";
        glsl << "}\n";

        return glsl.str();
    }

    static std::string generate_vertex_glsl(const SceGxmProgram &program) {
        R_PROFILE(__func__);

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

    static std::string get_fragment_glsl(GLSLCache &cache, const SceGxmProgram &fragment_program, const char *base_path) {
        const Sha256Hash hash_bytes = sha256(&fragment_program, fragment_program.size);
        const GLSLCache::const_iterator cached = cache.find(hash_bytes);
        if (cached != cache.end()) {
            return cached->second;
        }

        const Sha256HashText hash_text = hex(hash_bytes);
        std::string source = load_shader(hash_text.data(), "frag", base_path);
        if (source.empty()) {
            LOG_ERROR("Missing fragment shader {}", hash_text.data());
            source = generate_fragment_glsl(fragment_program);
            dump_missing_shader(hash_text.data(), "frag", fragment_program, source.c_str());
        }

        cache.emplace(hash_bytes, source);

        return source;
    }

    static GLenum translate_blend_func(SceGxmBlendFunc src) {
        R_PROFILE(__func__);

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

    static GLenum translate_blend_factor(SceGxmBlendFactor src) {
        R_PROFILE(__func__);

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

    static std::string get_vertex_glsl(GLSLCache &cache, const SceGxmProgram &vertex_program, const char *base_path) {
        const Sha256Hash hash_bytes = sha256(&vertex_program, vertex_program.size);
        const GLSLCache::const_iterator cached = cache.find(hash_bytes);
        if (cached != cache.end()) {
            return cached->second;
        }

        const Sha256HashText hash_text = hex(hash_bytes);
        std::string source = load_shader(hash_text.data(), "vert", base_path);
        if (source.empty()) {
            LOG_ERROR("Missing vertex shader {}", hash_text.data());
            source = generate_vertex_glsl(vertex_program);
            dump_missing_shader(hash_text.data(), "vert", vertex_program, source.c_str());
        }

        cache.emplace(hash_bytes, source);

        return source;
    }

    static AttributeLocations attribute_locations(const SceGxmProgram &vertex_program) {
        R_PROFILE(__func__);
        AttributeLocations locations;

        const SceGxmProgramParameter *const parameters = gxm::program_parameters(vertex_program);
        for (uint32_t i = 0; i < vertex_program.parameter_count; ++i) {
            const SceGxmProgramParameter &parameter = parameters[i];
            if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
                std::string name = gxm::parameter_name_raw(parameter);
                const auto struct_idx = name.find('.');
                const bool is_struct_field = struct_idx != std::string::npos;
                if (is_struct_field)
                    name.replace(struct_idx, 1, ""); //Workaround for input.field on glsl version 120
                locations.emplace(parameter.resource_index, name);
            }
        }

        return locations;
    }

    static void bind_attribute_locations(GLuint gl_program, const VertexProgram &program) {
        R_PROFILE(__func__);

        for (const AttributeLocations::value_type &binding : program.attribute_locations) {
            glBindAttribLocation(gl_program, binding.first / sizeof(uint32_t), binding.second.c_str());
        }
    }

    static void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels) {
        R_PROFILE(__func__);

        uint32_t *row1 = &pixels[0];
        uint32_t *row2 = &pixels[(height - 1) * stride_in_pixels];

        while (row1 < row2) {
            std::swap_ranges(&row1[0], &row1[width], &row2[0]);
            row1 += stride_in_pixels;
            row2 -= stride_in_pixels;
        }
    }

    static SharedGLObject get_program(Context &context, const GxmContextState &state, const MemState &mem) {
        R_PROFILE(__func__);

        assert(state.fragment_program);
        assert(state.vertex_program);

        const FragmentProgram &fragment_program = *state.fragment_program.get(mem)->renderer.get();
        const VertexProgram &vertex_program = *state.vertex_program.get(mem)->renderer.get();
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
        const SharedGLObject vertex_shader = compile_glsl(GL_VERTEX_SHADER, vertex_program.glsl);
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

            LOG_ERROR("{}\n", log.data());
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

    template <class T>
    static void uniform_4(GLint location, GLsizei count, const T *value);

    template <>
    void uniform_4<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
        glUniform4fv(location, count, value);
    }

    template <>
    void uniform_4<GLint>(GLint location, GLsizei count, const GLint *value) {
        glUniform4iv(location, count, value);
    }

    template <class T>
    static void uniform_1(GLint location, GLsizei count, const T *value);

    template <>
    void uniform_1<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
        glUniform1fv(location, count, value);
    }

    template <>
    void uniform_1<GLint>(GLint location, GLsizei count, const GLint *value) {
        glUniform1iv(location, count, value);
    }

    template <class T>
    static void uniform_matrix_4(GLint location, GLsizei count, GLboolean, const T *value);

    template <>
    void uniform_matrix_4<GLfloat>(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
        glUniformMatrix4fv(location, count, transpose, value);
    }

    template <>
    void uniform_matrix_4<GLint>(GLint location, GLsizei count, GLboolean transpose, const GLint *value) {
        glUniform4iv(location, count, (GLint *)value);
    }

    template <class T>
    static void set_uniform(GLint location, size_t component_count, GLsizei array_size, const T *value) {
        switch (component_count) {
        case 1:
            switch (array_size) {
            case 1:
                uniform_1<T>(location, array_size, value);
                break;
            }
            break;
        case 4:
            if (array_size % 4 == 0) {
                uniform_matrix_4<T>(location, array_size / 4, GL_FALSE, value);
            } else {
                uniform_4<T>(location, array_size, value);
            }
            break;

        default:
            LOG_WARN("Unhandled uniform component count {}.", component_count);
            break;
        }
    }

    static void set_uniforms(GLuint gl_program, const UniformBuffers &uniform_buffers, const SceGxmProgram &gxm_program, const MemState &mem) {
        R_PROFILE(__func__);

        const SceGxmProgramParameter *const parameters = gxm::program_parameters(gxm_program);
        for (size_t i = 0; i < gxm_program.parameter_count; ++i) {
            const SceGxmProgramParameter &parameter = parameters[i];
            if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
                continue;
            }

            auto name = gxm::parameter_name(parameter);
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

            const GLint *src_s32;
            const GLfloat *src_f32;

            const uint8_t *const base = static_cast<const uint8_t *>(uniform_buffer.get(mem));
            switch (type) {
            case SCE_GXM_PARAMETER_TYPE_S32:
                src_s32 = reinterpret_cast<const GLint *>(base + parameter.resource_index * 4); // TODO What offset?
                set_uniform<GLint>(location, parameter.component_count, parameter.array_size, src_s32);
                break;
            case SCE_GXM_PARAMETER_TYPE_F32:
                src_f32 = reinterpret_cast<const GLfloat *>(base + parameter.resource_index * 4); // TODO What offset?
                set_uniform<GLfloat>(location, parameter.component_count, parameter.array_size, src_f32);
                break;
            default:
                LOG_WARN("Type {} not handled for uniform parameter {}.", type, name);
                break;
            }
        }
    }

    static void set_uniforms(GLuint program, const Context &context, const GxmContextState &state, const MemState &mem) {
        R_PROFILE(__func__);

        assert(state.fragment_program);
        assert(state.vertex_program);

        const SceGxmFragmentProgram &fragment_program = *state.fragment_program.get(mem);
        const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
        assert(fragment_program.program);
        assert(vertex_program.program);

        set_uniforms(program, state.fragment_uniform_buffers, *fragment_program.program.get(mem), mem);
        set_uniforms(program, state.vertex_uniform_buffers, *vertex_program.program.get(mem), mem);
    }

    static GLenum translate_depth_func(SceGxmDepthFunc depth_func) {
        R_PROFILE(__func__);

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

    static GLenum translate_stencil_op(SceGxmStencilOp stencil_op) {
        R_PROFILE(__func__);

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

    static GLenum translate_stencil_func(SceGxmStencilFunc stencil_func) {
        R_PROFILE(__func__);

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

    static void apply_stencil_state(GLenum face, const GxmStencilState &state) {
        glStencilOpSeparate(face,
            translate_stencil_op(state.stencil_fail),
            translate_stencil_op(state.depth_fail),
            translate_stencil_op(state.depth_pass));
        glStencilFuncSeparate(face, translate_stencil_func(state.func), state.ref, state.compare_mask);
        glStencilMaskSeparate(face, state.write_mask);
    }

    static GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format) {
        R_PROFILE(__func__);

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
        case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED:
            LOG_WARN("Unsupported attribute format SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED");
            return GL_UNSIGNED_BYTE;
        }

        LOG_ERROR("Unsupported attribute format {}", log_hex(format));
        return GL_UNSIGNED_BYTE;
    }

    static GLboolean attribute_format_normalised(SceGxmAttributeFormat format) {
        R_PROFILE(__func__);

        switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
            return GL_TRUE;
        default:
            return GL_FALSE;
        }
    }

    bool create(Context &context, SDL_Window *window) {
        R_PROFILE(__func__);

        assert(SDL_GL_GetCurrentContext() == nullptr);
        context.gl = GLContextPtr(SDL_GL_CreateContext(window), SDL_GL_DeleteContext);
        assert(context.gl != nullptr);

        const glbinding::GetProcAddress get_proc_address = [](const char *name) {
            return reinterpret_cast<ProcAddress>(SDL_GL_GetProcAddress(name));
        };
        Binding::initialize(get_proc_address, false);

        LOG_INFO("GL_VERSION = {}", glGetString(GL_VERSION));
        LOG_INFO("GL_SHADING_LANGUAGE_VERSION = {}", glGetString(GL_SHADING_LANGUAGE_VERSION));

        // TODO This is just for debugging.
        glClearColor(0.0625f, 0.125f, 0.25f, 0);

        if (!init(context.texture_cache) || !context.vertex_array.init(glGenVertexArrays, glDeleteVertexArrays) || !context.element_buffer.init(glGenBuffers, glDeleteBuffers) || !context.stream_vertex_buffers.init(glGenBuffers, glDeleteBuffers)) {
            return false;
        }

        glBindVertexArray(context.vertex_array[0]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);

        return true;
    }

    bool create(RenderTarget &rt, const SceGxmRenderTargetParams &params) {
        R_PROFILE(__func__);

        if (!rt.renderbuffers.init(glGenRenderbuffers, glDeleteRenderbuffers) || !rt.framebuffer.init(glGenFramebuffers, glDeleteFramebuffers)) {
            return false;
        }

        glBindRenderbuffer(GL_RENDERBUFFER, rt.renderbuffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, params.width, params.height);
        glBindRenderbuffer(GL_RENDERBUFFER, rt.renderbuffers[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, params.width, params.height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer[0]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt.renderbuffers[0]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.renderbuffers[1]);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return true;
    }

    bool create(FragmentProgram &fp, State &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, const char *base_path) {
        R_PROFILE(__func__);

        fp.glsl = get_fragment_glsl(state.fragment_glsl_cache, program, base_path);

        // Translate blending.
        if (blend != nullptr) {
            fp.color_mask_red = ((blend->colorMask & SCE_GXM_COLOR_MASK_R) != 0) ? GL_TRUE : GL_FALSE;
            fp.color_mask_green = ((blend->colorMask & SCE_GXM_COLOR_MASK_G) != 0) ? GL_TRUE : GL_FALSE;
            fp.color_mask_blue = ((blend->colorMask & SCE_GXM_COLOR_MASK_B) != 0) ? GL_TRUE : GL_FALSE;
            fp.color_mask_alpha = ((blend->colorMask & SCE_GXM_COLOR_MASK_A) != 0) ? GL_TRUE : GL_FALSE;
            fp.blend_enabled = true;
            fp.color_func = translate_blend_func(blend->colorFunc);
            fp.alpha_func = translate_blend_func(blend->alphaFunc);
            fp.color_src = translate_blend_factor(blend->colorSrc);
            fp.color_dst = translate_blend_factor(blend->colorDst);
            fp.alpha_src = translate_blend_factor(blend->alphaSrc);
            fp.alpha_dst = translate_blend_factor(blend->alphaDst);
        }

        return true;
    }

    bool create(VertexProgram &vp, State &state, const SceGxmProgram &program, const char *base_path) {
        R_PROFILE(__func__);

        vp.glsl = get_vertex_glsl(state.vertex_glsl_cache, program, base_path);
        vp.attribute_locations = attribute_locations(program);

        return true;
    }

    void begin_scene(const RenderTarget &rt) {
        R_PROFILE(__func__);

        glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer[0]);

        // TODO This is just for debugging.
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void end_scene(Context &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels) {
        R_PROFILE(__func__);

        glPixelStorei(GL_PACK_ROW_LENGTH, stride_in_pixels);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);

        flip_vertically(pixels, width, height, stride_in_pixels);

        ++context.texture_cache.timestamp;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool sync_state(Context &context, const GxmContextState &state, const MemState &mem, bool enable_texture_cache) {
        R_PROFILE(__func__);

        // TODO Use some kind of caching to avoid setting every draw call?
        const SharedGLObject program = get_program(context, state, mem);
        if (!program) {
            return false;
        }
        glUseProgram(program->get());

        // Viewport.
        const GLsizei display_w = state.color_surface.pbeEmitWords[0];
        const GLsizei display_h = state.color_surface.pbeEmitWords[1];
        const GxmViewport &viewport = state.viewport;
        if (viewport.enable == SCE_GXM_VIEWPORT_ENABLED) {
            const GLfloat w = viewport.scale.x * 2;
            const GLfloat h = viewport.scale.y * -2;
            const GLfloat x = viewport.offset.x - viewport.scale.x;
            const GLfloat y = display_h - (viewport.offset.y - viewport.scale.y);
            glViewportIndexedf(0, x, y, w, h);
            glDepthRange(viewport.offset.z - viewport.scale.z, viewport.offset.z + viewport.scale.z);
        } else {
            glViewport(0, 0, display_w, display_h);
            glDepthRange(0, 1);
        }

        // Clipping.
        // TODO: There was some math here to round the scissor rect, but it looked potentially incorrect.
        const GLsizei scissor_x = state.region_clip_min.x;
        const GLsizei scissor_y = display_h - state.region_clip_min.y;
        const GLsizei scissor_w = state.region_clip_max.x - state.region_clip_min.x;
        const GLsizei scissor_h = state.region_clip_max.y - state.region_clip_min.y;
        switch (state.region_clip_mode) {
        case SCE_GXM_REGION_CLIP_NONE:
            glDisable(GL_SCISSOR_TEST);
            break;
        case SCE_GXM_REGION_CLIP_ALL:
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, 0, 0);
            break;
        case SCE_GXM_REGION_CLIP_OUTSIDE:
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor_x, scissor_y, scissor_w, scissor_h);
            break;
        case SCE_GXM_REGION_CLIP_INSIDE:
            // TODO: Implement SCE_GXM_REGION_CLIP_INSIDE
            glDisable(GL_SCISSOR_TEST);
            LOG_WARN("Unimplemented region clip mode used: SCE_GXM_REGION_CLIP_INSIDE");
            break;
        }

        // Culling.
        switch (state.cull_mode) {
        case SCE_GXM_CULL_CCW:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case SCE_GXM_CULL_CW:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case SCE_GXM_CULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
        }

        // Depth test.
        if (state.depth_stencil_surface.depthData) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(translate_depth_func(state.front_depth_func));
            glDepthMask(state.front_depth_write_enable == SCE_GXM_DEPTH_WRITE_ENABLED ? GL_TRUE : GL_FALSE);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        // Stencil.
        if (state.depth_stencil_surface.stencilData) {
            glEnable(GL_STENCIL_TEST);
            apply_stencil_state(GL_FRONT, state.front_stencil);
            apply_stencil_state(GL_BACK, state.back_stencil);
        } else {
            glDisable(GL_STENCIL_TEST);
        }

        // Blending.
        const FragmentProgram &fragment_program = *state.fragment_program.get(mem)->renderer.get();
        glColorMask(fragment_program.color_mask_red, fragment_program.color_mask_green, fragment_program.color_mask_blue, fragment_program.color_mask_alpha);
        if (fragment_program.blend_enabled) {
            glEnable(GL_BLEND);
            glBlendEquationSeparate(fragment_program.color_func, fragment_program.alpha_func);
            glBlendFuncSeparate(fragment_program.color_src, fragment_program.color_dst, fragment_program.alpha_src, fragment_program.alpha_dst);
        } else {
            glDisable(GL_BLEND);
        }

        // Textures.
        for (size_t i = 0; i < SCE_GXM_MAX_TEXTURE_UNITS; ++i) {
            const SceGxmTexture &texture = state.fragment_textures[i];
            if (texture.data_addr != 0) {
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
                cache_and_bind_texture(context.texture_cache, texture, mem, enable_texture_cache);
            }
        }
        glActiveTexture(GL_TEXTURE0);

        // Uniforms.
        set_uniforms(program->get(), context, state, mem);

        // Vertex attributes.
        const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
        for (const emu::SceGxmVertexAttribute &attribute : vertex_program.attributes) {
            const SceGxmVertexStream &stream = vertex_program.streams[attribute.streamIndex];
            const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
            const GLenum type = attribute_format_to_gl_type(attribute_format);
            const GLboolean normalised = attribute_format_normalised(attribute_format);
            const int attrib_location = attribute.regIndex / sizeof(uint32_t);

            glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[attribute.streamIndex]);
            glVertexAttribPointer(attrib_location, attribute.componentCount, type, normalised, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset));
            glEnableVertexAttribArray(attrib_location);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return true;
    }

    void finish(Context &context) {
        glFinish();
    }
}
