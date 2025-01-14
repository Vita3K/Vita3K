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

#define SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID 0x01015CE3

#define SCE_NGS_ENVELOPE_MAX_POINTS 4

enum SceNgsEnvelopeCurveType : uint32_t {
    SCE_NGS_ENVELOPE_LINEAR,
    SCE_NGS_ENVELOPE_CURVED
};

struct SceNgsEnvelopePoint {
    SceUInt32 uMsecsToNextPoint;
    SceFloat32 fAmplitude;
    SceNgsEnvelopeCurveType eCurveType;
};

struct SceNgsEnvelopeParams {
    SceNgsParamsDescriptor desc;
    SceNgsEnvelopePoint envelopePoints[SCE_NGS_ENVELOPE_MAX_POINTS];
    SceUInt32 uReleaseMsecs;
    SceUInt32 uNumPoints;
    SceUInt32 uLoopStart;
    SceInt32 nLoopEnd;
};

struct SceNgsEnvelopeStates {
    SceFloat32 fCurrentHeight;
    SceFloat32 fPosition;
    SceFloat32 fReleaseScale;
    SceInt32 nCurrentPoint;
    SceInt32 nReleasing;
};

namespace ngs {

struct EnvelopeModule : public Module {
public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CE3; }

    static constexpr uint32_t get_max_parameter_size() {
        return sizeof(SceNgsEnvelopeParams);
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};

} // namespace ngs
