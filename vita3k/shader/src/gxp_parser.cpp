#include <gxm/functions.h>
#include <shader/gxp_parser.h>
#include <shader/usse_program_analyzer.h>
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

std::vector<UniformBuffer> shader::get_uniform_buffers(const SceGxmProgram &program) {
    std::vector<UniformBuffer> output_buffers;

    const std::uint64_t *secondary_program_insts = reinterpret_cast<const std::uint64_t *>(
        reinterpret_cast<const std::uint8_t *>(&program.secondary_program_offset) + program.secondary_program_offset);

    const std::uint64_t *primary_program_insts = program.primary_program_start();

    UniformBufferMap buffer_size_map;
    // Analyze the shader to get maximum uniform buffer data
    // Analyze secondary program
    data_analyze(
        static_cast<shader::usse::USSEOffset>((program.secondary_program_offset_end + 4 - program.secondary_program_offset)) / 8,
        [&](USSEOffset off) -> std::uint64_t {
            return secondary_program_insts[off];
        },
        buffer_size_map);

    // Analyze primary program
    data_analyze(
        static_cast<shader::usse::USSEOffset>(program.primary_program_instr_count),
        [&](USSEOffset off) -> std::uint64_t {
            return primary_program_insts[off];
        },
        buffer_size_map);

    // move into load buffers function now?
    const auto buffer_info = reinterpret_cast<const SceGxmUniformBufferInfo *>(
        reinterpret_cast<const std::uint8_t *>(&program.uniform_buffer_offset) + program.uniform_buffer_offset);

    auto buffer_container = gxp::get_container_by_index(program, 19);
    uint32_t buffer_base = buffer_container ? buffer_container->base_sa_offset : 0;

    for (size_t i = 0; i < program.uniform_buffer_count; ++i) {
        //const SceGxmProgramParameter &parameter = gxp_parameters[i];
        const SceGxmUniformBufferInfo *buffer = &buffer_info[i];
        //const SceGxmProgramParameter *parameter = nullptr;

        //for (uint32_t a = 0; a < program.parameter_count; a++) {
        //    if (gxp_parameters[a].resource_index == buffer->reside_buffer) {
        //        parameter = &gxp_parameters[a];
        //        break;
        //    }
        //}

        uint32_t this_buffer_base = buffer_base + buffer->base_offset;

        auto buffer_info = buffer_size_map.find(this_buffer_base);
        if (buffer_info == buffer_size_map.end()) {
            LOG_WARN("Cannot find buffer at index {} and base {}, skipping.", i, this_buffer_base);
            continue;
        }

        uint32_t buffer_size = buffer_info->second.size;

        if (buffer_size == -1) {
            LOG_WARN("Could not analyze buffer size at index {} and base {}, skipping.", i, this_buffer_base);
            continue;
        }

        UniformBuffer item;
        item.base = this_buffer_base;
        item.index = ((buffer->reside_buffer + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER);
        item.size = buffer_size;
        output_buffers.push_back(item);
    }
    return output_buffers;
}

ProgramInput shader::get_program_input(const SceGxmProgram &program) {
    ProgramInput program_input;
    program_input.uniform_buffers = get_uniform_buffers(program);

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

            if (container && parameter.resource_index < container->size_in_f32) {
                offset = container->base_sa_offset + parameter.resource_index;
            }

            if (container && container->size_in_f32 > 0) {
                UniformBlock block;
                block.block_num = (container->container_index + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER;
                block.offset = container->base_sa_offset;
                block.size = container->size_in_f32;
                program_input.uniform_blocks.emplace(container->container_index, block);
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
                item.source = source;
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
        default: {
            LOG_CRITICAL("Unknown parameter type used in shader.");
            break;
        }
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
