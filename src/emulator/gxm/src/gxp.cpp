#include <gxm/functions.h>

#include <gxm/types.h>
#include <util/log.h>

namespace gxp {

const char *log_parameter_semantic(const SceGxmProgramParameter &parameter) {
    SceGxmParameterSemantic semantic = static_cast<SceGxmParameterSemantic>(parameter.semantic);

    // clang-format off
    switch (semantic) {
    case SCE_GXM_PARAMETER_SEMANTIC_NONE: return "NONE";
    case SCE_GXM_PARAMETER_SEMANTIC_ATTR: return "ATTR";
    case SCE_GXM_PARAMETER_SEMANTIC_BCOL: return "BCOL";
    case SCE_GXM_PARAMETER_SEMANTIC_BINORMAL: return "BINORMAL";
    case SCE_GXM_PARAMETER_SEMANTIC_BLENDINDICES: return "BLENDINDICES";
    case SCE_GXM_PARAMETER_SEMANTIC_BLENDWEIGHT: return "BLENDWEIGHT";
    case SCE_GXM_PARAMETER_SEMANTIC_COLOR: return "COLOR";
    case SCE_GXM_PARAMETER_SEMANTIC_DIFFUSE: return "DIFFUSE";
    case SCE_GXM_PARAMETER_SEMANTIC_FOGCOORD: return "FOGCOORD";
    case SCE_GXM_PARAMETER_SEMANTIC_NORMAL: return "NORMAL";
    case SCE_GXM_PARAMETER_SEMANTIC_POINTSIZE: return "POINTSIZE";
    case SCE_GXM_PARAMETER_SEMANTIC_POSITION: return "POSITION";
    case SCE_GXM_PARAMETER_SEMANTIC_SPECULAR: return "SPECULAR";
    case SCE_GXM_PARAMETER_SEMANTIC_TANGENT: return "TANGENT";
    case SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD: return "TEXCOORD";
    default: return "UNKNOWN";
    }
    // clang-format on
}

void log_parameter(const SceGxmProgramParameter &parameter) {
    std::string category;
    switch (parameter.category) {
    case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE:
        category = "Vertex attribute";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
        category = "Uniform";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_SAMPLER:
        category = "Sampler";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE:
        category = "Auxiliary surface";
        break;
    case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER:
        category = "Uniform buffer";
        break;
    default:
        category = "Unknown type";
        break;
    }
    LOG_DEBUG("{}: name:{:s} semantic:{} type:{:d} component_count:{} container_index:{}",
        category, parameter_name_raw(parameter), log_parameter_semantic(parameter), parameter.type, uint8_t(parameter.component_count), log_hex(uint8_t(parameter.container_index)));
}

const SceGxmProgramParameter *program_parameters(const SceGxmProgram &program) {
    return reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program.parameters_offset) + program.parameters_offset);
}

SceGxmParameterType parameter_type(const SceGxmProgramParameter &parameter) {
    return static_cast<SceGxmParameterType>(
        static_cast<uint16_t>(parameter.type));
}

GenericParameterType parameter_generic_type(const SceGxmProgramParameter &parameter) {
    if (parameter.component_count > 1) {
        if (parameter.array_size > 1) {
            return GenericParameterType::Matrix;
        } else {
            return GenericParameterType::Vector;
        }
    } else {
        return GenericParameterType::Scalar;
    }
}
std::string parameter_name_raw(const SceGxmProgramParameter &parameter) {
    const uint8_t *const bytes = reinterpret_cast<const uint8_t *>(&parameter);
    return reinterpret_cast<const char *>(bytes + parameter.name_offset);
}

std::string parameter_name(const SceGxmProgramParameter &parameter) {
    auto full_name = gxp::parameter_name_raw(parameter);
    auto struct_idx = full_name.find('.');
    bool is_struct_field = struct_idx != std::string::npos;

    if (is_struct_field)
        return full_name.substr(struct_idx + 1, full_name.length());
    else
        return full_name;
}

std::string parameter_struct_name(const SceGxmProgramParameter &parameter) {
    auto full_name = gxp::parameter_name_raw(parameter);
    auto struct_idx = full_name.find('.');
    bool is_struct_field = struct_idx != std::string::npos;

    if (is_struct_field)
        return full_name.substr(0, struct_idx);
    else
        return "";
}

SceGxmVertexProgramOutputs get_vertex_outputs(const SceGxmProgram &program) {
    if (!program.is_vertex())
        return _SCE_GXM_VERTEX_PROGRAM_OUTPUT_INVALID;

    auto vertex_outputs_ptr = reinterpret_cast<const SceGxmProgramVertexOutput *>(reinterpret_cast<const std::uint8_t *>(&program.vertex_outputs_offset) + program.vertex_outputs_offset);
    const std::uint32_t vo1 = vertex_outputs_ptr->vertex_outputs1;
    const std::uint32_t vo2 = vertex_outputs_ptr->vertex_outputs2;

    const bool has_fog = vo1 & 0x200;
    const bool has_color = vo1 & 0x800;

    int res = SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0 | SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION;
    int res_nocolor = SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION;

    if (has_fog) {
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG;
        res_nocolor |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG;
    }

    if (!has_color)
        res = res_nocolor;

    if (vo1 & 0x400)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1;
    if (vo2 & 7)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0;
    if (vo2 & 0x38)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD1;
    if (vo2 & 0x1C0)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD2;
    if (vo2 & 0xE00)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD3;
    if (vo2 & 0x7000)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD4;
    if (vo2 & 0x38000)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD5;
    if (vo2 & 0x1C0000)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD6;
    if (vo2 & 0xE00000)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD7;
    if (vo2 & 0x7000000)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD8;
    if (vo2 & 0x38000000)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9;
    if (vo1 & 0x100)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_PSIZE;
    if (vo1 & 1)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP0;
    if (vo1 & 2)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP1;
    if (vo1 & 4)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP2;
    if (vo1 & 8)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP3;
    if (vo1 & 0x10)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP4;
    if (vo1 & 0x20)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP5;
    if (vo1 & 0x40)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP6;
    if (vo1 & 0x80)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP7;

    return static_cast<SceGxmVertexProgramOutputs>(res);
}

} // namespace gxp
