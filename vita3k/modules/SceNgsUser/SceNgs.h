// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <module/module.h>

BRIDGE_DECL(sceNgsAT9GetSectionDetails)
BRIDGE_DECL(sceNgsModuleGetNumPresets)
BRIDGE_DECL(sceNgsModuleGetPreset)
BRIDGE_DECL(sceNgsPatchCreateRouting)
BRIDGE_DECL(sceNgsPatchGetInfo)
BRIDGE_DECL(sceNgsPatchRemoveRouting)
BRIDGE_DECL(sceNgsRackGetRequiredMemorySize)
BRIDGE_DECL(sceNgsRackGetVoiceHandle)
BRIDGE_DECL(sceNgsRackInit)
BRIDGE_DECL(sceNgsRackRelease)
BRIDGE_DECL(sceNgsRackSetParamErrorCallback)
BRIDGE_DECL(sceNgsSystemGetRequiredMemorySize)
BRIDGE_DECL(sceNgsSystemInit)
BRIDGE_DECL(sceNgsSystemLock)
BRIDGE_DECL(sceNgsSystemRelease)
BRIDGE_DECL(sceNgsSystemSetFlags)
BRIDGE_DECL(sceNgsSystemSetParamErrorCallback)
BRIDGE_DECL(sceNgsSystemUnlock)
BRIDGE_DECL(sceNgsSystemUpdate)
BRIDGE_DECL(sceNgsVoiceBypassModule)
BRIDGE_DECL(sceNgsVoiceDefGetAtrac9Voice)
BRIDGE_DECL(sceNgsVoiceDefGetCompressorBuss)
BRIDGE_DECL(sceNgsVoiceDefGetCompressorSideChainBuss)
BRIDGE_DECL(sceNgsVoiceDefGetDelayBuss)
BRIDGE_DECL(sceNgsVoiceDefGetDistortionBuss)
BRIDGE_DECL(sceNgsVoiceDefGetEnvelopeBuss)
BRIDGE_DECL(sceNgsVoiceDefGetEqBuss)
BRIDGE_DECL(sceNgsVoiceDefGetMasterBuss)
BRIDGE_DECL(sceNgsVoiceDefGetMixerBuss)
BRIDGE_DECL(sceNgsVoiceDefGetPauserBuss)
BRIDGE_DECL(sceNgsVoiceDefGetPitchShiftBuss)
BRIDGE_DECL(sceNgsVoiceDefGetReverbBuss)
BRIDGE_DECL(sceNgsVoiceDefGetSasEmuVoice)
BRIDGE_DECL(sceNgsVoiceDefGetScreamAtrac9Voice)
BRIDGE_DECL(sceNgsVoiceDefGetScreamVoice)
BRIDGE_DECL(sceNgsVoiceDefGetSimpleAtrac9Voice)
BRIDGE_DECL(sceNgsVoiceDefGetSimpleVoice)
BRIDGE_DECL(sceNgsVoiceDefGetTemplate1)
BRIDGE_DECL(sceNgsVoiceGetInfo)
BRIDGE_DECL(sceNgsVoiceGetModuleBypass)
BRIDGE_DECL(sceNgsVoiceGetModuleType)
BRIDGE_DECL(sceNgsVoiceGetOutputPatch)
BRIDGE_DECL(sceNgsVoiceGetParamsOutOfRange)
BRIDGE_DECL(sceNgsVoiceGetStateData)
BRIDGE_DECL(sceNgsVoiceInit)
BRIDGE_DECL(sceNgsVoiceKeyOff)
BRIDGE_DECL(sceNgsVoiceKill)
BRIDGE_DECL(sceNgsVoiceLockParams)
BRIDGE_DECL(sceNgsVoicePatchSetVolume)
BRIDGE_DECL(sceNgsVoicePatchSetVolumes)
BRIDGE_DECL(sceNgsVoicePatchSetVolumesMatrix)
BRIDGE_DECL(sceNgsVoicePause)
BRIDGE_DECL(sceNgsVoicePlay)
BRIDGE_DECL(sceNgsVoiceResume)
BRIDGE_DECL(sceNgsVoiceSetFinishedCallback)
BRIDGE_DECL(sceNgsVoiceSetModuleCallback)
BRIDGE_DECL(sceNgsVoiceSetParamsBlock)
BRIDGE_DECL(sceNgsVoiceSetPreset)
BRIDGE_DECL(sceNgsVoiceUnlockParams)
BRIDGE_DECL(sceSulphaNgsGetDefaultConfig)
BRIDGE_DECL(sceSulphaNgsGetNeededMemory)
BRIDGE_DECL(sceSulphaNgsInit)
BRIDGE_DECL(sceSulphaNgsSetRackName)
BRIDGE_DECL(sceSulphaNgsSetSampleName)
BRIDGE_DECL(sceSulphaNgsSetSynthName)
BRIDGE_DECL(sceSulphaNgsSetVoiceName)
BRIDGE_DECL(sceSulphaNgsShutdown)
BRIDGE_DECL(sceSulphaNgsTrace)
