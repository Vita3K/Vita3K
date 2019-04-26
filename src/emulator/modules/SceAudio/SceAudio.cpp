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

EXPORT(int, sceAudioOutGetRestSample) {
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
    port->callback.stream = stream;

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

    const std::lock_guard<std::mutex> lock(thread->mutex);
    if (thread->to_do != ThreadToDo::run)
        return 0;
    thread->to_do = ThreadToDo::wait;
    stop(*thread->cpu);

    AudioOutput output;
    output.buf = static_cast<const uint8_t *>(buf);
    output.len_bytes = prt->ro.len_bytes;
    output.thread = thread_id;

    {
        const std::lock_guard<std::mutex> lock(prt->shared.mutex);
        prt->shared.outputs.push(output);
    }

    return 0;
}

EXPORT(int, sceAudioOutReleasePort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAlcMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetConfig) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetVolume) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceAudioOutGetAdopt)
BRIDGE_IMPL(sceAudioOutGetConfig)
BRIDGE_IMPL(sceAudioOutGetRestSample)
BRIDGE_IMPL(sceAudioOutOpenPort)
BRIDGE_IMPL(sceAudioOutOutput)
BRIDGE_IMPL(sceAudioOutReleasePort)
BRIDGE_IMPL(sceAudioOutSetAlcMode)
BRIDGE_IMPL(sceAudioOutSetConfig)
BRIDGE_IMPL(sceAudioOutSetVolume)
