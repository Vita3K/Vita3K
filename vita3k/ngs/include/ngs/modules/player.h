#pragma once

#include <ngs/system.h>

namespace ngs::player {
enum {
    SCE_NGS_PLAYER_CALLBACK_REASON_DONE_ALL = 0,
    SCE_NGS_PLAYER_CALLBACK_REASON_DONE_ONE_BUFFER = 1,
    SCE_NGS_PLAYER_CALLBACK_REASON_START_LOOP = 2,
    SCE_NGS_PLAYER_CALLBACK_REASON_DECODE_ERROR = 3
};

struct BufferParameter {
    Ptr<void> buffer;
    std::int32_t bytes_count;
    std::int16_t loop_count;
    std::int16_t next_buffer_index;
};

static constexpr std::uint32_t MAX_BUFFER_PARAMS = 4;

enum ParameterAudioType : std::uint8_t {
    ParameterAudioTypePCM = 0,
    ParameterAudioTypeADPCM = 1
};

struct State {
    std::int32_t current_byte_position_in_buffer;
    std::int32_t current_buffer;
    std::int32_t samples_generated_since_key_on;
    std::int32_t bytes_consumed_since_key_on;
    std::int32_t total_bytes_consumed;
    std::int8_t current_loop_count;
};

struct Parameters {
    ngs::ModuleParameterHeader header;
    BufferParameter buffer_params[MAX_BUFFER_PARAMS];
    float playback_frequency;
    float playback_scalar;
    std::int32_t lead_in_samples;
    std::int32_t limit_number_of_samples_played;
    std::int32_t start_bytes;
    std::int8_t channels;
    std::int8_t channel_map[2];
    ParameterAudioType type;
    std::int8_t unk60;
    std::int8_t start_buffer;
    std::int8_t unk62;
    std::int8_t unk63;
};

struct Module : public ngs::Module {
private:
    PCMChannelBuf decoded_pending;
    std::uint32_t decoded_samples_pending;
    std::uint32_t decoded_passed;

public:
    explicit Module();
    void process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) override;
    void get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) override {}
    std::uint32_t module_id() const { return 0x5CE6; }
    std::size_t get_buffer_parameter_size() const override;
};
}; // namespace ngs::player
