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

#include <SceAudio/exports.h>

#include <util/lock_and_find.h>

#include <psp2/audioout.h>

EXPORT(int, sceAudioOutGetAdopt) {
    return unimplemented("sceAudioOutGetAdopt");
}

EXPORT(int, sceAudioOutGetConfig) {
    return unimplemented("sceAudioOutGetConfig");
}

EXPORT(int, sceAudioOutGetRestSample) {
    return unimplemented("sceAudioOutGetRestSample");
}

EXPORT(int, sceAudioOutOpenPort, SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
    assert(type == SCE_AUDIO_OUT_PORT_TYPE_MAIN);
    assert(len > 0);
    assert(freq == 48000);
    assert((mode == SCE_AUDIO_OUT_MODE_MONO) || (mode == SCE_AUDIO_OUT_MODE_STEREO));

    const int channels = (mode == SCE_AUDIO_OUT_MODE_MONO) ? 1 : 2;
    const AudioStreamPtr stream(SDL_NewAudioStream(AUDIO_S16LSB, channels, freq, host.audio.ro.spec.format, host.audio.ro.spec.channels, host.audio.ro.spec.freq), SDL_FreeAudioStream);
    if (!stream) {
        return SCE_AUDIO_OUT_ERROR_NOT_OPENED;
    }

    const AudioOutPortPtr port = std::make_shared<AudioOutPort>();
    port->ro.len_bytes = len * channels * sizeof(int16_t);
    port->callback.stream = stream;

    const std::unique_lock<std::mutex> lock(host.audio.shared.mutex);
    const int port_id = host.audio.shared.next_port_id++;
    host.audio.shared.out_ports.emplace(port_id, port);

    return port_id;
}

EXPORT(int, sceAudioOutOutput, int port, const void *buf) {
    const AudioOutPortPtr prt = lock_and_find(port, host.audio.shared.out_ports, host.audio.shared.mutex);
    if (!prt) {
        return SCE_AUDIO_OUT_ERROR_INVALID_PORT;
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!thread) {
        return SCE_AUDIO_OUT_ERROR_INVALID_PORT;
    }

    const std::unique_lock<std::mutex> lock(thread->mutex);
    assert(thread->to_do == ThreadToDo::run);
    thread->to_do = ThreadToDo::wait;
    stop(*thread->cpu);

    AudioOutput output;
    output.buf = static_cast<const uint8_t *>(buf);
    output.len_bytes = prt->ro.len_bytes;
    output.thread = thread_id;

    {
        const std::unique_lock<std::mutex> lock(prt->shared.mutex);
        prt->shared.outputs.push(output);
    }

    return 0;
}

EXPORT(int, sceAudioOutReleasePort) {
    return unimplemented("sceAudioOutReleasePort");
}

EXPORT(int, sceAudioOutSetAlcMode) {
    return unimplemented("sceAudioOutSetAlcMode");
}

EXPORT(int, sceAudioOutSetConfig) {
    return unimplemented("sceAudioOutSetConfig");
}

EXPORT(int, sceAudioOutSetVolume) {
    return unimplemented("sceAudioOutSetVolume");
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
