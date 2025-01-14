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

#include <codec/state.h>
#include <ngs/system.h>
#include <ngs/types.h>

enum {
    SCE_NGS_PLAYER_END_OF_DATA = 0,
    SCE_NGS_PLAYER_SWAPPED_BUFFER = 1,
    SCE_NGS_PLAYER_LOOPED_BUFFER = 2,
};

struct SceNgsPlayerBufferParams {
    const Ptr<void> buffer;
    SceInt32 bytes_count;
    SceInt16 loop_count;
    SceInt16 next_buffer_index;
};

#define SCE_NGS_PLAYER_MAX_BUFFERS 4
#define SCE_NGS_PLAYER_MAX_PCM_CHANNELS 2

enum ParameterAudioType : SceInt8 {
    ParameterAudioTypePCM = 0,
    ParameterAudioTypeADPCM = 1
};

struct SceNgsPlayerStates {
    SceInt32 current_byte_position_in_buffer = 0;
    SceInt32 current_buffer = 0;
    SceInt32 samples_generated_since_key_on = 0;
    SceInt32 bytes_consumed_since_key_on = 0;
    SceInt32 samples_generated_total = 0;
    SceInt32 total_bytes_consumed = 0;

    // INTERNAL
    int8_t current_loop_count = 0;
    uint32_t decoded_samples_pending = 0;
    uint32_t decoded_samples_passed = 0;
    // needed for he_adpcm because a same decoder can be used for many voices
    ADPCMHistory adpcm_history[SCE_NGS_PLAYER_MAX_PCM_CHANNELS] = {};
    // used if the input must be resampled
    SwrContext *swr = nullptr;
    // if we need at some point to reset the resampler params
    bool reset_swr = false;
};

struct SceNgsPlayerParams {
    SceNgsParamsDescriptor descriptor;
    SceNgsPlayerBufferParams buffer_params[SCE_NGS_PLAYER_MAX_BUFFERS];
    SceFloat32 playback_frequency;
    SceFloat32 playback_scalar;
    SceInt32 lead_in_samples;
    SceInt32 limit_number_of_samples_played;
    SceInt32 start_bytes;
    SceInt8 channels;
    SceInt8 channel_map[SCE_NGS_PLAYER_MAX_PCM_CHANNELS];
    ParameterAudioType type;
    SceInt8 res;
    SceInt8 start_buffer;
    SceInt8 padding[2];
};

struct SceNgsPlayerParamsBlock {
    SceNgsModuleParamHeader moduleInfo;
    SceNgsPlayerParams params;
};

namespace ngs {

class PlayerModule : public Module {
private:
    std::unique_ptr<PCMDecoderState> decoder;

public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CE6; }
    void on_state_change(const MemState &mem, ModuleData &v, const VoiceState previous) override;
    void on_param_change(const MemState &mem, ModuleData &data) override;

    static constexpr uint32_t get_max_parameter_size() {
        return sizeof(SceNgsPlayerParams);
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};
} // namespace ngs
