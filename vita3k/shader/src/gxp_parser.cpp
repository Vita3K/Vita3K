#include <gxm/functions.h>
#include <shader/gxp_parser.h>
#include <shader/usse_program_analyzer.h>
#include <util/align.h>
#include <util/log.h>

using namespace shader::usse;

GenericType shader::translate_generic_type(const gxp::GenericParameterType &type) {
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

std::tuple<DataType, std::string> shader::get_parameter_type_store_and_name(const SceGxmParameterType &type) {
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
        break;
    }
}

ProgramInput shader::get_program_input(const SceGxmProgram &program) {
    ProgramInput program_input;
    std::map<int, UniformBuffer> uniform_buffers;

    // TODO split these to functions (e.g. get_literals, get_paramters)
    const SceGxmProgramParameter *const gxp_parameters = gxp::program_parameters(program);

    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = gxp_parameters[i];

        bool is_uniform = false;
        uint16_t curi = parameter.category;

        switch (parameter.category) {
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
            is_uniform = true;
            [[fallthrough]];

        // fallthrough
        case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE: {
            std::string var_name = gxp::parameter_name(parameter);

            auto container = gxp::get_container_by_index(program, parameter.container_index);
            std::uint32_t offset = parameter.resource_index;

            if (container) {
                offset = container->base_sa_offset + parameter.resource_index;
            }

            const auto parameter_type = gxp::parameter_type(parameter);
            const auto [store_type, param_type_name] = shader::get_parameter_type_store_and_name(parameter_type);

            // Make the type
            std::string param_log = fmt::format("[{} + {}] {}a{} = ({}{}) {}",
                gxp::get_container_name(parameter.container_index), parameter.resource_index,
                is_uniform ? "s" : "p", offset, param_type_name, parameter.component_count, var_name);

            if (parameter.array_size > 1) {
                param_log += fmt::format("[{}]", parameter.array_size);
            }

            LOG_DEBUG(param_log);

            Input item;
            item.type = store_type;
            item.offset = offset;
            item.component_count = parameter.component_count;
            item.array_size = parameter.array_size;
            gxp::GenericParameterType param_type = gxp::parameter_generic_type(parameter);
            item.generic_type = translate_generic_type(param_type);
            item.bank = is_uniform ? RegisterBank::SECATTR : RegisterBank::PRIMATTR;
            if (!is_uniform) {
                AttributeInputSoucre source;
                source.name = var_name;
                source.index = parameter.resource_index;
                item.source = source;
            } else {
                UniformInputSource source;
                source.name = var_name;
                source.index = parameter.resource_index;
                const uint32_t reg_block_size = container ? container->size_in_f32 : 0;
                source.in_mem = parameter.resource_index > reg_block_size;
                item.source = source;
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
                if (uniform_buffers.find(parameter.container_index) == uniform_buffers.end()) {
                    UniformBuffer buffer;
                    buffer.index = (parameter.container_index + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER;
                    buffer.reg_block_size = reg_block_size;
                    buffer.rw = false;
                    buffer.reg_start_offset = parameter.resource_index;
                    buffer.size = parameter.resource_index + parameter_size_in_f32;
                    uniform_buffers.emplace(parameter.container_index, buffer);
                } else {
                    auto &buffer = uniform_buffers.at(parameter.container_index);
                    buffer.size = std::max(parameter.resource_index + parameter_size_in_f32, buffer.size);
                    buffer.reg_start_offset = std::min(buffer.reg_start_offset, static_cast<uint32_t>(parameter.resource_index));
                }
            }

            program_input.inputs.push_back(item);
            break;
        }

        case SCE_GXM_PARAMETER_CATEGORY_SAMPLER: {
            const std::string name = gxp::parameter_name(parameter);
            Sampler sampler;
            sampler.name = name;
            sampler.dependent = false;
            sampler.index = parameter.resource_index;
            program_input.samplers.push_back(sampler);
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE: {
            assert(parameter.component_count == 0);
            LOG_CRITICAL("auxiliary_surface used in shader");
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER: {
            if (uniform_buffers.find(parameter.resource_index) == uniform_buffers.end()) {
                UniformBuffer buffer;
                buffer.index = (parameter.resource_index + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER;
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

    for (auto &[_, buffer] : uniform_buffers) {
        program_input.uniform_buffers.push_back(buffer);
    }

    const auto buffer_infoes = reinterpret_cast<const SceGxmUniformBufferInfo *>(
        reinterpret_cast<const std::uint8_t *>(&program.uniform_buffer_offset) + program.uniform_buffer_offset);

    const auto buffer_container = gxp::get_container_by_index(program, 19);
    const uint32_t base_offset = buffer_container ? buffer_container->base_sa_offset : 0;

    for (size_t i = 0; i < program.uniform_buffer_count; ++i) {
        const SceGxmUniformBufferInfo *buffer_info = &buffer_infoes[i];

        const uint32_t offset = base_offset + buffer_info->base_offset;

        const auto buffer = uniform_buffers.at(buffer_info->reside_buffer);

        Input item;
        item.type = DataType::UINT32;
        item.offset = offset;
        item.component_count = 1;
        item.array_size = 1;
        UniformBufferInputSource source;
        source.base = buffer.reg_block_size;
        source.index = buffer.index;
        item.source = source;

        program_input.inputs.push_back(item);
    }

    const SceGxmProgramParameterContainer *container = gxp::get_container_by_index(program, 19);

    if (container) {
        // Create dependent sampler
        const SceGxmDependentSampler *dependent_samplers = reinterpret_cast<const SceGxmDependentSampler *>(reinterpret_cast<const std::uint8_t *>(&program.dependent_sampler_offset)
            + program.dependent_sampler_offset);

        for (std::uint32_t i = 0; i < program.dependent_sampler_count; i++) {
            const std::uint32_t rsc_index = dependent_samplers[i].resource_index_layout_offset / 4;

            const auto sampler = std::find_if(program_input.samplers.begin(), program_input.samplers.end(), [=](auto x) { return x.index == rsc_index; });
            sampler->dependent = true;
            sampler->offset = container->base_sa_offset + dependent_samplers[i].sa_offset;
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
    } else {
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

            LOG_TRACE("[LITERAL + {}] sa{} = {} (0x{:X})", literals[i], literal_offset, literal_data, literals[i + 1]);
        }
    }

    return program_input;
}
