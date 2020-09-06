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

#include <ngs/modules/atrac9.h>
#include <ngs/system.h>
#include <util/log.h>

#include "SceNgs.h"

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

using SceNgsSynthSystemHandle = Ptr<ngs::System>;
using SceNgsRackHandle = Ptr<ngs::Rack>;
using SceNgsVoiceHandle = Ptr<ngs::Voice>;
using SceNgsPatchHandle = Ptr<ngs::Patch>;

struct SceNgsCallbackInfo {
    SceNgsVoiceHandle hVoiceHandle;
    SceNgsRackHandle hRackHandle;
    uint32_t uModuleID;
    uint32_t nCallbackData;
    uint32_t nCallbackData2;
    Ptr<void> pCallbackPtr;
    Ptr<void> pUserData;
};

static constexpr SceUInt32 SCE_NGS_OK = 0;
static constexpr SceUInt32 SCE_NGS_ERROR = 0x804A0001;
static constexpr SceUInt32 SCE_NGS_ERROR_INVALID_ARG = 0x804A0002;
static constexpr SceUInt32 SCE_NGS_SIZE_MISMATCH = 0x804A000D;

EXPORT(int, sceNgsAT9GetSectionDetails, const std::uint32_t samples_start, const std::uint32_t num_samples, const std::uint32_t config_data, ngs::atrac9::SkipBufferInfo *info) {
    if (host.cfg.disable_ngs) {
        return -1;
    }
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

EXPORT(int, sceNgsPatchCreateRouting, ngs::PatchSetupInfo *patch_info, SceNgsPatchHandle *handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }
    assert(handle);

    // Make the scheduler order this right based on dependencies request
    ngs::Voice *source = patch_info->source.get(host.mem);

    if (!source) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    *handle = source->rack->system->voice_scheduler.patch(host.mem, patch_info);

    if (!*handle) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return SCE_NGS_OK;
}

EXPORT(int, sceNgsPatchGetInfo, std::uint32_t patch, Ptr<SceNgsPatchInfo1> patch_info1_, Ptr<SceNgsPatchInfo2> patch_info2_) {
    auto *patch_info1 = patch_info1_.get(host.mem);

    patch_info1->in_channels = 2;
    patch_info1->out_channels = 2;

    return STUBBED("2 in/out channels");
}

EXPORT(int, sceNgsPatchRemoveRouting, SceNgsPatchHandle patch_handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::Patch *patch = patch_handle.get(host.mem);

    if (!patch_handle.valid(host.mem) || !patch) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (!patch->source->remove_patch(host.mem, patch_handle)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return 0;
}

EXPORT(int, sceNgsRackGetRequiredMemorySize, SceNgsSynthSystemHandle sys_handle, ngs::RackDescription *description, uint32_t *size) {
    if (host.cfg.disable_ngs) {
        *size = 1;
        return 0;
    }

    *size = ngs::Rack::get_required_memspace_size(host.mem, description);
    return 0;
}

EXPORT(SceUInt32, sceNgsRackGetVoiceHandle, SceNgsRackHandle rack_handle, const std::uint32_t index, SceNgsVoiceHandle *voice_handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }
    ngs::Rack *rack = rack_handle.get(host.mem);

    if (!rack || !voice_handle) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (index >= rack->voices.size()) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    *voice_handle = rack->voices[index];
    return SCE_NGS_OK;
}

EXPORT(SceUInt32, sceNgsRackInit, SceNgsSynthSystemHandle sys_handle, ngs::BufferParamsInfo *info, const ngs::RackDescription *description, SceNgsRackHandle *handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }
    assert(sys_handle);
    assert(info);
    assert(description);

    ngs::System *system = sys_handle.get(host.mem);

    if (!ngs::init_rack(host.ngs, host.mem, system, info, description)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    *handle = info->data.cast<ngs::Rack>();
    return SCE_NGS_OK;
}

EXPORT(int, sceNgsRackRelease) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsRackSetParamErrorCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsSystemGetRequiredMemorySize, ngs::SystemInitParameters *params, uint32_t *size) {
    if (host.cfg.disable_ngs) {
        *size = 1;
        return 0;
    }

    *size = ngs::System::get_required_memspace_size(params); // System struct size
    return 0;
}

EXPORT(SceUInt32, sceNgsSystemInit, Ptr<void> memspace, const std::uint32_t memspace_size, ngs::SystemInitParameters *params,
    SceNgsSynthSystemHandle *handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    if (!ngs::init_system(host.ngs, host.mem, params, memspace, memspace_size)) {
        return RET_ERROR(SCE_NGS_ERROR); // TODO: Better error code
    }

    *handle = memspace.cast<ngs::System>();
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

EXPORT(SceUInt32, sceNgsSystemUpdate, SceNgsSynthSystemHandle handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::System *sys = handle.get(host.mem);
    sys->voice_scheduler.update(host.mem);

    return SCE_NGS_OK;
}

EXPORT(int, sceNgsVoiceBypassModule) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetAtrac9Voice) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_ATRAC9);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetCompressorBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_COMPRESSOR);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetCompressorSideChainBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_SIDE_CHAIN_COMPRESSOR);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetDelayBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_DELAY);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetDistortionBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_DISTORTION);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetEnvelopeBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_ENVELOPE);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetEqBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_EQUALIZATION);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetMasterBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_MASTER);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetMixerBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_MIXER);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetPauserBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_PAUSER);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetPitchShiftBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_PITCH_SHIFT);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetReverbBuss) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_REVERB);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetSasEmuVoice) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_SAS_EMULATION);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetScreamAtrac9Voice) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_SCREAM_ATRAC9);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetScreamVoice) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_SCREAM);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetSimpleAtrac9Voice) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_SIMPLE_ATRAC9);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetSimpleVoice) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_SIMPLE);
}

EXPORT(Ptr<ngs::VoiceDefinition>, sceNgsVoiceDefGetTemplate1) {
    if (host.cfg.disable_ngs) {
        return Ptr<ngs::VoiceDefinition>(0);
    }

    return get_voice_definition(host.ngs, host.mem, ngs::BussType::BUSS_NORMAL_PLAYER);
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

EXPORT(int, sceNgsVoiceGetStateData, SceNgsVoiceHandle voice_handle, const std::uint32_t unk, void *mem, const std::uint32_t space_size) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::Voice *voice = voice_handle.get(host.mem);
    std::memcpy(mem, voice->voice_state_data.data(), std::min<std::size_t>(space_size, voice->voice_state_data.size()));

    return SCE_NGS_OK;
}

EXPORT(int, sceNgsVoiceInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceKeyOff) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceKill, SceNgsVoiceHandle voice_handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::Voice *voice = voice_handle.get(host.mem);

    if (!voice) {
        return RET_ERROR(SCE_NGS_ERROR_INVALID_ARG);
    }

    if (!voice->rack->system->voice_scheduler.stop(voice)) {
        return RET_ERROR(SCE_NGS_ERROR);
    }

    return 0;
}

EXPORT(SceUInt32, sceNgsVoiceLockParams, SceNgsVoiceHandle voice_handle, std::uint32_t unk1, std::uint32_t unk2, Ptr<ngs::BufferParamsInfo> buf) {
    if (host.cfg.disable_ngs) {
        auto *buffer_info = buf.get(host.mem);

        buffer_info->data = alloc(host.mem, 10, "SceNgs buffer stub");
        buffer_info->size = 10;
        return 0;
    }

    ngs::Voice *voice = voice_handle.get(host.mem);
    ngs::BufferParamsInfo *info = voice->lock_params(host.mem);

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

EXPORT(SceUInt32, sceNgsVoicePlay, SceNgsVoiceHandle handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::Voice *voice = handle.get(host.mem);

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

EXPORT(int, sceNgsVoiceSetModuleCallback, SceNgsVoiceHandle voice_handle, uint32_t module, Ptr<void> callback, Ptr<void> user_data) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::Voice *voice = voice_handle.get(host.mem);
    voice->callback = callback;
    voice->user_data = user_data;

    return 0;
}

EXPORT(int, sceNgsVoiceSetParamsBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNgsVoiceSetPreset) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceNgsVoiceUnlockParams, SceNgsVoiceHandle handle) {
    if (host.cfg.disable_ngs) {
        return 0;
    }

    ngs::Voice *voice = handle.get(host.mem);

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
