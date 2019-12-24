// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <util/log.h>
#include <ngs/system.h>
#include <ngs/modules/atrac9.h>

#include "SceNgs.h"

namespace emu {
struct SceNgsPatchInfo1 {
    std::int32_t out_channels;
    std::int32_t in_channels;
    float unk[3];
};
static_assert(sizeof(SceNgsPatchInfo1) == 20);

struct SceNgsPatchInfo2 {
    std::int32_t unk[5];
};
static_assert(sizeof(SceNgsPatchInfo2) == 20);

using SceNgsSynthSystemHandle = Ptr<emu::ngs::System>;
using SceNgsRackHandle = Ptr<emu::ngs::Rack>;
using SceNgsVoiceHandle = Ptr<emu::ngs::Voice>;
using SceNgsPatchHandle = Ptr<emu::ngs::Patch>;

} // namespace emu

static constexpr SceUInt32 SCE_NGS_OK = 0;
static constexpr SceUInt32 SCE_NGS_ERROR = 0x804A0001;
static constexpr SceUInt32 SCE_NGS_ERROR_INVALID_ARG = 0x804A0002;
static constexpr SceUInt32 SCE_NGS_SIZE_MISMATCH = 0x804A000D;

EXPORT(int, sceNgsAT9GetSectionDetails, const std::uint32_t samples_start, const std::uint32_t num_samples, const std::uint32_t config_data, emu::ngs::atrac9::SkipBufferInfo *info) {
    // Check magic!
    if ((config_data & 0xFF) != 0xFE && info) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    get_buffer_parameter(samples_start, num_samples, config_data, *info);
    return 0;
}

EXPORT(int, sceNgsModuleGetNumPresets) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsModuleGetPreset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsPatchCreateRouting, emu::ngs::PatchSetupInfo *patch_info, emu::SceNgsPatchHandle *handle) {
    assert(handle);
    
    // Make the scheduler order this right based on dependencies request
    emu::ngs::Voice *source = patch_info->source.get(host.mem);
    
    if (!source) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    *handle = source->rack->system->voice_scheduler.patch(host.mem, patch_info);

    if (!*handle) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return SCE_NGS_OK;
}

EXPORT(int, sceNgsPatchGetInfo, std::uint32_t patch, Ptr<emu::SceNgsPatchInfo1> patch_info1_, Ptr<emu::SceNgsPatchInfo2> patch_info2_) {
    auto *patch_info1 = patch_info1_.get(host.mem);

    patch_info1->in_channels = 2;
    patch_info1->out_channels = 2;

    return STUBBED("2 in/out channels");
}

EXPORT(int, sceNgsPatchRemoveRouting, emu::SceNgsPatchHandle patch_handle) {
    emu::ngs::Patch *patch = patch_handle.get(host.mem);

    if (!patch_handle.valid(host.mem) || !patch) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (!patch->source->remove_patch(host.mem, patch_handle)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return 0;
}

EXPORT(int, sceNgsRackGetRequiredMemorySize, emu::SceNgsSynthSystemHandle sys_handle, emu::ngs::RackDescription *description, uint32_t *size) {
    *size = emu::ngs::Rack::get_required_memspace_size(host.mem, description);
    return 0;
}

EXPORT(SceUInt32, sceNgsRackGetVoiceHandle, emu::SceNgsRackHandle rack_handle, const std::uint32_t index, emu::SceNgsVoiceHandle *voice_handle) {
    emu::ngs::Rack *rack = rack_handle.get(host.mem);

    if (!rack || !voice_handle) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (index >= rack->voices.size()) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    *voice_handle = rack->voices[index];
    return SCE_NGS_OK;
}

EXPORT(SceUInt32, sceNgsRackInit, emu::SceNgsSynthSystemHandle sys_handle, emu::ngs::BufferParamsInfo *info, const emu::ngs::RackDescription *description, emu::SceNgsRackHandle *handle) {
    assert(sys_handle);
    assert(info);
    assert(description);

    emu::ngs::System *system = sys_handle.get(host.mem);

    if (!emu::ngs::init_rack(host.ngs, host.mem, system, info, description)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    *handle = info->data.cast<emu::ngs::Rack>();
    return SCE_NGS_OK;
}

EXPORT(int, sceNgsRackRelease) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsRackSetParamErrorCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsSystemGetRequiredMemorySize, emu::ngs::SystemInitParameters *params, uint32_t *size) {
    *size = emu::ngs::System::get_required_memspace_size(params);           // System struct size
    return 0;
}

EXPORT(SceUInt32, sceNgsSystemInit, Ptr<void> memspace, const std::uint32_t memspace_size, emu::ngs::SystemInitParameters *params,
    emu::SceNgsSynthSystemHandle *handle) {
    if (!emu::ngs::init_system(host.ngs, host.mem, params, memspace, memspace_size)) {
        return RET_ERROR(SCE_NGS_ERROR);      // TODO: Better error code
    }

    *handle = memspace.cast<emu::ngs::System>();
    return SCE_NGS_OK;
}

EXPORT(int, sceNgsSystemLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsSystemRelease) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsSystemSetFlags) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsSystemSetParamErrorCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsSystemUnlock) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceNgsSystemUpdate, emu::SceNgsSynthSystemHandle handle) {
    emu::ngs::System *sys = handle.get(host.mem);
    sys->voice_scheduler.update(host.mem);

    return SCE_NGS_OK;
}

EXPORT(int, sceNgsVoiceBypassModule) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetAtrac9Voice) {
    return host.ngs.definitions[emu::ngs::BUSS_ATRAC9];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetCompressorBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_COMPRESSOR];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetCompressorSideChainBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_SIDE_CHAIN_COMPRESSOR];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetDelayBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_DELAY];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetDistortionBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_DISTORTION];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetEnvelopeBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_ENVELOPE];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetEqBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_EQUALIZATION];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetMasterBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_MASTER];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetMixerBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_MIXER];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetPauserBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_PAUSER];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetPitchShiftBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_PITCH_SHIFT];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetReverbBuss) {
    return host.ngs.definitions[emu::ngs::BUSS_REVERB];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetSasEmuVoice) {
    return host.ngs.definitions[emu::ngs::BUSS_SAS_EMULATION];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetScreamAtrac9Voice) {
    return host.ngs.definitions[emu::ngs::BUSS_SCREAM_ATRAC9];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetScreamVoice) {
    return host.ngs.definitions[emu::ngs::BUSS_SCREAM];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetSimpleAtrac9Voice) {
    return host.ngs.definitions[emu::ngs::BUSS_SIMPLE_ATRAC9];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetSimpleVoice) {
    return host.ngs.definitions[emu::ngs::BUSS_SIMPLE];
}

EXPORT(Ptr<emu::ngs::VoiceDefinition>, sceNgsVoiceDefGetTemplate1) {
    return host.ngs.definitions[emu::ngs::BUSS_NORMAL_PLAYER];
}

EXPORT(int, sceNgsVoiceGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceGetModuleBypass) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceGetModuleType) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceGetOutputPatch) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceGetParamsOutOfRange) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceGetStateData, emu::SceNgsVoiceHandle voice_handle, const std::uint32_t unk, void *mem, const std::uint32_t space_size) {
    emu::ngs::Voice *voice = voice_handle.get(host.mem);
    std::memcpy(mem, &voice->voice_state_data[0], std::min<std::size_t>(space_size, voice->voice_state_data.size()));
    
    return SCE_NGS_OK;
}

EXPORT(int, sceNgsVoiceInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceKeyOff) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceKill, emu::SceNgsVoiceHandle voice_handle) {
    emu::ngs::Voice *voice = voice_handle.get(host.mem);

    if (!voice) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (!voice->rack->system->voice_scheduler.stop(voice)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return 0;
}

EXPORT(SceUInt32, sceNgsVoiceLockParams, emu::SceNgsVoiceHandle voice_handle, std::uint32_t unk1, std::uint32_t unk2, Ptr<emu::ngs::BufferParamsInfo> buf) {
    emu::ngs::Voice *voice = voice_handle.get(host.mem);
    emu::ngs::BufferParamsInfo *info = voice->lock_params(host.mem);

    if (!info) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    *(buf.get(host.mem)) = *info;
    return SCE_NGS_OK;
}

EXPORT(int, sceNgsVoicePatchSetVolume) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoicePatchSetVolumes) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoicePatchSetVolumesMatrix) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoicePause) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceNgsVoicePlay, emu::SceNgsVoiceHandle handle) {
    emu::ngs::Voice *voice = handle.get(host.mem);

    if (!voice) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (!voice->rack->system->voice_scheduler.play(host.mem, voice)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return SCE_NGS_OK;
}

EXPORT(int, sceNgsVoiceResume) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceSetFinishedCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceSetModuleCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceSetParamsBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceSetPreset) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceNgsVoiceUnlockParams, emu::SceNgsVoiceHandle handle) {
    emu::ngs::Voice *voice = handle.get(host.mem);

    if (!voice->unlock_params()) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return SCE_NGS_OK;
}

EXPORT(int, sceSulphaNgsGetDefaultConfig) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsGetNeededMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsSetRackName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsSetSampleName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsSetSynthName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsSetVoiceName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsShutdown) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSulphaNgsTrace) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceNgsAT9GetSectionDetails)
BRIDGE_IMPL(sceNgsModuleGetNumPresets)
BRIDGE_IMPL(sceNgsModuleGetPreset)
BRIDGE_IMPL(sceNgsPatchCreateRouting)
BRIDGE_IMPL(sceNgsPatchGetInfo)
BRIDGE_IMPL(sceNgsPatchRemoveRouting)
BRIDGE_IMPL(sceNgsRackGetRequiredMemorySize)
BRIDGE_IMPL(sceNgsRackGetVoiceHandle)
BRIDGE_IMPL(sceNgsRackInit)
BRIDGE_IMPL(sceNgsRackRelease)
BRIDGE_IMPL(sceNgsRackSetParamErrorCallback)
BRIDGE_IMPL(sceNgsSystemGetRequiredMemorySize)
BRIDGE_IMPL(sceNgsSystemInit)
BRIDGE_IMPL(sceNgsSystemLock)
BRIDGE_IMPL(sceNgsSystemRelease)
BRIDGE_IMPL(sceNgsSystemSetFlags)
BRIDGE_IMPL(sceNgsSystemSetParamErrorCallback)
BRIDGE_IMPL(sceNgsSystemUnlock)
BRIDGE_IMPL(sceNgsSystemUpdate)
BRIDGE_IMPL(sceNgsVoiceBypassModule)
BRIDGE_IMPL(sceNgsVoiceDefGetAtrac9Voice)
BRIDGE_IMPL(sceNgsVoiceDefGetCompressorBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetCompressorSideChainBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetDelayBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetDistortionBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetEnvelopeBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetEqBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetMasterBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetMixerBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetPauserBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetPitchShiftBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetReverbBuss)
BRIDGE_IMPL(sceNgsVoiceDefGetSasEmuVoice)
BRIDGE_IMPL(sceNgsVoiceDefGetScreamAtrac9Voice)
BRIDGE_IMPL(sceNgsVoiceDefGetScreamVoice)
BRIDGE_IMPL(sceNgsVoiceDefGetSimpleAtrac9Voice)
BRIDGE_IMPL(sceNgsVoiceDefGetSimpleVoice)
BRIDGE_IMPL(sceNgsVoiceDefGetTemplate1)
BRIDGE_IMPL(sceNgsVoiceGetInfo)
BRIDGE_IMPL(sceNgsVoiceGetModuleBypass)
BRIDGE_IMPL(sceNgsVoiceGetModuleType)
BRIDGE_IMPL(sceNgsVoiceGetOutputPatch)
BRIDGE_IMPL(sceNgsVoiceGetParamsOutOfRange)
BRIDGE_IMPL(sceNgsVoiceGetStateData)
BRIDGE_IMPL(sceNgsVoiceInit)
BRIDGE_IMPL(sceNgsVoiceKeyOff)
BRIDGE_IMPL(sceNgsVoiceKill)
BRIDGE_IMPL(sceNgsVoiceLockParams)
BRIDGE_IMPL(sceNgsVoicePatchSetVolume)
BRIDGE_IMPL(sceNgsVoicePatchSetVolumes)
BRIDGE_IMPL(sceNgsVoicePatchSetVolumesMatrix)
BRIDGE_IMPL(sceNgsVoicePause)
BRIDGE_IMPL(sceNgsVoicePlay)
BRIDGE_IMPL(sceNgsVoiceResume)
BRIDGE_IMPL(sceNgsVoiceSetFinishedCallback)
BRIDGE_IMPL(sceNgsVoiceSetModuleCallback)
BRIDGE_IMPL(sceNgsVoiceSetParamsBlock)
BRIDGE_IMPL(sceNgsVoiceSetPreset)
BRIDGE_IMPL(sceNgsVoiceUnlockParams)
BRIDGE_IMPL(sceSulphaNgsGetDefaultConfig)
BRIDGE_IMPL(sceSulphaNgsGetNeededMemory)
BRIDGE_IMPL(sceSulphaNgsInit)
BRIDGE_IMPL(sceSulphaNgsSetRackName)
BRIDGE_IMPL(sceSulphaNgsSetSampleName)
BRIDGE_IMPL(sceSulphaNgsSetSynthName)
BRIDGE_IMPL(sceSulphaNgsSetVoiceName)
BRIDGE_IMPL(sceSulphaNgsShutdown)
BRIDGE_IMPL(sceSulphaNgsTrace)
