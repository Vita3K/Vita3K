#include <gxm/functions.h>

#include <gxm/types.h>

namespace gxm {
    const SceGxmProgramParameter *program_parameters(const SceGxmProgram &program) {
        return reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program.parameters_offset) + program.parameters_offset);
    }

    std::string parameter_name_raw(const SceGxmProgramParameter &parameter) {
        const uint8_t *const bytes = reinterpret_cast<const uint8_t *>(&parameter);
        return reinterpret_cast<const char *>(bytes + parameter.name_offset);
    }

    std::string parameter_name(const SceGxmProgramParameter &parameter) {
        auto full_name = parameter_name_raw(parameter);
        auto struct_idx = full_name.find('.');
        bool is_struct_field = struct_idx != std::string::npos;

        if (is_struct_field)
            return full_name.substr(struct_idx + 1, full_name.length());
        else
            return full_name;
    }
}
