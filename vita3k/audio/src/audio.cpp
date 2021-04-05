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

#include <audio/functions.h>
#include <audio/state.h>

#include <microprofile.h>

#include <util/log.h>

#include <algorithm>
#include <cassert>
#include <cstring>

#define AUDIO_PROFILE(name) MICROPROFILE_SCOPEI("Audio", name, MP_THISTLE)

static void mix_out_port(uint8_t *stream, uint8_t *temp_buffer, int len, AudioOutPort &port, const ResumeAudioThread &resume_thread) {
    AUDIO_PROFILE(__func__);

    // How much data is available?
    std::unique_lock<std::mutex> lock(port.shared.mutex);
    const int bytes_available = SDL_AudioStreamAvailable(port.shared.stream.get());
    assert(bytes_available >= 0);

    // Running out of data?
    // The (len * 2) is just a guess that seems to work.
    if (bytes_available < (len * 2)) {
        // Is there a thread waiting for playback to finish?
        if (port.shared.thread >= 0) {
            // Wake the thread up.
            resume_thread(port.shared.thread);
            port.shared.thread = -1;
        }
    }

    // Mix as much as we need.
    const int bytes_to_get = std::min(len, bytes_available);
    const int bytes_got = SDL_AudioStreamGet(port.shared.stream.get(), temp_buffer, bytes_to_get);
    lock.unlock();
    if (bytes_got > 0) {
        SDL_MixAudio(stream, temp_buffer, bytes_got, port.volume);
    }
}

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len) {
    AUDIO_PROFILE(__func__);

    assert(userdata != nullptr);
    assert(stream != nullptr);
    AudioState &state = *static_cast<AudioState *>(userdata);
    assert(len == state.ro.spec.size);
    assert(len == state.callback.temp_buffer.size());

    std::vector<AudioOutPortPtr> ports;
    {
        // Read from shared state.
        const std::lock_guard<std::mutex> lock(state.shared.mutex);
        ports.reserve(state.shared.out_ports.size());
        for (const AudioOutPortPtrs::value_type &port : state.shared.out_ports) {
            ports.push_back(port.second);
        }
    }

    std::memset(stream, state.ro.spec.silence, len);

    for (const AudioOutPortPtr &port : ports) {
        mix_out_port(stream, state.callback.temp_buffer.data(), len, *port, state.ro.resume_thread);
    }
}

static void close_audio(void *) {
    SDL_CloseAudio();
}

bool init(AudioState &state, ResumeAudioThread resume_thread) {
    state.ro.resume_thread = resume_thread;

    SDL_AudioSpec desired = {};
    desired.freq = 48000;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = &audio_callback;
    desired.userdata = &state;

    if (SDL_OpenAudio(&desired, &state.ro.spec) != 0) {
        LOG_ERROR("SDL audio error: {}", SDL_GetError());
        return false;
    }

    state.device = AudioDevicePtr(nullptr, close_audio);
    state.callback.temp_buffer.resize(state.ro.spec.size);

    SDL_PauseAudio(0);

    return true;
}
