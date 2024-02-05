// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <SDL.h>

#include "util/log.h"

static void SDLCALL sdl_audio_callback(void *userdata, Uint8 *stream, int len) {
    assert(userdata != nullptr);
    assert(stream != nullptr);
    SDLAudioAdapter *adapter = static_cast<SDLAudioAdapter *>(userdata);

    adapter->audio_callback(stream, len);
}

SDLAudioAdapter::SDLAudioAdapter(AudioState &audio_state)
    : AudioAdapter(audio_state) {}

SDLAudioAdapter::~SDLAudioAdapter() {
    if (device_id > 0) {
        SDL_PauseAudioDevice(device_id, 1);
        SDL_CloseAudioDevice(device_id);
    }
}

bool SDLAudioAdapter::init() {
    SDL_AudioSpec desired = {};
    desired.freq = 48000;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 512;
    desired.callback = sdl_audio_callback;
    desired.userdata = this;

    // we only allow the samples amount to be changed
    // if resampling or format change has to be done, it is better
    // to do it after all channels have been merged
    device_id = SDL_OpenAudioDevice(nullptr, false, &desired, &spec, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (device_id <= 0) {
        LOG_ERROR("SDL audio error: {}", SDL_GetError());
        return false;
    }

    state.spec = {
        .freq = spec.freq,
        .nb_samples = spec.samples,
        .silence = spec.silence
    };

    SDL_PauseAudioDevice(device_id, 0);

    return true;
}

void SDLAudioAdapter::switch_state(const bool pause) {
    SDL_PauseAudioDevice(device_id, pause);
}
