#pragma once

#include <ngs/system.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace emu::ngs::atrac9 {
    struct BufferParameter {
        Ptr<void> buffer;
        std::int32_t bytes_count;
        std::int16_t loop_count;
        std::int16_t next_buffer_index;
        std::int16_t samples_discard_start_off;
        std::int16_t samples_discard_end_off;
    };

    static constexpr std::uint32_t MAX_BUFFER_PARAMS = 4;

    struct Parameters {
        emu::ngs::ModuleParameterHeader header;
        BufferParameter buffer_params[MAX_BUFFER_PARAMS];
        float playback_frequency;
        float playback_scalar;
        std::int32_t lead_in_samples;
        std::int32_t limit_number_of_samples_played;
        std::int8_t channels;
        std::int8_t channel_map[2];
        std::int8_t unk5B;
        std::int32_t unk5C;
    };

    struct Module: public emu::ngs::Module {
    private:
        AVCodecContext *context;
        AVCodec *codec;

        bool init();

    public:
        explicit Module();
        void process(const MemState &mem, Voice *voice) override;
    };

    struct VoiceDefinition: public emu::ngs::VoiceDefinition {
        std::size_t get_buffer_parameter_size() const override {
            return sizeof(Parameters);
        }

        std::unique_ptr<emu::ngs::Module> new_module() override;
    };
};