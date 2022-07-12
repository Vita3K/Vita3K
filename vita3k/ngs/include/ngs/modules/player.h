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

namespace ngs::player {
enum {
    SCE_NGS_PLAYER_END_OF_DATA = 0,
    SCE_NGS_PLAYER_SWAPPED_BUFFER = 1,
    SCE_NGS_PLAYER_LOOPED_BUFFER = 2,
};

struct BufferParameters {
    const Ptr<void> buffer;
    SceInt32 bytes_count;
    SceInt16 loop_count;
    SceInt16 next_buffer_index;
};

#define MAX_BUFFER_PARAMS 4
#define MAX_PCM_CHANNELS 2

enum ParameterAudioType : SceInt8 {
    ParameterAudioTypePCM = 0,
    ParameterAudioTypeADPCM = 1
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
    std::uint32_t decoded_samples_passed = 0;
    // needed for he_adpcm because a same decoder can be used for many voices
    ADPCMHistory adpcm_history[MAX_PCM_CHANNELS] = {};
    // used if the input must be resampled
    SwrContext *swr = nullptr;
    // if we need at some point to reset the resampler params
    bool reset_swr = false;
};

struct Parameters {
    ngs::ParametersDescriptor descriptor;
    BufferParameters buffer_params[MAX_BUFFER_PARAMS];
    SceFloat32 playback_frequency;

    /**
     * @brief Controls playback rate scaling
     * @details A scaling factor of 1 keeps the playback rate without changes.
     * A value greater than 1 increases the playback rate, as well as the perceived
     * speed of the audio and the pitch.
     * A value lower than 1 decreases the playback rate, as well as the perceived
     * speed of the audio and the pitch.
     * The final playback rate compared to the original one will be the result of
     * `playback_rate * scaling_factor`.
     *
     * Please note that this has nothing to do with the playback rate of the host audio
     * stream.
     */
    SceFloat32 playback_scalar;
    SceInt32 lead_in_samples;
    SceInt32 limit_number_of_samples_played;
    SceInt32 start_bytes;
    SceInt8 channels;
    SceInt8 channel_map[MAX_PCM_CHANNELS];
    ParameterAudioType type;
    SceInt8 res;
    SceInt8 start_buffer;
    SceInt8 padding[2];
};

struct Module : public ngs::Module {
private:
    std::unique_ptr<PCMDecoderState> decoder;

public:
    explicit Module();
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    std::uint32_t module_id() const override { return 0x5CE6; }
    std::size_t get_buffer_parameter_size() const override;
    void on_state_change(ModuleData &v, const VoiceState previous) override;
    void on_param_change(const MemState &mem, ModuleData &data) override;
};
} // namespace ngs::player
