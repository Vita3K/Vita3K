// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <ngs/system.h>
#include <ngs/types.h>

#include <codec/state.h>

enum {
    SCE_NGS_AT9_END_OF_DATA = 0,
    SCE_NGS_AT9_SWAPPED_BUFFER = 1,
    SCE_NGS_AT9_HEADER_ERROR = 2,
    SCE_NGS_AT9_DECODE_ERROR = 3,
    SCE_NGS_AT9_LOOPED_BUFFER = 4
};

struct SceNgsAT9BufferParams {
    const Ptr<void> buffer;
    SceInt32 bytes_count;
    SceInt16 loop_count;
    SceInt16 next_buffer_index;
    SceInt16 samples_discard_start_off;
    SceInt16 samples_discard_end_off;
};

#define SCE_NGS_AT9_MAX_BUFFER_PARAMS 4
#define SCE_NGS_AT9_MAX_PCM_CHANNELS 2

struct SceNgsAT9Params {
    SceNgsParamsDescriptor descriptor;
    SceNgsAT9BufferParams buffer_params[SCE_NGS_AT9_MAX_BUFFER_PARAMS];
    SceFloat32 playback_frequency;
    SceFloat32 playback_scalar;
    SceInt32 lead_in_samples;
    SceInt32 limit_number_of_samples_played;
    SceInt8 channels;
    SceInt8 channel_map[SCE_NGS_AT9_MAX_PCM_CHANNELS];
    SceInt8 reserved;
    SceInt32 config_data;
};

struct SceNgsAT9States {
    SceInt32 current_byte_position_in_buffer = 0;
    SceInt32 current_buffer = 0;
    SceInt32 samples_generated_since_key_on = 0;
    SceInt32 bytes_consumed_since_key_on = 0;
    SceInt32 samples_generated_total = 0;
    SceInt32 total_bytes_consumed = 0;

    // INTERNAL
    uint32_t decoded_samples_pending = 0;
    uint32_t decoded_passed = 0;
    uint32_t nb_channels = 0;
    // used if the input must be resampled
    SwrContext *swr = nullptr;
    int8_t current_loop_count = 0;
    // necessary if the decoder is using multiple states
    Atrac9DecoderSavedState saved_state{};
};

namespace ngs {
class Atrac9Module : public Module {
private:
    std::unique_ptr<Atrac9DecoderState> decoder;
    uint32_t last_config = 0;
    std::vector<uint8_t> temp_buffer;
    SceNgsAT9States *last_state = nullptr;

    static SwrContext *swr_mono_to_stereo;
    static SwrContext *swr_stereo;

    // return false if data could not be decoded (error or no more data available)
    bool decode_more_data(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, const SceNgsAT9Params *params, SceNgsAT9States *state, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock);

public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CAA; }
    void on_state_change(const MemState &mem, ModuleData &v, const VoiceState previous) override;
    void on_param_change(const MemState &mem, ModuleData &data) override;

    static constexpr uint32_t get_max_parameter_size() {
        return sizeof(SceNgsAT9Params);
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};

} // namespace ngs
