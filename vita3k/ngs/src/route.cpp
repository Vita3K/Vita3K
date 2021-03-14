#include <ngs/system.h>

#include <codec/state.h>
#include <util/log.h>

namespace ngs {
bool deliver_data(const MemState &mem, Voice *source, const std::uint8_t output_port,
    const VoiceProduct &data_to_deliver) {
    if (!data_to_deliver.data) {
        return false;
    }

    for (std::size_t i = 0; i < source->patches[output_port].size(); i++) {
        Patch *patch = source->patches[output_port][i].get(mem);

        if (!patch || patch->output_sub_index == -1) {
            continue;
        }

        if (data_to_deliver.data != nullptr) {
            const std::lock_guard<std::mutex> guard(*patch->dest->voice_lock);
            patch->dest->inputs.receive(patch, data_to_deliver);
        }
    }

    return true;
}
} // namespace ngs