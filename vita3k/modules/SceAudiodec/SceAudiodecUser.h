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

enum SceAudiodecCodec : uint32_t {
    SCE_AUDIODEC_TYPE_AT9 = 0x1003,
    SCE_AUDIODEC_TYPE_MP3 = 0x1004,
    SCE_AUDIODEC_TYPE_AAC = 0x1005,
    SCE_AUDIODEC_TYPE_CELP = 0x1006,
};

struct SceAudiodecInitStreamParam {
    SceUInt32 size;
    SceUInt32 totalStreams;
};

struct SceAudiodecInitChParam {
    SceUInt32 size;
    SceUInt32 totalCh;
};

union SceAudiodecInitParam {
    SceUInt32 size;
    SceAudiodecInitChParam at9;
    SceAudiodecInitStreamParam mp3;
    SceAudiodecInitStreamParam aac;
    SceAudiodecInitStreamParam celp;
};

EXPORT(SceInt32, sceAudiodecInitLibrary, SceUInt32 codecType, SceAudiodecInitParam *pInitParam);
EXPORT(SceInt32, sceAudiodecTermLibrary, SceUInt32 codecType);

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
