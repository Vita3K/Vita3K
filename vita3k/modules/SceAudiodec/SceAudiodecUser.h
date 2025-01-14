// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

DECL_EXPORT(SceInt32, sceAudiodecInitLibrary, SceAudiodecCodec codecType, SceAudiodecInitParam *pInitParam);
DECL_EXPORT(SceInt32, sceAudiodecTermLibrary, SceAudiodecCodec codecType);
