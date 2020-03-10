#include <ngs/modules/reverb.h>

#include <util/log.h>

namespace ngs::reverb {
    std::size_t VoiceDefinition::get_buffer_parameter_size() const {
        return sizeof(Parameters);
    }

    std::unique_ptr<ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }

    Module::Module() : ngs::Module(ngs::BussType::BUSS_REVERB) { }

    void Module::process(const MemState &mem, Voice *voice) {
        const std::lock_guard<std::mutex> guard(*voice->voice_lock);

        auto *params = voice->get_parameters<Parameters>(mem);

        assert(voice->inputs.inputs.size() == 1);
        assert(voice->inputs.inputs[0].bufs.size() == 1);
        uint8_t *output_data = voice->inputs.inputs[0].bufs[0].data();

        constexpr uint32_t channels = 2; // fix later
        constexpr uint32_t frequency = 48000; // fix later
        const uint32_t samples = voice->inputs.inputs[0].bufs[0].size() / sizeof(uint16_t) / channels;

        if (!route(mem, voice, nullptr, channels, samples, frequency, AudioDataType::S16)) {
            route(mem, voice, output_data, channels, samples, frequency, AudioDataType::S16);
        }

        // TODO: actually implement reverb, plan is to pass through for now
    }

    void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) { }
}