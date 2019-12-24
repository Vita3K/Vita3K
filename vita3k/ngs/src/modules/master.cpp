#include <ngs/modules/master.h>
#include <util/log.h>

#include <fstream>

namespace emu::ngs::master {
    std::unique_ptr<emu::ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }
    
    Module::Module() 
        : emu::ngs::Module(emu::ngs::BUSS_MASTER) {
    }

    void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) {
        *expect_audio_type = AudioDataType::S16;
        *expect_channel_count = 2;
    }
    
    void Module::process(const MemState &mem, Voice *voice) {
        // Merge all voices. This buss manually outputs 2 channels
        if (voice->voice_state_data.empty()) {
            voice->voice_state_data.resize(voice->rack->system->granularity * sizeof(std::uint16_t) * 2);
        }

        std::fill(voice->voice_state_data.begin(), voice->voice_state_data.end(), 0);

        if (voice->inputs.inputs[0].bufs.size() == 0) {
            return;
        }

        const std::uint32_t gran_byte_count = voice->rack->system->granularity * sizeof(std::int16_t) * 2;

        for (std::int32_t i = 0; i < voice->inputs.inputs[0].bufs.size(); i++) {
            std::int16_t *samples_to_mix = reinterpret_cast<std::int16_t*>(&voice->inputs.inputs[0].bufs[i][0]);
            std::int16_t *dest_samples = reinterpret_cast<std::int16_t*>(&voice->voice_state_data[0]);

            for (std::int32_t k = 0; k < voice->rack->system->granularity * 2; k++) {
                std::int32_t current_sample_mixed = dest_samples[k];
                current_sample_mixed += samples_to_mix[k];

                // Clip the sample!
                if (current_sample_mixed > 32767)
                    current_sample_mixed = 32767;

                if (current_sample_mixed < -32768)
                    current_sample_mixed = -32768;
                    
                dest_samples[k] = current_sample_mixed;
            }
        }
    }
};