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

// SceAudiodecUser
BRIDGE_DECL(sceAudiodecClearContext)
BRIDGE_DECL(sceAudiodecCreateDecoder)
BRIDGE_DECL(sceAudiodecCreateDecoderExternal)
BRIDGE_DECL(sceAudiodecCreateDecoderResident)
BRIDGE_DECL(sceAudiodecDecode)
BRIDGE_DECL(sceAudiodecDecodeNFrames)
BRIDGE_DECL(sceAudiodecDecodeNStreams)
BRIDGE_DECL(sceAudiodecDeleteDecoder)
BRIDGE_DECL(sceAudiodecDeleteDecoderExternal)
BRIDGE_DECL(sceAudiodecDeleteDecoderResident)
BRIDGE_DECL(sceAudiodecGetContextSize)
BRIDGE_DECL(sceAudiodecGetInternalError)
BRIDGE_DECL(sceAudiodecInitLibrary)
BRIDGE_DECL(sceAudiodecPartlyDecode)
BRIDGE_DECL(sceAudiodecTermLibrary)
