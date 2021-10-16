// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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
#include <shader/gxp_parser.h>
#include <shader/profile.h>
#include <shader/usse_translator_entry.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_utilities.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/overloaded.h>

#include <SPIRV/SpvBuilder.h>
#include <SPIRV/disassemble.h>
#include <spirv_glsl.hpp>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

static constexpr bool LOG_SHADER_CODE = true;
static constexpr bool DUMP_SPIRV_BINARIES = false;

using namespace shader::usse;

namespace shader {

// **************
// * Constants *
// **************

static constexpr int REG_PA_COUNT = 32 * 4;
static constexpr int REG_SA_COUNT = 32 * 4;
static constexpr int REG_I_COUNT = 3 * 4;
static constexpr int REG_TEMP_COUNT = 20 * 4;
static constexpr int REG_INDEX_COUNT = 2 * 4;
static constexpr int REG_PRED_COUNT = 4 * 4;
static constexpr int REG_O_COUNT = 20 * 4;

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

// TODO do we need this? This is made to avoid spir-v validation error regarding interface variables
struct VarToReg {
    spv::Id var;
    bool pa; // otherwise sa
    uint32_t offset;
    uint32_t size;
    DataType dtype;
};

struct TranslationState {
    std::string hash;
    spv::Id last_frag_data_id = spv::NoResult;
    spv::Id color_attachment_id = spv::NoResult;
    spv::Id color_attachment_raw_id = spv::NoResult;
    spv::Id mask_id = spv::NoResult;
    spv::Id frag_coord_id = spv::NoResult; ///< gl_FragCoord, not built-in in SPIR-V.
    spv::Id render_info_id = spv::NoResult;
    std::vector<VarToReg> var_to_regs;
    std::vector<spv::Id> interfaces;
    bool is_maskupdate{};
    bool is_fragment{};
    bool should_gl_spirv_compatible{};
};

struct VertexProgramOutputProperties {
    std::string name;
    std::uint32_t component_count;
    std::uint32_t location;

    VertexProgramOutputProperties()
        : name(nullptr)
        , component_count(0)
        , location(0) {}

    VertexProgramOutputProperties(const char *name, std::uint32_t component_count, std::uint32_t location)
        : name(name)
        , component_count(component_count)
        , location(location) {}
};
using VertexProgramOutputPropertiesMap = std::map<SceGxmVertexProgramOutputs, VertexProgramOutputProperties>;

// ******************************
// * Functions (implementation) *
// ******************************

static spv::Id create_array_if_needed(spv::Builder &b, const spv::Id param_id, const Input &input, const uint32_t explicit_array_size = 0) {
    // disabled
    return param_id;

    const auto array_size = explicit_array_size == 0 ? input.array_size : explicit_array_size;
    if (array_size > 1) {
        const auto array_size_id = b.makeUintConstant(array_size);
        return b.makeArrayType(param_id, array_size_id, 0);
    }
    return param_id;
}

static spv::Id get_type_basic(spv::Builder &b, const Input &input) {
    switch (input.type) {
    // clang-format off
    case DataType::F16:
    case DataType::F32:
         return b.makeFloatType(32);

    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::UINT32:
        return b.makeUintType(32);

    case DataType::INT8:
    case DataType::INT16:
    case DataType::INT32:
        return b.makeIntType(32);

    // clang-format on
    default: {
        LOG_ERROR("Unsupported parameter type {} used in shader.", log_hex(input.type));
        return get_type_fallback(b);
    }
    }
}

static spv::Id get_type_fallback(spv::Builder &b) {
    return b.makeFloatType(32);
}

static spv::Id get_type_scalar(spv::Builder &b, const Input &input) {
    spv::Id param_id = get_type_basic(b, input);
    param_id = create_array_if_needed(b, param_id, input);
    return param_id;
}

static spv::Id get_type_vector(spv::Builder &b, const Input &input) {
    if (input.component_count == 1) {
        return get_type_scalar(b, input);
    }
    spv::Id param_id = get_type_basic(b, input);
    param_id = b.makeVectorType(param_id, input.component_count);

    return param_id;
}

static spv::Id get_type_array(spv::Builder &b, const Input &input) {
    spv::Id param_id = get_type_basic(b, input);
    if (input.component_count > 1) {
        param_id = b.makeVectorType(param_id, input.component_count);
    }

    // TODO: Stride
    param_id = b.makeArrayType(param_id, b.makeUintConstant(input.array_size), 1);

    return param_id;
}

static spv::Id get_param_type(spv::Builder &b, const Input &input) {
    switch (input.generic_type) {
    case GenericType::SCALER:
        return get_type_scalar(b, input);
    case GenericType::VECTOR:
        return get_type_vector(b, input);
    case GenericType::ARRAY:
        return get_type_array(b, input);
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

static spv::Id create_param_sampler(spv::Builder &b, const std::string &name, const spv::Dim dim_type) {
    spv::Id sampled_type = b.makeFloatType(32);
    spv::Id image_type = b.makeImageType(sampled_type, dim_type, false, false, false, 1, spv::ImageFormatUnknown);
    spv::Id sampled_image_type = b.makeSampledImageType(image_type);
    return b.createVariable(spv::NoPrecision, spv::StorageClassUniformConstant, sampled_image_type, name.c_str());
}

static spv::Id create_input_variable(spv::Builder &b, SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils, const FeatureState &features, const char *name, const RegisterBank bank, const std::uint32_t offset, spv::Id type, const std::uint32_t size, spv::Id force_id = spv::NoResult, DataType dtype = DataType::F32) {
    std::uint32_t total_var_comp = size / 4;
    spv::Id var = !force_id ? (b.createVariable(spv::NoPrecision, reg_type_to_spv_storage_class(bank), type, name)) : force_id;
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

    if (total_var_comp != 1 && b.isArrayType(b.getContainedTypeId(b.getTypeId(var)))) {
        spv::Id arr_type = b.getContainedTypeId(b.getTypeId(var));
        spv::Id comp_type = b.getContainedTypeId(arr_type);
        total_var_comp = b.getNumTypeComponents(comp_type);

        // Reget the mask
        get_dest_mask();

        for (auto i = 0; i < b.getNumTypeComponents(arr_type); i++) {
            spv::Id elm = b.createOp(spv::OpAccessChain, b.makePointer(spv::StorageClassPrivate, comp_type),
                { var, b.makeIntConstant(i) });
            utils::store(b, parameters, utils, features, dest, b.createLoad(elm, spv::NoPrecision), dest_mask, 0 + i * 4);
        }
    } else {
        get_dest_mask();

        if (!b.isConstant(var)) {
            var = b.createLoad(var, spv::NoPrecision);
            var = utils::finalize(b, var, var, SWIZZLE_CHANNEL_4_DEFAULT, 0, dest_mask);
        }

        utils::store(b, parameters, utils, features, dest, var, dest_mask, 0);
    }

    return var;
}

static spv::Id create_builtin_sampler(spv::Builder &b, const FeatureState &features, TranslationState &translation_state, const std::string &name) {
    spv::Id sampled_type = b.makeFloatType(32);

    int sampled = 2;
    spv::ImageFormat img_format = spv::ImageFormatRgba8;

    spv::Id image_type = b.makeImageType(sampled_type, spv::Dim2D, false, false, false, sampled, img_format);
    spv::Id sampler = b.createVariable(spv::NoPrecision, spv::StorageClassUniformConstant, image_type, name.c_str());
    translation_state.interfaces.push_back(sampler);

    return sampler;
}

static spv::Id create_builtin_sampler_for_raw(spv::Builder &b, const FeatureState &features, TranslationState &translation_state, const std::string &name) {
    spv::Id ui32 = b.makeUintType(32);
    int sampled = 2;
    spv::ImageFormat img_format = spv::ImageFormatRgba16ui;

    spv::Id image_type = b.makeImageType(ui32, spv::Dim2D, false, false, false, sampled, img_format);
    spv::Id sampler = b.createVariable(spv::NoPrecision, spv::StorageClassUniformConstant, image_type, name.c_str());
    translation_state.interfaces.push_back(sampler);

    return sampler;
}

static void create_fragment_inputs(spv::Builder &b, SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils, const FeatureState &features, TranslationState &translation_state, NonDependentTextureQueryCallInfos &tex_query_infos, SamplerMap &samplers,
    const SceGxmProgram &program) {
    static const std::unordered_map<std::uint32_t, std::pair<std::string, std::uint32_t>> name_map = {
        { 0xD000, { "v_Position", 0 } },
        { 0xC000, { "v_Fog", 3 } },
        { 0xA000, { "v_Color0", 1 } },
        { 0xB000, { "v_Color1", 2 } },
        { 0x0, { "v_TexCoord0", 4 } },
        { 0x1000, { "v_TexCoord1", 5 } },
        { 0x2000, { "v_TexCoord2", 6 } },
        { 0x3000, { "v_TexCoord3", 7 } },
        { 0x4000, { "v_TexCoord4", 8 } },
        { 0x5000, { "v_TexCoord5", 9 } },
        { 0x6000, { "v_TexCoord6", 10 } },
        { 0x7000, { "v_TexCoord7", 11 } },
        { 0x8000, { "v_TexCoord8", 12 } },
        { 0x9000, { "v_TexCoord9", 13 } },
    };

    // Both vertex output and this struct should stay in a larger varying struct
    auto vertex_varyings_ptr = program.vertex_varyings();

    const SceGxmProgramAttributeDescriptor *descriptor = reinterpret_cast<const SceGxmProgramAttributeDescriptor *>(
        reinterpret_cast<const std::uint8_t *>(&vertex_varyings_ptr->vertex_outputs1) + vertex_varyings_ptr->vertex_outputs1);

    std::uint32_t pa_offset = 0;
    std::uint32_t anon_tex_count = 0;

    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);

    // Store the coords
    std::array<shader::usse::Coord, 11> coords;

    spv::Id f32 = b.makeFloatType(32);
    spv::Id v4 = b.makeVectorType(f32, 4);

    spv::Id current_coord = b.createVariable(spv::NoPrecision, spv::StorageClassInput, v4, "gl_FragCoord");
    b.addDecoration(current_coord, spv::DecorationBuiltIn, spv::BuiltInFragCoord);

    translation_state.interfaces.push_back(current_coord);
    translation_state.frag_coord_id = current_coord;

    // It may actually be total fragments input
    for (size_t i = 0; i < vertex_varyings_ptr->varyings_count; i++, descriptor++) {
        // 4 bit flag indicates a PA!
        if ((descriptor->attribute_info & 0x4000F000) != 0xF000) {
            std::uint32_t input_id = (descriptor->attribute_info & 0x4000F000);

            std::string pa_name;
            std::uint32_t pa_loc = 0;

            if (input_id & 0x40000000) {
                pa_name = "v_SpriteCoord";
            } else {
                pa_name = name_map.at(input_id).first;
                pa_loc = name_map.at(input_id).second;
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
            spv::Id pa_iter_var = spv::NoResult;

            // TODO how about centroid?
            if (input_id == 0xD000) {
                pa_iter_var = translation_state.frag_coord_id;
            } else {
                pa_iter_var = b.createVariable(spv::NoPrecision, spv::StorageClassInput, pa_iter_type, pa_name.c_str());
                b.addDecoration(pa_iter_var, spv::DecorationLocation, pa_loc);
            }

            translation_state.var_to_regs.push_back(
                { pa_iter_var,
                    true,
                    pa_offset,
                    pa_iter_size,
                    pa_dtype });
            translation_state.interfaces.push_back(pa_iter_var);
            LOG_DEBUG("Iterator: pa{} = ({}{}) {}", pa_offset, pa_type, num_comp, pa_name);

            bool do_coord = false;

            if (input_id >= 0 && input_id <= 0x9000) {
                input_id /= 0x1000;
                do_coord = true;
            } else if (input_id == 0xD000) {
                // Not sure, comment out for now
                // input_id = 10;
                // do_coord = true;
            }

            if (do_coord) {
                coords[input_id].first = pa_iter_var;
                coords[input_id].second = static_cast<int>(DataType::F32);
            }

            pa_offset += ((descriptor->size >> 4) & 3) + 1;
        }

        uint32_t tex_coord_index = (descriptor->attribute_info & 0x40F);

        // Process texture query variable (iterator), stored on a PA (primary attribute) register
        if (tex_coord_index != 0xF) {
            if (tex_coord_index == 0x400) {
                // Texcoord variable
                tex_coord_index = 10;
            }

            std::string tex_name = "";
            std::string sampling_type = "2D";
            spv::Dim dim_type = spv::Dim2D;
            uint32_t sampler_resource_index = 0;

            bool anonymous = false;

            for (std::uint32_t p = 0; p < program.parameter_count; p++) {
                const SceGxmProgramParameter &parameter = gxp_parameters[p];

                if (parameter.resource_index == descriptor->resource_index && parameter.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
                    tex_name = gxp::parameter_name(parameter);
                    sampler_resource_index = parameter.resource_index;

                    if (parameter.is_sampler_cube()) {
                        dim_type = spv::DimCube;
                        sampling_type = "CUBE";
                    }

                    break;
                }
            }

            if (tex_name.empty()) {
                LOG_INFO("Sample symbol stripped, using anonymous name");

                anonymous = true;
                tex_name = fmt::format("anonymousTexture{}", anon_tex_count++);
            }

            const auto component_type = (descriptor->component_info >> 4) & 3;
            const auto swizzle_texcoord = (descriptor->attribute_info & 0x300);

            std::string component_type_str = "????";
            DataType store_type = DataType::F16;
            switch (component_type) {
            case 0: {
                component_type_str = "uchar";
                store_type = DataType::UINT8;
                break;
            }
            case 1: {
                // Maybe char?
                LOG_WARN("Unsupported texture component: {}", component_type);
                break;
            }
            case 2: {
                component_type_str = "half";
                store_type = DataType::F16;
                break;
            }
            case 3: {
                component_type_str = "float";
                store_type = DataType::F32;
                break;
            }
            default: {
                LOG_WARN("Unsupported texture component: {}", component_type);
            }
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

            std::string texcoord_name = (tex_coord_index == 10) ? "POINTCOORD" : ("TEXCOORD" + std::to_string(tex_coord_index));
            LOG_TRACE("pa{} = tex{}{}<{}{}>({}, {}{}{})", pa_offset, sampling_type, projecting,
                component_type_str, num_component, tex_name, texcoord_name, centroid_str, swizzle_str);

            std::string tex_query_var_name = "tex_query";
            tex_query_var_name += std::to_string(tex_coord_index);

            NonDependentTextureQueryCallInfo tex_query_info;
            tex_query_info.store_type = static_cast<int>(store_type);

            // Size of this extra pa occupied
            // Force this to be PRIVATE
            const auto size = ((descriptor->size >> 6) & 3) + 1;
            tex_query_info.dest_offset = pa_offset;

            tex_query_info.coord_index = tex_coord_index;

            if (anonymous && (samplers.find(sampler_resource_index) == samplers.end())) {
                // Probably not gonna be used in future, just for non-dependent queries
                tex_query_info.sampler = create_param_sampler(b, (program.is_vertex() ? "vertTex_" : "fragTex_") + tex_name, dim_type);

                b.addDecoration(tex_query_info.sampler, spv::DecorationBinding, sampler_resource_index);
                samplers[sampler_resource_index] = tex_query_info.sampler;
            } else {
                tex_query_info.sampler = samplers[sampler_resource_index];
            }

            tex_query_infos.push_back(tex_query_info);

            pa_offset += size;
        }
    }

    static constexpr std::uint32_t TEXCOORD_BASE_LOCATION = 4;

    for (auto &query_info : tex_query_infos) {
        if (coords[query_info.coord_index].first == spv::NoResult) {
            // Create an 'in' variable
            // TODO: this really right?
            std::string coord_name = "v_TexCoord";

            if (query_info.coord_index == 10) {
                coord_name = "gl_PointCoord";
            } else {
                coord_name += std::to_string(query_info.coord_index);
            }

            coords[query_info.coord_index].first = b.createVariable(spv::NoPrecision, spv::StorageClassInput,
                b.makeVectorType(b.makeFloatType(32), /*tex_coord_comp_count*/ 4), coord_name.c_str());

            if (query_info.coord_index == 10) {
                b.addDecoration(coords[query_info.coord_index].first, spv::DecorationBuiltIn, spv::BuiltInPointCoord);
            }

            b.addDecoration(coords[query_info.coord_index].first, spv::DecorationLocation, TEXCOORD_BASE_LOCATION + query_info.coord_index);
            translation_state.interfaces.push_back(coords[query_info.coord_index].first);

            coords[query_info.coord_index].second = static_cast<int>(DataType::F32);
        }

        query_info.coord = coords[query_info.coord_index];
    }

    auto mask = create_builtin_sampler(b, features, translation_state, "f_mask");
    translation_state.mask_id = mask;

    b.addDecoration(mask, spv::DecorationBinding, MASK_TEXTURE_SLOT_IMAGE);

    if (program.is_frag_color_used()) {
        // There might be a chance that this shader also reads from OUTPUT bank. We will load last state frag data
        spv::Id source = spv::NoResult;

        Operand target_to_store;

        target_to_store.bank = RegisterBank::OUTPUT;
        target_to_store.num = 0;
        target_to_store.type = std::get<0>(shader::get_parameter_type_store_and_name(program.get_fragment_output_type()));

        auto store_source_result = [&](const bool direct_store = false) {
            if (source != spv::NoResult) {
                if (!direct_store && !is_float_data_type(target_to_store.type)) {
                    source = utils::convert_to_int(b, source, target_to_store.type, true);
                }

                utils::store(b, parameters, utils, features, target_to_store, source, 0b1111, 0);
            }
        };

        if (features.direct_fragcolor) {
            // The GPU supports gl_LastFragData. It's only OpenGL though
            // TODO: Make this not emit with OpenGL
            spv::Id v4_a = b.makeArrayType(v4, b.makeIntConstant(1), 0);
            spv::Id last_frag_data_arr = b.createVariable(spv::NoPrecision, spv::StorageClassInput, v4_a, "gl_LastFragData");
            translation_state.interfaces.push_back(last_frag_data_arr);
            spv::Id last_frag_data = b.createOp(spv::OpAccessChain, v4, { last_frag_data_arr, b.makeIntConstant(0) });

            // Copy outs into. The output data from last stage should has the same format as our
            source = last_frag_data;
            translation_state.last_frag_data_id = last_frag_data_arr;
        } else if (features.support_shader_interlock || features.support_texture_barrier) {
            // Create a global sampler, which is our color attachment
            spv::Id color_attachment = create_builtin_sampler(b, features, translation_state, "f_colorAttachment");
            spv::Id color_attachment_raw = spv::NoResult;

            b.addDecoration(color_attachment, spv::DecorationBinding, COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE);
            translation_state.color_attachment_id = color_attachment;

            spv::Id i32 = b.makeIntegerType(32, true);
            current_coord = b.createUnaryOp(spv::OpConvertFToS, b.makeVectorType(i32, 4), b.createLoad(current_coord, spv::NoPrecision));
            current_coord = b.createOp(spv::OpVectorShuffle, b.makeVectorType(i32, 2), { current_coord, current_coord, 0, 1 });

            if (features.preserve_f16_nan_as_u16) {
                spv::Id uiv4 = b.makeVectorType(b.makeUintType(32), 4);

                color_attachment_raw = create_builtin_sampler_for_raw(b, features, translation_state, "f_colorAttachment_rawUI");
                b.addDecoration(color_attachment_raw, spv::DecorationBinding, COLOR_ATTACHMENT_RAW_TEXTURE_SLOT_IMAGE);
                translation_state.color_attachment_raw_id = color_attachment_raw;

                spv::Id load_normal_cond = b.createBinOp(spv::OpFOrdLessThan, b.makeBoolType(), b.createAccessChain(spv::StorageClassPrivate, translation_state.render_info_id, { b.makeIntConstant(3) }), b.makeFloatConstant(0.5f));
                spv::Builder::If cond_builder(load_normal_cond, spv::SelectionControlMaskNone, b);

                source = b.createOp(spv::OpImageRead, v4, { b.createLoad(color_attachment, spv::NoPrecision), current_coord });
                store_source_result();
                cond_builder.makeBeginElse();
                color_attachment = color_attachment_raw;
                source = b.createOp(spv::OpImageRead, uiv4, { b.createLoad(color_attachment, spv::NoPrecision), current_coord });
                target_to_store.type = DataType::UINT16;
                store_source_result(true);
                cond_builder.makeEndIf();

                // Generated here already, so empty it out to prevent further gen
                source = spv::NoResult;
            } else {
                source = b.createOp(spv::OpImageRead, v4, { b.createLoad(color_attachment, spv::NoPrecision), current_coord });
            }
        } else {
            // Try to initialize outs[0] to some nice value. In case the GPU has garbage data for our shader
            spv::Id v4 = b.makeVectorType(b.makeFloatType(32), 4);
            spv::Id rezero = b.makeFloatConstant(0.0f);
            source = b.makeCompositeConstant(v4, { rezero, rezero, rezero, rezero });
        }

        store_source_result();
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
static void copy_uniform_block_to_register(spv::Builder &builder, spv::Id sa_bank, spv::Id block, spv::Id ite, const int start, const int vec4_count) {
    int start_in_vec4_granularity = start / 4;

    utils::make_for_loop(builder, ite, builder.makeIntConstant(0), builder.makeIntConstant(vec4_count), [&]() {
        spv::Id to_copy = builder.createAccessChain(spv::StorageClassUniform, block, { builder.createLoad(ite, spv::NoPrecision) });
        spv::Id dest = builder.createAccessChain(spv::StorageClassPrivate, sa_bank, { builder.createBinOp(spv::OpIAdd, builder.getTypeId(builder.createLoad(ite, spv::NoPrecision)), builder.createLoad(ite, spv::NoPrecision), builder.makeIntConstant(start_in_vec4_granularity)) });
        spv::Id dest_friend = spv::NoResult;

        if (start % 4 == 0) {
            builder.createStore(builder.createLoad(to_copy, spv::NoPrecision), dest);
        } else {
            dest_friend = builder.createAccessChain(spv::StorageClassPrivate, sa_bank, { builder.createBinOp(spv::OpIAdd, builder.getTypeId(builder.createLoad(ite, spv::NoPrecision)), builder.createLoad(ite, spv::NoPrecision), builder.makeIntConstant(start_in_vec4_granularity + 1)) });

            std::vector<spv::Id> ops_copy_1 = { dest, to_copy };
            std::vector<spv::Id> ops_copy_2 = { dest_friend, to_copy };

            for (int i = 0; i < start % 4; i++) {
                ops_copy_1.push_back(i);
                ops_copy_2.push_back(4 + (start % 4) + i);
            }

            for (int i = 0; i < (4 - start % 4); i++) {
                ops_copy_1.push_back(4 + i);
                ops_copy_2.push_back((start % 4) + i);
            }

            to_copy = builder.createOp(spv::OpVectorShuffle, builder.getTypeId(to_copy), ops_copy_1);
            spv::Id to_copy_2 = builder.createOp(spv::OpVectorShuffle, builder.getTypeId(to_copy), ops_copy_2);

            builder.createStore(builder.createLoad(to_copy, spv::NoPrecision), dest);
            builder.createStore(builder.createLoad(to_copy_2, spv::NoPrecision), dest_friend);
        }
    });
}

static SpirvShaderParameters create_parameters(spv::Builder &b, const SceGxmProgram &program, utils::SpirvUtilFunctions &utils,
    const FeatureState &features, TranslationState &translation_state, SceGxmProgramType program_type, NonDependentTextureQueryCallInfos &texture_queries,
    const std::vector<SceGxmVertexAttribute> *hint_attributes) {
    SpirvShaderParameters spv_params = {};
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);

    // Make array type. TODO: Make length configurable
    spv::Id f32_type = b.makeFloatType(32);
    spv::Id i32_type = b.makeIntType(32);
    spv::Id b_type = b.makeBoolType();

    spv::Id f32_v4_type = b.makeVectorType(f32_type, 4);
    spv::Id pa_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(REG_PA_COUNT / 4), 0);
    spv::Id sa_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(REG_SA_COUNT / 4), 0);
    spv::Id i_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(REG_I_COUNT / 4), 0);
    spv::Id temp_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(REG_TEMP_COUNT / 4), 0);
    spv::Id index_arr_type = b.makeArrayType(i32_type, b.makeIntConstant(REG_INDEX_COUNT / 4), 0);
    spv::Id pred_arr_type = b.makeArrayType(b_type, b.makeIntConstant(REG_PRED_COUNT / 4), 0);
    spv::Id o_arr_type = b.makeArrayType(f32_v4_type, b.makeIntConstant(REG_O_COUNT / 4), 0);

    // Create register banks
    spv_params.ins = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, pa_arr_type, "pa");
    spv_params.uniforms = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, sa_arr_type, "sa");
    spv_params.internals = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, i_arr_type, "internals");
    spv_params.temps = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, temp_arr_type, "r");
    spv_params.predicates = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, pred_arr_type, "p");
    spv_params.indexes = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, index_arr_type, "idx");
    spv_params.outs = b.createVariable(spv::NoPrecision, spv::StorageClassPrivate, o_arr_type, "outs");

    SamplerMap samplers;

    spv::Id ite_copy = b.createVariable(spv::NoPrecision, spv::StorageClassFunction, i32_type, "i");

    using literal_pair = std::pair<std::uint32_t, spv::Id>;

    std::vector<literal_pair> literal_pairs;

    const auto program_input = shader::get_program_input(program);

    std::map<int, int> buffer_bases;
    std::map<int, std::uint32_t> buffer_sizes;

    for (const auto &buffer : program_input.uniform_buffers) {
        const auto buffer_size = (buffer.size + 3) / 4;
        buffer_sizes.emplace(buffer.index, buffer_size);
    }

    int last_base = 0;
    int total_members = 0;

    if (!buffer_sizes.empty()) {
        usse::SpirvCode buffer_container_member_types;
        const bool is_vert = (program_type == SceGxmProgramType::Vertex);

        for (auto &[buffer_index, buffer_size] : buffer_sizes) {
            SpirvUniformBufferInfo buffer_info;
            buffer_info.base = last_base;
            buffer_info.size = buffer_size * 16;
            buffer_info.index_in_container = total_members++;

            buffer_bases.emplace(buffer_index, last_base);
            spv_params.buffers.emplace(buffer_index, buffer_info);

            last_base += buffer_size * 16;

            spv::Id buffer_type_arr = b.makeArrayType(f32_v4_type, b.makeIntConstant(buffer_size), 16);
            b.addDecoration(buffer_type_arr, spv::DecorationArrayStride, 16);

            buffer_container_member_types.push_back(buffer_type_arr);
        }

        spv::Id buffer_container_type = b.makeStructType(buffer_container_member_types,
            is_vert ? "vertexDataType" : "fragmentDataType");

        b.addDecoration(buffer_container_type, spv::DecorationBlock);
        b.addDecoration(buffer_container_type, spv::DecorationGLSLShared);
        b.addDecoration(buffer_container_type, spv::DecorationRestrict);
        b.addDecoration(buffer_container_type, spv::DecorationNonWritable);

        spv_params.buffer_container = b.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, buffer_container_type,
            is_vert ? "vertexData" : "fragmentData");

        b.addDecoration(spv_params.buffer_container, spv::DecorationBinding, is_vert ? 0 : 1);

        for (auto &[index, buffer] : spv_params.buffers) {
            const std::string member_name = fmt::format("buffer{}", index);
            b.addMemberDecoration(buffer_container_type, buffer.index_in_container, spv::DecorationOffset, buffer.base);
            b.addMemberName(buffer_container_type, buffer.index_in_container, member_name.c_str());
        }
    }

    for (const auto &buffer : program_input.uniform_buffers) {
        if (buffer.reg_block_size > 0) {
            const uint32_t reg_block_size_in_f32v = (buffer.reg_block_size + 3) / 4;
            const auto spv_buffer = b.createAccessChain(spv::StorageClassStorageBuffer, spv_params.buffer_container,
                { b.makeIntConstant(spv_params.buffers.at(buffer.index).index_in_container) });
            copy_uniform_block_to_register(b, spv_params.uniforms, spv_buffer, ite_copy, buffer.reg_start_offset, reg_block_size_in_f32v);
        }
    }

    const auto add_var_to_reg = [&](const Input &input, const std::string &name, std::uint16_t semantic, bool pa, bool regformat, std::int32_t location) {
        const spv::Id param_type = get_param_type(b, input);
        int type_size = get_data_type_size(input.type);
        spv::Id var;
        if (false) {
            int num_comp = type_size * input.array_size * input.component_count / 4;
            spv::Id type = utils::make_vector_or_scalar_type(b, b.makeIntType(32), num_comp);
            var = b.createVariable(spv::NoPrecision, spv::StorageClassInput, type, name.c_str());

            VarToReg var_to_reg = {};
            var_to_reg.var = var;
            var_to_reg.pa = pa;
            var_to_reg.offset = input.offset;
            var_to_reg.size = num_comp * 4;
            var_to_reg.dtype = DataType::INT32;
            translation_state.var_to_regs.push_back(var_to_reg);
        } else {
            var = b.createVariable(spv::NoPrecision, spv::StorageClassInput, param_type, name.c_str());

            switch (semantic) {
            case SCE_GXM_PARAMETER_SEMANTIC_INDEX:
                b.addDecoration(var, spv::DecorationBuiltIn, spv::BuiltInVertexId);
                break;

            case SCE_GXM_PARAMETER_SEMANTIC_INSTANCE:
                b.addDecoration(var, spv::DecorationBuiltIn, spv::BuiltInInstanceId);
                break;

            default:
                break;
            }

            if (location != -1) {
                b.addDecoration(var, spv::DecorationLocation, location);
            }

            VarToReg var_to_reg = {};
            var_to_reg.var = var;
            var_to_reg.pa = pa;
            var_to_reg.offset = input.offset;
            var_to_reg.size = input.array_size * input.component_count * 4;
            var_to_reg.dtype = input.type;
            translation_state.var_to_regs.push_back(var_to_reg);
        }

        translation_state.interfaces.push_back(var);
    };

    for (const auto &sampler : program_input.samplers) {
        const auto sampler_spv_var = create_param_sampler(b, (program.is_vertex() ? "vertTex_" : "fragTex_") + sampler.name, (sampler.is_cube ? spv::DimCube : spv::Dim2D));
        samplers.emplace(sampler.index, sampler_spv_var);

        // Prefer smaller slot index for fragments since they are gonna be used frequently.
        b.addDecoration(sampler_spv_var, spv::DecorationBinding, sampler.index + (program.is_vertex() ? SCE_GXM_MAX_TEXTURE_UNITS : 0));
    }

    // Log parameters
    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = gxp_parameters[i];
        uint16_t curi = parameter.category;
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM || parameter.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
            bool is_uniform = parameter.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM;
            std::string var_name = gxp::parameter_name(parameter);

            auto container = gxp::get_container_by_index(program, parameter.container_index);
            std::uint32_t offset = parameter.resource_index;

            if (container) {
                offset = container->base_sa_offset + parameter.resource_index;
            }

            const auto parameter_type = gxp::parameter_type(parameter);
            const auto [store_type, param_type_name] = shader::get_parameter_type_store_and_name(parameter_type);
            std::string param_log = fmt::format("[{} + {}] {}a{} = ({}{}) {}",
                gxp::get_container_name(parameter.container_index), parameter.resource_index,
                is_uniform ? "s" : "p", offset, param_type_name, parameter.component_count, var_name);

            if (parameter.array_size > 1) {
                param_log += fmt::format("[{}]", parameter.array_size);
            }

            LOG_DEBUG(param_log);
        }
    }

    std::int32_t in_fcount_allocated = 0;

    for (const auto &input : program_input.inputs) {
        std::visit(overloaded{
                       [&](const LiteralInputSource &s) {
                           literal_pairs.emplace_back(input.offset, b.makeFloatConstant(s.data));
                           // Pair sort automatically sort offset for us
                           std::sort(literal_pairs.begin(), literal_pairs.end());
                       },
                       [&](const UniformBufferInputSource &s) {
                           Operand reg;
                           reg.bank = RegisterBank::SECATTR;
                           reg.num = input.offset;
                           reg.type = DataType::INT32;
                           const auto base = buffer_bases.at(s.index) + s.base;
                           utils::store(b, spv_params, utils, features, reg, b.makeIntConstant(base), 0b1, 0);
                       },
                       [&](const DependentSamplerInputSource &s) {
                           const auto spv_sampler = samplers.at(s.index);
                           spv_params.samplers.emplace(input.offset, spv_sampler);
                       },
                       [&](const AttributeInputSource &s) {
                           add_var_to_reg(input, s.name, s.semantic, true, s.regformat, in_fcount_allocated / 4);
                           in_fcount_allocated += ((input.array_size * input.component_count + 3) / 4 * 4);
                       } },
            input.source);
    }

    if ((in_fcount_allocated == 0) && (program.primary_reg_count != 0)) {
        // Using hint to create attribute. Looks like attribute with F32 types are stripped, otherwise
        // whole shader symbols are kept...
        if (hint_attributes) {
            LOG_INFO("Shader stripped all symbols, trying to use hint attributes");

            for (std::size_t i = 0; i < hint_attributes->size(); i++) {
                Input inp;
                inp.offset = hint_attributes->at(i).regIndex;
                inp.bank = RegisterBank::PRIMATTR;
                inp.generic_type = GenericType::VECTOR;
                inp.type = DataType::F32;
                inp.component_count = 4;
                inp.array_size = (hint_attributes->at(i).componentCount + 3) >> 2;

                add_var_to_reg(inp, fmt::format("attribute{}", i), 0, true, false, static_cast<std::int32_t>(i));
            }
        }
    }

    // We should avoid ugly and long GLSL code generated. Also, inefficient SPIR-V code.
    // Packing literals into vector may help solving this.
    if (literal_pairs.size() != 0) {
        std::uint32_t composite_base = literal_pairs[0].first;

        std::vector<spv::Id> constituents;
        constituents.push_back(literal_pairs[0].second);

        auto create_new_literal_pack = [&]() {
            // Create new literal composite
            spv::Id composite_var;
            if (constituents.size() > 1) {
                composite_var = b.makeCompositeConstant(b.makeVectorType(f32_type, static_cast<int>(constituents.size())),
                    constituents);
            } else {
                composite_var = constituents[0];
            }
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

    spv::Id f32 = b.makeFloatType(32);
    spv::Id v4 = b.makeVectorType(f32, 4);

    if (program_type == SceGxmProgramType::Vertex) {
        // Create the default reg uniform buffer
        spv::Id render_buf_type = b.makeStructType({ v4, f32, f32, f32 }, "GxmRenderVertBufferBlock");
        b.addDecoration(render_buf_type, spv::DecorationBlock);
        b.addDecoration(render_buf_type, spv::DecorationGLSLShared);

        b.addMemberDecoration(render_buf_type, 0, spv::DecorationOffset, 0);
        b.addMemberDecoration(render_buf_type, 1, spv::DecorationOffset, 16);
        b.addMemberDecoration(render_buf_type, 2, spv::DecorationOffset, 20);
        b.addMemberDecoration(render_buf_type, 3, spv::DecorationOffset, 24);

        b.addMemberName(render_buf_type, 0, "viewport_flip");
        b.addMemberName(render_buf_type, 1, "viewport_flag");
        b.addMemberName(render_buf_type, 2, "screen_width");
        b.addMemberName(render_buf_type, 3, "screen_height");

        translation_state.render_info_id = b.createVariable(spv::NoPrecision, spv::StorageClassUniform, render_buf_type, "renderVertInfo");

        b.addDecoration(translation_state.render_info_id, spv::DecorationBinding, 2);
    }

    if (program_type == SceGxmProgramType::Fragment) {
        spv::Id render_buf_type = b.makeStructType({ f32, f32, f32, f32 }, "GxmRenderFragBufferBlock");

        b.addDecoration(render_buf_type, spv::DecorationBlock);
        b.addDecoration(render_buf_type, spv::DecorationGLSLShared);

        b.addMemberDecoration(render_buf_type, 0, spv::DecorationOffset, 0);
        b.addMemberDecoration(render_buf_type, 1, spv::DecorationOffset, 4);
        b.addMemberDecoration(render_buf_type, 2, spv::DecorationOffset, 8);
        b.addMemberDecoration(render_buf_type, 3, spv::DecorationOffset, 12);

        b.addMemberName(render_buf_type, 0, "back_disabled");
        b.addMemberName(render_buf_type, 1, "front_disabled");
        b.addMemberName(render_buf_type, 2, "writing_mask");
        b.addMemberName(render_buf_type, 3, "use_raw_image");

        translation_state.render_info_id = b.createVariable(spv::NoPrecision, spv::StorageClassUniform, render_buf_type, "renderFragInfo");

        b.addDecoration(translation_state.render_info_id, spv::DecorationBinding, 3);

        create_fragment_inputs(b, spv_params, utils, features, translation_state, texture_queries, samplers, program);
    }

    return spv_params;
}

static void generate_shader_body(spv::Builder &b, const SpirvShaderParameters &parameters, const SceGxmProgram &program,
    const FeatureState &features, utils::SpirvUtilFunctions &utils, spv::Function *begin_hook_func, spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &texture_queries) {
    // Do texture queries
    usse::convert_gxp_usse_to_spirv(b, program, features, parameters, utils, begin_hook_func, end_hook_func, texture_queries);
}

static spv::Function *make_frag_finalize_function(spv::Builder &b, const SpirvShaderParameters &parameters,
    const SceGxmProgram &program, utils::SpirvUtilFunctions &utils, const FeatureState &features, TranslationState &translate_state) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *frag_fin_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Function *frag_fin_func = b.makeFunctionEntry(spv::NoPrecision, b.makeVoidType(), "frag_output_finalize", {},
        decorations, &frag_fin_block);

    const SceGxmParameterType param_type = program.get_fragment_output_type();
    auto vertex_varyings_ptr = program.vertex_varyings();

    Operand color_val_operand;
    color_val_operand.bank = program.is_native_color() ? RegisterBank::OUTPUT : RegisterBank::PRIMATTR;
    color_val_operand.num = 0;
    color_val_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    color_val_operand.type = std::get<0>(shader::get_parameter_type_store_and_name(param_type));

    int reg_off = 0;
    if (!program.is_native_color() && vertex_varyings_ptr->output_param_type == 1) {
        reg_off = vertex_varyings_ptr->fragment_output_start;
        if (reg_off != 0) {
            LOG_INFO("Non zero pa offset: {} at {}", reg_off, translate_state.hash.c_str());
        }
    }

    spv::Id vec4_type = b.makeVectorType(b.makeFloatType(32), 4);
    spv::Id one_f = b.makeFloatConstant(1.0f);
    spv::Id color = spv::NoResult;

    color = utils::load(b, parameters, utils, features, color_val_operand, 0xF, reg_off);

    if (!is_float_data_type(color_val_operand.type))
        color = utils::convert_to_float(b, color, color_val_operand.type, true);

    if (program.is_frag_color_used() && features.should_use_shader_interlock() && !translate_state.should_gl_spirv_compatible) {
        spv::Id signed_i32 = b.makeIntegerType(32, true);
        spv::Id coord_id = b.createLoad(translate_state.frag_coord_id, spv::NoPrecision);
        spv::Id depth = b.createOp(spv::OpAccessChain, b.makeFloatType(32), { coord_id, b.makeIntConstant(2) });
        spv::Id translated_id = b.createUnaryOp(spv::OpConvertFToS, b.makeVectorType(signed_i32, 4), coord_id);
        translated_id = b.createOp(spv::OpVectorShuffle, b.makeVectorType(signed_i32, 2), { translated_id, translated_id, 0, 1 });
        b.createNoResultOp(spv::OpImageWrite, { b.createLoad(translate_state.color_attachment_id, spv::NoPrecision), translated_id, color });

        if (features.preserve_f16_nan_as_u16) {
            color_val_operand.type = DataType::UINT16;
            color = utils::load(b, parameters, utils, features, color_val_operand, 0xF, reg_off);

            b.createNoResultOp(spv::OpImageWrite, { b.createLoad(translate_state.color_attachment_raw_id, spv::NoPrecision), translated_id, color });
        }
    } else {
        spv::Id out = b.createVariable(spv::NoPrecision, spv::StorageClassOutput, b.makeVectorType(b.makeFloatType(32), 4), "out_color");
        translate_state.interfaces.push_back(out);
        b.addDecoration(out, spv::DecorationLocation, 0);
        b.createStore(color, out);

        if (features.preserve_f16_nan_as_u16) {
            const spv::Id u4 = b.makeVectorType(b.makeUintType(32), 4);
            const spv::Id out_u16_raw = b.createVariable(spv::NoPrecision, spv::StorageClassOutput, u4, "out_color_ui");
            translate_state.interfaces.push_back(out_u16_raw);
            b.addDecoration(out_u16_raw, spv::DecorationLocation, 1);

            color_val_operand.type = DataType::UINT16;
            color = utils::load(b, parameters, utils, features, color_val_operand, 0xF, reg_off);

            b.createStore(color, out_u16_raw);
        }
    }

    // Discard masked fragments
    spv::Id current_coord = translate_state.frag_coord_id;
    spv::Id i32 = b.makeIntegerType(32, true);
    current_coord = b.createUnaryOp(spv::OpConvertFToS, b.makeVectorType(i32, 4), b.createLoad(current_coord, spv::NoPrecision));
    current_coord = b.createOp(spv::OpVectorShuffle, b.makeVectorType(i32, 2), { current_coord, current_coord, 0, 1 });
    spv::Id sampled_type = b.makeFloatType(32);
    spv::Id v4 = b.makeVectorType(sampled_type, 4);
    spv::Id texel = b.createOp(spv::OpImageRead, v4, { b.createLoad(translate_state.mask_id, spv::NoPrecision), current_coord });
    spv::Id rezero = b.makeFloatConstant(0.5f);
    spv::Id zero = b.makeCompositeConstant(v4, { rezero, rezero, rezero, rezero });
    spv::Id pred = b.createOp(spv::OpFOrdLessThan, b.makeBoolType(), { texel, zero });
    spv::Id pred2 = b.createUnaryOp(spv::OpAll, b.makeBoolType(), pred);
    spv::Builder::If cond_builder(pred2, spv::SelectionControlMaskNone, b);
    b.makeDiscard();
    cond_builder.makeEndIf();

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

    gxp::GxmVertexOutputTexCoordInfos coord_infos = {};
    SceGxmVertexProgramOutputs vertex_outputs = gxp::get_vertex_outputs(program, &coord_infos);

    static const auto calculate_copy_comp_count = [](uint8_t info) {
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

    VertexProgramOutputPropertiesMap vertex_properties_map;
    // list is used here to gurantee the vertex outputs are written in right order
    std::list<SceGxmVertexProgramOutputs> vertex_outputs_list;
    const auto add_vertex_output_info = [&](SceGxmVertexProgramOutputs vo, const char *name, std::uint32_t component_count, std::uint32_t location) {
        vertex_properties_map.emplace(vo, VertexProgramOutputProperties(name, component_count, location));
        vertex_outputs_list.push_back(vo);
    };
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION, "v_Position", 4, 0);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0, "v_Color0", 4, 1);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1, "v_Color1", 4, 2);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG, "v_Fog", 2, 3);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0, "v_TexCoord0", calculate_copy_comp_count(coord_infos[0]), 4);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD1, "v_TexCoord1", calculate_copy_comp_count(coord_infos[1]), 5);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD2, "v_TexCoord2", calculate_copy_comp_count(coord_infos[2]), 6);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD3, "v_TexCoord3", calculate_copy_comp_count(coord_infos[3]), 7);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD4, "v_TexCoord4", calculate_copy_comp_count(coord_infos[4]), 8);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD5, "v_TexCoord5", calculate_copy_comp_count(coord_infos[5]), 9);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD6, "v_TexCoord6", calculate_copy_comp_count(coord_infos[6]), 10);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD7, "v_TexCoord7", calculate_copy_comp_count(coord_infos[7]), 11);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD8, "v_TexCoord8", calculate_copy_comp_count(coord_infos[8]), 12);
    add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9, "v_TexCoord9", calculate_copy_comp_count(coord_infos[9]), 13);
    // TODO: this should be translated to gl_PointSize
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_PSIZE, "v_Psize", 1);
    // TODO: these should be translated to gl_ClipDistance
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP0, "v_Clip0", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP1, "v_Clip1", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP2, "v_Clip2", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP3, "v_Clip3", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP4, "v_Clip4", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP5, "v_Clip5", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP6, "v_Clip6", 1);
    // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP7, "v_Clip7", 1);

    Operand o_op;
    o_op.bank = RegisterBank::OUTPUT;
    o_op.num = 0;
    o_op.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    for (const auto vo : vertex_outputs_list) {
        if (vertex_outputs & vo) {
            const auto vo_typed = static_cast<SceGxmVertexProgramOutputs>(vo);
            VertexProgramOutputProperties properties = vertex_properties_map.at(vo_typed);

            // TODO: use the actual size of variable
            const spv::Id out_type = b.makeVectorType(b.makeFloatType(32), 4);
            const spv::Id out_var = b.createVariable(spv::NoPrecision, spv::StorageClassOutput, out_type, properties.name.c_str());

            b.addDecoration(out_var, spv::DecorationLocation, properties.location);
            translation_state.interfaces.push_back(out_var);

            // Do store
            spv::Id o_val = utils::load(b, parameters, utils, features, o_op, 0b1111, 0);

            if (vo == SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION) {
                b.addDecoration(out_var, spv::DecorationBuiltIn, spv::BuiltInPosition);

                // Transform screen space coordinate to ndc when viewport is disabled.
                spv::Id f32 = b.makeFloatType(32);
                spv::Id v4 = b.makeVectorType(b.makeFloatType(32), 4);
                spv::Id half = b.makeFloatConstant(0.5f);
                spv::Id one = b.makeFloatConstant(1.0f);
                spv::Id neg_one = b.makeFloatConstant(-1.0f);
                spv::Id two = b.makeFloatConstant(2.0f);
                spv::Id neg_two = b.makeFloatConstant(-2.0f);
                spv::Id zero = b.makeFloatConstant(0.0f);

                spv::Id viewport_id = b.createAccessChain(spv::StorageClassUniform, translation_state.render_info_id, { b.makeIntConstant(1) });
                spv::Id screen_width_id = b.createAccessChain(spv::StorageClassUniform, translation_state.render_info_id, { b.makeIntConstant(2) });
                spv::Id screen_height_id = b.createAccessChain(spv::StorageClassUniform, translation_state.render_info_id, { b.makeIntConstant(3) });

                spv::Id pred = b.createOp(spv::OpFOrdLessThan, b.makeBoolType(), { b.createLoad(viewport_id, spv::NoPrecision), half });
                spv::Builder::If cond_builder(pred, spv::SelectionControlMaskNone, b);

                spv::Id width_recp = b.createBinOp(spv::OpFDiv, f32, two, b.createLoad(screen_width_id, spv::NoPrecision));
                spv::Id height_recp = b.createBinOp(spv::OpFDiv, f32, neg_two, b.createLoad(screen_height_id, spv::NoPrecision));
                spv::Id scale = b.createCompositeConstruct(v4, { width_recp, height_recp, one, one });
                spv::Id constant = b.createCompositeConstruct(v4, { neg_one, one, zero, zero });
                spv::Id o_val2 = b.createBinOp(spv::OpFMul, v4, o_val, scale);
                o_val2 = b.createBinOp(spv::OpFAdd, v4, o_val2, constant);

                if (translation_state.render_info_id != spv::NoResult) {
                    spv::Id flip_vec_id = b.createAccessChain(spv::StorageClassUniform, translation_state.render_info_id, { b.makeIntConstant(0) });
                    o_val2 = b.createBinOp(spv::OpFMul, v4, o_val2, flip_vec_id);
                }

                // o_val2 = (x,y) * (2/width, -2/height) + (-1,1)
                b.createStore(o_val2, out_var);

                // Note: Depth range and user clip planes are ineffective in this mode
                // However, that can't be directly translated, so we just gonna set it to w here
                spv::Id z_ref = b.createAccessChain(spv::StorageClassOutput, out_var, { b.makeIntConstant(2) });
                spv::Id w_ref = b.createAccessChain(spv::StorageClassOutput, out_var, { b.makeIntConstant(3) });

                b.createStore(b.createLoad(w_ref, spv::NoPrecision), z_ref);

                cond_builder.makeBeginElse();

                if (translation_state.render_info_id != spv::NoResult) {
                    spv::Id flip_vec_id = b.createAccessChain(spv::StorageClassUniform, translation_state.render_info_id, { b.makeIntConstant(0) });
                    o_val = b.createBinOp(spv::OpFMul, out_type, o_val, flip_vec_id);
                }

                b.createStore(o_val, out_var);
                cond_builder.makeEndIf();
            } else {
                b.createStore(o_val, out_var);
            }

            o_op.num += properties.component_count;
        }
    }

    b.makeReturn(false);
    b.setBuildPoint(last_build_point);

    return vert_fin_func;
}

static spv::Function *make_frag_initialize_function(spv::Builder &b, TranslationState &translate_state) {
    std::vector<std::vector<spv::Decoration>> decorations;

    spv::Block *frag_init_block;
    spv::Block *last_build_point = b.getBuildPoint();

    spv::Function *frag_init_func = b.makeFunctionEntry(spv::NoPrecision, b.makeVoidType(), "frag_init", {},
        decorations, &frag_init_block);

    // Note! We use CCW as Front face, however we invert the coordinates so the front face is actually CW, identical to GXM (GXM also has front-face as CW)
    spv::Id booltype = b.makeBoolType();
    spv::Id zero = b.makeFloatConstant(0.0f);

    spv::Id front_facing = b.createVariable(spv::NoPrecision, spv::StorageClassInput, booltype, "gl_FrontFacing");
    spv::Id front_disabled = b.createAccessChain(spv::StorageClassUniform, translate_state.render_info_id, { b.makeIntConstant(1) });
    spv::Id back_disabled = b.createAccessChain(spv::StorageClassUniform, translate_state.render_info_id, { b.makeIntConstant(0) });
    b.addDecoration(front_facing, spv::DecorationBuiltIn, spv::BuiltInFrontFacing);

    front_facing = b.createLoad(front_facing, spv::NoPrecision);

    spv::Id pred = b.createOp(spv::OpLogicalAnd, booltype, { b.createBinOp(spv::OpFOrdNotEqual, booltype, b.createLoad(front_disabled, spv::NoPrecision), zero), front_facing });

    spv::Builder::If front_disabled_cond_builder(pred, spv::SelectionControlMaskNone, b);
    b.makeDiscard();
    front_disabled_cond_builder.makeEndIf();

    pred = b.createOp(spv::OpLogicalAnd, booltype, { b.createBinOp(spv::OpFOrdNotEqual, booltype, b.createLoad(back_disabled, spv::NoPrecision), zero), b.createUnaryOp(spv::OpLogicalNot, booltype, front_facing) });

    spv::Builder::If back_disabled_cond_builder(pred, spv::SelectionControlMaskNone, b);
    b.makeDiscard();
    back_disabled_cond_builder.makeEndIf();

    b.makeReturn(false);
    b.setBuildPoint(last_build_point);

    return frag_init_func;
}

static void generate_update_mask_body(spv::Builder &b, utils::SpirvUtilFunctions &utils, const FeatureState &features, TranslationState &translate_state) {
    const spv::Id writing_mask_var = b.createAccessChain(spv::StorageClassUniform, translate_state.render_info_id, { b.makeIntConstant(2) });
    const spv::Id writing_mask = b.createLoad(writing_mask_var, spv::NoPrecision);

    const spv::Id v4 = b.makeVectorType(b.makeFloatType(32), 4);
    const spv::Id mask_v = b.createCompositeConstruct(v4, { writing_mask, writing_mask, writing_mask, writing_mask });

    const spv::Id out = b.createVariable(spv::NoPrecision, spv::StorageClassOutput, v4, "out_color");
    translate_state.interfaces.push_back(out);
    b.addDecoration(out, spv::DecorationLocation, 0);

    b.createStore(mask_v, out);
}

static SpirvCode convert_gxp_to_spirv_impl(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features, TranslationState &translation_state, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool force_shader_debug, std::function<bool(const std::string &ext, const std::string &dump)> dumper) {
    SpirvCode spirv;

    SceGxmProgramType program_type = program.get_type();

    spv::SpvBuildLogger spv_logger;
    spv::Builder b(/* SPV_VERSION*/ 0x10300, 0x1337 << 12, &spv_logger);
    b.setSourceFile(shader_hash);
    b.setEmitOpLines();
    b.addSourceExtension("gxp");
    b.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

    // Capabilities
    b.addCapability(spv::Capability::CapabilityShader);
    b.addCapability(spv::Capability::CapabilityFloat16);

    NonDependentTextureQueryCallInfos texture_queries;
    utils::SpirvUtilFunctions utils;

    std::string entry_point_name;
    spv::ExecutionModel execution_model;

    translation_state.hash = shader_hash;

    switch (program_type) {
    default:
        LOG_ERROR("Unknown GXM program type");
        [[fallthrough]];
    // fallthrough
    case Vertex:
        entry_point_name = "main_vs";
        execution_model = spv::ExecutionModelVertex;
        break;
    case Fragment:
        entry_point_name = "main_fs";
        execution_model = spv::ExecutionModelFragment;
        break;
    }

    std::string disasm_dump;

    // Put disasm storage
    disasm::disasm_storage = &disasm_dump;

    // Entry point
    spv::Function *spv_func_main = b.makeEntryPoint(entry_point_name.c_str());
    spv::Function *end_hook_func = nullptr;
    spv::Function *begin_hook_func = nullptr;

    std::vector<spv::Id> empty_args;

    // Lock/unlock and read texel for shader interlock. Texture barrier will have glTextureBarrier() called so we don't
    // have to worry too much. Texture barrier will not be accurate and may be broken though.
    if (program_type == SceGxmProgramType::Fragment) {
        b.addExecutionMode(spv_func_main, spv::ExecutionModeOriginLowerLeft);

        if (program.is_frag_color_used() && features.should_use_shader_interlock()) {
            b.addExecutionMode(spv_func_main, spv::ExecutionModePixelInterlockOrderedEXT);
            b.addExtension("SPV_EXT_fragment_shader_interlock");
            b.addCapability(spv::CapabilityFragmentShaderPixelInterlockEXT);

            b.createNoResultOp(spv::OpBeginInvocationInterlockEXT);
        }
    }

    // Generate parameters
    SpirvShaderParameters parameters = create_parameters(b, program, utils, features, translation_state, program_type, texture_queries, hint_attributes);

    if (!translation_state.is_maskupdate) {
        if (program.is_fragment()) {
            begin_hook_func = make_frag_initialize_function(b, translation_state);
            end_hook_func = make_frag_finalize_function(b, parameters, program, utils, features, translation_state);
        } else {
            end_hook_func = make_vert_finalize_function(b, parameters, program, utils, features, translation_state);
        }

        for (auto &var_to_reg : translation_state.var_to_regs) {
            create_input_variable(b, parameters, utils, features, "", var_to_reg.pa ? RegisterBank::PRIMATTR : RegisterBank::SECATTR,
                var_to_reg.offset, spv::NoResult, var_to_reg.size, var_to_reg.var, var_to_reg.dtype);
        }

        // Initialize vertex output to 0
        if (program.is_vertex()) {
            spv::Id i32_type = b.makeIntType(32);
            spv::Id ite = b.createVariable(spv::NoPrecision, spv::StorageClassFunction, i32_type, "i");
            spv::Id v4 = b.makeVectorType(b.makeFloatType(32), 4);
            spv::Id rezero = b.makeFloatConstant(0.0f);
            spv::Id rezero_v = b.makeCompositeConstant(v4, { rezero, rezero, rezero, rezero });
            utils::make_for_loop(b, ite, b.makeIntConstant(0), b.makeIntConstant(REG_O_COUNT / 4), [&]() {
                Operand target_to_store;
                spv::Id dest = b.createAccessChain(spv::StorageClassPrivate, parameters.outs, { b.createLoad(ite, spv::NoPrecision) });
                b.createStore(rezero_v, dest);
            });
        }

        generate_shader_body(b, parameters, program, features, utils, begin_hook_func, end_hook_func, texture_queries);
    } else {
        generate_update_mask_body(b, utils, features, translation_state);
    }
    b.leaveFunction();

    // Add entry point to Builder
    auto entrypoint = b.addEntryPoint(execution_model, spv_func_main, entry_point_name.c_str());
    for (auto &i : translation_state.interfaces) {
        entrypoint->addIdOperand(i);
    }

    auto spirv_log = spv_logger.getAllMessages();
    if (!spirv_log.empty())
        LOG_ERROR("SPIR-V Error:\n{}", spirv_log);

    if (dumper) {
        dumper("dsm", disasm_dump);
    }

    b.dump(spirv);

    if (LOG_SHADER_CODE || force_shader_debug) {
        std::string spirv_dump;
        spirv_disasm_print(spirv, &spirv_dump);
        if (dumper) {
            dumper("spv", spirv_dump);
        }
    }

    if (DUMP_SPIRV_BINARIES) {
        // TODO: use base path host var
        std::ofstream spirv_dump(shader_hash + ".spv", std::ios::binary);
        spirv_dump.write((char *)&spirv, spirv.size() * sizeof(uint32_t));
        spirv_dump.close();
    }

    return spirv;
}

static std::string convert_spirv_to_glsl(const std::string &shader_name, SpirvCode spirv_binary, const FeatureState &features, TranslationState &translation_state, bool is_frag_color_used) {
    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

    spirv_cross::CompilerGLSL::Options options;

    options.version = 430;
    options.es = false;
    options.enable_420pack_extension = true;

    // TODO: this might be needed in the future
    // options.vertex.flip_vert_y = true;

    glsl.set_common_options(options);
    glsl.add_header_line("// Shader Name: " + shader_name);

    if (features.direct_fragcolor && translation_state.last_frag_data_id != spv::NoResult) {
        glsl.require_extension("GL_EXT_shader_framebuffer_fetch");

        // Do not generate declaration for gl_LastFragData
        glsl.set_remapped_variable_state(translation_state.last_frag_data_id, true);
        glsl.set_name(translation_state.last_frag_data_id, "gl_LastFragData");
    }

    if (translation_state.frag_coord_id != spv::NoResult) {
        glsl.set_remapped_variable_state(translation_state.frag_coord_id, true);
        glsl.set_name(translation_state.frag_coord_id, "gl_FragCoord");
    }
    if (features.support_shader_interlock) {
        if (translation_state.is_fragment && is_frag_color_used) {
            glsl.add_header_line("layout(early_fragment_tests) in;\n");
        }
        glsl.require_extension("GL_ARB_fragment_shader_interlock");
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
usse::SpirvCode convert_gxp_to_spirv(const SceGxmProgram &program, const std::string &shader_name, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, bool force_shader_debug, std::function<bool(const std::string &ext, const std::string &dump)> dumper) {
    TranslationState translation_state;
    translation_state.is_fragment = program.is_fragment();
    translation_state.is_maskupdate = maskupdate;
    translation_state.should_gl_spirv_compatible = true;

    return convert_gxp_to_spirv_impl(program, shader_name, features, translation_state, hint_attributes, force_shader_debug, dumper);
}

std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_name, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, bool force_shader_debug, std::function<bool(const std::string &ext, const std::string &dump)> dumper) {
    TranslationState translation_state;
    translation_state.is_fragment = program.is_fragment();
    translation_state.is_maskupdate = maskupdate;
    translation_state.should_gl_spirv_compatible = false;

    std::vector<uint32_t> spirv_binary = convert_gxp_to_spirv_impl(program, shader_name, features, translation_state, hint_attributes, force_shader_debug, dumper);

    const auto source = convert_spirv_to_glsl(shader_name, spirv_binary, features, translation_state, program.is_frag_color_used());

    if (LOG_SHADER_CODE || force_shader_debug) {
        LOG_INFO("Generated GLSL:\n{}", source);
    }

    if (dumper) {
        if (program.is_fragment()) {
            dumper("frag", source);
        } else {
            dumper("vert", source);
        }
    }

    return source;
}

void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath) {
    const fs::path shader_filepath_str{ shader_filepath };
    std::ifstream gxp_stream(shader_filepath, std::ifstream::binary);

    if (!gxp_stream.is_open())
        return;

    const auto gxp_file_size = fs::file_size(shader_filepath_str);
    const auto gxp_program = static_cast<SceGxmProgram *>(calloc(gxp_file_size, 1));

    gxp_stream.read(reinterpret_cast<char *>(gxp_program), gxp_file_size);

    FeatureState features;
    features.direct_fragcolor = false;
    features.support_shader_interlock = true;

    convert_gxp_to_glsl(*gxp_program, shader_filepath_str.filename().string(), features, nullptr, false, true);

    free(gxp_program);
}

} // namespace shader
