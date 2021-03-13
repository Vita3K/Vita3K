#include <ngs/system.h>

#include <codec/state.h>
#include <util/log.h>

namespace ngs {
bool deliver_data(const MemState &mem, Voice *source, const std::uint8_t output_port,
    const std::uint8_t *output_data) {
    if (!output_data) {
        return false;
    }

    for (std::size_t i = 0; i < source->patches[output_port].size(); i++) {
        Patch *patch = source->patches[output_port][i].get(mem);

        if (!patch || patch->output_sub_index == -1) {
            continue;
        }

        if (output_data != nullptr) {
            patch->dest->inputs.receive(patch, &output_data);
        }
    }

    return true;
}
} // namespace ngs