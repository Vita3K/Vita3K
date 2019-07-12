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

#include "SceAudioIn.h"

#include <util/lock_and_find.h>

#include <psp2/audioin.h>

#define PORT_ID 0

EXPORT(int, sceAudioInGetAdopt, SceAudioInPortType portType) {
    if (portType != SCE_AUDIO_IN_PORT_TYPE_VOICE && portType != SCE_AUDIO_IN_PORT_TYPE_RAW) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE);
    }
    return 1;
}

EXPORT(int, sceAudioInGetInput) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInGetMicGain) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInGetStatus, int select) {
    if (select != SCE_AUDIO_IN_GETSTATUS_MUTE) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PARAMETER);
    }
    return host.audio.shared.in_port.running ? 0 : 1;
}

EXPORT(int, sceAudioInInput, int port, void *destPtr) {
    if (!host.audio.shared.in_port.running) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_NOT_OPENED);
    }
    if (port != PORT_ID) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM);
    }

    while (SDL_DequeueAudio(host.audio.shared.in_port.id, destPtr, host.audio.shared.in_port.len_bytes) > 0){}
    return 0;
}

EXPORT(int, sceAudioInInputWithInputDeviceState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInOpenPort, SceAudioInPortType portType, int grain, int freq, SceAudioInParam param) {
    if (host.audio.shared.in_port.running) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_PORT_FULL);
    }
    if (param != SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM);
    }
    if (portType != SCE_AUDIO_IN_PORT_TYPE_VOICE && portType != SCE_AUDIO_IN_PORT_TYPE_RAW) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE);
    }
    if (portType == SCE_AUDIO_IN_PORT_TYPE_VOICE) {
        if (freq != 16000) {
            return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ);
        }
        if (grain != 256 && grain != 512) {
            return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PARAMETER);
        }
    }
    if (portType == SCE_AUDIO_IN_PORT_TYPE_RAW) {
        if (freq != 16000 && freq != 48000) {
            return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ);
        }
        if ((grain != 256 && freq == 16000) || (grain != 768 && freq == 48000)) {
            return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PARAMETER);
        }
    }

    SDL_AudioSpec desired = {};
    SDL_AudioSpec received = {};
    desired.freq = freq;
    desired.format = AUDIO_S16LSB;
    desired.channels = 1;
    desired.samples = grain;
    desired.callback = nullptr;
    desired.userdata = nullptr;

    host.audio.shared.in_port.id = SDL_OpenAudioDevice(nullptr, 1, &desired, &received, 0);
    if (host.audio.shared.in_port.id == 0) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_FATAL);
    }

    SDL_PauseAudioDevice(host.audio.shared.in_port.id, 0);

    host.audio.shared.in_port.len_bytes = grain * 2;
    host.audio.shared.in_port.running = true;
    return PORT_ID;
}

EXPORT(int, sceAudioInOpenPortForDiag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInReleasePort, int port) {
    if (port != PORT_ID) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM);
    }
    if (!host.audio.shared.in_port.running) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_NOT_OPENED);
    }
    host.audio.shared.in_port.running = false;
    SDL_PauseAudioDevice(host.audio.shared.in_port.id, 1);
    SDL_CloseAudioDevice(host.audio.shared.in_port.id);
    return 0;
}

EXPORT(int, sceAudioInSelectInput) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInSetMicGain) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInSetMute) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceAudioInGetAdopt)
BRIDGE_IMPL(sceAudioInGetInput)
BRIDGE_IMPL(sceAudioInGetMicGain)
BRIDGE_IMPL(sceAudioInGetStatus)
BRIDGE_IMPL(sceAudioInInput)
BRIDGE_IMPL(sceAudioInInputWithInputDeviceState)
BRIDGE_IMPL(sceAudioInOpenPort)
BRIDGE_IMPL(sceAudioInOpenPortForDiag)
BRIDGE_IMPL(sceAudioInReleasePort)
BRIDGE_IMPL(sceAudioInSelectInput)
BRIDGE_IMPL(sceAudioInSetMicGain)
BRIDGE_IMPL(sceAudioInSetMute)
