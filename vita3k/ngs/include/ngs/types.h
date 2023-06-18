// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
#include <mem/ptr.h>
#include <util/types.h>

struct SwrContext;

namespace ngs {
struct Voice;
struct Rack;
struct VoiceDefinition;
} // namespace ngs

struct SceNgsSystemInitParams {
    int32_t max_racks;
    int32_t max_voices;
    int32_t granularity;
    int32_t sample_rate;
    int32_t unk16;
};

struct SceNgsPatchSetupInfo {
    Ptr<ngs::Voice> source;
    int32_t source_output_index;
    int32_t source_output_subindex;
    Ptr<ngs::Voice> dest;
    int32_t dest_input_index;
};

struct SceNgsRackDescription {
    Ptr<ngs::VoiceDefinition> definition;
    int32_t voice_count;
    int32_t channels_per_voice;
    int32_t max_patches_per_input;
    int32_t patches_per_output;
    Ptr<void> unk14;
};

struct SceNgsCallbackInfo {
    Ptr<void> voice_handle;
    Ptr<void> rack_handle;
    uint32_t module_id;
    uint32_t callback_reason;
    uint32_t callback_reason_2;
    Ptr<void> callback_ptr;
    Ptr<void> userdata;
};

struct SceNgsVoicePreset {
    SceInt32 name_offset;
    SceUInt32 name_length;
    SceInt32 preset_data_offset;
    SceUInt32 preset_data_size;
    SceInt32 bypass_flags_offset;
    SceUInt32 bypass_flags_nb;
};

struct SceNgsParamsDescriptor {
    SceUInt32 id;
    SceUInt32 size;
};

struct SceNgsModuleParamHeader {
    SceInt32 module_id;
    SceInt32 channel;
};

struct SceNgsBufferInfo {
    Ptr<void> data;
    SceUInt32 size;
};

enum SceNgsParamStructId : uint32_t {
    SCE_NGS_AT9_PARAMS_STRUCT_ID = 0x01015CAA,
    SCE_NGS_PLAYER_PARAMS_STRUCT_ID = 0x01015CE6,
};

// ATRAC9

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

struct SceNgsAT9SkipBufferInfo {
    SceInt32 start_byte_offset;
    SceInt32 num_bytes;
    SceInt16 start_skip;
    SceInt16 end_skip;
    SceInt32 is_super_packet;
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

struct SceNgsAT9ParamsBlock {
    SceNgsModuleParamHeader moduleInfo;
    SceNgsAT9Params params;
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
    // used if the input must be resampled
    SwrContext *swr = nullptr;
    int8_t current_loop_count = 0;
    // set to true if all the input has been read but not all data has been processed
    bool is_finished = false;
};

// Player

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
