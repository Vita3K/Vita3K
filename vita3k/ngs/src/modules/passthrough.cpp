#include <ngs/modules/passthrough.h>

#include <util/log.h>

namespace ngs::passthrough {
    // random number of bytes to make sure nothing bad happens
    constexpr size_t default_parameter_size = 256;

    std::size_t VoiceDefinition::get_buffer_parameter_size() const {
        return default_parameter_size;
    }

    std::unique_ptr<ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }

    Module::Module() : ngs::Module(ngs::BussType::BUSS_REVERB) { }

    void Module::process(const MemState &mem, Voice *voice) {
        const std::lock_guard<std::mutex> guard(*voice->voice_lock);

        if (voice->inputs.inputs.size() < 1) {
            return;
        }

        assert(voice->inputs.inputs.size() == 1);
        uint8_t *output_data = voice->inputs.inputs[0].data();

        deliver_data(mem, voice, 0, output_data);
    }

    void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) { }
}