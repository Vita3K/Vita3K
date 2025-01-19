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

class SDLAudioAdapter : public AudioAdapter {
    SDL_AudioDeviceID device_id = 0;
    SDL_AudioSpec spec{};

public:
    SDLAudioAdapter(AudioState &audio_state);
    ~SDLAudioAdapter() override;

    bool init() override;
    void switch_state(const bool pause) override;
};
