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

#pragma once

#include <module/module.h>

BRIDGE_DECL(sceNgsModuleCheckParamsInRangeInternal)
BRIDGE_DECL(sceNgsModuleGetNumPresetsInternal)
BRIDGE_DECL(sceNgsModuleGetPresetInternal)
BRIDGE_DECL(sceNgsPatchCreateRoutingInternal)
BRIDGE_DECL(sceNgsPatchRemoveRoutingInternal)
BRIDGE_DECL(sceNgsRackGetRequiredMemorySizeInternal)
BRIDGE_DECL(sceNgsRackGetVoiceHandleInternal6)
BRIDGE_DECL(sceNgsRackInitInternal)
BRIDGE_DECL(sceNgsRackReleaseInternal)
BRIDGE_DECL(sceNgsRackSetParamErrorCallbackInternal)
BRIDGE_DECL(sceNgsSulphaGetInfoInternal)
BRIDGE_DECL(sceNgsSulphaGetModuleListInternal)
BRIDGE_DECL(sceNgsSulphaGetSynthUpdateCallbackInternal)
BRIDGE_DECL(sceNgsSulphaQueryModuleInternal)
BRIDGE_DECL(sceNgsSulphaSetSynthUpdateCallbackInternal)
BRIDGE_DECL(sceNgsSystemGetCallbackListInternal)
BRIDGE_DECL(sceNgsSystemGetRequiredMemorySizeInternal)
BRIDGE_DECL(sceNgsSystemGetSysHandleFromRack)
BRIDGE_DECL(sceNgsSystemInitInternal)
BRIDGE_DECL(sceNgsSystemIsFixForBugzilla89940)
BRIDGE_DECL(sceNgsSystemLockInternal)
BRIDGE_DECL(sceNgsSystemPullDataInternal)
BRIDGE_DECL(sceNgsSystemPushDataInternal)
BRIDGE_DECL(sceNgsSystemReleaseInternal)
BRIDGE_DECL(sceNgsSystemSetFlagsInternal)
BRIDGE_DECL(sceNgsSystemSetParamErrorCallbackInternal)
BRIDGE_DECL(sceNgsSystemUnlockInternal)
BRIDGE_DECL(sceNgsSystemUpdateInternal)
BRIDGE_DECL(sceNgsVoiceBypassModuleInternal)
BRIDGE_DECL(sceNgsVoiceClearDirtyFlagInternal)
BRIDGE_DECL(sceNgsVoiceDefGetAtrac9VoiceInternal)
BRIDGE_DECL(sceNgsVoiceDefGetCompressorBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetCompressorSideChainBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetDelayBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetDistortionBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetEnvelopeBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetEqBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetMasterBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetMixerBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetPauserBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetPitchshiftBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetReverbBussInternal)
BRIDGE_DECL(sceNgsVoiceDefGetSasEmuVoiceInternal)
BRIDGE_DECL(sceNgsVoiceDefGetScreamVoiceAT9Internal)
BRIDGE_DECL(sceNgsVoiceDefGetScreamVoiceInternal)
BRIDGE_DECL(sceNgsVoiceDefGetSimpleAtrac9VoiceInternal)
BRIDGE_DECL(sceNgsVoiceDefGetSimpleVoiceInternal)
BRIDGE_DECL(sceNgsVoiceDefGetTemplate1Internal)
BRIDGE_DECL(sceNgsVoiceDefinitionGetPresetInternal)
BRIDGE_DECL(sceNgsVoiceGetModuleBypassInternal)
BRIDGE_DECL(sceNgsVoiceGetOutputPatchInternal)
BRIDGE_DECL(sceNgsVoiceGetParamsOutOfRangeBufferedInternal)
BRIDGE_DECL(sceNgsVoiceInitInternal)
BRIDGE_DECL(sceNgsVoiceKeyOffInternal)
BRIDGE_DECL(sceNgsVoiceKillInternal)
BRIDGE_DECL(sceNgsVoicePauseInternal)
BRIDGE_DECL(sceNgsVoicePlayInternal)
BRIDGE_DECL(sceNgsVoiceResumeInternal)
BRIDGE_DECL(sceNgsVoiceSetAllBypassesInternal)
BRIDGE_DECL(sceNgsVoiceSetFinishedCallbackInternal)
BRIDGE_DECL(sceNgsVoiceSetModuleCallbackInternal)
BRIDGE_DECL(sceNgsVoiceSetPresetInternal)
