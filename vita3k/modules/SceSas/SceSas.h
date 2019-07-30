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

BRIDGE_DECL(sceSasCore)
BRIDGE_DECL(sceSasCoreWithMix)
BRIDGE_DECL(sceSasExit)
BRIDGE_DECL(sceSasGetDryPeak)
BRIDGE_DECL(sceSasGetEndState)
BRIDGE_DECL(sceSasGetEnvelope)
BRIDGE_DECL(sceSasGetGrain)
BRIDGE_DECL(sceSasGetNeededMemorySize)
BRIDGE_DECL(sceSasGetOutputmode)
BRIDGE_DECL(sceSasGetPauseState)
BRIDGE_DECL(sceSasGetPreMasterPeak)
BRIDGE_DECL(sceSasGetWetPeak)
BRIDGE_DECL(sceSasInit)
BRIDGE_DECL(sceSasInitWithGrain)
BRIDGE_DECL(sceSasSetADSR)
BRIDGE_DECL(sceSasSetADSRmode)
BRIDGE_DECL(sceSasSetDistortion)
BRIDGE_DECL(sceSasSetEffect)
BRIDGE_DECL(sceSasSetEffectParam)
BRIDGE_DECL(sceSasSetEffectType)
BRIDGE_DECL(sceSasSetEffectVolume)
BRIDGE_DECL(sceSasSetGrain)
BRIDGE_DECL(sceSasSetKeyOff)
BRIDGE_DECL(sceSasSetKeyOn)
BRIDGE_DECL(sceSasSetNoise)
BRIDGE_DECL(sceSasSetOutputmode)
BRIDGE_DECL(sceSasSetPause)
BRIDGE_DECL(sceSasSetPitch)
BRIDGE_DECL(sceSasSetSL)
BRIDGE_DECL(sceSasSetSimpleADSR)
BRIDGE_DECL(sceSasSetVoice)
BRIDGE_DECL(sceSasSetVoicePCM)
BRIDGE_DECL(sceSasSetVolume)
