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

#define SCE_NGS_COMPRESSOR_PARAMS_STRUCT_ID 0x01015CE1
#define SCE_NGS_COMPRESSOR_PARAMS_STRUCT_ID_V2 0x01025CE1

enum SceNgsCompressorMode : int32_t {
    SCE_NGS_COMPRESSOR_RMS_MODE,
    SCE_NGS_COMPRESSOR_PEAK_MODE
};

enum SceNgsCompressorStereoLink : int32_t {
    SCE_NGS_COMPRESSOR_STEREO_LINK_OFF,
    SCE_NGS_COMPRESSOR_STEREO_LINK_ON
};

struct SceNgsCompressorParams {
    SceNgsParamsDescriptor desc;
    SceFloat32 fRatio;
    SceFloat32 fThreshold;
    SceFloat32 fAttack;
    SceFloat32 fRelease;
    SceFloat32 fMakeupGain;
    SceNgsCompressorStereoLink nStereoLink;
    SceNgsCompressorMode nPeakMode;
    SceFloat32 fSoftKnee;
};

struct SceNgsCompressorStates {
    SceFloat32 fInputLevel[SCE_NGS_MAX_SYSTEM_CHANNELS];
    SceFloat32 fOutputLevel[SCE_NGS_MAX_SYSTEM_CHANNELS];
};

namespace ngs {

struct CompressorModule : public Module {
public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CE1; }

    static constexpr uint32_t get_max_parameter_size() {
        return sizeof(SceNgsCompressorParams);
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};

} // namespace ngs
