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

#pragma once

#include "../state.h"

#include <SDL3/SDL_audio.h>

class SDLAudioAdapter : public AudioAdapter {
private:
    SDL_AudioDeviceID device_id = 0;
    int device_buffer_samples = 0;
    SDL_AudioSpec dst_spec;
    static void SDLCALL thread_wakeup_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);

public:
    SDLAudioAdapter(AudioState &audio_state);
    ~SDLAudioAdapter() override;

    bool init() override;
    void switch_state(const bool pause) override;
    AudioOutPortPtr open_port(int nb_channels, int freq, int nb_sample) override;
    void audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer) override;
    void set_volume(AudioOutPort &out_port, float volume) override;
    int get_rest_sample(AudioOutPort &out_port) override;
};

typedef std::shared_ptr<SDL_AudioStream> AudioStreamPtr;

struct SDLAudioOutPort : public AudioOutPort {
    //   thread currently waiting for the audio to be processed
    SceUID thread = -1;
    int channels = 2;
    AudioStreamPtr stream;
    SDLAudioAdapter &adapter;
    SDLAudioOutPort(AudioStreamPtr stream, AudioAdapter &adapter)
        : stream(std::move(stream))
        , adapter(dynamic_cast<SDLAudioAdapter &>(adapter)) {}
};
