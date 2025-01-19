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

#define SCE_NGS_FILTER_PARAMS_STRUCT_ID 0x01015CE4
#define SCE_NGS_FILTER_PARAMS_COEFF_STRUCT_ID 0x02015CE4

enum SceNgsParamFilterMode : uint32_t {
    SCE_NGS_FILTER_MODE_OFF,
    SCE_NGS_FILTER_LOWPASS_RESONANT,
    SCE_NGS_FILTER_HIGHPASS_RESONANT,
    SCE_NGS_FILTER_BANDPASS_PEAK,
    SCE_NGS_FILTER_BANDPASS_ZERO,
    SCE_NGS_FILTER_NOTCH,
    SCE_NGS_FILTER_PEAK,
    SCE_NGS_FILTER_HIGHSHELF,
    SCE_NGS_FILTER_LOWSHELF,
    SCE_NGS_FILTER_LOWPASS_ONEPOLE,
    SCE_NGS_FILTER_HIGHPASS_ONEPOLE,
    SCE_NGS_FILTER_ALLPASS,
    SCE_NGS_FILTER_LOWPASS_RESONANT_NORMALIZED
};

struct SceNgsParamFilter {
    SceNgsParamFilterMode eFilterMode;
    SceFloat32 fFrequency;
    SceFloat32 fResonance;
    SceFloat32 fGain;
};

struct SceNgsFilterParams {
    SceNgsParamsDescriptor desc;
    SceNgsParamFilter params;
};

// y[0] = fB0 x[0] + fB1 x[-1] + fB2 x[-2] - fA1 y[-1] - fA2 y[-2]
struct SceNgsParamCoEff {
    SceFloat32 fB0;
    SceFloat32 fB1;
    SceFloat32 fB2;
    SceFloat32 fA1;
    SceFloat32 fA2;
};

struct SceNgsFilterParamsCoEff {
    SceNgsParamsDescriptor desc;
    SceNgsParamCoEff params;
};

namespace ngs {

class FilterModule : public Module {
public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CE4; }

    static constexpr uint32_t get_max_parameter_size() {
        return std::max(sizeof(SceNgsFilterParams), sizeof(SceNgsFilterParamsCoEff));
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};

} // namespace ngs
