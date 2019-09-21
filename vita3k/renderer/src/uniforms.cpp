#include <renderer/functions.h>
#include <renderer/types.h>

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer {
void set_uniforms(State &state, Context *ctx, const SceGxmProgram &program, const UniformBuffers &buffers, const MemState &mem) {
    const SceGxmProgramParameter *const parameters = gxp::program_parameters(program);
    for (size_t i = 0; i < program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM)
            continue;

        const Ptr<const void> uniform_buffer = buffers[parameter.container_index];
        if (!uniform_buffer) {
            auto name = gxp::parameter_name(parameter);

            LOG_WARN("Uniform buffer {} not set for parameter {}.", parameter.container_index, name);
            continue;
        }

        const uint8_t *const base = static_cast<const uint8_t *>(uniform_buffer.get(mem));

        // TODO: Not hardcoded by 4 in backup case.
        const std::size_t uniform_raw_size = parameter.component_count * parameter.array_size * sizeof(std::uint32_t);
        const std::size_t offset = parameter.resource_index * sizeof(std::uint32_t);

        // Free later by the queue
        std::uint8_t *data = new std::uint8_t[uniform_raw_size];

        // Copy data to temporary buffer
        std::copy(base + offset, base + offset + uniform_raw_size, data);

        // Submit. First argument specify the shader type.
        renderer::set_uniform(state, ctx, !program.is_fragment(), &parameter, data);
    }
}

void set_uniform_buffers(State &state, Context *ctx, const SceGxmProgram &program, const UniformBuffers &buffers, const UniformBufferSizes &sizes, const MemState &mem) {
    for (std::size_t i = 0; i < buffers.size(); i++) {
        if (!buffers[i] || sizes.at(i) == 0) {
            continue;
        }

        // Shift all buffer by 1.
        // The ideal is: default uniform buffer block has the binding of 14. Not really ideal, so i move it to 0, and buffer 0 to 1, etc..
        renderer::set_uniform_buffer(state, ctx, !program.is_fragment(), static_cast<int>((i + 1) % SCE_GXM_REAL_MAX_UNIFORM_BUFFER), sizes.at(i) * 16, buffers[i].get(mem));
    }
}
} // namespace renderer