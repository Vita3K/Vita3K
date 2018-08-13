#include <shadergen/functions.h>

#include <SPIRV/SpvBuilder.h>
#include <SPIRV/disassemble.h>
#include <gxm/functions.h>
#include <gxm/types.h>
#include <shadergen/profile.h>
#include <spirv_glsl.hpp>
#include <util/log.h>

#include <sstream>
#include <utility>
#include <vector>

static constexpr bool LOG_SHADER_DEBUG = true;

using SpirvCode = std::vector<uint32_t>;

namespace shadergen {
namespace spirv {

// Prototypes

spv::Id get_type_default(spv::Builder &spv_builder);

// Keeps track of current struct declaration
// TODO: Handle struct arrays and multiple struct instances.
//       The current (and the former) approach is quite naive, in that it assumes:
//           1) there is only one struct instance per declared struct
//           2) there are no struct array instances
struct param_struct_t {
    std::string name;
    spv::StorageClass storage_class = spv::StorageClassMax;
    std::vector<spv::Id> field_ids;
    std::vector<std::string> field_names; // count must be equal to `field_ids`

    bool empty() const { return name.empty(); }
    void clear() { *this = {}; }
};

void create_array_if_needed(spv::Builder &spv_builder, spv::Id &param_id, const SceGxmProgramParameter &parameter, const uint32_t explicit_array_size = 0) {
    const auto array_size = explicit_array_size == 0 ? parameter.array_size : explicit_array_size;
    if (array_size > 1) {
        const auto array_size_id = spv_builder.makeUintConstant(array_size);
        param_id = spv_builder.makeArrayType(param_id, array_size_id, 0);
    }
}

spv::Id get_type_basic(spv::Builder &spv_builder, const SceGxmProgramParameter &parameter) {
    SceGxmParameterType type = gxp::parameter_type(parameter);

    // With half floats enabled, generated GLSL requires the "GL_AMD_gpu_shader_half_float" extension that macOS doesn't support
    // TODO: Detect support at runtime
    constexpr bool no_half_float =
#ifdef __APPLE__
        true;
#else
        false;
#endif

    switch (type) {
    // clang-format off
    case SCE_GXM_PARAMETER_TYPE_F16:return spv_builder.makeFloatType(no_half_float ? 32 : 16);
    case SCE_GXM_PARAMETER_TYPE_F32: return spv_builder.makeFloatType(32);
    case SCE_GXM_PARAMETER_TYPE_U8: return spv_builder.makeUintType(8);
    case SCE_GXM_PARAMETER_TYPE_U16: return spv_builder.makeUintType(16);
    case SCE_GXM_PARAMETER_TYPE_U32: return spv_builder.makeUintType(32);
    case SCE_GXM_PARAMETER_TYPE_S8: return spv_builder.makeIntType(8);
    case SCE_GXM_PARAMETER_TYPE_S16: return spv_builder.makeIntType(16);
    case SCE_GXM_PARAMETER_TYPE_S32: return spv_builder.makeIntType(32);
    // clang-format on
    default: {
        LOG_ERROR("Unsupported parameter type {} used in shader.", log_hex(type));
        return get_type_default(spv_builder);
    }
    }
}

spv::Id get_type_default(spv::Builder &spv_builder) {
    return spv_builder.makeFloatType(32);
}

spv::Id get_type_scalar(spv::Builder &spv_builder, const SceGxmProgramParameter &parameter) {
    spv::Id param_id = get_type_basic(spv_builder, parameter);
    create_array_if_needed(spv_builder, param_id, parameter);
    return param_id;
}

spv::Id get_type_vector(spv::Builder &spv_builder, const SceGxmProgramParameter &parameter) {
    spv::Id param_id = get_type_basic(spv_builder, parameter);
    param_id = spv_builder.makeVectorType(param_id, parameter.component_count);
    create_array_if_needed(spv_builder, param_id, parameter);
    return param_id;
}

spv::Id get_type_matrix(spv::Builder &spv_builder, const SceGxmProgramParameter &parameter) {
    spv::Id param_id = get_type_basic(spv_builder, parameter);

    // There's no information on whether the parameter was a matrix originally (such type info is lost)
    // so attempt to make a NxN matrix, or a NxN matrix array of size M if possible (else fall back to vector array)
    // where N = parameter.component_count
    //       M = matrix_array_size
    const auto total_type_size = parameter.component_count * parameter.array_size;
    const auto matrix_size = parameter.component_count * parameter.component_count;
    const auto matrix_array_size = total_type_size / matrix_size;
    const auto matrix_array_size_leftover = total_type_size % matrix_size;

    if (matrix_array_size_leftover == 0) {
        param_id = spv_builder.makeMatrixType(param_id, parameter.component_count, parameter.component_count);
        create_array_if_needed(spv_builder, param_id, parameter, matrix_array_size);
    } else {
        // fallback to vector array
        param_id = get_type_vector(spv_builder, parameter);
    }

    return param_id;
}

spv::Id get_param_type(spv::Builder &spv_builder, const SceGxmProgramParameter &parameter) {
    std::string name = gxp::parameter_name_raw(parameter);
    gxp::GenericParameterType param_type = gxp::parameter_generic_type(parameter);

    spv::Id param_id;

    switch (param_type) {
    case gxp::GenericParameterType::Scalar:
        param_id = get_type_scalar(spv_builder, parameter);
        break;
    case gxp::GenericParameterType::Vector:
        param_id = get_type_vector(spv_builder, parameter);
        break;
    case gxp::GenericParameterType::Matrix:
        param_id = get_type_matrix(spv_builder, parameter);
        break;
    default:
        param_id = get_type_default(spv_builder);
    }

    return param_id;
}

spv::Id create_param_sampler(spv::Builder &spv_builder, const SceGxmProgramParameter &parameter) {
    spv::Id sampled_type = spv_builder.makeFloatType(32);
    spv::Id image_type = spv_builder.makeImageType(sampled_type, spv::Dim2D, false, false, false, 1, spv::ImageFormatUnknown);
    spv::Id sampled_image_type = spv_builder.makeSampledImageType(image_type);
    std::string name = gxp::parameter_name_raw(parameter);

    return spv_builder.createVariable(spv::StorageClassUniformConstant, sampled_image_type, name.c_str());
}

spv::Id create_struct(spv::Builder &spv_builder, param_struct_t &param_struct) {
    assert(param_struct.field_ids.size() == param_struct.field_names.size());

    const spv::Id struct_type_id = spv_builder.makeStructType(param_struct.field_ids, param_struct.name.c_str());

    for (auto field_index = 0; field_index < param_struct.field_ids.size(); ++field_index) {
        const auto field_name = param_struct.field_names[field_index];

        spv_builder.addMemberName(struct_type_id, field_index, field_name.c_str());
    }

    const spv::Id struct_var_id = spv_builder.createVariable(param_struct.storage_class, struct_type_id, param_struct.name.c_str());

    param_struct.clear();
    return struct_var_id;
}

void create_parameters(spv::Builder &spv_builder, const SceGxmProgram &program, emu::SceGxmProgramType program_type) {
    const SceGxmProgramParameter *const parameters = gxp::program_parameters(program);
    param_struct_t param_struct = {};

    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];

        gxp::log_parameter(parameter);

        spv::StorageClass param_storage_class = spv::StorageClassInput;

        switch (parameter.category) {
        case SCE_GXM_PARAMETER_CATEGORY_UNIFORM:
            param_storage_class = spv::StorageClassUniformConstant;
            // fallthrough
        case SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE: {
            const std::string struct_name = gxp::parameter_struct_name(parameter);
            const bool is_struct_field = !struct_name.empty();
            const bool struct_decl_ended = !is_struct_field && !param_struct.empty() || (is_struct_field && !param_struct.empty() && param_struct.name != struct_name);

            if (struct_decl_ended)
                create_struct(spv_builder, param_struct);

            spv::Id param = get_param_type(spv_builder, parameter);

            if (is_struct_field) {
                const auto param_field_name = gxp::parameter_name(parameter);

                param_struct.name = struct_name;
                param_struct.field_ids.push_back(param);
                param_struct.field_names.push_back(param_field_name);
                param_struct.storage_class = param_storage_class;
            } else {
                const auto param_name_raw = gxp::parameter_name_raw(parameter);

                spv_builder.createVariable(param_storage_class, param, param_name_raw.c_str());
            }
            break;
        }
        case SCE_GXM_PARAMETER_CATEGORY_SAMPLER: {
            create_param_sampler(spv_builder, parameter);
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

    // Declarations ended with a struct, so it didn't get handled and we need to do it here
    if (!param_struct.empty())
        create_struct(spv_builder, param_struct);
}

SpirvCode generate_shader(const SceGxmProgram &program, emu::SceGxmProgramType program_type) {
    SpirvCode spirv;

    spv::SpvBuildLogger spv_logger;
    spv::Builder spv_builder(SPV_VERSION, 0x1337 << 12, &spv_logger);

    //spv_builder.setSourceFile();
    //spv_builder.setSourceText();
    spv_builder.addSourceExtension("gxp");
    spv_builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelSimple);

    // capabilities
    spv_builder.addCapability(spv::Capability::CapabilityShader);

    spirv::create_parameters(spv_builder, program);

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

    // entry point
    spv::Function *spv_func_main = spv_builder.makeEntryPoint(entry_point_name.c_str());
    // TODO: vertex/fragment shader body
    spv_builder.leaveFunction();

    // VS Execution Modes
    spv_builder.addExecutionMode(spv_func_main, spv::ExecutionModeOriginLowerLeft);

    // Add entry point to Builder
    spv_builder.addEntryPoint(execution_model, spv_func_main, entry_point_name.c_str());

    auto spirv_log = spv_logger.getAllMessages();
    if (!spirv_log.empty())
        LOG_ERROR("SPIR-V Error:\n{}", spirv_log);

    if (LOG_SHADER_DEBUG) {
        std::stringstream spirv_disasm;
        spv_builder.dump(spirv);
        spv::Disassemble(spirv_disasm, spirv);
        LOG_DEBUG("SPIR-V Disassembly:\n{}", spirv_disasm.str());
    }

    return spirv;
}

std::string to_glsl(SpirvCode spirv_binary) {
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

std::string generate_glsl(const SceGxmProgram &program, emu::SceGxmProgramType program_type) {
    std::vector<uint32_t> spirv_binary = spirv::generate_shader(program, program_type);

    const auto source = spirv::to_glsl(spirv_binary);

    if (LOG_SHADER_DEBUG)
        LOG_DEBUG("Generated GLSL:\n{}", source);

    return source;
}

std::string generate_vertex_glsl(const SceGxmProgram &program) {
    return generate_glsl(program, emu::SceGxmProgramType::Vertex);
}

std::string generate_fragment_glsl(const SceGxmProgram &program) {
    return generate_glsl(program, emu::SceGxmProgramType::Fragment);
}

} // namespace spirv
} // namespace shadergen
