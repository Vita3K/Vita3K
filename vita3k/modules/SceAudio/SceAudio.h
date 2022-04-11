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

enum SceAudioOutMode {
    SCE_AUDIO_OUT_MODE_MONO = 0,
    SCE_AUDIO_OUT_MODE_STEREO = 1
};

enum SceAudioOutAlcMode {
    SCE_AUDIO_ALC_OFF,
    SCE_AUDIO_ALC_MODE1,
    SCE_AUDIO_ALC_MODE_MAX
};

enum SceAudioOutPortType {
    //! Used for main audio output, freq must be set to 48000 Hz
    SCE_AUDIO_OUT_PORT_TYPE_MAIN = 0,
    //! Used for Background Music port
    SCE_AUDIO_OUT_PORT_TYPE_BGM = 1,
    //! Used for voice chat port
    SCE_AUDIO_OUT_PORT_TYPE_VOICE = 2
};

enum SceAudioOutErrorCode {
    SCE_AUDIO_OUT_ERROR_NOT_OPENED = 0x80260001,
    SCE_AUDIO_OUT_ERROR_BUSY = 0x80260002,
    SCE_AUDIO_OUT_ERROR_INVALID_PORT = 0x80260003,
    SCE_AUDIO_OUT_ERROR_INVALID_POINTER = 0x80260004,
    SCE_AUDIO_OUT_ERROR_PORT_FULL = 0x80260005,
    SCE_AUDIO_OUT_ERROR_INVALID_SIZE = 0x80260006,
    SCE_AUDIO_OUT_ERROR_INVALID_FORMAT = 0x80260007,
    SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ = 0x80260008,
    SCE_AUDIO_OUT_ERROR_INVALID_VOLUME = 0x80260009,
    SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE = 0x8026000A,
    SCE_AUDIO_OUT_ERROR_INVALID_FX_TYPE = 0x8026000B,
    SCE_AUDIO_OUT_ERROR_INVALID_CONF_TYPE = 0x8026000C,
    SCE_AUDIO_OUT_ERROR_OUT_OF_MEMORY = 0x8026000D
};

enum SceAudioOutChannelFlag {
    SCE_AUDIO_VOLUME_FLAG_L_CH = 1, //!< Left Channel
    SCE_AUDIO_VOLUME_FLAG_R_CH = 2 //!< Right Channel
};

enum SceAudioOutConfigType {
    SCE_AUDIO_OUT_CONFIG_TYPE_LEN,
    SCE_AUDIO_OUT_CONFIG_TYPE_FREQ,
    SCE_AUDIO_OUT_CONFIG_TYPE_MODE
};

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
