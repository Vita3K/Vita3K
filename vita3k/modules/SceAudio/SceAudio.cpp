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

#include "SceAudio.h"

#include <util/lock_and_find.h>

#include <psp2/audioout.h>

EXPORT(int, sceAudioOutGetAdopt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutGetConfig) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutGetPortVolume_forUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutOpenPort, SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
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
    const AudioStreamPtr stream(SDL_NewAudioStream(AUDIO_S16LSB, channels, freq, host.audio.ro.spec.format, host.audio.ro.spec.channels, host.audio.ro.spec.freq), SDL_FreeAudioStream);
    if (!stream) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_NOT_OPENED);
    }

    const AudioOutPortPtr port = std::make_shared<AudioOutPort>();
    port->ro.len_bytes = len * channels * sizeof(int16_t);
    port->shared.stream = stream;

    const std::lock_guard<std::mutex> lock(host.audio.shared.mutex);
    const int port_id = host.audio.shared.next_port_id++;
    host.audio.shared.out_ports.emplace(port_id, port);

    return port_id;
}

EXPORT(int, sceAudioOutOutput, int port, const void *buf) {
    const AudioOutPortPtr prt = lock_and_find(port, host.audio.shared.out_ports, host.audio.shared.mutex);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    // Put audio to the port's stream and see how much is left to play.
    std::unique_lock<std::mutex> lock(prt->shared.mutex);
    SDL_AudioStreamPut(prt->shared.stream.get(), buf, prt->ro.len_bytes);
    const int available = SDL_AudioStreamAvailable(prt->shared.stream.get());

    // If there's lots of audio left to play, stop this thread.
    // The audio callback will wake it up later when it's running out of data.
    if (available > host.audio.ro.spec.size) {
        prt->shared.thread = thread_id;
        lock.unlock();

        std::unique_lock<std::mutex> mlock(thread->mutex);
        if (thread->to_do != ThreadToDo::run)
            return 0;
        thread->to_do = ThreadToDo::wait;
        thread->something_to_do.wait(mlock);
    }

    return 0;
}

EXPORT(int, sceAudioOutGetRestSample) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutOpenExtPort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutReleasePort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAdoptMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAdopt_forUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAlcMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetCompress) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetConfig) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetEffectType) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetPortVolume_forUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetVolume) {
    return UNIMPLEMENTED();
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
