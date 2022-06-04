// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <codec/state.h>
#include <ngs/system.h>

struct SwrContext;

namespace ngs::atrac9 {
enum {
    SCE_NGS_AT9_END_OF_DATA = 0,
    SCE_NGS_AT9_SWAPPED_BUFFER = 1,
    SCE_NGS_AT9_HEADER_ERROR = 2,
    SCE_NGS_AT9_DECODE_ERROR = 3,
    SCE_NGS_AT9_LOOPED_BUFFER = 4
};

struct BufferParameters {
    const Ptr<void> buffer;
    SceInt32 bytes_count;
    SceInt16 loop_count;
    SceInt16 next_buffer_index;
    SceInt16 samples_discard_start_off;
    SceInt16 samples_discard_end_off;
};

struct SkipBufferInfo {
    SceInt32 start_byte_offset;
    SceInt32 num_bytes;
    SceInt16 start_skip;
    SceInt16 end_skip;
    SceInt32 is_super_packet;
};

#define MAX_BUFFER_PARAMS 4
#define MAX_PCM_CHANNELS 2

struct Parameters {
    ngs::ParametersDescriptor descriptor;
    BufferParameters buffer_params[MAX_BUFFER_PARAMS];
    SceFloat32 playback_frequency;
    SceFloat32 playback_scalar;
    SceInt32 lead_in_samples;
    SceInt32 limit_number_of_samples_played;
    SceInt8 channels;
    SceInt8 channel_map[MAX_PCM_CHANNELS];
    SceInt8 reserved;
    SceInt32 config_data;
};

struct State {
    SceInt32 current_byte_position_in_buffer = 0;
    SceInt32 current_buffer = 0;
    SceInt32 samples_generated_since_key_on = 0;
    SceInt32 bytes_consumed_since_key_on = 0;
    SceInt32 samples_generated_total = 0;
    SceInt32 total_bytes_consumed = 0;

    // INTERNAL
    std::int8_t current_loop_count = 0;
    std::uint32_t decoded_samples_pending = 0;
    std::uint32_t decoded_passed = 0;
    // used if the input must be resampled
    SwrContext *swr = nullptr;
    // set to true if all the input has been read but not all data has been processed
    bool is_finished = false;
};

struct Module : public ngs::Module {
private:
    std::unique_ptr<Atrac9DecoderState> decoder;
    std::uint32_t last_config;
    std::vector<uint8_t> temp_buffer;

    static SwrContext *swr_mono_to_stereo;
    static SwrContext *swr_stereo;

    // return false if data could not be decoded (error or no more data available)
    bool decode_more_data(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, const Parameters *params, State *state, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock);

public:
    explicit Module();

    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    std::uint32_t module_id() const override { return 0x5CAA; }
    std::size_t get_buffer_parameter_size() const override;
    void on_state_change(ModuleData &v, const VoiceState previous) override;
    void on_param_change(const MemState &mem, ModuleData &data) override;
};

void get_buffer_parameter(std::uint32_t start_sample, std::uint32_t num_samples, std::uint32_t info, SkipBufferInfo &parameter);
} // namespace ngs::atrac9
