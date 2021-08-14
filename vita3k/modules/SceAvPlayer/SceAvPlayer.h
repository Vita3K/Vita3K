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

BRIDGE_DECL(sceAvPlayerAddSource)
BRIDGE_DECL(sceAvPlayerClose)
BRIDGE_DECL(sceAvPlayerCurrentTime)
BRIDGE_DECL(sceAvPlayerDisableStream)
BRIDGE_DECL(sceAvPlayerEnableStream)
BRIDGE_DECL(sceAvPlayerGetAudioData)
BRIDGE_DECL(sceAvPlayerGetStreamInfo)
BRIDGE_DECL(sceAvPlayerGetVideoData)
BRIDGE_DECL(sceAvPlayerGetVideoDataEx)
BRIDGE_DECL(sceAvPlayerInit)
BRIDGE_DECL(sceAvPlayerIsActive)
BRIDGE_DECL(sceAvPlayerJumpToTime)
BRIDGE_DECL(sceAvPlayerPause)
BRIDGE_DECL(sceAvPlayerPostInit)
BRIDGE_DECL(sceAvPlayerResume)
BRIDGE_DECL(sceAvPlayerSetLooping)
BRIDGE_DECL(sceAvPlayerSetTrickSpeed)
BRIDGE_DECL(sceAvPlayerStart)
BRIDGE_DECL(sceAvPlayerStop)
BRIDGE_DECL(sceAvPlayerStreamCount)
