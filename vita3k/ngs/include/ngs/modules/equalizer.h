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

#include <ngs/modules/filter.h>

#define SCE_NGS_MAX_EQ_FILTERS 4

#define SCE_NGS_PARAM_EQ_STRUCT_ID 0x01015CEC
#define SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID 0x02015CEC

struct SceNgsParamEqParams {
    SceNgsParamsDescriptor desc;
    SceNgsParamFilter filter[SCE_NGS_MAX_EQ_FILTERS];
};

struct SceNgsParamEqParamsCoEff {
    SceNgsParamsDescriptor desc;
    SceNgsParamCoEff filterCoEff[SCE_NGS_MAX_EQ_FILTERS];
};

namespace ngs {

class EqualizerModule : public Module {
public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CEC; }

    static constexpr uint32_t get_max_parameter_size() {
        return std::max(sizeof(SceNgsParamEqParams), sizeof(SceNgsParamEqParamsCoEff));
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};

} // namespace ngs
