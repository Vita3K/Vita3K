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

#include <module/module.h>

#include <SDL3/SDL_audio.h>
#include <audio/state.h>
#include <util/tracy.h>

#define PORT_ID 0

TRACY_MODULE_NAME(SceAudioIn);

enum SceAudioInPortType {
    SCE_AUDIO_IN_PORT_TYPE_VOICE = 0,
    SCE_AUDIO_IN_PORT_TYPE_RAW = 2
};

enum SceAudioInParam {
    SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO = 0,
    SCE_AUDIO_IN_GETSTATUS_MUTE = 1
};

template <>
std::string to_debug_str<SceAudioInPortType>(const MemState &mem, SceAudioInPortType type) {
    switch (type) {
    case SceAudioInPortType::SCE_AUDIO_IN_PORT_TYPE_VOICE: return "SCE_AUDIO_IN_PORT_TYPE_VOICE"; break;
    case SceAudioInPortType::SCE_AUDIO_IN_PORT_TYPE_RAW: return "SCE_AUDIO_IN_PORT_TYPE_RAW"; break;
    default: return to_debug_str(mem, static_cast<std::underlying_type_t<SceAudioInPortType>>(type));
    }
}

template <>
std::string to_debug_str<SceAudioInParam>(const MemState &mem, SceAudioInParam type) {
    switch (type) {
    case SceAudioInParam::SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO: return "SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO"; break;
    case SceAudioInParam::SCE_AUDIO_IN_GETSTATUS_MUTE: return "SCE_AUDIO_IN_GETSTATUS_MUTE"; break;
    default: return to_debug_str(mem, static_cast<std::underlying_type_t<SceAudioInParam>>(type));
    }
}

enum SceAudioInErrorCode : uint32_t {
    //! Undefined error
    SCE_AUDIO_IN_ERROR_FATAL = 0x80260100,
    //! Bad value of port number
    SCE_AUDIO_IN_ERROR_INVALID_PORT = 0x80260101,
    //! Invalid sample length
    SCE_AUDIO_IN_ERROR_INVALID_SIZE = 0x80260102,
    //! Invalid sample frequency
    SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ = 0x80260103,
    //! Invalid port type
    SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE = 0x80260104,
    //! Invalid pointer value
    SCE_AUDIO_IN_ERROR_INVALID_POINTER = 0x80260105,
    //! Invalid port param
    SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM = 0x80260106,
    //! Cannot open no ports
    SCE_AUDIO_IN_ERROR_PORT_FULL = 0x80260107,
    //! Not enough memory
    SCE_AUDIO_IN_ERROR_OUT_OF_MEMORY = 0x80260108,
    //! Port is not opened
    SCE_AUDIO_IN_ERROR_NOT_OPENED = 0x80260109,
    //! Tried to input while busy
    SCE_AUDIO_IN_ERROR_BUSY = 0x8026010A,
    //! Invalid parameter
    SCE_AUDIO_IN_ERROR_INVALID_PARAMETER = 0x8026010B
};

EXPORT(int, sceAudioInGetAdopt, SceAudioInPortType portType) {
    TRACY_FUNC(sceAudioInGetAdopt, portType);
    if (portType != SCE_AUDIO_IN_PORT_TYPE_VOICE && portType != SCE_AUDIO_IN_PORT_TYPE_RAW) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE);
    }
    return 1;
}

EXPORT(int, sceAudioInGetInput) {
    TRACY_FUNC(sceAudioInGetInput);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInGetMicGain) {
    TRACY_FUNC(sceAudioInGetMicGain);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInGetStatus, int select) {
    TRACY_FUNC(sceAudioInGetStatus, select);
    if (select != SCE_AUDIO_IN_GETSTATUS_MUTE) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PARAMETER);
    }
    return emuenv.audio.in_port.running ? 0 : 1;
}

EXPORT(int, sceAudioInInput, int port, void *destPtr) {
    TRACY_FUNC(sceAudioInInput, port, destPtr);
    if (!emuenv.audio.in_port.running) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_NOT_OPENED);
    }
    if (port != PORT_ID) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM);
    }

    auto to_read = emuenv.audio.in_port.len_bytes;
    do {
        auto readed = SDL_GetAudioStreamData(static_cast<SDL_AudioStream *>(emuenv.audio.in_port.id), destPtr, to_read);
        if (readed < 0) {
            LOG_ERROR("AudioIn: Error while reading audio stream: {}", SDL_GetError());
            return RET_ERROR(SCE_AUDIO_IN_ERROR_FATAL);
        }
        to_read -= readed;
        destPtr = (char *)destPtr + readed;
    } while (to_read > 0);
    return 0;
}

EXPORT(int, sceAudioInInputWithInputDeviceState) {
    TRACY_FUNC(sceAudioInInputWithInputDeviceState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInOpenPort, SceAudioInPortType portType, int grain, int freq, SceAudioInParam param) {
    TRACY_FUNC(sceAudioInOpenPort, portType, grain, freq, param);
    if (emuenv.audio.in_port.running) {
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

    SDL_AudioSpec desired = {
        .format = SDL_AUDIO_S16LE,
        .channels = 1,
        .freq = freq
    };
    emuenv.audio.in_port.id = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &desired, nullptr, nullptr);
    if (!emuenv.audio.in_port.id) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_FATAL);
    }

    SDL_ResumeAudioStreamDevice(static_cast<SDL_AudioStream *>(emuenv.audio.in_port.id));

    emuenv.audio.in_port.len_bytes = grain * 2;
    emuenv.audio.in_port.running = true;
    return PORT_ID;
}

EXPORT(int, sceAudioInOpenPortForDiag) {
    TRACY_FUNC(sceAudioInOpenPortForDiag);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInReleasePort, int port) {
    TRACY_FUNC(sceAudioInReleasePort, port);
    if (port != PORT_ID) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM);
    }
    if (!emuenv.audio.in_port.running) {
        return RET_ERROR(SCE_AUDIO_IN_ERROR_NOT_OPENED);
    }
    emuenv.audio.in_port.running = false;
    SDL_DestroyAudioStream(static_cast<SDL_AudioStream *>(emuenv.audio.in_port.id));
    return 0;
}

EXPORT(int, sceAudioInSelectInput) {
    TRACY_FUNC(sceAudioInSelectInput);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInSetMicGain) {
    TRACY_FUNC(sceAudioInSetMicGain);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInSetMute) {
    TRACY_FUNC(sceAudioInSetMute);
    return UNIMPLEMENTED();
}
