#include <ngs/modules/master.h>
#include <util/log.h>

#include <fstream>

namespace ngs::master {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_MASTER) {
}

void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) {
    *expect_audio_type = AudioDataType::S16;
    *expect_channel_count = 2;
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

    std::copy(data.parent->inputs.inputs[0].data(), data.parent->inputs.inputs[0].data() + data.voice_state_data.size(), data.voice_state_data.data());
}
}; // namespace ngs::master
