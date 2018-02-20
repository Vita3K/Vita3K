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

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include <psp2/types.h>
#include <util/types.h>

#include <SDL_audio.h>

typedef std::shared_ptr<SDL_AudioStream> AudioStreamPtr;
typedef std::function<void(SceUID)> ResumeAudioThread;

struct AudioOutput {
    const u8 *buf = nullptr;
    s32 len_bytes = 0;
    SceUID thread = -1;
};

struct ReadOnlyAudioOutPortState {
    s32 len_bytes = 0;
};

struct AudioCallbackOutPortState {
    AudioStreamPtr stream;
};

struct SharedAudioOutPortState {
    std::mutex mutex;
    std::queue<AudioOutput> outputs;
};

struct AudioOutPort {
    ReadOnlyAudioOutPortState ro;
    AudioCallbackOutPortState callback;
    SharedAudioOutPortState shared;
};

typedef std::shared_ptr<AudioOutPort> AudioOutPortPtr;
typedef std::map<s32, AudioOutPortPtr> AudioOutPortPtrs;
typedef std::shared_ptr<void> AudioDevicePtr;

struct ReadOnlyAudioState {
    SDL_AudioSpec spec;
    ResumeAudioThread resume_thread;
};

struct AudioCallbackState {
    std::vector<u8> temp_buffer;
};

struct SharedAudioState {
    std::mutex mutex;
    s32 next_port_id = 0;
    AudioOutPortPtrs out_ports;
};

struct AudioState {
    ReadOnlyAudioState ro;
    AudioCallbackState callback;
    SharedAudioState shared;
    AudioDevicePtr device;
};
