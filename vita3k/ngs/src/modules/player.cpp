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

#include <ngs/modules/player.h>
#include <util/log.h>

extern "C" {
#include <libswresample/swresample.h>
}

#include <cassert>
#include <cstring>

namespace ngs {

void PlayerModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    SceNgsPlayerParams *params = data.get_parameters<SceNgsPlayerParams>(mem);
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE) {
        state->samples_generated_since_key_on = 0;
        state->bytes_consumed_since_key_on = 0;
        state->current_buffer = params->start_buffer;
        state->current_byte_position_in_buffer = params->start_bytes;
        state->current_loop_count = 0;

        memset(&state->adpcm_history, 0, sizeof(state->adpcm_history));
    } else if (data.parent->is_keyed_off) {
        state->current_buffer = params->start_buffer;
        state->current_byte_position_in_buffer = params->start_bytes;
        state->current_loop_count = 0;
        state->reset_swr = true;
    }
}

void PlayerModule::on_param_change(const MemState &mem, ModuleData &data) {
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    const SceNgsPlayerParams *old_params = reinterpret_cast<SceNgsPlayerParams *>(data.last_info.data());
    SceNgsPlayerParams *new_params = static_cast<SceNgsPlayerParams *>(data.info.data.get(mem));

    // check for invalid playback values
    const auto is_invalid_playback_value = [](const float playback_value, const float max_value) {
        return isnan(playback_value) || (playback_value < 0.f) || (playback_value > max_value);
    };

    if (is_invalid_playback_value(new_params->playback_scalar, 10.f)) {
        new_params->playback_scalar = old_params->playback_scalar;
        LOG_ERROR_ONCE("Invalid playback rate scaling.");
        if (is_invalid_playback_value(new_params->playback_scalar, 10.f)) {
            new_params->playback_scalar = 1.0;
        }
    }

    if (is_invalid_playback_value(new_params->playback_frequency, 192000.f)) {
        new_params->playback_frequency = old_params->playback_frequency;
        LOG_ERROR_ONCE("Invalid playback frequency.");
        if (is_invalid_playback_value(new_params->playback_frequency, 192000.f)) {
            new_params->playback_frequency = 48000.f;
        }
    }

    // if playback scaling changed, reset the resampler
    if (old_params->playback_frequency != new_params->playback_frequency || old_params->playback_scalar != new_params->playback_scalar) {
        ADPCMHistory hist_empty{};
        std::fill_n(state->adpcm_history, SCE_NGS_PLAYER_MAX_PCM_CHANNELS, hist_empty);

        state->reset_swr = true;
    }
}

void PlayerModule::set_default_preset(const MemState &mem, ModuleData &data) {
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    LOG_WARN_ONCE("Player reset state");
    *state = {};
}

bool PlayerModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    SceNgsPlayerParams *params = data.get_parameters<SceNgsPlayerParams>(mem);
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    bool finished = false;

    const int32_t sample_rate = data.parent->rack->system->sample_rate;
    const int32_t granularity = data.parent->rack->system->granularity;

    // fix right now because it might be set by default in SceNgsVoiceInit, which we do not support
    if (params->channels == 0)
        params->channels = 2;

    // If decoder hasn't been initialized
    if (!decoder) {
        // Create decoder specifying the desired destination sample rate
        decoder = std::make_unique<PCMDecoderState>(static_cast<float>(sample_rate));
    }

    // If the amount of samples already processed and pending to be passed is smaller than the amount of samples of the audio buffer
    if (static_cast<int>(state->decoded_samples_pending) < granularity) {
        // Memory cleaning check
        if (!data.extra_storage.empty()) {
            // Delete data from previous processing if memory isn't empty
            data.extra_storage.erase(data.extra_storage.begin(), data.extra_storage.begin() + state->decoded_samples_passed * 2 * sizeof(float));
        }

        // Reset the passed samples count to 0
        state->decoded_samples_passed = 0;

        while (static_cast<int>(state->decoded_samples_pending) < granularity) {
            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            if ((state->current_buffer == -1)
                || !params->buffer_params[state->current_buffer].buffer
                || (params->buffer_params[state->current_buffer].bytes_count == 0)) {
                // Stop processing if no valid buffer is available or if the buffer is empty
                finished = true;
                break;
            }
            // If the current byte position in the buffer exceeds the total amount of bytes in the buffer
            else if (state->current_byte_position_in_buffer >= params->buffer_params[state->current_buffer].bytes_count) {
                const int32_t prev_index = state->current_buffer;
                state->current_byte_position_in_buffer = 0;
                state->current_loop_count++;

                voice_lock.unlock();
                scheduler_lock.unlock();

                // Enable looping over the buffer if needed
                if (params->buffer_params[state->current_buffer].loop_count != -1
                    && state->current_loop_count > params->buffer_params[state->current_buffer].loop_count) {
                    state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
                    state->current_loop_count = 0;

                    if ((state->current_buffer == -1)
                        || !params->buffer_params[state->current_buffer].buffer
                        || (params->buffer_params[state->current_buffer].bytes_count == 0)) {
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_END_OF_DATA, 0, 0);

                        // we are done
                        finished = true;
                        scheduler_lock.lock();
                        voice_lock.lock();
                        break;
                    } else {
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_SWAPPED_BUFFER, prev_index,
                            params->buffer_params[state->current_buffer].buffer.address());
                    }
                } else {
                    data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_LOOPED_BUFFER, state->current_loop_count,
                        params->buffer_params[state->current_buffer].buffer.address());
                }

                scheduler_lock.lock();
                voice_lock.lock();
            }

            if (data.extra_storage.size() < sizeof(float) * 2 * granularity
                && state->current_buffer != -1
                && params->buffer_params[state->current_buffer].bytes_count != 0) {
                // Set up decoder
                decoder->source_channels = params->channels;
                decoder->source_frequency = params->playback_frequency;
                // Enable ADPCM mode on the decoder if needed, and restore state
                decoder->he_adpcm = static_cast<bool>(params->type);
                if (decoder->he_adpcm) {
                    std::copy_n(state->adpcm_history, decoder->source_channels, decoder->adpcm_history);
                }

                // Get audio buffer
                auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem);

                DecoderSize samples_count;
                // we need to know how many samples (not bytes!) we need to send (just enough for the system granularity)
                uint32_t samples_needed = granularity - state->decoded_samples_pending;

                if (params->playback_scalar != 1.0f) {
                    samples_needed = static_cast<uint32_t>(samples_needed * params->playback_scalar) + 0x10;
                }
                if (static_cast<int>(params->playback_frequency) != sample_rate) {
                    samples_needed = static_cast<uint32_t>((samples_needed * params->playback_frequency) / sample_rate) + 0x10;
                }

                // Convert samples count to actual bytes count that we need
                uint32_t bytes_to_send;
                if (decoder->he_adpcm) {
                    bytes_to_send = (samples_needed + 27) / 28 * params->channels * 16;
                } else {
                    bytes_to_send = samples_needed * params->channels * sizeof(int16_t);
                }

                // makes the value 4 bits aligned so we have no issue with decoding, adpcm or not and whether the sound is mono or stereo
                bytes_to_send = std::min<uint32_t>(bytes_to_send, params->buffer_params[state->current_buffer].bytes_count - state->current_byte_position_in_buffer);

                const uint8_t *chunk = input + state->current_byte_position_in_buffer;

                // HE-ADPCM requires full frames (0x10 bytes per channel).
                // We accumulate leftover bytes from previous chunks until we have enough to form one or more complete frames, then send them in a single block.
                if (decoder->he_adpcm) {
                    // Number of bytes carried over from the previous iteration (partial frame)
                    uint32_t remaining_bytes = state->adpcm_buffer.size();

                    // Preserve the original chunk size (needed later to compute leftover correctly)
                    const auto init_bytes_to_send = bytes_to_send;

                    // One HE-ADPCM frame = 0x10 bytes per channel
                    const uint32_t frame_size = 0x10 * params->channels;

                    // Total bytes available for this iteration (leftover + current chunk)
                    const auto combined_bytes = remaining_bytes + bytes_to_send;

                    // Number of complete frames we can output from combined_bytes
                    const uint32_t full_frames = combined_bytes / frame_size;
                    const uint32_t bytes_to_send_adpcm = full_frames * frame_size;

                    // Number of bytes required from the current chunk to complete the frames
                    const auto chunk_bytes_needed = bytes_to_send_adpcm - remaining_bytes;

                    // Case 1: No leftover -> frames can be sent directly from the input chunk
                    if (remaining_bytes == 0) {
                        decoder->send(chunk, bytes_to_send_adpcm);
                    } else { // Case 2: We have leftover -> complete the partial frame first

                        // Resize buffer to hold exactly the full frames we will output
                        state->adpcm_buffer.resize(bytes_to_send_adpcm);

                        // Copy only the bytes needed to complete the pending frame(s)
                        memcpy(state->adpcm_buffer.data() + remaining_bytes, chunk, chunk_bytes_needed);

                        // Send the completed frames from the temporary buffer
                        if (full_frames > 0)
                            decoder->send(state->adpcm_buffer.data(), bytes_to_send_adpcm);

                        // Update bytes_to_send to reflect only what was consumed from the current chunk
                        bytes_to_send = chunk_bytes_needed;

                        // Clear temporary buffer for next iteration
                        state->adpcm_buffer.clear();
                    }

                    // Compute leftover bytes that do not form a complete frame (typically end-of-buffer)
                    remaining_bytes = init_bytes_to_send - bytes_to_send_adpcm;
                    state->adpcm_buffer.resize(remaining_bytes);

                    // Store leftover bytes for the next iteration
                    if (remaining_bytes > 0)
                        memcpy(state->adpcm_buffer.data(), chunk + bytes_to_send_adpcm, remaining_bytes);
                } else {
                    // PCM case, send directly
                    decoder->send(chunk, bytes_to_send);
                }

                state->current_byte_position_in_buffer += bytes_to_send;
                state->bytes_consumed_since_key_on += bytes_to_send;
                state->total_bytes_consumed += bytes_to_send;
                // save he_adpcm state
                if (decoder->he_adpcm)
                    std::copy_n(decoder->adpcm_history, decoder->source_channels, state->adpcm_history);

                // Get the amount of samples about to be received from the decoder and dump the value in samples_count
                decoder->receive(nullptr, &samples_count);

                // Playback rate scaling
                float src_sample_rate = params->playback_frequency;
                if ((src_sample_rate >= 1.f) && ((params->playback_scalar != 1.f) || (static_cast<int>(src_sample_rate) != sample_rate))) {
                    LOG_INFO_ONCE("The currently running game requests playback rate scaling when decoding audio. Audio might crackle.");

                    // Received decoded samples from decoder
                    std::vector<uint8_t> decoded_data(samples_count.samples * sizeof(float) * 2, 0);

                    // Receive the samples processed by the decoder
                    decoder->receive(decoded_data.data(), nullptr);

                    // resample the audio
                    if (params->playback_scalar != 1.0f)
                        src_sample_rate *= params->playback_scalar;

                    if (!state->swr || state->reset_swr) {
                        if (state->swr)
                            swr_free(&state->swr);

                        AVChannelLayout layout_stereo = AV_CHANNEL_LAYOUT_STEREO;
                        int ret = swr_alloc_set_opts2(&state->swr,
                            &layout_stereo, AV_SAMPLE_FMT_FLT, sample_rate,
                            &layout_stereo, AV_SAMPLE_FMT_FLT, static_cast<int>(src_sample_rate),
                            0, nullptr);
                        assert(ret == 0);

                        ret = swr_init(state->swr);
                        assert(ret == 0);
                        state->reset_swr = false;
                    }
                    int scaled_samples_amount = swr_get_out_samples(state->swr, samples_count.samples);
                    std::vector<uint8_t> scaled_data(scaled_samples_amount * sizeof(float) * 2, 0);

                    uint8_t *scaled_dest_data = scaled_data.data();
                    const uint8_t *scaled_src_data = decoded_data.data();
                    scaled_samples_amount = swr_convert(state->swr, &scaled_dest_data, scaled_samples_amount, &scaled_src_data, samples_count.samples);
                    assert(scaled_samples_amount > 0);

                    // Get current size of audio queue for processed samples in memory
                    const uint32_t current_count = state->decoded_samples_pending * sizeof(float) * 2;

                    // Allocate memory to accommodate the result of the scaling process into the queue for the final audio buffer
                    data.extra_storage.resize(current_count + scaled_samples_amount * sizeof(float) * 2);

                    // Pass scaled audio data into the queue for the final audio buffer
                    memcpy(data.extra_storage.data() + current_count, scaled_data.data(), scaled_samples_amount * sizeof(float) * 2);

                } else {
                    // Get current size of audio buffer for processed samples in memory
                    const uint32_t current_count = state->decoded_samples_pending * sizeof(float) * 2;

                    // Increase the size the audio buffer for processed samples to accommodate the new about-to-be-received samples
                    data.extra_storage.resize(current_count + samples_count.samples * sizeof(float) * 2);

                    // Receive the samples processed by the decoder and append them to the buffer of already processed samples
                    decoder->receive(data.extra_storage.data() + current_count, nullptr);
                }
            }

            uint32_t bytes_left_in_buffer = data.extra_storage.size();
            uint32_t samples_to_take_per_channel = bytes_left_in_buffer / sizeof(float) / 2;

            state->decoded_samples_pending = samples_to_take_per_channel;
        }
    }

    uint32_t samples_to_be_passed = std::min<uint32_t>(state->decoded_samples_pending, granularity);

    auto const new_size = 2 * sizeof(float) * (state->decoded_samples_passed + granularity);
    if (data.extra_storage.size() < new_size) {
        data.extra_storage.resize(new_size);
    }
    uint8_t *data_ptr = data.extra_storage.data();
    data_ptr += 2 * sizeof(float) * state->decoded_samples_passed;

    data.parent->products[0].data = data_ptr;

    state->decoded_samples_pending -= samples_to_be_passed;
    state->decoded_samples_passed += samples_to_be_passed;
    state->samples_generated_since_key_on += samples_to_be_passed * params->channels;
    state->samples_generated_total += samples_to_be_passed * params->channels;

    return finished;
}
} // namespace ngs
