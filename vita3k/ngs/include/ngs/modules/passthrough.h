#pragma once

#include <ngs/system.h>

namespace ngs::passthrough {
struct Module : public ngs::Module {
    Module();

    void process(const MemState &mem, Voice *voice) override;
    void get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) override;
};

struct VoiceDefinition : public ngs::VoiceDefinition {
    std::unique_ptr<ngs::Module> new_module() override;
    std::size_t get_buffer_parameter_size() const override;
};
} // namespace ngs::passthrough