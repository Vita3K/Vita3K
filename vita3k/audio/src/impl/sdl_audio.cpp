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

#include "audio/impl/sdl_audio.h"

#include "kernel/thread/thread_state.h"

#include <SDL3/SDL_audio.h>

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

static constexpr int cached_buffers = 3;

static void SDLCALL thread_wakeup_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    assert(userdata != nullptr);
    assert(stream != nullptr);
    SDLAudioOutPort *port = static_cast<SDLAudioOutPort *>(userdata);
    // Is there a thread waiting for playback to finish?
    if (port->thread >= 0) {
        const int bytes_available = SDL_GetAudioStreamQueued(port->stream.get());
        assert(bytes_available >= 0);
        // Running out of data?
        // The (len * 3) is according to the value in sceAudioOutOutput
        if (bytes_available < port->len_bytes * cached_buffers) {
            // Wake the thread up.
            port->adapter.state.resume_thread(port->thread);
            port->thread = -1;
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
    // we only allow the samples amount to be changed
    // if resampling or format change has to be done, it is better
    // to do it after all channels have been merged
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
    SDL_AudioSpec dst_spec;
    SDL_CHECK(SDL_GetAudioDeviceFormat(device_id, &dst_spec, &device_buffer_size));
    const AudioStreamPtr stream(SDL_CreateAudioStream(&src_spec, &dst_spec), SDL_DestroyAudioStream);
    SDL_CHECK(stream);
    SDL_CHECK(SDL_BindAudioStream(device_id, stream.get()));
    auto port = std::make_shared<SDLAudioOutPort>(stream, this);
    SDL_CHECK(SDL_SetAudioStreamGetCallback(stream.get(), thread_wakeup_callback, port.get()));
    port->channels = nb_channels;
    port->len_microseconds = (nb_sample * 1'000'000ULL) / freq;
    port->len_bytes = nb_sample * nb_channels * sizeof(int16_t);
    switch_state(false);
    return port;
}
void SDLAudioAdapter::audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer) {
    //  Put audio to the port's stream and see how much is left to play.
    SDLAudioOutPort &port = static_cast<SDLAudioOutPort &>(out_port);
    std::unique_lock<std::mutex> lock(port.mutex);
    SDL_CHECK_VOID(SDL_PutAudioStreamData(port.stream.get(), buffer, out_port.len_bytes));
    const int available = SDL_GetAudioStreamQueued(port.stream.get());
    SDL_CHECK_EXT(available >= 0, );
    lock.unlock();
    // If there's lots of audio left to play, stop this thread.
    // The audio callback will wake it up later when it's running out of data.
    // the 3*(nb of samples for each callback) is needed for some games with an 480 host audiobuffer
    // sample size (what SDL audio gives us) to make sure this does not happen
    // we are supposed to wait for the existing samples to be processed (except the ones just passed)
    // but this would give a bad audio because the host buffer size is different compared to the guest buffer size
    // so we need to cache more data to make sure we always have enough
    if (available >= cached_buffers * out_port.len_bytes) {
        port.thread = thread.id;
        std::unique_lock<std::mutex> mlock(thread.mutex);
        thread.update_status(ThreadStatus::wait);
        thread.status_cond.wait(mlock, [&]() { return thread.status == ThreadStatus::run; });
    }
}
void SDLAudioAdapter::set_volume(AudioOutPort &out_port, float volume) {
    SDL_CHECK_VOID(SDL_SetAudioStreamGain(static_cast<SDLAudioOutPort &>(out_port).stream.get(), volume));
}

int SDLAudioAdapter::get_rest_sample(AudioOutPort &out_port) {
    auto &port = static_cast<SDLAudioOutPort &>(out_port);
    const int bytes_available = SDL_GetAudioStreamQueued(port.stream.get());
    SDL_CHECK_NEG(bytes_available);
    // we have the number of bytes left, we can convert it back to the number of samples left
    return bytes_available / (port.channels * sizeof(int16_t));
}
