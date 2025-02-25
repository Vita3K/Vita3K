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

#define SCE_NGS_REVERB_PARAMS_STRUCT_ID 0x01015CE7
#define SCE_NGS_REVERB_PARAMS_STRUCT_ID_V2 0x01025CE7

enum SceNgsReverbRoom : int32_t {
    SCE_NGS_REVERB_ROOM1_LEFT,
    SCE_NGS_REVERB_ROOM1_RIGHT,
    SCE_NGS_REVERB_ROOM2_LEFT,
    SCE_NGS_REVERB_ROOM2_RIGHT,
    SCE_NGS_REVERB_ROOM3_LEFT,
    SCE_NGS_REVERB_ROOM3_RIGHT
};

struct SceNgsReverbParams {
    SceNgsParamsDescriptor desc;
    SceFloat32 fRoom;
    SceFloat32 fRoomHF;
    SceFloat32 fDecayTime;
    SceFloat32 fDecayHFRatio;
    SceFloat32 fReflections;
    SceFloat32 fReflectionsDelay;
    SceFloat32 fReverb;
    SceFloat32 fReverbDelay;
    SceFloat32 fDiffusion;
    SceFloat32 fDensity;
    SceFloat32 fHFReference;
    SceNgsReverbRoom eEarlyReflectionPattern[SCE_NGS_MAX_SYSTEM_CHANNELS];
    SceFloat32 fEarlyReflectionScalar;
    SceFloat32 fLFReference;
    SceFloat32 fRoomLF;
    SceFloat32 fDryMB;
};

namespace ngs {

class ReverbModule : public Module {
public:
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) override;
    uint32_t module_id() const override { return 0x5CE7; }

    static constexpr uint32_t get_max_parameter_size() {
        return sizeof(SceNgsReverbParams);
    }
    uint32_t get_buffer_parameter_size() const override {
        return get_max_parameter_size();
    }
};

} // namespace ngs
