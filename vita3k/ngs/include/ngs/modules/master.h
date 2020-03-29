#pragma once

#include <ngs/system.h>

namespace ngs::master {
    struct Module: public ngs::Module {
    public:
        explicit Module();
        void process(const MemState &mem, Voice *voice) override;

        /**
         * \brief Get the expected audio input and audio output types.
         * 
         * They should stay the same for both input and output.
         */
        void get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) override;
    };

    struct VoiceDefinition: public ngs::VoiceDefinition {
        std::size_t get_buffer_parameter_size() const override {
            return 0;
        }

        std::unique_ptr<ngs::Module> new_module() override;
    };
};