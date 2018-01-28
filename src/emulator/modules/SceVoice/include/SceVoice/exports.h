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

// SceVoice
BRIDGE_DECL(sceVoiceCheckTopology)
BRIDGE_DECL(sceVoiceConnectIPortToOPort)
BRIDGE_DECL(sceVoiceCreatePort)
BRIDGE_DECL(sceVoiceDeletePort)
BRIDGE_DECL(sceVoiceDisconnectIPortFromOPort)
BRIDGE_DECL(sceVoiceEnd)
BRIDGE_DECL(sceVoiceGetBitRate)
BRIDGE_DECL(sceVoiceGetMuteFlag)
BRIDGE_DECL(sceVoiceGetPortAttr)
BRIDGE_DECL(sceVoiceGetPortInfo)
BRIDGE_DECL(sceVoiceGetResourceInfo)
BRIDGE_DECL(sceVoiceGetVolume)
BRIDGE_DECL(sceVoiceInit)
BRIDGE_DECL(sceVoicePausePort)
BRIDGE_DECL(sceVoicePausePortAll)
BRIDGE_DECL(sceVoiceReadFromOPort)
BRIDGE_DECL(sceVoiceResetPort)
BRIDGE_DECL(sceVoiceResumePort)
BRIDGE_DECL(sceVoiceResumePortAll)
BRIDGE_DECL(sceVoiceSetBitRate)
BRIDGE_DECL(sceVoiceSetMuteFlag)
BRIDGE_DECL(sceVoiceSetMuteFlagAll)
BRIDGE_DECL(sceVoiceSetPortAttr)
BRIDGE_DECL(sceVoiceSetVolume)
BRIDGE_DECL(sceVoiceStart)
BRIDGE_DECL(sceVoiceStop)
BRIDGE_DECL(sceVoiceUpdatePort)
BRIDGE_DECL(sceVoiceWriteToIPort)
