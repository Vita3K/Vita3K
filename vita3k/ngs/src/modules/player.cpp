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

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ngs {

std::unique_ptr<ModuleLogicalState> PlayerModule::create_logical_state() const {
    return std::make_unique<PlayerLogicalState>();
}

std::unique_ptr<ModuleRuntimeState> PlayerModule::create_runtime_state() const {
    return std::make_unique<PlayerRuntimeState>();
}

void PlayerModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    PlayerLogicalState *logical = data.get_logical_state<PlayerLogicalState>();
    SceNgsPlayerParams *params = data.get_parameters<SceNgsPlayerParams>(mem);

    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE) {
        state->samples_generated_since_key_on = 0;
        state->bytes_consumed_since_key_on = 0;
        state->current_buffer = params->start_buffer;
        state->current_byte_position_in_buffer = params->start_bytes;
        logical->current_loop_count = 0;
        logical->decoded_pcm.clear();
        logical->rate_resampler.reset();
        logical->adpcm_buffer.clear();

        std::memset(&logical->adpcm_history, 0, sizeof(logical->adpcm_history));
    } else if (data.parent->is_keyed_off) {
        state->current_buffer = params->start_buffer;
        state->current_byte_position_in_buffer = params->start_bytes;
        logical->current_loop_count = 0;
        logical->rate_resampler.reset();
    }
}

void PlayerModule::on_param_change(const MemState &mem, ModuleData &data) {
    PlayerLogicalState *logical = data.get_logical_state<PlayerLogicalState>();
    const SceNgsPlayerParams *old_params = reinterpret_cast<SceNgsPlayerParams *>(data.last_info.data());
    SceNgsPlayerParams *new_params = static_cast<SceNgsPlayerParams *>(data.info.data.get(mem));

    // check for invalid playback values
    const auto is_invalid_playback_value = [](const float playback_value, const float max_value) {
        return std::isnan(playback_value) || (playback_value < 0.f) || (playback_value > max_value);
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

    // if playback scaling changed, reset the rate resampler
    if (old_params->playback_frequency != new_params->playback_frequency || old_params->playback_scalar != new_params->playback_scalar) {
        ADPCMHistory hist_empty{};
        std::fill_n(logical->adpcm_history, SCE_NGS_PLAYER_MAX_PCM_CHANNELS, hist_empty);

        logical->rate_resampler.reset();
    }
}

void PlayerModule::set_default_preset(const MemState &mem, ModuleData &data) {
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    PlayerLogicalState *logical = data.get_logical_state<PlayerLogicalState>();

    LOG_WARN_ONCE("Player reset state");
    *state = {};
    *logical = {};
}

bool PlayerModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    SceNgsPlayerParams *params = data.get_parameters<SceNgsPlayerParams>(mem);
    SceNgsPlayerStates *state = data.get_state<SceNgsPlayerStates>();
    PlayerLogicalState *logical = data.get_logical_state<PlayerLogicalState>();
    PlayerRuntimeState *runtime = data.get_runtime_state<PlayerRuntimeState>();
    bool finished = false;

    const int32_t sample_rate = data.parent->rack->system->sample_rate;
    const int32_t granularity = data.parent->rack->system->granularity;

    // fix right now because it might be set by default in SceNgsVoiceInit, which we do not support
    if (params->channels == 0) {
        params->channels = 2;
    }

    // If decoder hasn't been initialized
    if (!runtime->decoder) {
        // Create decoder specifying the desired destination sample rate
        runtime->decoder = std::make_unique<PCMDecoderState>(static_cast<float>(sample_rate));
    }

    logical->decoded_pcm.compact();

    while (static_cast<int>(logical->decoded_pcm.available_frames()) < granularity) {
        if ((state->current_buffer == -1)
            || !params->buffer_params[state->current_buffer].buffer
            || (params->buffer_params[state->current_buffer].bytes_count == 0)) {
            // Stop processing if no valid buffer is available or if the buffer is empty
            finished = true;
            break;
        } else if (state->current_byte_position_in_buffer >= params->buffer_params[state->current_buffer].bytes_count) {
            const int32_t prev_index = state->current_buffer;
            state->current_byte_position_in_buffer = 0;
            logical->current_loop_count++;

            voice_lock.unlock();
            scheduler_lock.unlock();

            // Enable looping over the buffer if needed
            if (params->buffer_params[state->current_buffer].loop_count != -1
                && logical->current_loop_count > params->buffer_params[state->current_buffer].loop_count) {
                state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
                logical->current_loop_count = 0;

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
                data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_LOOPED_BUFFER, logical->current_loop_count,
                    params->buffer_params[state->current_buffer].buffer.address());
            }

            scheduler_lock.lock();
            voice_lock.lock();
        }

        runtime->decoder->source_channels = params->channels;
        runtime->decoder->source_frequency = params->playback_frequency;
        // Enable ADPCM mode on the decoder if needed, and restore state
        runtime->decoder->he_adpcm = static_cast<bool>(params->type);
        if (runtime->decoder->he_adpcm) {
            std::copy_n(logical->adpcm_history, runtime->decoder->source_channels, runtime->decoder->adpcm_history);
        }

        auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem);

        DecoderSize samples_count;
        // we need to know how many samples (not bytes!) we need to send (just enough for the system granularity)
        uint32_t samples_needed = granularity - logical->decoded_pcm.available_frames();

        if (params->playback_scalar != 1.0f) {
            samples_needed = static_cast<uint32_t>(samples_needed * params->playback_scalar) + 0x10;
        }
        if (static_cast<int>(params->playback_frequency) != sample_rate) {
            samples_needed = static_cast<uint32_t>((samples_needed * params->playback_frequency) / sample_rate) + 0x10;
        }

        // Convert samples count to actual bytes count that we need
        uint32_t bytes_to_send;
        if (runtime->decoder->he_adpcm) {
            bytes_to_send = (samples_needed + 27) / 28 * params->channels * 16;
        } else {
            bytes_to_send = samples_needed * params->channels * sizeof(int16_t);
        }

        // makes the value 4 bits aligned so we have no issue with decoding, adpcm or not and whether the sound is mono or stereo
        bytes_to_send = std::min<uint32_t>(bytes_to_send, params->buffer_params[state->current_buffer].bytes_count - state->current_byte_position_in_buffer);

        const uint8_t *chunk = input + state->current_byte_position_in_buffer;
        bool decoded_new_audio = true;
        uint32_t consumed_bytes = bytes_to_send;

        if (runtime->decoder->he_adpcm) {
            const uint32_t carried_bytes = static_cast<uint32_t>(logical->adpcm_buffer.size());
            const auto init_bytes_to_send = bytes_to_send;
            const uint32_t frame_size = 0x10 * params->channels;
            // We may have a partial HE-ADPCM frame carried over from the previous chunk,
            // so combine staged bytes with the current input before deciding how much is decodable.
            const auto combined_bytes = carried_bytes + bytes_to_send;
            const uint32_t full_frames = combined_bytes / frame_size;
            const uint32_t bytes_to_send_adpcm = full_frames * frame_size;
            const uint32_t chunk_bytes_needed = (bytes_to_send_adpcm > carried_bytes) ? (bytes_to_send_adpcm - carried_bytes) : 0;

            // HE-ADPCM requires full frames (0x10 bytes per channel).
            // We accumulate leftover bytes from previous chunks until we have enough to form one or more complete frames, then send them in a single block.
            decoded_new_audio = (bytes_to_send_adpcm != 0);
            if (carried_bytes == 0) {
                // Case 1: No leftover -> frames can be sent directly from the input chunk
                if (decoded_new_audio) {
                    runtime->decoder->send(chunk, bytes_to_send_adpcm);
                }
            } else {
                // Case 2: We have leftover -> complete the partial frame first
                const size_t old_size = logical->adpcm_buffer.size();
                logical->adpcm_buffer.resize(old_size + init_bytes_to_send);
                std::memcpy(logical->adpcm_buffer.data() + old_size, chunk, init_bytes_to_send);

                if (decoded_new_audio) {
                    runtime->decoder->send(logical->adpcm_buffer.data(), bytes_to_send_adpcm);
                }
            }

            // Compute leftover bytes that do not form a complete frame (typically end-of-buffer)
            const uint32_t leftover_bytes = combined_bytes - bytes_to_send_adpcm;
            logical->adpcm_buffer.resize(combined_bytes);
            if (leftover_bytes > 0) {
                if (carried_bytes == 0) {
                    std::memcpy(logical->adpcm_buffer.data(), chunk + chunk_bytes_needed, leftover_bytes);
                } else if (bytes_to_send_adpcm > 0) {
                    // Keep only the undecoded tail so the next iteration can complete the frame
                    std::memmove(logical->adpcm_buffer.data(), logical->adpcm_buffer.data() + bytes_to_send_adpcm, leftover_bytes);
                }
            }
            logical->adpcm_buffer.resize(leftover_bytes);

            consumed_bytes = init_bytes_to_send;
        } else {
            runtime->decoder->send(chunk, bytes_to_send);
        }

        state->current_byte_position_in_buffer += consumed_bytes;
        state->bytes_consumed_since_key_on += consumed_bytes;
        state->total_bytes_consumed += consumed_bytes;
        // save he_adpcm state
        if (runtime->decoder->he_adpcm && decoded_new_audio) {
            std::copy_n(runtime->decoder->adpcm_history, runtime->decoder->source_channels, logical->adpcm_history);
        }

        if (!decoded_new_audio) {
            continue;
        }

        runtime->decoder->receive(nullptr, &samples_count);

        // Playback rate scaling
        float src_sample_rate = params->playback_frequency;
        if ((src_sample_rate >= 1.f) && ((params->playback_scalar != 1.f) || (static_cast<int>(src_sample_rate) != sample_rate))) {
            runtime->decoded_chunk.resize(static_cast<size_t>(samples_count.samples) * sizeof(float) * 2);
            runtime->decoder->receive(runtime->decoded_chunk.data(), nullptr);

            if (params->playback_scalar != 1.0f) {
                src_sample_rate *= params->playback_scalar;
            }

            ensure_stereo_rate_resampler(runtime->rate_resampler, logical->rate_resampler,
                static_cast<int>(src_sample_rate), sample_rate);
            process_stereo_rate_resampler(runtime->rate_resampler, logical->rate_resampler,
                runtime->decoded_chunk.data(), samples_count.samples, logical->decoded_pcm);

        } else {
            uint8_t *decoded_dest = logical->decoded_pcm.append_uninitialized_bytes(samples_count.samples);
            runtime->decoder->receive(decoded_dest, nullptr);
        }
    }

    const uint32_t available_samples = logical->decoded_pcm.available_frames();
    const uint32_t samples_to_be_passed = std::min<uint32_t>(available_samples, granularity);

    if (available_samples >= static_cast<uint32_t>(granularity)) {
        data.parent->products[0].data = logical->decoded_pcm.read_bytes();
    } else {
        data.ensure_scratch_size(static_cast<size_t>(granularity) * sizeof(float) * 2);
        std::fill(data.scratch_data.begin(), data.scratch_data.end(), 0);
        if (available_samples > 0) {
            std::memcpy(data.scratch_data.data(), logical->decoded_pcm.read_bytes(), static_cast<size_t>(available_samples) * sizeof(float) * 2);
        }
        data.parent->products[0].data = data.scratch_data.data();
    }

    logical->decoded_pcm.consume_frames(samples_to_be_passed);
    state->samples_generated_since_key_on += samples_to_be_passed * params->channels;
    state->samples_generated_total += samples_to_be_passed * params->channels;

    return finished;
}

void PlayerModule::cleanup_voice_state(ModuleData &data) {
    if (auto *runtime = static_cast<PlayerRuntimeState *>(data.runtime_state.get())) {
        destroy_stereo_rate_resampler(runtime->rate_resampler);
    }
    data.runtime_state.reset();
}

} // namespace ngs
