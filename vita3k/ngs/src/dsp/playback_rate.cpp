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
 * @file playback_rate.cpp
 * @brief NGS playback rate scaling implementation
 * @details Allows audio to be both time-stretched and pitch-shifted at
 * the same time, almost as if the playing speed of a vinyl recorder changed
 * without sacrificing audio quality as much as if linear playback scaling
 * was used.
 */

#include <codec/state.h>
#include <ngs/dsp/playback_rate.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ngs::dsp::playback_rate {

Scaler::Scaler(ngs::dsp::playback_rate::scaling_settings *settings) {
    // Set settings
    this->settings = settings;

    // Set up SoundTouch object
    this->scaler.setSampleRate(int(this->settings->source_playback_rate));
    this->scaler.setChannels(int(this->settings->channels));
    this->scaler.setRate(this->settings->scaling_factor);
};

unsigned int Scaler::scale(std::vector<std::uint8_t> *audio_input) {
    unsigned int resulting_samples_count = 0;

    // Elements of the audio input array
    unsigned int input_elements_amount = 0;
    input_elements_amount = audio_input->size();

    // Real amount of input floating point elements
    unsigned int real_elements_amount = 0;
    real_elements_amount = input_elements_amount / sizeof(float);

    // Fixed length buffer for floating-point input samples
    const unsigned int bufferSize = 2048;
    float fsamples_input[bufferSize];
    for (unsigned int i = 0; i < bufferSize; i++) {
        fsamples_input[i] = 0;
    }

    // Fixed length buffer to receive values from scaler
    float fsamples_output_receive[bufferSize];
    for (unsigned int i = 0; i < bufferSize; i++) {
        fsamples_output_receive[i] = 0;
    }

    // Amount of input samples that have already been passed to scaler
    unsigned int already_passed_elements = 0;

    // Loop to control the i/o of audio data into/from scaler based on fixed-sized buffers
    do {
        // Amount of input samples passed to scaler this round
        unsigned int elements_to_pass_this_round = 0;
        if ((real_elements_amount - already_passed_elements) < bufferSize) {
            elements_to_pass_this_round = real_elements_amount - already_passed_elements;
        } else {
            elements_to_pass_this_round = bufferSize;
        }

        memcpy(fsamples_input, audio_input->data() + already_passed_elements * sizeof(float), elements_to_pass_this_round * sizeof(float));

        // Pass audio input samples to scaler
        this->scaler.putSamples(fsamples_input, elements_to_pass_this_round / this->settings->channels);
        already_passed_elements += elements_to_pass_this_round;

        // Amount of received samples from scaler on the last receive call
        unsigned int last_received_elements = 0;
        // Amount of output samples that have been collected from scaler
        unsigned int already_written_elements = 0;
        do {
            // Dump samples into fixed array for temporal values and get the amount of returned samples
            last_received_elements = this->scaler.receiveSamples(fsamples_output_receive, bufferSize / this->settings->channels) * this->settings->channels;

            // Resize output vector to receive the new samples just dumped into the fixed array
            this->fsamples_output.resize(this->fsamples_output.size() + last_received_elements);

            // Append samples to the cumulative output vector
            memcpy(fsamples_output.data() + already_written_elements, fsamples_output_receive, last_received_elements * sizeof(float));

            already_written_elements += last_received_elements;

        } while (last_received_elements != 0);

        // Flush remaining samples and append
        this->scaler.flush();
        do {
            // Dump samples into fixed array for temporal values and get the amount of returned samples
            last_received_elements = this->scaler.receiveSamples(fsamples_output_receive, bufferSize / this->settings->channels) * this->settings->channels;

            // Resize output vector to receive the new samples just dumped into the fixed array
            this->fsamples_output.resize(this->fsamples_output.size() + last_received_elements);

            // Append samples to the cumulative output vector
            memcpy(fsamples_output.data() + already_written_elements, fsamples_output_receive, last_received_elements * sizeof(float));

            already_written_elements += last_received_elements;

        } while (last_received_elements != 0);

    } while (already_passed_elements < real_elements_amount);

    // Clear internal buffers of scaler
    this->scaler.clear();

    resulting_samples_count = fsamples_output.size() / this->settings->channels;

    return resulting_samples_count;
};

int Scaler::receive(std::vector<std::uint8_t> *audio_output) {
    memcpy(audio_output->data(), fsamples_output.data(), fsamples_output.size() * sizeof(float));

    return 0;
};

}; // namespace ngs::dsp::playback_rate
