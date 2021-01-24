#include <ngs/modules/master.h>
#include <util/log.h>

#include <fstream>

namespace ngs::master {
std::unique_ptr<ngs::Module> VoiceDefinition::new_module() {
    return std::make_unique<Module>();
}

Module::Module()
    : ngs::Module(ngs::BussType::BUSS_MASTER) {
}

void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) {
    *expect_audio_type = AudioDataType::S16;
    *expect_channel_count = 2;
}

void Module::process(const MemState &mem, Voice *voice) {
    // Lock voice to avoid resource modification from other thread
    const std::lock_guard<std::mutex> guard(*voice->voice_lock);

    // Merge all voices. This buss manually outputs 2 channels
    if (voice->voice_state_data.empty()) {
        voice->voice_state_data.resize(voice->rack->system->granularity * sizeof(std::uint16_t) * 2);
    }

    std::fill(voice->voice_state_data.begin(), voice->voice_state_data.end(), 0);

    if (voice->inputs.inputs.empty()) {
        return;
    }

    std::copy(voice->inputs.inputs[0].data(), voice->inputs.inputs[0].data() + voice->voice_state_data.size(), voice->voice_state_data.data());
}
}; // namespace ngs::master