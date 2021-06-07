// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <ngs/dsp/playback_rate.h>
#include <ngs/modules/player.h>
#include <util/log.h>

#include <cassert>
#include <cstring>

namespace ngs::player {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_NORMAL_PLAYER) {}

std::size_t Module::get_buffer_parameter_size() const {
    return sizeof(Parameters);
}

void Module::on_state_change(ModuleData &data, const VoiceState previous) {
    State *state = data.get_state<State>();
    if (data.parent->state == VOICE_STATE_AVAILABLE) {
        state->current_byte_position_in_buffer = 0;
        state->current_loop_count = 0;
        state->current_buffer = 0;
    }
}

bool Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    const Parameters *params = data.get_parameters<Parameters>(mem);
    State *state = data.get_state<State>();
    bool finished = false;

    // If decoder hasn't been initialized or ADPCM format is going to be used
    if (!decoder || (decoder->he_adpcm != static_cast<bool>(params->type))) {
        // Create decoder specifying the desired destination sample rate
        decoder = std::make_unique<PCMDecoderState>(data.parent->rack->system->sample_rate);

        // Enable ADPCM mode on the just created decoder if needed
        decoder->he_adpcm = static_cast<bool>(params->type);
    }

    // If the amount of samples already processed and pending to be passed is smaller than the amount of samples of the audio buffer
    if (static_cast<std::int32_t>(state->decoded_gran_pending) < data.parent->rack->system->granularity) {
        // Memory cleaning check
        if (!data.extra_storage.empty()) {
            // Delete data from previous processing if memory isn't empty
            data.extra_storage.erase(data.extra_storage.begin(), data.extra_storage.begin() + state->decoded_gran_passed * 8);
        } else {
            // If memory is already empty and there's no buffer in need of processing or the current buffer has no data then stop processing
            if ((state->current_buffer == -1) || (params->buffer_params[state->current_buffer].bytes_count == 0)) {
                return true;
            }
        }

        // Reset the passed samples count to 0
        state->decoded_gran_passed = 0;

        while (static_cast<std::int32_t>(state->decoded_gran_pending) < data.parent->rack->system->granularity) {
            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            if ((data.extra_storage.size() < sizeof(float) * 2 * data.parent->rack->system->granularity) && (state->current_buffer != -1)) {
                // Set up decoder
                decoder->source_channels = params->channels;
                decoder->source_frequency = params->playback_frequency;

                // Get audio buffer
                auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem);

                DecoderSize samples_count;

                // Send buffered audio data to decoder
                decoder->send(input, params->buffer_params[state->current_buffer].bytes_count);

                // Get the amount of samples about to be received from the decoder and dump the value in samples_count
                decoder->receive(nullptr, &samples_count);

                // Playback rate scaling
                if (params->playback_scalar != 1) {
                    LOG_INFO_IF(this->LOG_PLAYBACK_SCALING, "The currently running game requests playback rate scaling when decoding audio. Audio might crackle.");
                    this->LOG_PLAYBACK_SCALING = false;

                    // Received decoded samples from decoder
                    std::vector<std::uint8_t> decoded_data(samples_count.samples * sizeof(float), 0);

                    // Receive the samples processed by the decoder
                    decoder->receive(decoded_data.data(), nullptr);

                    // Playback scaler settings
                    ngs::dsp::playback_rate::scaling_settings scaling_settings;
                    scaling_settings.scaling_factor = params->playback_scalar;
                    scaling_settings.source_playback_rate = decoder->source_frequency;
                    scaling_settings.channels = decoder->source_channels;

                    // Initiate scaler object
                    ngs::dsp::playback_rate::Scaler scaler(&scaling_settings);

                    // Scale the playback rate of the contents received from the decoder with the desired settings
                    // and get the amount of resulting samples
                    unsigned int scaled_samples_amount = 0;
                    scaled_samples_amount = scaler.scale(&decoded_data);

                    // Received scaled samples from scaler
                    std::vector<std::uint8_t> scaled_data(scaled_samples_amount * sizeof(float), 0);

                    // Receive scaled samples from scaler
                    scaler.receive(&scaled_data);

                    // Get current size of audio queue for processed samples in memory
                    const std::size_t current_count = data.extra_storage.size();

                    // Allocate memory to accommodate the result of the scaling process into the queue for the final audio buffer
                    data.extra_storage.resize(current_count + scaled_samples_amount * sizeof(float));

                    // Pass scaled audio data into the queue for the final audio buffer
                    std::memcpy(current_count + data.extra_storage.data(), scaled_data.data(), scaled_data.size());
                } else {
                    // Get current size of audio buffer for processed samples in memory
                    const std::size_t current_count = data.extra_storage.size();

                    // Increase the size the audio buffer for processed samples to accommodate the new about-to-be-received samples
                    data.extra_storage.resize(current_count + samples_count.samples * sizeof(float));

                    // Receive the samples processed by the decoder and append them to the buffer of already processed samples
                    decoder->receive(current_count + data.extra_storage.data(), nullptr);
                }
            }

            std::uint32_t bytes_left_in_buffer = data.extra_storage.size();
            std::uint32_t samples_to_take_per_channel = bytes_left_in_buffer / sizeof(float) / 2;

            state->bytes_consumed_since_key_on += params->buffer_params[state->current_buffer].bytes_count;
            state->current_byte_position_in_buffer += params->buffer_params[state->current_buffer].bytes_count;
            state->total_bytes_consumed += params->buffer_params[state->current_buffer].bytes_count;

            state->decoded_gran_pending = samples_to_take_per_channel;

            if ((state->current_buffer == -1) || (params->buffer_params[state->current_buffer].bytes_count == 0)) {
                // If no buffer is found, stop processing
                finished = true;
                break;
            } else {
                // If the current byte possition in the buffer exceeds the total amount of bytes in the buffer
                if (state->current_byte_position_in_buffer >= params->buffer_params[state->current_buffer].bytes_count) {
                    const std::int32_t prev_index = state->current_buffer;

                    // Enable looping over the buffer if needed
                    if (params->buffer_params[state->current_buffer].loop_count != -1) {
                        state->current_loop_count++;

                        if (state->current_loop_count > params->buffer_params[state->current_buffer].loop_count) {
                            state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
                            state->current_loop_count = 0;
                            state->current_byte_position_in_buffer = 0;

                            data.parent->voice_lock->unlock();

                            if (state->current_buffer == -1) {
                                data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_CALLBACK_REASON_DONE_ALL, 0, 0);
                                finished = true;
                                // TODO: Free all occupied input routes
                                //unroute_occupied(mem, voice);
                            } else {
                                data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_CALLBACK_REASON_DONE_ONE_BUFFER, prev_index,
                                    params->buffer_params[state->current_buffer].buffer.address());
                            }

                            data.parent->voice_lock->lock();
                        }
                    } else {
                        data.parent->voice_lock->unlock();
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_CALLBACK_REASON_START_LOOP, state->current_loop_count,
                            params->buffer_params[state->current_buffer].buffer.address());
                        data.parent->voice_lock->lock();
                    }

                    state->current_byte_position_in_buffer = 0;
                }
            }
        }
    }

    std::uint32_t gran_to_be_passed = std::min<std::uint32_t>(state->decoded_gran_pending, data.parent->rack->system->granularity);

    auto const new_size = 2 * sizeof(float) * (state->decoded_gran_passed + data.parent->rack->system->granularity);
    if (data.extra_storage.size() < new_size) {
        LOG_TRACE("Resize extra_storage from {} to {}", data.extra_storage.size(), new_size);
        data.extra_storage.resize(new_size);
    }
    std::uint8_t *data_ptr = data.extra_storage.data();
    data_ptr += 2 * sizeof(float) * state->decoded_gran_passed;

    data.parent->products[0].data = data_ptr;

    state->decoded_gran_pending = (state->decoded_gran_pending < state->decoded_gran_passed) ? 0 : (state->decoded_gran_pending - gran_to_be_passed);
    state->decoded_gran_passed += gran_to_be_passed;

    state->samples_generated_since_key_on += gran_to_be_passed * 2;

    return finished;
}
} // namespace ngs::player
