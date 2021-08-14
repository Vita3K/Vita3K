// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

namespace ngs::atrac9 {
enum {
    SCE_NGS_AT9_CALLBACK_REASON_DONE_ALL = 0,
    SCE_NGS_AT9_CALLBACK_REASON_DONE_ONE_BUFFER = 1,
    SCE_NGS_AT9_CALLBACK_REASON_START_LOOP = 2,
    SCE_NGS_AT9_CALLBACK_REASON_DECODE_ERROR = 3
};

struct BufferParameter {
    Ptr<void> buffer;
    std::int32_t bytes_count;
    std::int16_t loop_count;
    std::int16_t next_buffer_index;
    std::int16_t samples_discard_start_off;
    std::int16_t samples_discard_end_off;
};

struct SkipBufferInfo {
    std::int32_t start_byte_offset;
    std::int32_t num_bytes;
    std::int16_t start_skip;
    std::int16_t end_skip;
    std::int32_t is_super_packet;
};

static constexpr std::uint32_t MAX_BUFFER_PARAMS = 4;

struct Parameters {
    ngs::ModuleParameterHeader header;
    BufferParameter buffer_params[MAX_BUFFER_PARAMS];
    float playback_frequency;
    float playback_scalar;
    std::int32_t lead_in_samples;
    std::int32_t limit_number_of_samples_played;
    std::int8_t channels;
    std::int8_t channel_map[2];
    std::int8_t unk5B;
    std::uint32_t config_data;
};

struct State {
    std::int32_t current_byte_position_in_buffer = 0;
    std::int32_t current_buffer = 0;
    std::int32_t samples_generated_since_key_on = 0;
    std::int32_t bytes_consumed_since_key_on = 0;
    std::int32_t total_bytes_consumed = 0;

    // INTERNAL
    std::int8_t current_loop_count = 0;
    std::uint32_t decoded_samples_pending = 0;
    std::uint32_t decoded_passed = 0;
};

struct Module : public ngs::Module {
private:
    std::unique_ptr<Atrac9DecoderState> decoder;
    std::uint32_t last_config;

public:
    explicit Module();

    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) override;
    std::uint32_t module_id() const override { return 0x5CAA; }
    std::size_t get_buffer_parameter_size() const override;
    void on_state_change(ModuleData &v, const VoiceState previous) override;
};

void get_buffer_parameter(std::uint32_t start_sample, std::uint32_t num_samples, std::uint32_t info, SkipBufferInfo &parameter);
} // namespace ngs::atrac9
