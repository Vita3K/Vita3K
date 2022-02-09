// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

/**
 * @file playback_rate.h
 * @brief NGS playback rate scaling implementation
 * @details Allows audio to be both time-stretched and pitch-shifted at
 * the same time, almost as if the playing speed of a vinyl recorder changed
 * without sacrificing audio quality as much as if linear playback scaling
 * was used.
 */

#pragma once

#include <SoundTouch.h>
#include <ngs/modules/player.h>

#include <vector>

namespace ngs::dsp::playback_rate {
/**
    * @brief Playback rate scaling settings to be passed to a playback rate
    * scaling object
    */
struct scaling_settings {
    // Scaling factor in a scale of 1
    float scaling_factor = 1;

    // Source playback rate in Hz (hertz)
    unsigned int source_playback_rate;

    // Source audio channels
    unsigned int channels = 2;
};

/**
    * @brief Playback rate scaler abstraction class
    */
class Scaler {
    // Actual scaler object
    soundtouch::SoundTouch scaler;

    // Pointer to decoder state used to get settings
    ngs::dsp::playback_rate::scaling_settings *settings = nullptr;

    // Result of the scaling process in floating-point values
    // To be set by Scaler::scale once the process is finished and then get
    // reutilized by Scaler::receive
    std::vector<float> fsamples_output;

public:
    /**
            * @brief Construct a new Scaler object
            *
            * @param settings Scaling settings
            */
    Scaler(ngs::dsp::playback_rate::scaling_settings *settings);

    /**
            * @brief Take an audio input buffer and apply playback rate scaling according to
            * the settings of the provided decoder.
            *
            * @param audio_input Floating-point PCM audio samples that need to be scaled
            * @return The size in samples of the resulting audio buffer due to the scaling
            * process
            */
    unsigned int scale(std::vector<std::uint8_t> *audio_input);

    /**
            * @brief Get audio data resulting of the scaling process
            *
            * @param audio_output Pointer to a vector with enough size to fit the amount of samples
            * specified in the return of `Scaler::scale()`
            */
    int receive(std::vector<std::uint8_t> *audio_output);
};

} // namespace ngs::dsp::playback_rate
