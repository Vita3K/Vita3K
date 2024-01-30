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

#pragma once

#include <util/types.h>

#include <SDL_audio.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#define SCE_AUDIO_OUT_MAX_VOL 32768 //!< Maximum output port volume
#define SCE_AUDIO_VOLUME_0DB SCE_AUDIO_OUT_MAX_VOL //!< Maximum output port volume

typedef std::shared_ptr<SDL_AudioStream> AudioStreamPtr;
typedef std::function<void(SceUID)> ResumeAudioThread;

struct AudioOutPort {
    // Channel range from 0 - 32768
    int left_channel_volume = SCE_AUDIO_VOLUME_0DB;
    int right_channel_volume = SCE_AUDIO_VOLUME_0DB;
    // Volume range from 0 to 1
    float volume = 1.0f;
    // length of the buffer for each call
    int len_bytes = 0;

    // current config
    int type = 0;
    int len = 0;
    int freq = 0;
    int mode = 0;

    std::mutex mutex;
    // stream to get the data
    AudioStreamPtr stream;
    // thread currently waiting for the audio to be processed
    SceUID thread = -1;
};

typedef std::shared_ptr<AudioOutPort> AudioOutPortPtr;
typedef std::map<int, AudioOutPortPtr> AudioOutPortPtrs;

struct AudioInPort {
    SDL_AudioDeviceID id;
    bool running = false;
    int len_bytes = 0;
};

struct AudioSpec {
    int freq;
    // number of samples to be processed per callback per channel
    int nb_samples = 0;
    // value for silence
    uint8_t silence;
};

struct ThreadState;
struct AudioState;

// abstract class that need to be overloaded with an audio implementation
class AudioAdapter {
private:
    // buffer used to mix audio
    std::vector<uint8_t> temp_buffer;

protected:
    AudioState &state;
    // are we using a single stream and mixing everything inside or multiple streams?
    bool single_stream = true;

public:
    // called by subclasses once they get called by their implementation callback
    // stream points to the location where we need to write state.ro.len_bytes bytes
    void audio_callback(uint8_t *stream, int len_bytes);

    AudioAdapter(AudioState &audio_state)
        : state(audio_state) {}
    virtual ~AudioAdapter() {}

    virtual bool init() = 0;
    virtual AudioOutPortPtr open_port(int nb_channels, int freq, int nb_sample) { return nullptr; }
    virtual void audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer) {}
    virtual void set_volume(AudioOutPort &out_port, float volume) {}
    virtual void switch_state(const bool pause) {}

    friend struct AudioState;
};

struct AudioState {
    AudioSpec spec;
    // the adapter must be before out_ports for the destructors to work correctly
    std::unique_ptr<AudioAdapter> adapter;
    std::mutex mutex;
    int next_port_id = 1;
    AudioOutPortPtrs out_ports;
    AudioInPort in_port;
    ResumeAudioThread resume_thread;
    std::string audio_backend;
    float global_volume;

    bool init(const ResumeAudioThread &resume_thread, const std::string &adapter_name);
    void set_backend(const std::string &adapter_name);
    AudioOutPortPtr open_port(int nb_channels, int freq, int nb_sample);
    void audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer);
    void set_volume(AudioOutPort &out_port, float volume);
    void set_global_volume(float volume);
    void switch_state(const bool pause);
};
