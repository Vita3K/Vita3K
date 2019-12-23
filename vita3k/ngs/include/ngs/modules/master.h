#pragma once

#include <ngs/system.h>

namespace emu::ngs::master {
    struct Module: public emu::ngs::Module {
    public:
        explicit Module();
        void process(const MemState &mem, Voice *voice) override;
        void get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) override;
    };

    struct VoiceDefinition: public emu::ngs::VoiceDefinition {
        std::size_t get_buffer_parameter_size() const override {
            return 0;
        }

        std::unique_ptr<emu::ngs::Module> new_module() override;
    };
};