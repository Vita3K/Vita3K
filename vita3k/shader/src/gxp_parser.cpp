// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
#include <shader/gxp_parser.h>
#include <shader/usse_program_analyzer.h>
#include <util/align.h>
#include <util/log.h>

namespace shader {

using namespace usse;

GenericType translate_generic_type(const gxp::GenericParameterType &type) {
    switch (type) {
    case gxp::GenericParameterType::Scalar:
        return GenericType::SCALER;
    case gxp::GenericParameterType::Vector:
        return GenericType::VECTOR;
    case gxp::GenericParameterType::Array:
        return GenericType::ARRAY;
    default:
        return GenericType::INVALID;
    }
}

std::tuple<DataType, std::string> get_parameter_type_store_and_name(const SceGxmParameterType &type) {
    switch (type) {
    case SCE_GXM_PARAMETER_TYPE_F32: {
        return std::make_tuple(DataType::F32, "float");
    }

    case SCE_GXM_PARAMETER_TYPE_F16: {
        return std::make_tuple(DataType::F16, "half");
    }

    case SCE_GXM_PARAMETER_TYPE_U16: {
        return std::make_tuple(DataType::UINT16, "ushort");
    }

    case SCE_GXM_PARAMETER_TYPE_S16: {
        return std::make_tuple(DataType::INT16, "ishort");
    }

    case SCE_GXM_PARAMETER_TYPE_U8: {
        return std::make_tuple(DataType::UINT8, "uchar");
    }

    case SCE_GXM_PARAMETER_TYPE_S8: {
        return std::make_tuple(DataType::INT8, "ichar");
    }

    case SCE_GXM_PARAMETER_TYPE_U32: {
        return std::make_tuple(DataType::UINT32, "uint");
    }
    case SCE_GXM_PARAMETER_TYPE_S32: {
        return std::make_tuple(DataType::INT32, "int");
    }

    default:
        return std::make_tuple(DataType::UNK, "unk");
    }
}

ProgramInput get_program_input(const SceGxmProgram &program) {
    ProgramInput program_input;
    std::map<int, UniformBuffer> uniform_buffers;

    // TODO split these to functions (e.g. get_literals, get_paramters)
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);
    auto vertex_varyings_ptr = program.vertex_varyings();

    std::uint32_t investigated_ub = 0;
    bool seems_symbols_stripped = (program.primary_reg_count == 0);

    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = gxp_parameters[i];

        bool is_uniform = false;

        switch (parameter.category) {
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
            is_uniform = true;
            [[fallthrough]];

        // fallthrough
        case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE: {
            std::string var_name = gxp::parameter_name(parameter);

            auto container = gxp::get_container_by_index(program, parameter.container_index);
            std::uint32_t offset = parameter.resource_index;

            if (container && is_uniform) {
                offset = container->base_sa_offset + parameter.resource_index;
            }

            const auto parameter_type = gxp::parameter_type(parameter);
            const auto [store_type, param_type_name] = shader::get_parameter_type_store_and_name(parameter_type);

            // Make the type
            gxp::GenericParameterType param_type = gxp::parameter_generic_type(parameter);
            if (!is_uniform) {
                Input item;
                item.type = store_type;
                item.offset = offset;
                item.component_count = parameter.component_count;
                item.array_size = parameter.array_size;

                item.generic_type = translate_generic_type(param_type);
                item.bank = RegisterBank::PRIMATTR;

                AttributeInputSource source = {};
                source.name = var_name;
                source.index = parameter.resource_index;
                source.semantic = parameter.semantic;
                source.regformat = (vertex_varyings_ptr->untyped_pa_regs & ((uint64_t)1 << parameter.resource_index)) != 0;

                item.source = source;
                program_input.inputs.push_back(item);
            } else {
                uint32_t vector_size = parameter.component_count * get_data_type_size(store_type);
                if (parameter.array_size != 1) {
                    if (is_float_data_type(store_type) && parameter.component_count != 1) {
                        vector_size = align(vector_size, 8);
                    } else {
                        vector_size = align(vector_size, 4);
                    }
                }
                const uint32_t parameter_size = parameter.array_size * vector_size;
                const uint32_t parameter_size_in_f32 = (parameter_size + 3) / 4;

                investigated_ub |= (1 << parameter.container_index);

                if (!uniform_buffers.contains(parameter.container_index)) {
                    const std::uint32_t reg_block_size = container ? container->size_in_f32 : 0;

                    UniformBuffer buffer;
                    buffer.index = parameter.container_index;
                    buffer.reg_block_size = reg_block_size;
                    buffer.rw = false;
                    buffer.reg_start_offset = container ? container->base_sa_offset : offset;
                    buffer.size = parameter.resource_index + parameter_size_in_f32;
                    uniform_buffers.emplace(parameter.container_index, buffer);
                } else {
                    auto &buffer = uniform_buffers.at(parameter.container_index);
                    buffer.size = std::max(parameter.resource_index + parameter_size_in_f32, buffer.size);

                    if (!container) {
                        buffer.reg_start_offset = std::min(buffer.reg_start_offset, static_cast<uint32_t>(offset));
                    }
                }

                // hack (kind of) : Uncharted declares a uniform containing only a vec4 g_mSkinTransforms[96]
                // however it accesses  g_mSkinTransforms[147] (for some default value I guess)
                // so set the size to unbounded in the shader
                if (var_name == "g_mSkinTransforms")
                    uniform_buffers[parameter.container_index].size = SCE_GXM_MAX_UB_IN_FLOAT_UNIT;
            }
            break;
        }

        case SCE_GXM_PARAMETER_CATEGORY_SAMPLER: {
            const std::string name = gxp::parameter_name(parameter);
            Sampler sampler;
            sampler.name = name;
            sampler.index = parameter.resource_index;
            sampler.is_cube = parameter.is_sampler_cube();
            program_input.samplers.push_back(sampler);
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE: {
            assert(parameter.component_count == 0);
            LOG_CRITICAL("auxiliary_surface used in shader");
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER: {
            if (!uniform_buffers.contains(parameter.resource_index)) {
                UniformBuffer buffer;
                buffer.index = parameter.resource_index;
                buffer.reg_block_size = 0;
                buffer.rw = false;
                buffer.reg_start_offset = 0;
                buffer.size = (parameter.array_size + 3) / 4;
                uniform_buffers.emplace(parameter.resource_index, buffer);
            } else {
                auto &buffer = uniform_buffers.at(parameter.resource_index);
                buffer.size = std::max((parameter.array_size + 3) / 4, buffer.size);
            }
            break;
        }
        default: {
            LOG_CRITICAL("Unknown parameter type used in shader.");
            break;
        }
        }
    }

    for (auto &[index, buffer] : uniform_buffers) {
        static constexpr std::uint32_t MAXIMUM_GXP_ARRAY_SIZE = 1024;
        if ((((investigated_ub & (1 << index)) == 0) && seems_symbols_stripped) || (buffer.size == MAXIMUM_GXP_ARRAY_SIZE)) {
            // Symbols stripped shader with uniform buffer not referencing any uniform parameters, or
            // buffer that has the potential of outsizing 1024 (due to limits on the size variable), will
            // get their buffer turns to MAX_UB_IN_VEC4_UNIT here. Their upload amount will be controlled!
            buffer.size = SCE_GXM_MAX_UB_IN_FLOAT_UNIT;
        }

        program_input.uniform_buffers.push_back(buffer);
    }

    auto default_ub_ite = std::find_if(program_input.uniform_buffers.begin(), program_input.uniform_buffers.end(), [](const shader::usse::UniformBuffer &buffer) {
        return buffer.index == 14;
    });

    if ((default_ub_ite == program_input.uniform_buffers.end()) && (program.default_uniform_buffer_count != 0)) {
        // Must there be a one if possible
        UniformBuffer buffer;
        buffer.index = 14;
        buffer.reg_block_size = program.default_uniform_buffer_count;
        buffer.reg_start_offset = 0;
        buffer.rw = false;
        buffer.size = program.default_uniform_buffer_count;

        program_input.uniform_buffers.push_back(buffer);
        uniform_buffers.emplace(14, buffer);
    }

    const auto buffer_infoes = reinterpret_cast<const SceGxmUniformBufferInfo *>(
        reinterpret_cast<const std::uint8_t *>(&program.uniform_buffer_offset) + program.uniform_buffer_offset);

    const auto buffer_container = gxp::get_container_by_index(program, 19);
    const uint32_t base_offset = buffer_container ? buffer_container->base_sa_offset : 0;

    for (size_t i = 0; i < program.uniform_buffer_count; ++i) {
        const SceGxmUniformBufferInfo *buffer_info = &buffer_infoes[i];
        const uint32_t offset = base_offset + buffer_info->ldst_base_offset;

        // handle literal and texture buffer here
        if (buffer_info->reside_buffer >= SCE_GXM_REAL_MAX_UNIFORM_BUFFER) {
            UniformBuffer buffer{
                .index = buffer_info->reside_buffer,
                .reg_start_offset = 0,
                .reg_block_size = 0,
                .rw = false
            };

            switch (buffer_info->reside_buffer) {
            case SCE_GXM_TEXTURE_BUFFER:
                LOG_INFO("Shader is using a texture buffer");
                buffer.size = program.texture_buffer_count;
                break;
            case SCE_GXM_LITERAL_BUFFER:
                LOG_INFO("Shader is using a literal buffer");
                buffer.size = program.literal_buffer_count;
                break;
            case SCE_GXM_THREAD_BUFFER:
                LOG_INFO("Shader is using a thread buffer");
                buffer.size = program.thread_buffer_count;
                break;
            default:
                LOG_ERROR("Shader is using an unkown buffer type {}", buffer_info->reside_buffer);
                break;
            }

            if (buffer.size == 0)
                continue;

            program_input.uniform_buffers.push_back(buffer);
            uniform_buffers.emplace(buffer_info->reside_buffer, buffer);
        }

        auto buffer = uniform_buffers.find(buffer_info->reside_buffer);

        // buffer = null seems to happen when there's a leftover uniform buffer (uniform buffer that's not used in shader code)
        // This case needs more investigation
        if (buffer != uniform_buffers.end()) {
            Input item{
                .type = DataType::UINT32,
                .offset = offset,
                .component_count = 1,
                .array_size = 1,
                .source = UniformBufferInputSource{
                    .base = static_cast<uint32_t>(buffer_info->ldst_base_value),
                    .index = buffer->second.index }
            };

            program_input.inputs.push_back(item);
        }
    }

    const SceGxmProgramParameterContainer *container = gxp::get_container_by_index(program, 19);

    if (container) {
        // Create dependent sampler
        const SceGxmDependentSampler *dependent_samplers = reinterpret_cast<const SceGxmDependentSampler *>(reinterpret_cast<const std::uint8_t *>(&program.dependent_sampler_offset)
            + program.dependent_sampler_offset);

        for (std::uint32_t i = 0; i < program.dependent_sampler_count; i++) {
            const std::uint32_t rsc_index = dependent_samplers[i].resource_index_layout_offset / 4;

            const auto sampler = std::find_if(program_input.samplers.begin(), program_input.samplers.end(), [=](auto x) { return x.index == rsc_index; });

            Input item;
            item.type = DataType::UINT32;
            item.offset = container->base_sa_offset + dependent_samplers[i].sa_offset;
            item.component_count = 1;
            item.array_size = 1;
            DependentSamplerInputSource source;

            if (sampler == program_input.samplers.end()) {
                source.name = fmt::format("anonymousSampler{}", rsc_index);

                Sampler sampler_info;
                sampler_info.name = source.name;
                sampler_info.index = rsc_index;
                sampler_info.is_cube = false; // I don't know :(

                program_input.samplers.push_back(std::move(sampler_info));
            } else {
                source.name = sampler->name;
            }

            source.index = rsc_index;
            source.layout_position = dependent_samplers[i].resource_index_layout_offset % 4;
            item.source = source;

            program_input.inputs.push_back(item);
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
    if (container) {
        for (std::uint32_t i = 0; i < program.literals_count * 2; i += 2) {
            auto literal_offset = container->base_sa_offset + literals[i];
            auto literal_data = *reinterpret_cast<const float *>(&literals[i + 1]);

            LiteralInputSource source;
            source.data = literal_data;

            Input item;
            item.component_count = 1;
            item.type = DataType::F32;
            item.bank = RegisterBank::SECATTR;
            item.offset = literal_offset;
            item.array_size = 1;
            item.generic_type = GenericType::SCALER;
            item.source = source;

            program_input.inputs.push_back(item);
        }
    }

    // Parse special semantics
    if (program.is_vertex()) {
        Input item;
        item.component_count = 1;
        item.array_size = 1;
        item.bank = RegisterBank::PRIMATTR;
        item.generic_type = GenericType::SCALER;
        item.type = DataType::UINT32;

        if (program.program_flags & SCE_GXM_PROGRAM_FLAG_INDEX_USED) {
            AttributeInputSource source = {};
            source.semantic = SCE_GXM_PARAMETER_SEMANTIC_INDEX;
            source.name = "gl_VertexID";
            source.regformat = false;

            item.offset = vertex_varyings_ptr->semantic_index_offset;
            item.source = source;

            program_input.inputs.push_back(item);
        }

        if (program.program_flags & SCE_GXM_PROGRAM_FLAG_INSTANCE_USED) {
            AttributeInputSource source = {};
            source.semantic = SCE_GXM_PARAMETER_SEMANTIC_INSTANCE;
            source.name = "gl_InstanceID";
            source.regformat = false;

            item.offset = vertex_varyings_ptr->semantic_instance_offset;
            item.source = source;

            program_input.inputs.push_back(item);
        }
    }

    return program_input;
}

DataType get_texture_component_type(SceGxmTextureFormat format) {
    SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    // TODO: won't be surprised if some of them are wrong
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        return DataType::UINT8;

    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return DataType::INT8;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
        return DataType::UINT16;

    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
        return DataType::INT16;

    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return DataType::F16;

    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    // should these formats be left as F32?
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return DataType::F32;

    // TODO are these stored as U8, F16 or F32?
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return DataType::UINT8;
    }
    return DataType::UNK;
}

uint8_t get_texture_component_count(SceGxmTextureFormat format) {
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const uint32_t swizzle = format & SCE_GXM_TEXTURE_SWIZZLE_MASK;
    switch (base_format) {
    // 1 Component.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        return (swizzle == SCE_GXM_TEXTURE_SWIZZLE1_R) ? 1U : 4U;

    // 2 components
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
        return (swizzle == SCE_GXM_TEXTURE_SWIZZLE2_GR) ? 2U : 4U;

    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return 1U;

    // 3 and 4 components, always 4
    default:
        return 4U;
    }
}

} // namespace shader