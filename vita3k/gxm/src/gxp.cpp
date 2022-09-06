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

#include <gxm/functions.h>

#include <gxm/types.h>
#include <util/log.h>

#include <algorithm>
#include <string>

namespace gxp {

const static char *log_parameter_semantic(const SceGxmProgramParameter &parameter) {
    // clang-format off
    switch (parameter.semantic) {
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
    std::string_view category;
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
    LOG_DEBUG("{}: name:{:s} semantic:{} type:{:d} component_count:{} container_index:0x{:0X} semantic_index:{} array_size:{} resource_index:{}",
        category, parameter.name(), log_parameter_semantic(parameter), uint8_t(parameter.type), uint8_t(parameter.component_count), uint8_t(parameter.container_index),
        parameter.semantic_index, parameter.array_size, parameter.resource_index);
}

int get_parameter_type_size(const SceGxmParameterType type) {
    switch (type) {
    case SCE_GXM_PARAMETER_TYPE_F32:
    case SCE_GXM_PARAMETER_TYPE_U32:
    case SCE_GXM_PARAMETER_TYPE_S32:
        return 4;

    case SCE_GXM_PARAMETER_TYPE_F16:
    case SCE_GXM_PARAMETER_TYPE_U16:
    case SCE_GXM_PARAMETER_TYPE_S16:
        return 2;

    case SCE_GXM_PARAMETER_TYPE_U8:
    case SCE_GXM_PARAMETER_TYPE_S8:
        return 1;

    default:
        break;
    }

    return 4;
}

int get_num_32_bit_components(const SceGxmParameterType type, const uint16_t num_comp) {
    const int param_size = get_parameter_type_size(type);
    return (num_comp + (4 / param_size - 1)) * param_size / 4;
}

GenericParameterType parameter_generic_type(const SceGxmProgramParameter &parameter) {
    if (parameter.array_size > 1) {
        return GenericParameterType::Array;
    } else if (parameter.component_count > 1) {
        return GenericParameterType::Vector;
    } else {
        return GenericParameterType::Scalar;
    }
}

std::string parameter_name(const SceGxmProgramParameter &parameter) {
    std::string full_name = parameter.name();
    const std::size_t dot_pos = full_name.find_first_of('.');
    const bool is_struct_type = dot_pos != std::string::npos;
    std::replace(full_name.begin(), full_name.end(), '.', '_');

    // replace brackets by underscores as on some drivers (Adreno) they cause the shader to crash
    std::replace(full_name.begin(), full_name.end(), '[', '_');
    std::replace(full_name.begin(), full_name.end(), ']', '_');

    return full_name;
}

std::string parameter_struct_name(const SceGxmProgramParameter &parameter) {
    std::string full_name = parameter.name();
    auto struct_idx = full_name.find('.');
    bool is_struct_field = struct_idx != std::string::npos;

    if (is_struct_field)
        return full_name.substr(0, struct_idx);
    else
        return "";
}

SceGxmVertexProgramOutputs get_vertex_outputs(const SceGxmProgram &program, GxmVertexOutputTexCoordInfos *coord_infos) {
    if (!program.is_vertex())
        return _SCE_GXM_VERTEX_PROGRAM_OUTPUT_INVALID;

    auto vertex_varyings_ptr = program.vertex_varyings();

    const uint32_t vo1 = vertex_varyings_ptr->vertex_outputs1;
    const uint32_t vo2 = vertex_varyings_ptr->vertex_outputs2;

    int res = SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION;

    const bool has_fog = vo1 & 0x200;
    if (has_fog)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG;

    const bool has_color = vo1 & 0x800;
    if (has_color)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0;

    if (vo1 & 0x400)
        res |= SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1;

    uint32_t coordinfo_mask = 0b111;
    uint32_t to_shift = 0;

    for (uint8_t i = 0; i < 10; i++) {
        if (vo2 & coordinfo_mask) {
            res |= (SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0 << i);

            if (coord_infos)
                (*coord_infos)[i] = static_cast<uint8_t>((vo2 >> to_shift) & 0b111u);
        }

        to_shift += 3;
        coordinfo_mask <<= 3;
    }

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

SceGxmFragmentProgramInputs get_fragment_inputs(const SceGxmProgram &program) {
    if (!program.is_fragment())
        return _SCE_GXM_FRAGMENT_PROGRAM_INPUT_NONE;

    const SceGxmProgramVertexVaryings *vo_ptr = program.vertex_varyings();

    // following code is adapted from decompilation
    const SceGxmProgramAttributeDescriptor *current_interpolant = vo_ptr->frag_attribute_descriptor();
    const SceGxmProgramAttributeDescriptor *const interpolants_end = current_interpolant + vo_ptr->varyings_count;

    uint32_t result = _SCE_GXM_FRAGMENT_PROGRAM_INPUT_NONE;

    while (current_interpolant != interpolants_end) {
        uint32_t interpolant_info = current_interpolant->attribute_info;

        if ((interpolant_info & 0xF) != 0xF) {
            if (interpolant_info & 0x400)
                result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_SPRITECOORD;
            else {
                switch (interpolant_info & 0xF) {
                case 0: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD0; break;
                case 1: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD1; break;
                case 2: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD2; break;
                case 3: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD3; break;
                case 4: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD4; break;
                case 5: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD5; break;
                case 6: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD6; break;
                case 7: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD7; break;
                case 8: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD8; break;
                case 9: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD9; break;
                default: break;
                }
            }
        }

        if ((interpolant_info & 0xF000) != 0xF000) {
            if (interpolant_info & 0x40000000)
                result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_SPRITECOORD;
            else {
                switch (interpolant_info & 0xF000) {
                case 0x0000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD0; break;
                case 0x1000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD1; break;
                case 0x2000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD2; break;
                case 0x3000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD3; break;
                case 0x4000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD4; break;
                case 0x5000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD5; break;
                case 0x6000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD6; break;
                case 0x7000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD7; break;
                case 0x8000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD8; break;
                case 0x9000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD9; break;
                case 0xA000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_COLOR0; break;
                case 0xB000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_COLOR1; break;
                case 0xC000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_FOG; break;
                case 0xD000: result |= SCE_GXM_FRAGMENT_PROGRAM_INPUT_POSITION; break;
                default: break;
                }
            }
        }

        current_interpolant++;
    }

    return static_cast<SceGxmFragmentProgramInputs>(result);
}

const SceGxmProgramParameterContainer *get_container_by_index(const SceGxmProgram &program, const uint16_t idx) {
    const SceGxmProgramParameterContainer *container = program.container();

    for (uint32_t i = 0; i < program.container_count; i++) {
        if (container[i].container_index == idx) {
            return &container[i];
        }
    }

    return nullptr;
}

int get_uniform_buffer_base(const SceGxmProgram &program, const SceGxmProgramParameter &parameter) {
    assert(parameter.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER);

    auto container = get_container_by_index(program, 19);
    int base = (container ? container->base_sa_offset : 0);

    const SceGxmUniformBufferInfo *info = program.uniform_buffer();

    if (program.uniform_buffer_count == 1) {
        base += info->ldst_base_offset;
    } else {
        for (uint32_t i = 0; i < program.uniform_buffer_count; i++) {
            if (info[i].reside_buffer == parameter.resource_index) {
                base += info[i].ldst_base_offset;
            }
        }
    }

    return base;
}

const char *get_container_name(const uint16_t idx) {
    switch (idx) {
    case 0:
        return "BUFFER0 ";
    case 1:
        return "BUFFER1 ";
    case 2:
        return "BUFFER2 ";
    case 3:
        return "BUFFER3 ";
    case 4:
        return "BUFFER4 ";
    case 5:
        return "BUFFER5 ";
    case 6:
        return "BUFFER6 ";
    case 7:
        return "BUFFER7 ";
    case 8:
        return "BUFFER8 ";
    case 9:
        return "BUFFER9 ";
    case 10:
        return "BUFFER10";
    case 11:
        return "BUFFER11";
    case 12:
        return "BUFFER12";
    case 13:
        return "BUFFER13";
    case 14:
        return "DEFAULT ";
    case 15:
        return "TEXTURE ";
    case 16:
        return "LITERAL ";
    case 17:
        return "SCRATCH ";
    case 18:
        return "THREAD  ";
    case 19:
        return "DATA    ";
    default:
        break;
    }

    return "INVALID ";
}

TextureInfo get_textures_used(const SceGxmProgram &program_gxp) {
    TextureInfo textures_used;

    const auto parameters = program_gxp.program_parameters();

    for (uint32_t i = 0; i < program_gxp.parameter_count; ++i) {
        const auto parameter = parameters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
            textures_used[parameter.resource_index] = true;
        }
    }

    // symbols may be stripped, look for an anonymous texture
    auto vertex_varyings_ptr = program_gxp.vertex_varyings();
    const SceGxmProgramAttributeDescriptor *descriptor = vertex_varyings_ptr->frag_attribute_descriptor();

    for (uint16_t i = 0; i < vertex_varyings_ptr->varyings_count; i++, descriptor++) {
        const uint32_t tex_coord_index = (descriptor->attribute_info & 0x40F);
        if (tex_coord_index != 0xF)
            textures_used[descriptor->resource_index] = true;
    }

    // also look for an anonymous sampler
    const SceGxmProgramParameterContainer *container = gxp::get_container_by_index(program_gxp, 19);
    if (container) {
        const SceGxmDependentSampler *dependent_samplers = program_gxp.dependent_sampler();

        for (uint32_t i = 0; i < program_gxp.dependent_sampler_count; i++) {
            const uint16_t rsc_index = dependent_samplers[i].resource_index_layout_offset / 4;
            textures_used[rsc_index] = true;
        }
    }

    return textures_used;
}
} // namespace gxp
