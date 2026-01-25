// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "audio/impl/sdl_audio.h"

#include "kernel/thread/thread_state.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>

#include "util/log.h"

#define SDL_CHECK_EXT(condition, ret)                         \
    do {                                                      \
        if (!(condition)) {                                   \
            LOG_ERROR("SDL audio error: {}", SDL_GetError()); \
            return ret;                                       \
        }                                                     \
    } while (0)

#define SDL_CHECK(f_call) SDL_CHECK_EXT(f_call, {})
#define SDL_CHECK_VOID(f_call) SDL_CHECK_EXT(f_call, )
#define SDL_CHECK_NEG(f_call) SDL_CHECK_EXT((f_call) >= 0, {})

void SDLCALL SDLAudioAdapter::audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    assert(userdata != nullptr);
    assert(stream != nullptr);

    if (additional_amount <= 0)
        return;

    SDLAudioOutPort *port = static_cast<SDLAudioOutPort *>(userdata);

    // Copy data from ring buffer to SDL stream, just like in cubeb adapter.
    while (additional_amount > 0 && port->nb_buffers_ready > 0) {
        AudioBuffer &audio_buffer = port->audio_buffers[port->next_audio_buffer];
        const int bytes_remaining = port->len_bytes - audio_buffer.buffer_position;
        const int bytes_to_copy = std::min(additional_amount, bytes_remaining);

        SDL_PutAudioStreamData(stream, &audio_buffer.buffer[audio_buffer.buffer_position], bytes_to_copy);
        audio_buffer.buffer_position += bytes_to_copy;
        additional_amount -= bytes_to_copy;

        if (audio_buffer.buffer_position == port->len_bytes) {
            // if we are done with this buffer, tell it
            std::unique_lock<std::mutex> lock(port->mutex);
            port->next_audio_buffer = (port->next_audio_buffer + 1) % port->audio_buffers.size();
            port->nb_buffers_ready--;
            lock.unlock();
            port->cond_var.notify_one();
        }
    }
}

SDLAudioAdapter::SDLAudioAdapter(AudioState &audio_state)
    : AudioAdapter(audio_state) {}

SDLAudioAdapter::~SDLAudioAdapter() {
    if (device_id > 0) {
        SDL_CloseAudioDevice(device_id);
    }
}

bool SDLAudioAdapter::init() {
    // SDL3 default is 1024 sample frames for 48kHz audio, which is higher than cubeb.
    // Request smaller device buffer for lower latency callbacks.
    // 512 sample frames = 2048 bytes for stereo 16-bit, matching cubeb's callback size.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    SDL_CHECK_EXT(device_id > 0, false);
    return true;
}

void SDLAudioAdapter::switch_state(const bool pause) {
    if (pause)
        SDL_CHECK_VOID(SDL_PauseAudioDevice(device_id));
    else
        SDL_CHECK_VOID(SDL_ResumeAudioDevice(device_id));
}

AudioOutPortPtr SDLAudioAdapter::open_port(int nb_channels, int freq, int nb_sample) {
    SDL_AudioSpec src_spec = {
        .format = SDL_AUDIO_S16LE,
        .channels = nb_channels,
        .freq = freq
    };
    SDL_CHECK(SDL_GetAudioDeviceFormat(device_id, &dst_spec, &device_buffer_samples));
    const AudioStreamPtr stream(SDL_CreateAudioStream(&src_spec, &dst_spec), SDL_DestroyAudioStream);
    SDL_CHECK(stream);
    SDL_CHECK(SDL_BindAudioStream(device_id, stream.get()));
    auto port = std::make_shared<SDLAudioOutPort>(stream, *this);
    SDL_CHECK(SDL_SetAudioStreamGetCallback(stream.get(), SDLAudioAdapter::audio_callback, port.get()));
    port->channels = nb_channels;
    port->len_microseconds = (nb_sample * 1'000'000ULL) / freq;
    port->len_bytes = nb_sample * nb_channels * sizeof(int16_t);

    // SDL3 has no equivalent to cubeb_get_min_latency(), so we calculate buffer count
    // based on a target latency. 25ms matches cubeb's PulseAudio safe minimum.
    const int target_latency_ms = 25;
    const int target_latency_samples = target_latency_ms * freq / 1000;
    const int nb_buffers = (target_latency_samples + nb_sample - 1) / nb_sample + 1;
    port->audio_buffers.resize(nb_buffers);
    for (AudioBuffer &audio_buffer : port->audio_buffers) {
        // initialize all the buffers
        audio_buffer.buffer.resize(port->len_bytes);
        audio_buffer.buffer_position = 0;
    }

    switch_state(false);
    return port;
}

void SDLAudioAdapter::audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer) {
    SDLAudioOutPort &port = static_cast<SDLAudioOutPort &>(out_port);

    std::unique_lock<std::mutex> lock(port.mutex);
    if (port.nb_buffers_ready == port.audio_buffers.size()) {
        // is it really useful to update the thread status?
        thread.update_status(ThreadStatus::wait);
        port.cond_var.wait(lock);
        thread.update_status(ThreadStatus::run);
    }

    assert(port.nb_buffers_ready < port.audio_buffers.size());
    if (buffer) {
        // the buffer can be empty to drain the port
        const int next_buffer_pos = (port.next_audio_buffer + port.nb_buffers_ready) % port.audio_buffers.size();
        // we could unlock the lock here and re-lock it right after, but will this be faster?
        memcpy(port.audio_buffers[next_buffer_pos].buffer.data(), buffer, port.len_bytes);
        port.audio_buffers[next_buffer_pos].buffer_position = 0;
        port.nb_buffers_ready++;
    }
}

void SDLAudioAdapter::set_volume(AudioOutPort &out_port, float volume) {
    SDL_CHECK_VOID(SDL_SetAudioStreamGain(static_cast<SDLAudioOutPort &>(out_port).stream.get(), volume));
}

int SDLAudioAdapter::get_rest_sample(AudioOutPort &out_port) {
    auto &port = static_cast<SDLAudioOutPort &>(out_port);
    const int bytes_available = SDL_GetAudioStreamAvailable(port.stream.get());
    SDL_CHECK_NEG(bytes_available);
    // we have the number of bytes left, we can convert it back to the number of samples left
    return bytes_available / SDL_AUDIO_FRAMESIZE(dst_spec);
}
