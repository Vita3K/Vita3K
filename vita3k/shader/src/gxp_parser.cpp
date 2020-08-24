#include <shader/gxp_parser.h>
#include <gxm/functions.h>
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

    std::array<const SceGxmProgramParameterContainer *, SCE_GXM_REAL_MAX_UNIFORM_BUFFER> all_buffers_in_register;

    // Empty them out
    for (auto &buffer : all_buffers_in_register) {
        buffer = nullptr;
    }

    // move into load buffers function now?
    const auto *buffer_info = reinterpret_cast<const SceGxmUniformBufferInfo *>(
        reinterpret_cast<const std::uint8_t *>(&program.uniform_buffer_offset) + program.uniform_buffer_offset);

    auto *buffer_container = gxp::get_container_by_index(program, 19);
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
        item.rw = ((buffer->reside_buffer + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER) == 0; // TODO: confirm this
        item.size = buffer_size;
        output_buffers.push_back(item);
    }
    return output_buffers;
}
