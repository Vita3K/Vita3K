#include <ngs/modules/master.h>
#include <util/log.h>

#include <fstream>

namespace ngs::master {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_MASTER) {
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    // Merge all voices. This buss manually outputs 2 channels
    if (data.voice_state_data.empty()) {
        data.voice_state_data.resize(data.parent->rack->system->granularity * sizeof(std::uint16_t) * 2);
    }

    std::fill(data.voice_state_data.begin(), data.voice_state_data.end(), 0);

    if (data.parent->inputs.inputs.empty()) {
        return;
    }

    std::int16_t *dest_data = reinterpret_cast<std::int16_t *>(data.voice_state_data.data());
    float *source_data = reinterpret_cast<float *>(data.parent->inputs.inputs[0].data());

    // Convert FLTP to S16
    for (std::uint32_t i = 0; i < data.parent->rack->system->granularity * 2; i++) {
        dest_data[i] = static_cast<std::int16_t>(std::clamp(source_data[i] * 32768.0f, -32768.0f, 32767.0f));
    }
}
}; // namespace ngs::master
