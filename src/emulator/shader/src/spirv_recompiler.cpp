// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <shader/spirv_recompiler.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <shader/profile.h>
#include <shader/usse_translator_entry.h>
#include <shader/usse_translator_types.h>
#include <util/fs.h>
#include <util/log.h>

#include <SPIRV/SpvBuilder.h>
#include <SPIRV/disassemble.h>
#include <boost/optional.hpp>
#include <spirv_glsl.hpp>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

using boost::optional;

static constexpr bool LOG_SHADER_CODE = true;
static constexpr bool DUMP_SPIRV_BINARIES = false;

using namespace shader::usse;

namespace shader {

// **************
// * Prototypes *
// **************

static spv::Id get_type_fallback(spv::Builder &b);

// ******************
// * Helper structs *
// ******************

// Keeps track of current struct declaration
// TODO: Handle struct arrays and multiple struct instances.
//       The current (and the former) approach is quite naive, in that it assumes:
//           1) there is only one struct instance per declared struct
//           2) there are no struct array instances
struct StructDeclContext {
    std::string name;
    usse::RegisterBank reg_type = usse::RegisterBank::INVALID;
    std::vector<spv::Id> field_ids;
    std::vector<std::string> field_names; // count must be equal to `field_ids`
    bool is_interaface_block{ false };

    bool empty() const { return name.empty(); }
    void clear() { *this = {}; }
};

struct VertexProgramOutputProperties {
    std::string name;
    std::uint32_t component_count;

    VertexProgramOutputProperties()
        : name(nullptr)
        , component_count(0) {}

    VertexProgramOutputProperties(const char *name, std::uint32_t component_count)
        : name(name)
        , component_count(component_count) {}
};
using VertexProgramOutputPropertiesMap = std::map<SceGxmVertexProgramOutputs, VertexProgramOutputProperties>;

struct FragmentProgramInputProperties {
    std::string name;
    std::uint32_t component_count;

    FragmentProgramInputProperties()
        : name(nullptr)
        , component_count(0) {}

    FragmentProgramInputProperties(const char *name, std::uint32_t component_count)
        : name(name)
        , component_count(component_count) {}
};
using FragmentProgramInputPropertiesMap = std::map<SceGxmFragmentProgramInputs, FragmentProgramInputProperties>;

// ******************************
// * Functions (implementation) *
// ******************************

static spv::Id create_array_if_needed(spv::Builder &b, const spv::Id param_id, const SceGxmProgramParameter &parameter, const uint32_t explicit_array_size = 0) {
    // disabled
    return param_id;

    const auto array_size = explicit_array_size == 0 ? parameter.array_size : explicit_array_size;
    if (array_size > 1) {
        const auto array_size_id = b.makeUintConstant(array_size);
        return b.makeArrayType(param_id, array_size_id, 0);
    }
    return param_id;
}

static spv::Id get_type_basic(spv::Builder &b, const SceGxmProgramParameter &parameter) {
    SceGxmParameterType type = gxp::parameter_type(parameter);

    switch (type) {
    // clang-format off
    case SCE_GXM_PARAMETER_TYPE_F16: return b.makeFloatType(32); // TODO: support f16
    case SCE_GXM_PARAMETER_TYPE_F32: return b.makeFloatType(32);
    case SCE_GXM_PARAMETER_TYPE_U8: return b.makeUintType(8);
    case SCE_GXM_PARAMETER_TYPE_U16: return b.makeUintType(16);
    case SCE_GXM_PARAMETER_TYPE_U32: return b.makeUintType(32);
    case SCE_GXM_PARAMETER_TYPE_S8: return b.makeIntType(8);
    case SCE_GXM_PARAMETER_TYPE_S16: return b.makeIntType(16);
    case SCE_GXM_PARAMETER_TYPE_S32: return b.makeIntType(32);
    // clang-format on
    default: {
        LOG_ERROR("Unsupported parameter type {} used in shader.", log_hex(type));
        return get_type_fallback(b);
    }
    }
}

static spv::Id get_type_fallback(spv::Builder &b) {
    return b.makeFloatType(32);
}

static spv::Id get_type_scalar(spv::Builder &b, const SceGxmProgramParameter &parameter) {
    spv::Id param_id = get_type_basic(b, parameter);
    param_id = create_array_if_needed(b, param_id, parameter);
    return param_id;
}

static spv::Id get_type_vector(spv::Builder &b, const SceGxmProgramParameter &parameter) {
    spv::Id param_id = get_type_basic(b, parameter);
    param_id = b.makeVectorType(param_id, parameter.component_count);
    return param_id;
}

static spv::Id get_type_array(spv::Builder &b, const SceGxmProgramParameter &parameter) {
    spv::Id param_id = get_type_basic(b, parameter);
    if (parameter.component_count > 1) {
        param_id = b.makeVectorType(param_id, parameter.component_count);
    }

    // TODO: Stride
    param_id = b.makeArrayType(param_id, b.makeUintConstant(parameter.array_size), 1);

    return param_id;
}

static spv::Id get_param_type(spv::Builder &b, const SceGxmProgramParameter &parameter) {
    std::string name = gxp::parameter_name_raw(parameter);
    gxp::GenericParameterType param_type = gxp::parameter_generic_type(parameter);

    switch (param_type) {
    case gxp::GenericParameterType::Scalar:
        return get_type_scalar(b, parameter);
    case gxp::GenericParameterType::Vector:
        return get_type_vector(b, parameter);
    case gxp::GenericParameterType::Array:
        return get_type_array(b, parameter);
    default:
        return get_type_fallback(b);
    }
}

static void sanitize_variable_name(std::string &var_name) {
    // Remove consecutive occurences of the character '_'
    var_name.erase(
        std::unique(var_name.begin(), var_name.end(),
            [](char c1, char c2) { return c1 == c2 && c2 == '_'; }),
        var_name.end());
}

spv::StorageClass reg_type_to_spv_storage_class(usse::RegisterBank reg_type) {
    switch (reg_type) {
    case usse::RegisterBank::TEMP:
        return spv::StorageClassPrivate;
    case usse::RegisterBank::PRIMATTR:
        return spv::StorageClassInput;
    case usse::RegisterBank::OUTPUT:
        return spv::StorageClassOutput;
    case usse::RegisterBank::SECATTR:
        return spv::StorageClassUniformConstant;
    case usse::RegisterBank::FPINTERNAL:
        return spv::StorageClassPrivate;

    case usse::RegisterBank::SPECIAL: break;
    case usse::RegisterBank::GLOBAL: break;
    case usse::RegisterBank::FPCONSTANT: break;
    case usse::RegisterBank::IMMEDIATE: break;
    case usse::RegisterBank::INDEX: break;
    case usse::RegisterBank::INDEXED: break;

    case usse::RegisterBank::MAXIMUM:
    case usse::RegisterBank::INVALID:
    default:
        return spv::StorageClassMax;
    }

    LOG_WARN("Unsupported reg_type {}", static_cast<uint32_t>(reg_type));
    return spv::StorageClassMax;
}

static spv::Id create_spirv_var_reg(spv::Builder &b, SpirvShaderParameters &parameters, std::string &name, usse::RegisterBank reg_type, uint32_t size, spv::Id type, optional<spv::StorageClass> force_storage = boost::none, boost::optional<std::uint32_t> force_offset = boost::none) {
    sanitize_variable_name(name);

    const auto storage_class = force_storage ? *force_storage : reg_type_to_spv_storage_class(reg_type);
    spv::Id var_id = b.createVariable(storage_class, type, name.c_str());

    SpirvVarRegBank *var_group;

    switch (reg_type) {
    case usse::RegisterBank::SECATTR:
        var_group = &parameters.uniforms;
        break;
    case usse::RegisterBank::PRIMATTR:
        var_group = &parameters.ins;
        break;
    case usse::RegisterBank::OUTPUT:
        var_group = &parameters.outs;
        break;
    case usse::RegisterBank::TEMP:
        var_group = &parameters.temps;
        break;
    case usse::RegisterBank::FPINTERNAL:
        var_group = &parameters.internals;
        break;
    default:
        LOG_WARN("Unsupported reg_type {}", static_cast<uint32_t>(reg_type));
        return spv::NoResult;
    }

    if (force_offset) {
        var_group->set_next_offset(force_offset.value());
    }

    var_group->push({ type, var_id }, size);

    return var_id;
}

static spv::Id create_struct(spv::Builder &b, SpirvShaderParameters &parameters, StructDeclContext &param_struct, emu::SceGxmProgramType program_type) {
    assert(param_struct.field_ids.size() == param_struct.field_names.size());

    const spv::Id struct_type_id = b.makeStructType(param_struct.field_ids, param_struct.name.c_str());

    // NOTE: This will always be true until we support uniform structs (see comment below)
    if (param_struct.is_interaface_block)
        b.addDecoration(struct_type_id, spv::DecorationBlock);

    for (auto field_index = 0; field_index < param_struct.field_ids.size(); ++field_index) {
        const auto field_name = param_struct.field_names[field_index];

        b.addMemberName(struct_type_id, field_index, field_name.c_str());
    }

    // TODO: Size doesn't make sense here, so just use 1
    const spv::Id struct_var_id = create_spirv_var_reg(b, parameters, param_struct.name, param_struct.reg_type, 1, struct_type_id);

    param_struct.clear();
    return struct_var_id;
}

static spv::Id create_param_sampler(spv::Builder &b, SpirvShaderParameters &parameters, const SceGxmProgramParameter &parameter) {
    spv::Id sampled_type = b.makeFloatType(32);
    spv::Id image_type = b.makeImageType(sampled_type, spv::Dim2D, false, false, false, 1, spv::ImageFormatUnknown);
    spv::Id sampled_image_type = b.makeSampledImageType(image_type);
    std::string name = gxp::parameter_name_raw(parameter);

    return b.createVariable(spv::StorageClassUniformConstant, sampled_image_type, name.c_str());
}

static void create_vertex_outputs(spv::Builder &b, SpirvShaderParameters &parameters, const SceGxmProgram &program) {
    auto set_property = [](SceGxmVertexProgramOutputs vo, const char *name, std::uint32_t component_count) {
        return std::make_pair(vo, VertexProgramOutputProperties(name, component_count));
    };

    // TODO: Verify component counts
    static const VertexProgramOutputPropertiesMap vertex_properties_map = {
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION, "v_Position", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG, "v_Fog", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0, "v_Color0", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1, "v_Color1", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0, "v_TexCoord0", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD1, "v_TexCoord1", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD2, "v_TexCoord2", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD3, "v_TexCoord3", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD4, "v_TexCoord4", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD5, "v_TexCoord5", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD6, "v_TexCoord6", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD7, "v_TexCoord7", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD8, "v_TexCoord8", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9, "v_TexCoord9", 2),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_PSIZE, "v_Psize", 1),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP0, "v_Clip0", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP1, "v_Clip1", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP2, "v_Clip2", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP3, "v_Clip3", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP4, "v_Clip4", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP5, "v_Clip5", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP6, "v_Clip6", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP7, "v_Clip7", 4),
    };

    const SceGxmVertexProgramOutputs vertex_outputs = gxp::get_vertex_outputs(program);

    for (int vo = SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION; vo < _SCE_GXM_VERTEX_PROGRAM_OUTPUT_LAST; vo <<= 1) {
        if (vertex_outputs & vo) {
            const auto vo_typed = static_cast<SceGxmVertexProgramOutputs>(vo);
            VertexProgramOutputProperties properties = vertex_properties_map.at(vo_typed);

            const spv::Id out_type = b.makeVectorType(b.makeFloatType(32), properties.component_count);
            const spv::Id out_var = create_spirv_var_reg(b, parameters, properties.name, usse::RegisterBank::OUTPUT, properties.component_count, out_type);

            // TODO: More decorations needed?
            if (vo == SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION)
                b.addDecoration(out_var, spv::DecorationBuiltIn, spv::BuiltInPosition);
        }
    }
}

static void create_fragment_inputs(spv::Builder &b, SpirvShaderParameters &parameters, NonDependentTextureQueryCallInfos &tex_query_infos, SamplerMap &samplers, const SceGxmProgram &program) {
    static const std::unordered_map<std::uint32_t, std::string> name_map = {
        { 0xD000, "v_Position" },
        { 0xC000, "v_Fog" },
        { 0xA000, "v_Color0" },
        { 0xB000, "v_Color1" },
        { 0x0, "v_TexCoord0" },
        { 0x1000, "v_TexCoord1" },
        { 0x2000, "v_TexCoord2" },
        { 0x3000, "v_TexCoord3" },
        { 0x4000, "v_TexCoord4" },
        { 0x5000, "v_TexCoord5" },
        { 0x6000, "v_TexCoord6" },
        { 0x7000, "v_TexCoord7" },
        { 0x8000, "v_TexCoord8" },
        { 0x9000, "v_TexCoord9" },
    };

    // Both vertex output and this struct should stay in a larger varying struct
    auto vertex_outputs_ptr = reinterpret_cast<const SceGxmProgramVertexOutput *>(
        reinterpret_cast<const std::uint8_t *>(&program.varyings_offset) + program.varyings_offset);

    const SceGxmProgramAttributeDescriptor *descriptor = reinterpret_cast<const SceGxmProgramAttributeDescriptor *>(
        reinterpret_cast<const std::uint8_t *>(&vertex_outputs_ptr->vertex_outputs1) + vertex_outputs_ptr->vertex_outputs1);

    std::uint32_t pa_offset = 0;
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);

    // Store the coords
    std::array<spv::Id, 9> coords;
    std::fill(coords.begin(), coords.end(), spv::NoResult);

    // It may actually be total fragments input
    for (size_t i = 0; i < vertex_outputs_ptr->varyings_count; i++, descriptor++) {
        // 4 bit flag indicates a PA!
        if ((descriptor->attribute_info & 0x4000F000) != 0xF000) {
            const uint32_t input_id = (descriptor->attribute_info & 0x4000F000);
            std::string pa_name;

            if (input_id & 0x40000000) {
                pa_name = "v_SpriteCoord";
            } else {
                pa_name = name_map.at(input_id);
            }

            std::string pa_type = "uchar";

            uint32_t input_type = (descriptor->attribute_info & 0x30100000);

            if (input_type == 0x20000000) {
                pa_type = "half";
            } else if (input_type == 0x10000000) {
                pa_type = "fixed";
            } else if (input_type == 0x100000) {
                if (input_id == 0xA000 || input_id == 0xB000) {
                    pa_type = "float";
                }
            } else if (input_id != 0xA000 && input_id != 0xB000) {
                pa_type = "float";
            }

            // Create PA Iterator SPIR-V variable
            const auto num_comp = ((descriptor->attribute_info >> 22) & 3) + 1;
            const auto pa_iter_type = b.makeVectorType(b.makeFloatType(32), num_comp);
            const auto pa_iter_size = ((descriptor->size >> 4) & 3) + 1;
            const auto pa_iter_var = create_spirv_var_reg(b, parameters, pa_name, RegisterBank::PRIMATTR, pa_iter_size, pa_iter_type);

            LOG_DEBUG("Iterator: pa{} = ({}{}) {}", pa_offset, pa_type, num_comp, pa_name);

            if (input_id >= 0 && input_id <= 0x9000) {
                coords[input_id / 0x1000] = pa_iter_var;
            }

            pa_offset += ((descriptor->size >> 4) & 3) + 1;
        }

        uint32_t tex_coord_index = (descriptor->attribute_info & 0x40F);

        // Process texture query variable (iterator), stored on a PA (primary attribute) register
        if (tex_coord_index != 0xF) {
            std::string tex_name = "";
            std::string sampling_type = "2D";
            uint32_t sampler_resource_index = 0;

            for (std::uint32_t p = 0; p < program.parameter_count; p++) {
                const SceGxmProgramParameter &parameter = gxp_parameters[p];

                if (parameter.resource_index == descriptor->resource_index && parameter.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
                    tex_name = gxp::parameter_name_raw(parameter);
                    sampler_resource_index = parameter.resource_index;

                    // ????
                    if (parameter.is_sampler_cube()) {
                        sampling_type = "CUBE";
                    }

                    break;
                }
            }

            const auto component_type = (descriptor->component_info >> 4) & 3;
            const auto swizzle_texcoord = (descriptor->attribute_info & 0x300);

            std::string component_type_str = "????";

            if (component_type == 3) {
                component_type_str = "float";
            } else if (component_type == 2) {
                component_type_str = "half";
            }

            std::string swizzle_str = ".xy";
            std::string projecting;

            if (swizzle_texcoord != 0x100) {
                projecting = "proj";
            }

            int tex_coord_comp_count = 2;

            if (swizzle_texcoord == 0x300) {
                swizzle_str = ".xyz";
                tex_coord_comp_count = 3;
            } else if (swizzle_texcoord == 0x200) {
                swizzle_str = ".xyw";

                // Not really sure
                tex_coord_comp_count = 4;
            }

            std::string centroid_str;

            if ((descriptor->attribute_info & 0x10) == 0x10) {
                centroid_str = "_CENTROID";
            }

            uint32_t num_component = 0;

            if ((descriptor->component_info & 0x40) != 0x40) {
                num_component = 4;
            }

            std::string texcoord_name = "TEXCOORD" + std::to_string(tex_coord_index);
            LOG_TRACE("pa{} = tex{}{}<{}{}>({}, {}{}{})", pa_offset, sampling_type, projecting,
                component_type_str, num_component, tex_name, texcoord_name, centroid_str, swizzle_str);

            std::string tex_query_var_name = "tex_query";
            tex_query_var_name += std::to_string(tex_coord_index);

            NonDependentTextureQueryCallInfo tex_query_info;
            tex_query_info.store_type = (component_type == 3) ? static_cast<int>(DataType::F32) : static_cast<int>(DataType::F16);

            // Size of this extra pa occupied
            // Force this to be PRIVATE
            const optional<spv::StorageClass> texture_query_storage{ spv::StorageClassPrivate };
            const auto size = ((descriptor->size >> 6) & 3) + 1;
            const auto type = b.makeVectorType(b.makeFloatType(32), num_component * size / 4);

            tex_query_info.dest = create_spirv_var_reg(b, parameters, tex_query_var_name, RegisterBank::PRIMATTR, size, type, texture_query_storage);

            if (coords[tex_coord_index] == spv::NoResult) {
                // Create an 'in' variable
                // TODO: this really right?
                std::string coord_name = "v_TexCoord";
                coord_name += std::to_string(tex_coord_index);

                coords[tex_coord_index] = b.createVariable(spv::StorageClassInput,
                    b.makeVectorType(b.makeFloatType(32), tex_coord_comp_count), coord_name.c_str());
            }

            tex_query_info.coord = coords[tex_coord_index];
            tex_query_info.sampler = samplers[sampler_resource_index];

            tex_query_infos.push_back(tex_query_info);

            pa_offset += ((descriptor->size >> 6) & 3) + 1;
        }
    }
}

static void create_fragment_output(spv::Builder &b, SpirvShaderParameters &parameters, const SceGxmProgram &program) {
    // HACKY: We assume output size and format

    std::string frag_color_name = "out_color";
    const spv::Id frag_color_type = b.makeVectorType(b.makeFloatType(32), 4);
    const spv::Id frag_color_var = create_spirv_var_reg(b, parameters, frag_color_name, usse::RegisterBank::OUTPUT, 4, frag_color_type);

    b.addDecoration(frag_color_var, spv::DecorationLocation, 0);

    parameters.outs.push({ frag_color_type, frag_color_var }, 4);
}

static const SceGxmProgramParameterContainer *get_containers(const SceGxmProgram &program) {
    const SceGxmProgramParameterContainer *containers = reinterpret_cast<const SceGxmProgramParameterContainer *>(reinterpret_cast<const std::uint8_t *>(&program.container_offset) + program.container_offset);

    return containers;
}

static const SceGxmProgramParameterContainer *get_container_by_index(const SceGxmProgram &program, const std::uint16_t idx) {
    const SceGxmProgramParameterContainer *container = get_containers(program);

    for (std::uint32_t i = 0; i < program.container_count; i++) {
        if (container[i].container_index == idx) {
            return &container[i];
        }
    }

    return nullptr;
}

static const char *get_container_name(const std::uint16_t idx) {
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

static SpirvShaderParameters create_parameters(spv::Builder &b, const SceGxmProgram &program, emu::SceGxmProgramType program_type, NonDependentTextureQueryCallInfos &texture_queries) {
    SpirvShaderParameters spv_params = {};
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);
    StructDeclContext param_struct = {};

    SamplerMap samplers;

    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = gxp_parameters[i];

        usse::RegisterBank param_reg_type = usse::RegisterBank::PRIMATTR;

        switch (parameter.category) {
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
            param_reg_type = usse::RegisterBank::SECATTR;
            [[fallthrough]];

        // fallthrough
        case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE: {
            const std::string struct_name = gxp::parameter_struct_name(parameter);
            const bool is_struct_field = !struct_name.empty();
            const bool struct_decl_ended = !is_struct_field && !param_struct.empty() || (is_struct_field && !param_struct.empty() && param_struct.name != struct_name);

            if (struct_decl_ended)
                create_struct(b, spv_params, param_struct, program_type);

            spv::Id param_type = get_param_type(b, parameter);

            const bool is_uniform = param_reg_type == usse::RegisterBank::SECATTR;
            const bool is_vertex_output = param_reg_type == usse::RegisterBank::OUTPUT && program_type == emu::SceGxmProgramType::Vertex;
            const bool is_fragment_input = param_reg_type == usse::RegisterBank::PRIMATTR && program_type == emu::SceGxmProgramType::Fragment;
            const bool can_be_interface_block = is_vertex_output || is_fragment_input;

            // TODO: I haven't seen uniforms in 'structs' anywhere and can't test atm, so for now let's
            //       not try to implement emitting structs or interface blocks (probably the former)
            //       for them. Look below for current workaround (won't work for all cases).
            //       Cg most likely supports them so we should support it too at some point.
            if (is_struct_field && is_uniform)
                LOG_WARN("Uniform structs not fully supported!");
            const bool can_be_struct = can_be_interface_block; // || is_uniform

            if (is_struct_field && can_be_struct) {
                const auto param_field_name = gxp::parameter_name(parameter);

                param_struct.name = struct_name;
                param_struct.field_ids.push_back(param_type);
                param_struct.field_names.push_back(param_field_name);
                param_struct.reg_type = param_reg_type;
                param_struct.is_interaface_block = can_be_interface_block;
            } else {
                std::string var_name;

                if (is_uniform) {
                    // TODO: Hacky, ignores struct name/array index, uniforms names could collide if:
                    //           1) a global uniform is named the same as a struct field uniform
                    //           2) uniform struct arrays are used
                    //       It should work for other cases though, since set_uniforms also uses gxp::parameter_name
                    //       To fix this properly we need to emit structs properly first (see comment above
                    //       param_struct_t) and change set_uniforms to use gxp::parameter_name_raw.
                    //       Or we could just flatten everything.
                    var_name = gxp::parameter_name(parameter);
                } else {
                    var_name = gxp::parameter_name_raw(parameter);

                    if (is_struct_field)
                        // flatten struct
                        std::replace(var_name.begin(), var_name.end(), '.', '_');
                }

                auto container = get_container_by_index(program, parameter.container_index);
                std::uint32_t offset = parameter.resource_index;

                if (container && parameter.resource_index < container->max_resource_index) {
                    offset = container->base_sa_offset + parameter.resource_index;
                }

                // Make the type
                std::string param_log = fmt::format("[{} + {}] {}a{} = {}",
                    get_container_name(parameter.container_index), parameter.resource_index,
                    is_uniform ? "s" : "p", offset, var_name);

                if (parameter.array_size > 1) {
                    param_log += fmt::format("[{}]", parameter.array_size);
                }

                LOG_DEBUG(param_log);

                // TODO: Size is not accurate.
                create_spirv_var_reg(b, spv_params, var_name, param_reg_type, parameter.array_size * parameter.component_count, param_type, boost::none, offset);
            }
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_SAMPLER: {
            const auto sampler_spv_var = create_param_sampler(b, spv_params, parameter);
            samplers.emplace(parameter.resource_index, sampler_spv_var);
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

    const std::uint32_t *literals = reinterpret_cast<const std::uint32_t *>(reinterpret_cast<const std::uint8_t *>(&program.literals_offset)
        + program.literals_offset);

    // Get base SA offset for literal
    // The container index of those literals are 16
    auto container = get_container_by_index(program, 16);

    if (!container) {
        // Alternative is 19, which is DATA
        container = get_container_by_index(program, 19);
    }

    if (!container) {
        LOG_WARN("Container for literal not found, skipping creating literals!");
    } else if (program.literals_count != 0) {
        spv::Id f32_type = b.makeFloatType(32);
        using literal_pair = std::pair<std::uint32_t, spv::Id>;

        std::vector<literal_pair> literal_pairs;

        for (std::uint32_t i = 0; i < program.literals_count * 2; i += 2) {
            auto literal_offset = container->base_sa_offset + literals[i];
            auto literal_data = reinterpret_cast<const float *>(literals)[i + 1];

            literal_pairs.emplace_back(literal_offset, b.makeFloatConstant(literal_data));

            // Pair sort automatically sort offset for us
            std::sort(literal_pairs.begin(), literal_pairs.end());

            LOG_TRACE("[LITERAL + {}] sa{} = {} (0x{:X})", literals[i], literal_offset, literal_data, literals[i + 1]);
        }

        std::uint32_t composite_base = literal_pairs[0].first;

        // We should avoid ugly and long GLSL code generated. Also, inefficient SPIR-V code.
        // Packing literals into vector may help solving this.
        std::vector<spv::Id> constituents;
        constituents.push_back(literal_pairs[0].second);

        auto create_new_literal_pack = [&]() {
            // Create new literal composite
            spv_params.uniforms.set_next_offset(composite_base);
            spv::Id composite_var = b.makeCompositeConstant(b.makeVectorType(f32_type, static_cast<int>(constituents.size())),
                constituents);

            spv_params.uniforms.push({ f32_type, composite_var }, static_cast<int>(constituents.size()));
        };

        for (std::uint32_t i = 1; i < program.literals_count; i++) {
            // Detect sequence literals.
            if (literal_pairs[i].first == composite_base + constituents.size() && constituents.size() < 4) {
                constituents.push_back(literal_pairs[i].second);
            } else {
                // The sequence ended. Create new literal pack.
                create_new_literal_pack();

                // Reset and set new base
                composite_base = literal_pairs[i].first;
                constituents.clear();
                constituents.push_back(literal_pairs[i].second);
            }
        }

        // Is there any constituents left ? We should create a literal pack if there is.
        if (!constituents.empty()) {
            create_new_literal_pack();
        }
    }

    // Declarations ended with a struct, so it didn't get handled and we need to do it here
    if (!param_struct.empty())
        create_struct(b, spv_params, param_struct, program_type);

    if (program_type == emu::SceGxmProgramType::Vertex)
        create_vertex_outputs(b, spv_params, program);
    else if (program_type == emu::SceGxmProgramType::Fragment) {
        create_fragment_inputs(b, spv_params, texture_queries, samplers, program);
        create_fragment_output(b, spv_params, program);
    }

    // Create temp reg vars
    const std::size_t total_temp_reg_v4 = (program.temp_reg_count1 / 4) + ((program.temp_reg_count1 % 4) ? 1 : 0);

    for (auto i = 0; i < total_temp_reg_v4; i++) {
        auto name = fmt::format("r{}", i * 4);
        auto type = b.makeVectorType(b.makeFloatType(32), 4); // TODO: Figure out correct type
        create_spirv_var_reg(b, spv_params, name, usse::RegisterBank::TEMP, 4, type);
    }

    // Create internal reg vars
    for (auto i = 0; i < 3; i++) {
        auto name = fmt::format("i{}", i);
        // TODO: these are actually 128 bits long
        auto type = b.makeVectorType(b.makeFloatType(32), 4); // TODO: Figure out correct type
        create_spirv_var_reg(b, spv_params, name, usse::RegisterBank::FPINTERNAL, 16, type);
    }

    // If this is a non-native color fragment shader (uses configurable blending, doesn't write to color buffer directly):
    // Add extra dummy primary attributes that on hw would be patched by the shader patcher depending on blending
    // Instead, in this case we write to the color buffer directly and emulate configurable blending with OpenGL
    // TODO: Verify creation logic. Should we just check if there are _no_ PAs ? Or is the current approach correct?
    if (program_type == emu::Fragment && !program.is_native_color()) {
        const auto missing_primary_attrs = program.primary_reg_count - spv_params.ins.size();

        if (missing_primary_attrs > 2) {
            LOG_ERROR("missing primary attributes: {}", missing_primary_attrs);
        } else if (missing_primary_attrs > 0) {
            const auto pa_type = b.makeVectorType(b.makeFloatType(32), static_cast<int>(missing_primary_attrs * 2));
            std::string pa_name = "pa0_blend";
            create_spirv_var_reg(b, spv_params, pa_name, usse::RegisterBank::PRIMATTR, static_cast<std::uint32_t>(missing_primary_attrs * 2), pa_type); // TODO: * 2 is a hack because we don't yet support f16
        }
    }

    return spv_params;
}

static void generate_shader_body(spv::Builder &b, const SpirvShaderParameters &parameters, const SceGxmProgram &program, const NonDependentTextureQueryCallInfos &texture_queries) {
    // Do texture queries
    usse::convert_gxp_usse_to_spirv(b, program, parameters, texture_queries);
}

static SpirvCode convert_gxp_to_spirv(const SceGxmProgram &program, const std::string &shader_hash, bool force_shader_debug) {
    SpirvCode spirv;

    emu::SceGxmProgramType program_type = program.get_type();

    spv::SpvBuildLogger spv_logger;
    spv::Builder b(SPV_VERSION, 0x1337 << 12, &spv_logger);
    b.setSourceFile(shader_hash);
    b.setEmitOpLines();
    b.addSourceExtension("gxp");
    b.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

    // Capabilities
    b.addCapability(spv::Capability::CapabilityShader);

    NonDependentTextureQueryCallInfos texture_queries;
    SpirvShaderParameters parameters = create_parameters(b, program, program_type, texture_queries);

    std::string entry_point_name;
    spv::ExecutionModel execution_model;

    switch (program_type) {
    default:
        LOG_ERROR("Unknown GXM program type");
    // fallthrough
    case emu::Vertex:
        entry_point_name = "main_vs";
        execution_model = spv::ExecutionModelVertex;
        break;
    case emu::Fragment:
        entry_point_name = "main_fs";
        execution_model = spv::ExecutionModelFragment;
        break;
    }

    // Entry point
    spv::Function *spv_func_main = b.makeEntryPoint(entry_point_name.c_str());

    generate_shader_body(b, parameters, program, texture_queries);

    b.leaveFunction();

    // Execution modes
    if (program_type == emu::SceGxmProgramType::Fragment)
        b.addExecutionMode(spv_func_main, spv::ExecutionModeOriginLowerLeft);

    // Add entry point to Builder
    auto entry_point = b.addEntryPoint(execution_model, spv_func_main, entry_point_name.c_str());

    for (auto id : parameters.ins.get_vars())
        entry_point->addIdOperand(id.var_id);
    for (auto id : parameters.outs.get_vars())
        entry_point->addIdOperand(id.var_id);

    auto spirv_log = spv_logger.getAllMessages();
    if (!spirv_log.empty())
        LOG_ERROR("SPIR-V Error:\n{}", spirv_log);

    b.dump(spirv);

    if (LOG_SHADER_CODE || force_shader_debug)
        spirv_disasm_print(spirv);

    if (DUMP_SPIRV_BINARIES) {
        // TODO: use base path host var
        std::ofstream spirv_dump(shader_hash + ".spv", std::ios::binary);
        spirv_dump.write((char *)&spirv, spirv.size() * sizeof(uint32_t));
        spirv_dump.close();
    }

    return spirv;
}

static std::string convert_spirv_to_glsl(SpirvCode spirv_binary) {
    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

    spirv_cross::CompilerGLSL::Options options;
    options.version = 410;
    options.es = false;
    options.enable_420pack_extension = true;
    // TODO: this might be needed in the future
    //options.vertex.flip_vert_y = true;

    glsl.set_common_options(options);

    // Compile to GLSL, ready to give to GL driver.
    std::string source = glsl.compile();
    return source;
}

// ***********************
// * Functions (utility) *
// ***********************

void spirv_disasm_print(const usse::SpirvCode &spirv_binary) {
    std::stringstream spirv_disasm;
    spv::Disassemble(spirv_disasm, spirv_binary);
    LOG_DEBUG("SPIR-V Disassembly:\n{}", spirv_disasm.str());
}

// ***************************
// * Functions (exposed API) *
// ***************************

std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_name, bool force_shader_debug) {
    std::vector<uint32_t> spirv_binary = convert_gxp_to_spirv(program, shader_name, force_shader_debug);

    const auto source = convert_spirv_to_glsl(spirv_binary);

    if (LOG_SHADER_CODE || force_shader_debug)
        LOG_DEBUG("Generated GLSL:\n{}", source);

    return source;
}

void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath_str) {
    const fs::path shader_filepath{ shader_filepath_str };
    fs::ifstream gxp_stream(shader_filepath, std::ios::binary);

    if (!gxp_stream.is_open())
        return;

    const auto gxp_file_size = fs::file_size(shader_filepath);
    auto gxp_program = static_cast<SceGxmProgram *>(calloc(gxp_file_size, 1));

    gxp_stream.read(reinterpret_cast<char *>(gxp_program), gxp_file_size);

    convert_gxp_to_glsl(*gxp_program, shader_filepath.filename().string(), true);

    free(gxp_program);
}

} // namespace shader
