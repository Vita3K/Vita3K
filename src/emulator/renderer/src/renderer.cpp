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

    void finish(Context &context) {
        glFinish();
    }
}
