#pragma once

#include <ngs/system.h>

namespace ngs::reverb {
    struct Parameters {
        ngs::ModuleParameterHeader header;
        float room;
        float room_hf;
        float decay_time;
        float decay_hf_ratio;
        float reflections;
        float reflections_delay;
        float reverb;
        float reverb_delay;
        float diffusion;
        float density;
        float hf_reference;
        int32_t reflection_pattern[2];
        float reflection_scalar;
        float lf_reference;
        float room_lf;
        float dry_mb;
    };

    struct Module : public ngs::Module {
        Module();

        void process(const MemState &mem, Voice *voice) override;
        void get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) override;
    };

    struct VoiceDefinition : public ngs::VoiceDefinition {
        std::unique_ptr<ngs::Module> new_module() override;
        std::size_t get_buffer_parameter_size() const override;
    };
}