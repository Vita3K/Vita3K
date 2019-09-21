// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
// Copyright (c) 2002-2011 The ANGLE Project Authors.
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
#include <shader/usse_disasm.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_utilities.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <shader/profile.h>
#include <shader/usse_translator_entry.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_utilities.h>
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

struct TranslationState {
    spv::Id last_frag_data_id = spv::NoResult;
    spv::Id color_attachment_id = spv::NoResult;
    spv::Id frag_coord_id = spv::NoResult; ///< gl_FragCoord, not built-in in SPIR-V.
    spv::Id flip_vec_id = spv::NoResult;
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
    case SCE_GXM_PARAMETER_TYPE_F16:
    case SCE_GXM_PARAMETER_TYPE_F32:
         return b.makeFloatType(32);

    case SCE_GXM_PARAMETER_TYPE_U8:
    case SCE_GXM_PARAMETER_TYPE_U16:
    case SCE_GXM_PARAMETER_TYPE_U32:
        return b.makeUintType(32);

    case SCE_GXM_PARAMETER_TYPE_S8:
    case SCE_GXM_PARAMETER_TYPE_S16:
    case SCE_GXM_PARAMETER_TYPE_S32:
        return b.makeIntType(32);

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
    case usse::RegisterBank::INDEXED1: break;
    case usse::RegisterBank::INDEXED2: break;

    case usse::RegisterBank::MAXIMUM:
    case usse::RegisterBank::INVALID:
    default:
        return spv::StorageClassMax;
    }

    LOG_WARN("Unsupported reg_type {}", static_cast<uint32_t>(reg_type));
    return spv::StorageClassMax;
}

static spv::Id create_param_sampler(spv::Builder &b, const SceGxmProgramParameter &parameter) {
    spv::Id sampled_type = b.makeFloatType(32);
    spv::Id image_type = b.makeImageType(sampled_type, spv::Dim2D, false, false, false, 1, spv::ImageFormatUnknown);
    spv::Id sampled_image_type = b.makeSampledImageType(image_type);
    std::string name = gxp::parameter_name(parameter);

    return b.createVariable(spv::StorageClassUniformConstant, sampled_image_type, name.c_str());
}

static spv::Id create_input_variable(spv::Builder &b, SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils, const FeatureState &features, const char *name, const RegisterBank bank, const std::uint32_t offset, spv::Id type, const std::uint32_t size, spv::Id force_id = spv::NoResult, DataType dtype = DataType::F32) {
    std::uint32_t total_var_comp = size / 4;
    spv::Id var = !force_id ? (b.createVariable(reg_type_to_spv_storage_class(bank), type, name)) : force_id;

    Operand dest;
    dest.bank = bank;
    dest.num = offset;
    dest.type = dtype;

    Imm4 dest_mask = 0b1111;

    auto get_dest_mask = [&]() {
        switch (total_var_comp) {
        case 1:
            dest_mask = 0b1;
            break;

        case 2:
            dest_mask = 0b11;
            break;

        case 3:
            dest_mask = 0b111;
            break;

        default:
            break;
        }
    };

    if (b.isArrayType(b.getContainedTypeId(b.getTypeId(var)))) {
        spv::Id arr_type = b.getContainedTypeId(b.getTypeId(var));
        spv::Id comp_type = b.getContainedTypeId(arr_type);
        total_var_comp = b.getNumTypeComponents(comp_type);

        // Reget the mask
        get_dest_mask();

        for (auto i = 0; i < b.getNumTypeComponents(arr_type); i++) {
            spv::Id elm = b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, comp_type),
                { var, b.makeIntConstant(i) });
            utils::store(b, parameters, utils, features, dest, b.createLoad(elm), dest_mask, 0 + i * 4);
        }
    } else {
        get_dest_mask();

        if (!b.isConstant(var)) {
            var = b.createLoad(var);

            if (total_var_comp > 1) {
                var = utils::finalize(b, var, var, SWIZZLE_CHANNEL_4_DEFAULT, 0, dest_mask);
            }
        }

        utils::store(b, parameters, utils, features, dest, var, dest_mask, 0);
    }

    return var;
}

static DataType gxm_parameter_type_to_usse_data_type(const SceGxmParameterType param_type) {
    switch (param_type) {
    case SCE_GXM_PARAMETER_TYPE_F16:
        return DataType::F16;

    case SCE_GXM_PARAMETER_TYPE_F32:
        return DataType::F32;
        break;

    case SCE_GXM_PARAMETER_TYPE_U8:
    case SCE_GXM_PARAMETER_TYPE_S8:
        return DataType::C10;

    default:
        LOG_WARN("Unsupported output register format {}, default to F16", (int)param_type);
        break;
    }

    return DataType::F16;
}

static void create_fragment_inputs(spv::Builder &b, SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils, const FeatureState &features, TranslationState &translation_state, NonDependentTextureQueryCallInfos &tex_query_infos, SamplerMap &samplers,
    const SceGxmProgram &program) {
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
    std::array<shader::usse::Coord, 10> coords;

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
            DataType pa_dtype = DataType::UINT8;

            uint32_t input_type = (descriptor->attribute_info & 0x30100000);

            if (input_type == 0x20000000) {
                pa_type = "half";
                pa_dtype = DataType::F16;
            } else if (input_type == 0x10000000) {
                pa_type = "fixed";
                // TODO: Supply data type
            } else if (input_type == 0x100000) {
                if (input_id == 0xA000 || input_id == 0xB000) {
                    pa_type = "float";
                    pa_dtype = DataType::F32;
                }
            } else if (input_id != 0xA000 && input_id != 0xB000) {
                pa_type = "float";
                pa_dtype = DataType::F32;
            }

            // Create PA Iterator SPIR-V variable
            const auto num_comp = ((descriptor->attribute_info >> 22) & 3) + 1;

            // Force this to 4. TODO: Don't
            // Reason is for compability between vertex and fragment. This is like an anti-crash when linking.
            // Fragment will only copy what it needed.
            const auto pa_iter_type = b.makeVectorType(b.makeFloatType(32), 4);
            const auto pa_iter_size = num_comp * 4;
            const auto pa_iter_var = create_input_variable(b, parameters, utils, features, pa_name.c_str(), RegisterBank::PRIMATTR,
                pa_offset, pa_iter_type, pa_iter_size, spv::NoResult, pa_dtype);

            LOG_DEBUG("Iterator: pa{} = ({}{}) {}", pa_offset, pa_type, num_comp, pa_name);

            if (input_id >= 0 && input_id <= 0x9000) {
                coords[input_id / 0x1000].first = pa_iter_var;
                coords[input_id / 0x1000].second = static_cast<int>(pa_dtype);
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
                    tex_name = gxp::parameter_name(parameter);
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
            const auto size = ((descriptor->size >> 6) & 3) + 1;
            tex_query_info.dest_offset = pa_offset;

            tex_query_info.coord_index = tex_coord_index;
            tex_query_info.sampler = samplers[sampler_resource_index];

            tex_query_infos.push_back(tex_query_info);

            pa_offset += size;
        }
    }

    for (auto &query_info : tex_query_infos) {
        if (coords[query_info.coord_index].first == spv::NoResult) {
            // Create an 'in' variable
            // TODO: this really right?
            std::string coord_name = "v_TexCoord";
            coord_name += std::to_string(query_info.coord_index);

            coords[query_info.coord_index].first = b.createVariable(spv::StorageClassInput,
                b.makeVectorType(b.makeFloatType(32), /*tex_coord_comp_count*/ 4), coord_name.c_str());

            coords[query_info.coord_index].second = static_cast<int>(DataType::F32);
        }

        query_info.coord = coords[query_info.coord_index];
    }

    spv::Id f32 = b.makeFloatType(32);
    spv::Id v4 = b.makeVectorType(f32, 4);

    if (program.is_native_color()) {
        // There might be a chance that this shader also reads from OUTPUT bank. We will load last state frag data
        spv::Id source = spv::NoResult;

        if (features.direct_fragcolor) {
            // The GPU supports gl_LastFragData. It's only OpenGL though
            // TODO: Make this not emit with OpenGL
            spv::Id v4_a = b.makeArrayType(v4, b.makeIntConstant(1), 0);
            spv::Id last_frag_data_arr = b.createVariable(spv::StorageClassInput, v4_a, "gl_LastFragData");
            spv::Id last_frag_data = b.createOp(spv::OpAccessChain, v4, { last_frag_data_arr, b.makeIntConstant(0) });

            // Copy outs into. The output data from last stage should has the same format as our
            source = last_frag_data;
            translation_state.last_frag_data_id = last_frag_data_arr;
        } else if (features.support_shader_interlock || features.support_texture_barrier) {
            std::vector<spv::Id> empty_args;
            int sampled = 1;
            spv::ImageFormat img_format = spv::ImageFormatUnknown;

            if (features.should_use_shader_interlock()) {
                sampled = 2;
                img_format = spv::ImageFormatRgba8;
            }

            // Create a global sampler, which is our color attachment
            spv::Id sampled_type = b.makeFloatType(32);
            spv::Id image_type = b.makeImageType(sampled_type, spv::Dim2D, false, false, false, sampled, img_format);

            if (features.should_use_texture_barrier()) {
                // Make it a sampler
                image_type = b.makeSampledImageType(image_type);
            }

            spv::Id v4 = b.makeVectorType(sampled_type, 4);

            spv::Id color_attachment = b.createVariable(spv::StorageClassUniformConstant, image_type, "f_colorAttachment");
            spv::Id current_coord = b.createVariable(spv::StorageClassInput, v4, "gl_FragCoord");
            translation_state.frag_coord_id = current_coord;
            translation_state.color_attachment_id = color_attachment;

            spv::Id i32 = b.makeIntegerType(32, true);
            current_coord = b.createUnaryOp(spv::OpConvertFToS, b.makeVectorType(i32, 4), current_coord);
            current_coord = b.createOp(spv::OpVectorShuffle, b.makeVectorType(i32, 2), { current_coord, current_coord, 0, 1 });

            spv::Id texel = spv::NoResult;

            if (features.should_use_shader_interlock())
                texel = b.createOp(spv::OpImageRead, v4, { color_attachment, current_coord });
            else
                texel = b.createOp(spv::OpImageFetch, v4, { color_attachment, current_coord });

            source = texel;
        } else {
            // Try to initialize outs[0] to some nice value. In case the GPU has garbage data for our shader
            spv::Id v4 = b.makeVectorType(b.makeFloatType(32), 4);
            spv::Id rezero = b.makeFloatConstant(0.0f);
            source = b.makeCompositeConstant(v4, { rezero, rezero, rezero, rezero });
        }

        if (source != spv::NoResult) {
            Operand target_to_store;

            target_to_store.bank = RegisterBank::OUTPUT;
            target_to_store.num = 0;
            target_to_store.type = gxm_parameter_type_to_usse_data_type(program.get_fragment_output_type());

            utils::store(b, parameters, utils, features, target_to_store, source, 0b1111, 0);
        }
    }
}

static void get_parameter_type_store_and_name(const SceGxmProgramParameter &parameter, DataType &store_type, const char *&param_type_name) {
    switch (parameter.type) {
    case SCE_GXM_PARAMETER_TYPE_F16: {
        param_type_name = "half";
        store_type = DataType::F16;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_U16: {
        param_type_name = "ushort";
        store_type = DataType::UINT16;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_S16: {
        param_type_name = "ishort";
        store_type = DataType::INT16;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_U8: {
        param_type_name = "uchar";
        store_type = DataType::UINT8;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_S8: {
        param_type_name = "ichar";
        store_type = DataType::INT8;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_U32: {
        param_type_name = "uint";
        store_type = DataType::UINT32;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_S32: {
        param_type_name = "int";
        store_type = DataType::INT32;
        break;
    }

    default:
        break;
    }

}

/**
 * \brief Calculate variable size, in float granularity.
 */
static size_t calculate_variable_size(const SceGxmProgramParameter &parameter, const DataType store_type) {
    size_t element_size = shader::usse::get_data_type_size(store_type) * parameter.component_count;

    if (parameter.array_size == 1) {
        return (element_size + 3) / 4;
    }

    // Need to do alignment
    if (parameter.component_count == 1) {
        // Each element is a scalar. Apply 32-bit alignment.
        // Assuming no scalar has larger size than 4.
        return parameter.array_size;
    }

    // Apply 64-bit alignment
    return parameter.array_size * (((element_size + 8 - (element_size & 7)) + 3) / 4);
}

// For uniform buffer resigned in registers
static void copy_uniform_block_to_register(spv::Builder &builder, spv::Id sa_bank, spv::Id block, spv::Id ite, const int vec4_count) {
    utils::make_for_loop(builder, ite, builder.makeIntConstant(0), builder.makeIntConstant(vec4_count), [&]() {
        spv::Id to_copy = builder.createAccessChain(spv::StorageClassUniform, block, { builder.makeIntConstant(0), ite });
        spv::Id dest = builder.createAccessChain(spv::StorageClassPrivate, sa_bank, { ite });

        builder.createStore(builder.createLoad(to_copy), dest);
    });
}

static spv::Id create_uniform_block(spv::Builder &b, const int base_binding, const int size_vec4_granularity, const bool is_vert) {
    spv::Id f32_type = b.makeFloatType(32);

    spv::Id f32_v4_type = b.makeVectorType(f32_type, 4);
    spv::Id vec4_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(size_vec4_granularity), 16);

    std::string name_type = (is_vert ? "vert" : "frag");

    if (base_binding == 0) {
        name_type += "_defaultUniformBuffer";
    } else {
        name_type += fmt::format("_buffer{}", base_binding);
    }

    // Create the default reg uniform buffer
    spv::Id default_buffer_type = b.makeStructType({ vec4_arr_type }, name_type.c_str());
    b.addDecoration(default_buffer_type, spv::DecorationBlock, 1);
    b.addDecoration(default_buffer_type, spv::DecorationGLSLShared, 1);

    // Default uniform buffer always has binding of 0
    const std::string buffer_var_name = fmt::format("buffer{}", base_binding);
    spv::Id default_buffer = b.createVariable(spv::StorageClassUniform, default_buffer_type, buffer_var_name.c_str());
    b.addDecoration(default_buffer, spv::DecorationBinding, (!is_vert) ? 15 : 0);

    b.addMemberDecoration(default_buffer_type, 0, spv::DecorationOffset, 0);
    b.addDecoration(vec4_arr_type, spv::DecorationArrayStride, 16);
    b.addMemberName(default_buffer_type, 0, "buffer");

    return default_buffer;
}

static SpirvShaderParameters create_parameters(spv::Builder &b, const SceGxmProgram &program, utils::SpirvUtilFunctions &utils,
    const FeatureState &features, TranslationState &translation_state, emu::SceGxmProgramType program_type, NonDependentTextureQueryCallInfos &texture_queries) {
    SpirvShaderParameters spv_params = {};
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);

    // Make array type. TODO: Make length configurable
    spv::Id f32_type = b.makeFloatType(32);
    spv::Id i32_type = b.makeIntType(32);
    spv::Id b_type = b.makeBoolType();

    spv::Id f32_v4_type = b.makeVectorType(f32_type, 4);
    spv::Id pa_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(32), 0);
    spv::Id sa_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(32), 0);
    spv::Id i_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(3), 0);
    spv::Id temp_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(20), 0);
    spv::Id index_arr_type = b.makeArrayType(i32_type, b.makeIntConstant(2), 0);
    spv::Id pred_arr_type = b.makeArrayType(b_type, b.makeIntConstant(4), 0);
    spv::Id o_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(10), 0);

    // Create register banks
    spv_params.ins = b.createVariable(spv::StorageClassPrivate, pa_arr_type, "pa");
    spv_params.uniforms = b.createVariable(spv::StorageClassPrivate, sa_arr_type, "sa");
    spv_params.internals = b.createVariable(spv::StorageClassPrivate, i_arr_type, "internals");
    spv_params.temps = b.createVariable(spv::StorageClassPrivate, temp_arr_type, "r");
    spv_params.predicates = b.createVariable(spv::StorageClassPrivate, pred_arr_type, "p");
    spv_params.indexes = b.createVariable(spv::StorageClassPrivate, index_arr_type, "idx");
    spv_params.outs = b.createVariable(spv::StorageClassPrivate, o_arr_type, "outs");

    SamplerMap samplers;
    UniformBufferMap buffers;

    const std::uint64_t *secondary_program_insts = reinterpret_cast<const std::uint64_t*>(
        reinterpret_cast<const std::uint8_t*>(&program.secondary_program_offset) + program.secondary_program_offset);

    const std::uint64_t *primary_program_insts = program.primary_program_start();

    // Analyze the shader to get maximum uniform buffer data
    // Analyze secondary program
    usse::data_analyze(
        static_cast<shader::usse::USSEOffset>((program.secondary_program_offset_end + 4 - program.secondary_program_offset)) / 8,
        [&](usse::USSEOffset off) -> std::uint64_t { 
            return secondary_program_insts[off];
        }, buffers);

    // Analyze primary program
    usse::data_analyze(
        static_cast<shader::usse::USSEOffset>(program.primary_program_instr_count),
        [&](usse::USSEOffset off) -> std::uint64_t { 
            return primary_program_insts[off];
        }, buffers);

    spv::Id ite_copy = b.createVariable(spv::StorageClassFunction, i32_type, "i");

    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = gxp_parameters[i];

        usse::RegisterBank param_reg_type = usse::RegisterBank::PRIMATTR;

        switch (parameter.category) {
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
            param_reg_type = usse::RegisterBank::SECATTR;
            [[fallthrough]];

        // fallthrough
        case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE: {
            spv::Id param_type = get_param_type(b, parameter);

            const bool is_uniform = param_reg_type == usse::RegisterBank::SECATTR;

            std::string var_name = gxp::parameter_name(parameter);

            auto container = gxp::get_container_by_index(program, parameter.container_index);
            std::uint32_t offset = parameter.resource_index;

            if (container && parameter.resource_index < container->max_resource_index) {
                offset = container->base_sa_offset + parameter.resource_index;
            }

            auto param_type_name = "float";
            DataType store_type = DataType::F32;

            get_parameter_type_store_and_name(parameter, store_type, param_type_name);

            // Make the type
            std::string param_log = fmt::format("[{} + {}] {}a{} = ({}{}) {}",
                gxp::get_container_name(parameter.container_index), parameter.resource_index,
                is_uniform ? "s" : "p", offset, param_type_name, parameter.component_count, var_name);

            if (parameter.array_size > 1) {
                param_log += fmt::format("[{}]", parameter.array_size);
            }

            LOG_DEBUG(param_log);

            if (!is_uniform || !features.use_ubo) {
                int type_size = gxp::get_parameter_type_size(static_cast<SceGxmParameterType>((uint16_t)parameter.type));
                spv::Id var = create_input_variable(b, spv_params, utils, features, var_name.c_str(), param_reg_type, offset, param_type,
                    parameter.array_size * parameter.component_count * 4, 0, store_type);
            }

            break;
        }

        case SCE_GXM_PARAMETER_CATEGORY_SAMPLER: {
            const auto sampler_spv_var = create_param_sampler(b, parameter);
            samplers.emplace(parameter.resource_index, sampler_spv_var);

            // TODO: I really want to give you binding.

            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE: {
            assert(parameter.component_count == 0);
            LOG_CRITICAL("auxiliary_surface used in shader");
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER: {
            const int buffer_size = match_uniform_buffer_with_buffer_size(program, parameter, buffers);

            // Search for the buffer from analyzed list
            if (buffer_size != -1) {
                const int base = gxp::get_uniform_buffer_base(program, parameter);

                spv::Id block = create_uniform_block(b, (parameter.resource_index + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER,
                    buffer_size, !program.is_fragment());

                // We found it. Make things
                spv_params.buffers.emplace(base, block);
            }

            break;
        }
        default: {
            LOG_CRITICAL("Unknown parameter type used in shader.");
            break;
        }
        }
    }

    if (features.use_ubo && program.default_uniform_buffer_count != 0) {
        // Create an array of vec4
        const int total_vec4 = static_cast<int>((program.default_uniform_buffer_count + 3) / 4);
        spv::Id default_buffer = create_uniform_block(b, 0, total_vec4, !program.is_fragment());
        copy_uniform_block_to_register(b, spv_params.uniforms, default_buffer, ite_copy, total_vec4);
    }

    const SceGxmProgramParameterContainer *container = gxp::get_container_by_index(program, 19);

    if (container) {
        // Create dependent sampler
        const SceGxmDependentSampler *dependent_samplers = reinterpret_cast<const SceGxmDependentSampler *>(reinterpret_cast<const std::uint8_t *>(&program.dependent_sampler_offset)
            + program.dependent_sampler_offset);

        for (std::uint32_t i = 0; i < program.dependent_sampler_count; i++) {
            const std::uint32_t rsc_index = dependent_samplers[i].resource_index_layout_offset / 4;
            spv_params.samplers.emplace(container->base_sa_offset + dependent_samplers[i].sa_offset, samplers[rsc_index]);
        }
    }

    const std::uint32_t *literals = reinterpret_cast<const std::uint32_t *>(reinterpret_cast<const std::uint8_t *>(&program.literals_offset)
        + program.literals_offset);

    // Get base SA offset for literal
    // The container index of those literals are 16
    container = gxp::get_container_by_index(program, 16);

    if (!container) {
        // Alternative is 19, which is DATA
        container = gxp::get_container_by_index(program, 19);
    }

    if (!container) {
        LOG_WARN("Container for literal not found, skipping creating literals!");
    } else if (program.literals_count != 0) {
        spv::Id f32_type = b.makeFloatType(32);
        using literal_pair = std::pair<std::uint32_t, spv::Id>;

        std::vector<literal_pair> literal_pairs;

        for (std::uint32_t i = 0; i < program.literals_count * 2; i += 2) {
            auto literal_offset = container->base_sa_offset + literals[i];
            auto literal_data = *reinterpret_cast<const float *>(&literals[i + 1]);

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
            spv::Id composite_var = b.makeCompositeConstant(b.makeVectorType(f32_type, static_cast<int>(constituents.size())),
                constituents);
            create_input_variable(b, spv_params, utils, features, nullptr, RegisterBank::SECATTR, composite_base, spv::NoResult,
                static_cast<int>(constituents.size() * 4), composite_var);
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

    if (program_type == emu::SceGxmProgramType::Vertex && features.hardware_flip) {
        // Create variable that helps us do flipping
        // TODO: Not emit this on Vulkan or DirectX
        spv::Id f32 = b.makeFloatType(32);
        spv::Id v4 = b.makeVectorType(f32, 4);

        spv::Id flip_vec = b.createVariable(spv::StorageClassUniformConstant, v4, "flip_vec");
        translation_state.flip_vec_id = flip_vec;
    }

    if (program_type == emu::SceGxmProgramType::Fragment) {
        create_fragment_inputs(b, spv_params, utils, features, translation_state, texture_queries, samplers, program);
    }

    return spv_params;
}

static void generate_shader_body(spv::Builder &b, const SpirvShaderParameters &parameters, const SceGxmProgram &program,
    const FeatureState &features, utils::SpirvUtilFunctions &utils, spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &texture_queries) {
    // Do texture queries
    usse::convert_gxp_usse_to_spirv(b, program, features, parameters, utils, end_hook_func, texture_queries);
}

static spv::Function *make_frag_finalize_function(spv::Builder &b, const SpirvShaderParameters &parameters,
    const SceGxmProgram &program, utils::SpirvUtilFunctions &utils, const FeatureState &features, TranslationState &translate_state) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *frag_fin_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Function *frag_fin_func = b.makeFunctionEntry(spv::NoPrecision, b.makeVoidType(), "frag_output_finalize", {},
        decorations, &frag_fin_block);

    const SceGxmParameterType param_type = program.get_fragment_output_type();

    Operand color_val_operand;
    color_val_operand.bank = program.is_native_color() ? RegisterBank::OUTPUT : RegisterBank::PRIMATTR;
    color_val_operand.num = 0;
    color_val_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    color_val_operand.type = gxm_parameter_type_to_usse_data_type(param_type);

    spv::Id color = utils::load(b, parameters, utils, features, color_val_operand, 0xF, 0);
    if (program.is_native_color() && features.should_use_shader_interlock()) {
        spv::Id signed_i32 = b.makeIntegerType(32, true);
        spv::Id translated_id = b.createUnaryOp(spv::OpConvertFToS, b.makeVectorType(signed_i32, 4), translate_state.frag_coord_id);
        translated_id = b.createOp(spv::OpVectorShuffle, b.makeVectorType(signed_i32, 2), { translated_id, translated_id, 0, 1 });
        b.createNoResultOp(spv::OpImageWrite, { translate_state.color_attachment_id, translated_id, color });
    } else {
        spv::Id out = b.createVariable(spv::StorageClassOutput, b.makeVectorType(b.makeFloatType(32), 4), "out_color");
        b.addDecoration(out, spv::DecorationLocation, 0);
        b.createStore(color, out);
    }

    b.makeReturn(false);
    b.setBuildPoint(last_build_point);

    return frag_fin_func;
}

static spv::Function *make_vert_finalize_function(spv::Builder &b, const SpirvShaderParameters &parameters,
    const SceGxmProgram &program, utils::SpirvUtilFunctions &utils, const FeatureState &features, TranslationState &translation_state) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *vert_fin_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Function *vert_fin_func = b.makeFunctionEntry(spv::NoPrecision, b.makeVoidType(), "vert_output_finalize", {},
        decorations, &vert_fin_block);

    SceGxmVertexOutputTexCoordInfos coord_infos = {};
    SceGxmVertexProgramOutputs vertex_outputs = gxp::get_vertex_outputs(program, &coord_infos);

    auto set_property = [](SceGxmVertexProgramOutputs vo, const char *name, std::uint32_t component_count) {
        return std::make_pair(vo, VertexProgramOutputProperties(name, component_count));
    };

    static auto calculate_copy_comp_count = [](uint8_t info) {
        // TexCoord info uses preset values described below for determining lengths.
        uint8_t length = 0;
        if (info & 0b001u)
            length += 2; // uses xy
        if (info & 0b010u)
            length += 1; // uses z
        if (info & 0b100u)
            length += 1; // uses w

        return length;
    };

    // TODO: Verify component counts
    VertexProgramOutputPropertiesMap vertex_properties_map = {
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION, "v_Position", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG, "v_Fog", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0, "v_Color0", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1, "v_Color1", 4),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0, "v_TexCoord0", calculate_copy_comp_count(coord_infos[0])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD1, "v_TexCoord1", calculate_copy_comp_count(coord_infos[1])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD2, "v_TexCoord2", calculate_copy_comp_count(coord_infos[2])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD3, "v_TexCoord3", calculate_copy_comp_count(coord_infos[3])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD4, "v_TexCoord4", calculate_copy_comp_count(coord_infos[4])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD5, "v_TexCoord5", calculate_copy_comp_count(coord_infos[5])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD6, "v_TexCoord6", calculate_copy_comp_count(coord_infos[6])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD7, "v_TexCoord7", calculate_copy_comp_count(coord_infos[7])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD8, "v_TexCoord8", calculate_copy_comp_count(coord_infos[8])),
        set_property(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9, "v_TexCoord9", calculate_copy_comp_count(coord_infos[9])),
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

    Operand o_op;
    o_op.bank = RegisterBank::OUTPUT;
    o_op.num = 0;
    o_op.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    const Imm4 DEST_MASKS[] = { 0, 0b1, 0b11, 0b111, 0b1111 };

    for (int vo = SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION; vo < _SCE_GXM_VERTEX_PROGRAM_OUTPUT_LAST; vo <<= 1) {
        if (vertex_outputs & vo) {
            const auto vo_typed = static_cast<SceGxmVertexProgramOutputs>(vo);
            VertexProgramOutputProperties properties = vertex_properties_map.at(vo_typed);

            int number_of_comp_vec = properties.component_count;

            if (vo >= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0 && vo <= SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9) {
                number_of_comp_vec = 4;
            }

            const spv::Id out_type = b.makeVectorType(b.makeFloatType(32), number_of_comp_vec);
            const spv::Id out_var = b.createVariable(spv::StorageClassOutput, out_type, properties.name.c_str());

            // Do store
            spv::Id o_val = utils::load(b, parameters, utils, features, o_op, DEST_MASKS[number_of_comp_vec], 0);

            // TODO: More decorations needed?
            if (vo == SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION) {
                b.addDecoration(out_var, spv::DecorationBuiltIn, spv::BuiltInPosition);

                if (translation_state.flip_vec_id != spv::NoResult) {
                    o_val = b.createBinOp(spv::OpFMul, out_type, o_val, translation_state.flip_vec_id);
                }
            }

            b.createStore(o_val, out_var);

            o_op.num += properties.component_count;
        }
    }

    b.makeReturn(false);
    b.setBuildPoint(last_build_point);

    return vert_fin_func;
}

static SpirvCode convert_gxp_to_spirv(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features, TranslationState &translation_state, bool force_shader_debug, std::string *spirv_dump, std::string *disasm_dump) {
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
    utils::SpirvUtilFunctions utils;

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

    // Put disasm storage
    disasm::disasm_storage = disasm_dump;

    // Entry point
    spv::Function *spv_func_main = b.makeEntryPoint(entry_point_name.c_str());
    spv::Function *end_hook_func = nullptr;

    std::vector<spv::Id> empty_args;

    // Lock/unlock and read texel for shader interlock. Texture barrier will have glTextureBarrier() called so we don't
    // have to worry too much. Texture barrier will not be accurate and may be broken though.
    if (features.should_use_shader_interlock() && program.is_fragment() && program.is_native_color())
        b.createOp(spv::OpBeginInvocationInterlockEXT, spv::OpTypeVoid, empty_args);

    // Generate parameters
    SpirvShaderParameters parameters = create_parameters(b, program, utils, features, translation_state, program_type, texture_queries);

    if (program.is_fragment()) {
        end_hook_func = make_frag_finalize_function(b, parameters, program, utils, features, translation_state);
    } else {
        end_hook_func = make_vert_finalize_function(b, parameters, program, utils, features, translation_state);
    }

    generate_shader_body(b, parameters, program, features, utils, end_hook_func, texture_queries);

    // Execution modes
    if (program_type == emu::SceGxmProgramType::Fragment) {
        b.addExecutionMode(spv_func_main, spv::ExecutionModeOriginLowerLeft);

        if (program.is_native_color() && features.should_use_shader_interlock()) {
            // Add execution mode
            b.addExecutionMode(spv_func_main, spv::ExecutionModePixelInterlockOrderedEXT);
        }
    }

    // Add entry point to Builder
    b.addEntryPoint(execution_model, spv_func_main, entry_point_name.c_str());
    auto spirv_log = spv_logger.getAllMessages();
    if (!spirv_log.empty())
        LOG_ERROR("SPIR-V Error:\n{}", spirv_log);

    b.dump(spirv);

    if (LOG_SHADER_CODE || force_shader_debug) {
        spirv_disasm_print(spirv, spirv_dump);
    }

    if (DUMP_SPIRV_BINARIES) {
        // TODO: use base path host var
        std::ofstream spirv_dump(shader_hash + ".spv", std::ios::binary);
        spirv_dump.write((char *)&spirv, spirv.size() * sizeof(uint32_t));
        spirv_dump.close();
    }

    return spirv;
}

static std::string convert_spirv_to_glsl(SpirvCode spirv_binary, const FeatureState &features, TranslationState &translation_state) {
    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

    spirv_cross::CompilerGLSL::Options options;
    if (features.direct_pack_unpack_half) {
        options.version = 420;
    } else {
        options.version = 410;
    }
    options.es = false;
    options.enable_420pack_extension = true;

    // TODO: this might be needed in the future
    //options.vertex.flip_vert_y = true;

    glsl.set_common_options(options);

    if (!features.direct_pack_unpack_half) {
        if (features.pack_unpack_half_through_ext) {
            glsl.require_extension("GL_ARB_shading_language_packing");
        } else {
            // Emit pack/unpack ourself
            // Please thanks Google for this.
            // https://github.com/google/angle/blob/master/src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
            glsl.add_header_line(
                "\n"
                "uint f32tof16(float val)\n"
                "{\n"
                "    uint f32 = floatBitsToUint(val);\n"
                "    uint f16 = 0u;\n"
                "    uint sign = (f32 >> 16) & 0x8000u;\n"
                "    int exponent = int((f32 >> 23) & 0xFFu) - 127;\n"
                "    uint mantissa = f32 & 0x007FFFFFu;\n"
                "    if (exponent == 128)\n"
                "    {\n"
                "        // Infinity or NaN\n"
                "        // NaN bits that are masked out by 0x3FF get discarded.\n"
                "        // This can turn some NaNs to infinity, but this is allowed by the spec.\n"
                "        f16 = sign | (0x1Fu << 10);\n"
                "        f16 |= (mantissa & 0x3FFu);\n"
                "    }\n"
                "    else if (exponent > 15)\n"
                "    {\n"
                "        // Overflow - flush to Infinity\n"
                "        f16 = sign | (0x1Fu << 10);\n"
                "    }\n"
                "    else if (exponent > -15)\n"
                "    {\n"
                "        // Representable value\n"
                "        exponent += 15;\n"
                "        mantissa >>= 13;\n"
                "        f16 = sign | uint(exponent << 10) | mantissa;\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        f16 = sign;\n"
                "    }\n"
                "    return f16;\n"
                "}\n"
                "\n"
                "uint packHalf2x16(vec2 v)\n"
                "{\n"
                "     uint x = f32tof16(v.x);\n"
                "     uint y = f32tof16(v.y);\n"
                "     return (y << 16) | x;\n"
                "}\n");

            glsl.add_header_line(
                "float f16tof32(uint val)\n"
                "{\n"
                "    uint sign = (val & 0x8000u) << 16;\n"
                "    int exponent = int((val & 0x7C00u) >> 10);\n"
                "    uint mantissa = val & 0x03FFu;\n"
                "    float f32 = 0.0;\n"
                "    if(exponent == 0)\n"
                "    {\n"
                "        if (mantissa != 0u)\n"
                "        {\n"
                "            const float scale = 1.0 / (1 << 24);\n"
                "            f32 = scale * mantissa;\n"
                "        }\n"
                "    }\n"
                "    else if (exponent == 31)\n"
                "    {\n"
                "        return uintBitsToFloat(sign | 0x7F800000u | mantissa);\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        exponent -= 15;\n"
                "        float scale;\n"
                "        if(exponent < 0)\n"
                "        {\n"
                "            // The negative unary operator is buggy on OSX.\n"
                "            // Work around this by using abs instead.\n"
                "            scale = 1.0 / (1 << abs(exponent));\n"
                "        }\n"
                "        else\n"
                "        {\n"
                "            scale = 1 << exponent;\n"
                "        }\n"
                "        float decimal = 1.0 + float(mantissa) / float(1 << 10);\n"
                "        f32 = scale * decimal;\n"
                "    }\n"
                "\n"
                "    if (sign != 0u)\n"
                "    {\n"
                "        f32 = -f32;\n"
                "    }\n"
                "\n"
                "     return f32;\n"
                "}\n"
                "\n"
                "vec2 unpackHalf2x16(uint u)\n"
                "{\n"
                "    uint y = (u >> 16);\n"
                "    uint x = u & 0xFFFFu;\n"
                "    return vec2(f16tof32(x), f16tof32(y));\n"
                "}\n");
        }
    }

    if (features.direct_fragcolor && translation_state.last_frag_data_id != spv::NoResult) {
        glsl.require_extension("GL_EXT_shader_framebuffer_fetch");

        // Do not generate declaration for gl_LastFragData
        glsl.set_remapped_variable_state(translation_state.last_frag_data_id, true);
        glsl.set_name(translation_state.last_frag_data_id, "gl_LastFragData");
    }

    if (features.is_programmable_blending_need_to_bind_color_attachment() && translation_state.frag_coord_id != spv::NoResult) {
        glsl.set_remapped_variable_state(translation_state.frag_coord_id, true);
        glsl.set_name(translation_state.frag_coord_id, "gl_FragCoord");
    }

    // Compile to GLSL, ready to give to GL driver.
    std::string source = glsl.compile();
    return source;
}

// ***********************
// * Functions (utility) *
// ***********************

void spirv_disasm_print(const usse::SpirvCode &spirv_binary, std::string *spirv_dump) {
    std::stringstream spirv_disasm;
    spv::Disassemble(spirv_disasm, spirv_binary);

    if (spirv_dump) {
        *spirv_dump = spirv_disasm.str();
    }

    LOG_DEBUG("SPIR-V Disassembly:\n{}", spirv_dump ? *spirv_dump : spirv_disasm.str());
}

// ***************************
// * Functions (exposed API) *
// ***************************

std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_name, const FeatureState &features, bool force_shader_debug, std::string *spirv_dump, std::string *disasm_dump) {
    TranslationState translation_state;
    std::vector<uint32_t> spirv_binary = convert_gxp_to_spirv(program, shader_name, features, translation_state, force_shader_debug, spirv_dump, disasm_dump);

    const auto source = convert_spirv_to_glsl(spirv_binary, features, translation_state);

    if (LOG_SHADER_CODE || force_shader_debug)
        LOG_DEBUG("Generated GLSL:\n{}", source);

    return source;
}

void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath) {
    const fs::path shader_filepath_str{ shader_filepath };
    fs::ifstream gxp_stream(shader_filepath_str, fs::ifstream::binary);

    if (!gxp_stream.is_open())
        return;

    const auto gxp_file_size = fs::file_size(shader_filepath_str);
    const auto gxp_program = static_cast<SceGxmProgram *>(calloc(gxp_file_size, 1));

    gxp_stream.read(reinterpret_cast<char *>(gxp_program), gxp_file_size);

    FeatureState features;
    features.direct_pack_unpack_half = true;
    features.direct_fragcolor = false;
    features.support_shader_interlock = true;
    features.pack_unpack_half_through_ext = false;
    features.use_ubo = true;

    convert_gxp_to_glsl(*gxp_program, shader_filepath_str.filename().string(), features, true);

    free(gxp_program);
}

} // namespace shader
