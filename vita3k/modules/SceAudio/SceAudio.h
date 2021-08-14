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

BRIDGE_DECL(sceAudioOutGetAdopt)
BRIDGE_DECL(sceAudioOutGetConfig)
BRIDGE_DECL(sceAudioOutGetPortVolume_forUser)
BRIDGE_DECL(sceAudioOutGetRestSample)
BRIDGE_DECL(sceAudioOutOpenExtPort)
BRIDGE_DECL(sceAudioOutOpenPort)
BRIDGE_DECL(sceAudioOutOutput)
BRIDGE_DECL(sceAudioOutReleasePort)
BRIDGE_DECL(sceAudioOutSetAdoptMode)
BRIDGE_DECL(sceAudioOutSetAdopt_forUser)
BRIDGE_DECL(sceAudioOutSetAlcMode)
BRIDGE_DECL(sceAudioOutSetCompress)
BRIDGE_DECL(sceAudioOutSetConfig)
BRIDGE_DECL(sceAudioOutSetEffectType)
BRIDGE_DECL(sceAudioOutSetPortVolume_forUser)
BRIDGE_DECL(sceAudioOutSetVolume)
