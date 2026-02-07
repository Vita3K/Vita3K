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
#include "util/log.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>

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

void SDLCALL SDLAudioAdapter::thread_wakeup_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    assert(userdata != nullptr);
    assert(stream != nullptr);
    const int bytes_available = SDL_GetAudioStreamAvailable(stream);
    if (bytes_available < 2 * total_amount || additional_amount > 0) {
        SDLAudioOutPort *port = static_cast<SDLAudioOutPort *>(userdata);
        port->cond_var.notify_one();
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
    SDL_CHECK(SDL_SetAudioStreamGetCallback(stream.get(), SDLAudioAdapter::thread_wakeup_callback, port.get()));
    port->channels = nb_channels;
    port->len_microseconds = (nb_sample * 1'000'000ULL) / freq;
    port->len_bytes = nb_sample * nb_channels * sizeof(int16_t);
    switch_state(false);
    return port;
}
void SDLAudioAdapter::audio_output(AudioOutPort &out_port, const void *buffer) {
    //  Put audio to the port's stream and see how much is left to play.
    SDLAudioOutPort &port = static_cast<SDLAudioOutPort &>(out_port);
    // If there's lots of audio left to play, stop this thread.
    // The audio callback will wake it up later when it's running out of data.
    const int samples_available = get_rest_sample(port);
    if (samples_available > device_buffer_samples) {
        std::unique_lock<std::mutex> lock(port.mutex);
        port.cond_var.wait_for(lock, std::chrono::microseconds(port.len_microseconds * 2));
    }
    SDL_CHECK_VOID(SDL_PutAudioStreamData(port.stream.get(), buffer, out_port.len_bytes));
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
