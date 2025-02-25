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

#include <audio/state.h>

#include <tracy/Tracy.hpp>

#include <audio/impl/cubeb_audio.h>
#include <audio/impl/sdl_audio.h>

#include <kernel/thread/thread_state.h>

#include <util/log.h>

#include <algorithm>
#include <cassert>
#include <cstring>

static void mix_out_port(uint8_t *stream, uint8_t *temp_buffer, int len, float global_volume, AudioOutPort &port, const ResumeAudioThread &resume_thread) {
    ZoneScopedC(0xF6C2FF); // Tracy - Track function scope with color thistle

    // How much data is available?
    std::unique_lock<std::mutex> lock(port.mutex);
    const int bytes_available = SDL_AudioStreamAvailable(port.stream.get());
    assert(bytes_available >= 0);

    // Running out of data?
    // The (len * 3) is according to the value in sceAudioOutOutput
    if (bytes_available < len * 3) {
        // Is there a thread waiting for playback to finish?
        if (port.thread >= 0) {
            // Wake the thread up.
            resume_thread(port.thread);
            port.thread = -1;
        }
    }

    if (bytes_available == 0)
        return;

    // Mix as much as we need.
    const int bytes_to_get = std::min(len, bytes_available);
    const int bytes_got = SDL_AudioStreamGet(port.stream.get(), temp_buffer, bytes_to_get);
    lock.unlock();
    if (bytes_got > 0) {
        SDL_MixAudioFormat(stream, temp_buffer, AUDIO_S16LSB, bytes_got, static_cast<int>(port.volume * global_volume * SDL_MIX_MAXVOLUME));
    }
}

void AudioAdapter::audio_callback(uint8_t *stream, int len_bytes) {
    tracy::SetThreadName("Host audio thread"); // Tracy - Declare belonging of this function to the audio thread
    ZoneScopedC(0xF6C2FF); // Tracy - Track function scope with color thistle

    std::vector<AudioOutPortPtr> ports;
    {
        // Read from shared state.
        const std::lock_guard<std::mutex> lock(state.mutex);
        ports.reserve(state.out_ports.size());
        for (const auto &[_, port] : state.out_ports) {
            ports.push_back(port);
        }
    }
    std::memset(stream, state.spec.silence, len_bytes);

    for (const AudioOutPortPtr &port : ports) {
        mix_out_port(stream, temp_buffer.data(), len_bytes, state.global_volume, *port.get(), state.resume_thread);
    }

    FrameMarkNamed("Audio"); // Tracy - End discontinuous frame for audio rendering
}

bool AudioState::init(const ResumeAudioThread &resume_thread, const std::string &adapter_name) {
    this->resume_thread = resume_thread;

    set_backend(adapter_name);
    if (!adapter)
        return false;

    return true;
}

void AudioState::set_backend(const std::string &adapter_name) {
    if (adapter_name == this->audio_backend)
        return;

    // first delete all ports then delete the backend
    out_ports.clear();
    adapter.reset();
    if (adapter_name == "SDL") {
        adapter = std::make_unique<SDLAudioAdapter>(*this);
    } else if (adapter_name == "Cubeb") {
        adapter = std::make_unique<CubebAudioAdapter>(*this);
    } else {
        LOG_ERROR("Unknown audio adapter {}", adapter_name);
        return;
    }
    this->audio_backend = adapter_name;

    // lock the mutex to make sure nothing happens until the initialisation is done
    const std::lock_guard<std::mutex> lock(mutex);
    if (!adapter->init()) {
        adapter.reset();
        return;
    }

    adapter->temp_buffer.resize(spec.nb_samples * 2 * sizeof(uint16_t));
}

AudioOutPortPtr AudioState::open_port(int nb_channels, int freq, int nb_sample) {
    if (adapter->single_stream) {
        // handle everything here
        const AudioStreamPtr stream(SDL_NewAudioStream(AUDIO_S16LSB, nb_channels, freq, AUDIO_S16LSB, 2, spec.freq), SDL_FreeAudioStream);
        if (!stream)
            return nullptr;

        AudioOutPortPtr port = std::make_shared<AudioOutPort>();
        port->len_microseconds = (nb_sample * 1'000'000ULL) / freq;
        port->len_bytes = nb_sample * nb_channels * sizeof(int16_t);
        port->stream = stream;

        return port;
    } else {
        // let the adapter open the port
        AudioOutPortPtr port = adapter->open_port(nb_channels, freq, nb_sample);
        adapter->set_volume(*port, global_volume);
        return port;
    }
}

void AudioState::audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer) {
    if (adapter->single_stream) {
        // Put audio to the port's stream and see how much is left to play.
        std::unique_lock<std::mutex> lock(out_port.mutex);
        SDL_AudioStreamPut(out_port.stream.get(), buffer, out_port.len_bytes);

        const int available = SDL_AudioStreamAvailable(out_port.stream.get());
        lock.unlock();

        // If there's lots of audio left to play, stop this thread.
        // The audio callback will wake it up later when it's running out of data.
        // the 3*(nb of samples for each callback) is needed for some games with an 480 host audiobuffer
        // sample size (what SDL audio gives us) to make sure this does not happen
        // we are supposed to wait for the existing samples to be processed (except the ones just passed)
        // but this would give a bad audio because the host buffer size is different compared to the guest buffer size
        // so we need to cache more data to make sure we always have enough
        if (available >= 3 * spec.nb_samples * 2 * sizeof(uint16_t)) {
            out_port.thread = thread.id;

            std::unique_lock<std::mutex> mlock(thread.mutex);
            thread.update_status(ThreadStatus::wait);
            thread.status_cond.wait(mlock, [&]() { return thread.status == ThreadStatus::run; });
        }
    } else {
        adapter->audio_output(thread, out_port, buffer);
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    uint64_t diff = now - out_port.last_output;
    uint64_t to_wait = out_port.len_microseconds - diff;
    if (diff < out_port.len_microseconds && to_wait > 1000) {
        // This is what we should be waiting to be perfectly accurate
        // However, doing so would cause the host audio buffer to often lack samples to output
        // This is because the PS Vita and the host audio parameters do not match exactly
        // So instead only wait 50% of the time
        // also don't sleep for less than 0.5 ms
        to_wait /= 2;
        std::this_thread::sleep_for(std::chrono::microseconds(to_wait));
        out_port.last_output = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    } else {
        out_port.last_output = now;
    }
}

void AudioState::set_volume(AudioOutPort &out_port, float volume) {
    out_port.volume = volume;

    if (!adapter->single_stream) {
        adapter->set_volume(out_port, volume * global_volume);
    }
}

void AudioState::set_global_volume(float volume) {
    global_volume = volume;

    if (!adapter->single_stream) {
        // Update adapter volume for each port.
        const std::lock_guard lock(mutex);
        for (const auto &[_, port] : out_ports) {
            adapter->set_volume(*port, port->volume * volume);
        }
    }
}

void AudioState::switch_state(const bool pause) {
    adapter->switch_state(pause);
}
