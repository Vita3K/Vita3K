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

#include "SceAudio.h"

#include <audio/state.h>
#include <kernel/state.h>
#include <util/lock_and_find.h>
#include <util/tracy.h>

TRACY_MODULE_NAME(SceAudio);

template <>
std::string to_debug_str<SceAudioOutPortType>(const MemState &mem, SceAudioOutPortType type) {
    switch (type) {
    case SCE_AUDIO_OUT_PORT_TYPE_MAIN:
        return "SCE_AUDIO_OUT_PORT_TYPE_MAIN";
    case SCE_AUDIO_OUT_PORT_TYPE_BGM:
        return "SCE_AUDIO_OUT_PORT_TYPE_BGM";
    case SCE_AUDIO_OUT_PORT_TYPE_VOICE:
        return "SCE_AUDIO_OUT_PORT_TYPE_VOICE";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutConfigType>(const MemState &mem, SceAudioOutConfigType type) {
    switch (type) {
    case SCE_AUDIO_OUT_CONFIG_TYPE_LEN:
        return "SCE_AUDIO_OUT_CONFIG_TYPE_LEN";
    case SCE_AUDIO_OUT_CONFIG_TYPE_FREQ:
        return "SCE_AUDIO_OUT_CONFIG_TYPE_FREQ";
    case SCE_AUDIO_OUT_CONFIG_TYPE_MODE:
        return "SCE_AUDIO_OUT_CONFIG_TYPE_MODE";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutMode>(const MemState &mem, SceAudioOutMode type) {
    switch (type) {
    case SCE_AUDIO_OUT_MODE_MONO:
        return "SCE_AUDIO_OUT_MODE_MONO";
    case SCE_AUDIO_OUT_MODE_STEREO:
        return "SCE_AUDIO_OUT_MODE_STEREO";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutAlcMode>(const MemState &mem, SceAudioOutAlcMode type) {
    switch (type) {
    case SCE_AUDIO_ALC_OFF:
        return "SCE_AUDIO_ALC_OFF";
    case SCE_AUDIO_ALC_MODE1:
        return "SCE_AUDIO_ALC_MODE1";
    case SCE_AUDIO_ALC_MODE_MAX:
        return "SCE_AUDIO_ALC_MODE_MAX";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutChannelFlag>(const MemState &mem, SceAudioOutChannelFlag type) {
    switch (type) {
    case SCE_AUDIO_VOLUME_FLAG_L_CH:
        return "SCE_AUDIO_VOLUME_FLAG_L_CH";
    case SCE_AUDIO_VOLUME_FLAG_R_CH:
        return "SCE_AUDIO_VOLUME_FLAG_R_CH";
        // Third case for special flag SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH
        // passed by games to set the volume of both channels at the same time
    case (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH):
        return "SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH";
    }
    return std::to_string(type);
}

EXPORT(int, sceAudioOutGetAdopt, SceAudioOutPortType type) {
    TRACY_FUNC(sceAudioOutGetAdopt, type);
    // this is used in case the vita is using voice chat, not useful for us
    // all types of audio ports are always enabled
    return 1;
}

EXPORT(int, sceAudioOutGetConfig, int port, SceAudioOutConfigType type) {
    TRACY_FUNC(sceAudioOutGetConfig, type);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutGetPortVolume_forUser) {
    TRACY_FUNC(sceAudioOutGetPortVolume_forUser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutOpenPort, SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
    TRACY_FUNC(sceAudioOutOpenPort, type, len, freq, mode);
    if (type < SCE_AUDIO_OUT_PORT_TYPE_MAIN || type > SCE_AUDIO_OUT_PORT_TYPE_VOICE) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE);
    }
    if (type == SCE_AUDIO_OUT_PORT_TYPE_MAIN && freq != 48000) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ);
    }
    if ((mode != SCE_AUDIO_OUT_MODE_MONO) && (mode != SCE_AUDIO_OUT_MODE_STEREO)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_FORMAT);
    }
    if (len <= 0) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SIZE);
    }

    const int channels = (mode == SCE_AUDIO_OUT_MODE_MONO) ? 1 : 2;
    const AudioStreamPtr stream(SDL_NewAudioStream(AUDIO_S16LSB, channels, freq, emuenv.audio.ro.spec.format, emuenv.audio.ro.spec.channels, emuenv.audio.ro.spec.freq), SDL_FreeAudioStream);
    if (!stream) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_NOT_OPENED);
    }

    const AudioOutPortPtr port = std::make_shared<AudioOutPort>();
    port->ro.len_bytes = len * channels * sizeof(int16_t);
    port->shared.stream = stream;

    const std::lock_guard<std::mutex> lock(emuenv.audio.shared.mutex);
    const int port_id = emuenv.audio.shared.next_port_id++;
    emuenv.audio.shared.out_ports.emplace(port_id, port);

    return port_id;
}

EXPORT(int, sceAudioOutOutput, int port, const void *buf) {
    TRACY_FUNC(sceAudioOutOutput, port, buf);
    const AudioOutPortPtr prt = lock_and_find(port, emuenv.audio.shared.out_ports, emuenv.audio.shared.mutex);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    // Put audio to the port's stream and see how much is left to play.
    std::unique_lock<std::mutex> lock(prt->shared.mutex);
    SDL_AudioStreamPut(prt->shared.stream.get(), buf, prt->ro.len_bytes);
    const int available = SDL_AudioStreamAvailable(prt->shared.stream.get());

    // If there's lots of audio left to play, stop this thread.
    // The audio callback will wake it up later when it's running out of data.
    // the 3*emuenv.audio.ro.spec.size is needed for some games with an 480 host audiobuffer
    // sample size (what SDL audio gives us) to make sure this does not happen
    // we are supposed to wait for the existing samples to be processed (except the ones just passed)
    // but this would give a bad audio because the host buffer size is different compared to the guest buffer size
    // so we need to cache more data to make sure we always have enough
    if (available >= 3 * emuenv.audio.ro.spec.size) {
        prt->shared.thread = thread_id;

        std::unique_lock<std::mutex> mlock(thread->mutex);
        thread->update_status(ThreadStatus::wait);
        lock.unlock();
        thread->status_cond.wait(mlock, [&]() { return thread->status == ThreadStatus::run; });
    }

    return 0;
}

EXPORT(int, sceAudioOutGetRestSample, int port) {
    TRACY_FUNC(sceAudioOutGetRestSample, port);
    const AudioOutPortPtr prt = lock_and_find(port, emuenv.audio.shared.out_ports, emuenv.audio.shared.mutex);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    const int bytes_available = SDL_AudioStreamAvailable(prt->shared.stream.get());
    assert(emuenv.audio.ro.spec.format == AUDIO_S16LSB);

    // we have the number of bytes left, we can convert it back to the number of samples left
    return bytes_available / (emuenv.audio.ro.spec.channels * sizeof(int16_t));
}

EXPORT(int, sceAudioOutOpenExtPort) {
    TRACY_FUNC(sceAudioOutOpenExtPort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutReleasePort, int port) {
    TRACY_FUNC(sceAudioOutReleasePort, port);
    const std::lock_guard<std::mutex> guard(emuenv.audio.shared.mutex);
    if (!emuenv.audio.shared.out_ports.erase(port)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    return 0;
}

EXPORT(int, sceAudioOutSetAdoptMode) {
    TRACY_FUNC(sceAudioOutSetAdoptMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAdopt_forUser) {
    TRACY_FUNC(sceAudioOutSetAdopt_forUser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAlcMode, SceAudioOutAlcMode mode) {
    TRACY_FUNC(sceAudioOutSetAlcMode, mode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetCompress) {
    TRACY_FUNC(sceAudioOutSetCompress);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetConfig, int port, SceSize len, int freq, SceAudioOutMode mode) {
    TRACY_FUNC(sceAudioOutSetConfig, port, len, freq, mode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetEffectType) {
    TRACY_FUNC(sceAudioOutSetEffectType);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetPortVolume_forUser) {
    TRACY_FUNC(sceAudioOutSetPortVolume_forUser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetVolume, int port, SceAudioOutChannelFlag ch, int *vol) {
    TRACY_FUNC(sceAudioOutSetVolume, port, ch, vol);
    if (!ch) // no channel selected, no changes
        return 0;

    const AudioOutPortPtr prt = lock_and_find(port, emuenv.audio.shared.out_ports, emuenv.audio.shared.mutex);

    if (!prt)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);

    // Unsure of what happens if only one channel is selected, this will break if program passes a size 1 int array
    const int left = (ch & SCE_AUDIO_VOLUME_FLAG_L_CH) ? vol[0] : prt->left_channel_volume;
    const int right = (ch & SCE_AUDIO_VOLUME_FLAG_R_CH) ? vol[1] : prt->right_channel_volume;
    const float volume_level = (static_cast<float>(left + right) / static_cast<float>(SCE_AUDIO_VOLUME_0DB * 2));
    const int volume = static_cast<int>(SDL_MIX_MAXVOLUME * volume_level);

    prt->volume = volume;
    // then update channel volumes in case there was a change
    prt->left_channel_volume = left;
    prt->right_channel_volume = right;

    return 0;
}

BRIDGE_IMPL(sceAudioOutGetAdopt)
BRIDGE_IMPL(sceAudioOutGetConfig)
BRIDGE_IMPL(sceAudioOutGetPortVolume_forUser)
BRIDGE_IMPL(sceAudioOutGetRestSample)
BRIDGE_IMPL(sceAudioOutOpenExtPort)
BRIDGE_IMPL(sceAudioOutOpenPort)
BRIDGE_IMPL(sceAudioOutOutput)
BRIDGE_IMPL(sceAudioOutReleasePort)
BRIDGE_IMPL(sceAudioOutSetAdoptMode)
BRIDGE_IMPL(sceAudioOutSetAdopt_forUser)
BRIDGE_IMPL(sceAudioOutSetAlcMode)
BRIDGE_IMPL(sceAudioOutSetCompress)
BRIDGE_IMPL(sceAudioOutSetConfig)
BRIDGE_IMPL(sceAudioOutSetEffectType)
BRIDGE_IMPL(sceAudioOutSetPortVolume_forUser)
BRIDGE_IMPL(sceAudioOutSetVolume)
