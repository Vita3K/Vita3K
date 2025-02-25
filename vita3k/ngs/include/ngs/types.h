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

#include <mem/ptr.h>
#include <util/types.h>

#define SCE_NGS_MAX_SYSTEM_CHANNELS 2

namespace ngs {
struct Voice;
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
    SCE_NGS_PARAM_EQ_STRUCT_ID = 0x01015CEC,
    SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID = 0x02015CEC,
};

// put it here because it is used by sceNgsAT9GetSectionDetails
struct SceNgsAT9SkipBufferInfo {
    SceInt32 start_byte_offset;
    SceInt32 num_bytes;
    SceInt16 start_skip;
    SceInt16 end_skip;
    SceInt32 is_super_packet;
};
