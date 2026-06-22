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

#include <ngs/modules/atrac9.h>
#include <util/log.h>

extern "C" {
#include <libswresample/swresample.h>
}

#include <cassert>
#include <cmath>
#include <cstring>

namespace ngs {

SwrContext *Atrac9Module::swr_mono_to_stereo = nullptr;
SwrContext *Atrac9Module::swr_stereo = nullptr;

std::unique_ptr<ModuleLogicalState> Atrac9Module::create_logical_state() const {
    return std::make_unique<Atrac9LogicalState>();
}

std::unique_ptr<ModuleRuntimeState> Atrac9Module::create_runtime_state() const {
    return std::make_unique<Atrac9RuntimeState>();
}

void Atrac9Module::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    SceNgsAT9States *state = data.get_state<SceNgsAT9States>();
    Atrac9LogicalState *logical = data.get_logical_state<Atrac9LogicalState>();

    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE) {
        state->samples_generated_since_key_on = 0;
        state->bytes_consumed_since_key_on = 0;
        state->current_byte_position_in_buffer = 0;
        logical->current_loop_count = 0;
        state->current_buffer = 0;
        logical->decoded_pcm.clear();
        logical->rate_resampler.reset();
        logical->superframe_staging.clear();
        std::memset(&logical->saved_state, 0, sizeof(logical->saved_state));
    } else if (data.parent->is_keyed_off) {
        state->current_byte_position_in_buffer = 0;
        logical->current_loop_count = 0;
        state->current_buffer = 0;
        logical->rate_resampler.reset();
    }
}

void Atrac9Module::on_param_change(const MemState &mem, ModuleData &data) {
    Atrac9LogicalState *logical = data.get_logical_state<Atrac9LogicalState>();
    const SceNgsAT9Params *old_params = reinterpret_cast<SceNgsAT9Params *>(data.last_info.data());
    const SceNgsAT9Params *new_params = static_cast<SceNgsAT9Params *>(data.info.data.get(mem));

    if (old_params->playback_frequency != new_params->playback_frequency || old_params->playback_scalar != new_params->playback_scalar) {
        logical->rate_resampler.reset();
    }
}

bool Atrac9Module::decode_more_data(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, const SceNgsAT9Params *params, SceNgsAT9States *state, Atrac9LogicalState *logical, Atrac9RuntimeState *runtime, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    const SceNgsAT9BufferParams &bufparam = params->buffer_params[state->current_buffer];

    const bool config_changed = (params->config_data != logical->decoder_config);

    // re-create the decoder if necessary
    if (!runtime->decoder || config_changed) {
        runtime->decoder = std::make_unique<Atrac9DecoderState>(params->config_data);
        if (config_changed) {
            logical->decoder_config = params->config_data;
            std::memset(&logical->saved_state, 0, sizeof(logical->saved_state));
            logical->superframe_staging.clear();
        }
    }

    // re-apply the logical decoder state before we resume decoding
    runtime->decoder->load_state(&logical->saved_state);

    if (state->current_byte_position_in_buffer >= bufparam.bytes_count) {
        const int32_t prev_index = state->current_buffer;

        voice_lock.unlock();
        scheduler_lock.unlock();

        logical->current_loop_count++;
        state->current_byte_position_in_buffer = 0;

        if ((bufparam.loop_count != -1) && (logical->current_loop_count > bufparam.loop_count)) {
            state->current_buffer = bufparam.next_buffer_index;
            logical->current_loop_count = 0;

            if ((state->current_buffer == -1)
                || !params->buffer_params[state->current_buffer].buffer
                || (params->buffer_params[state->current_buffer].bytes_count == 0)) {
                data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_END_OF_DATA, 0, 0);

                // we are done
                scheduler_lock.lock();
                voice_lock.lock();
                return false;
            } else {
                data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_SWAPPED_BUFFER, prev_index,
                    params->buffer_params[state->current_buffer].buffer.address());
            }
        } else {
            data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_LOOPED_BUFFER, logical->current_loop_count,
                params->buffer_params[state->current_buffer].buffer.address());
        }

        scheduler_lock.lock();
        voice_lock.lock();

        // re-call this function
        return true;
    }

    // now we are sure we have a buffer with some data in it
    uint8_t *input = bufparam.buffer.cast<uint8_t>().get(mem) + state->current_byte_position_in_buffer;

    const uint32_t superframe_size = runtime->decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);
    uint32_t frame_bytes_gotten = bufparam.bytes_count - state->current_byte_position_in_buffer;
    if (frame_bytes_gotten < superframe_size || !logical->superframe_staging.empty()) {
        // the superframe overlaps two buffers...
        uint32_t bytes_transferred = std::min<uint32_t>(frame_bytes_gotten, superframe_size - static_cast<uint32_t>(logical->superframe_staging.size()));
        uint32_t old_size = static_cast<uint32_t>(logical->superframe_staging.size());
        logical->superframe_staging.resize(old_size + bytes_transferred);
        std::memcpy(logical->superframe_staging.data() + old_size, input, bytes_transferred);

        if (logical->superframe_staging.size() < superframe_size) {
            // continue getting data
            state->current_byte_position_in_buffer = bufparam.bytes_count;
            return true;
        }

        // make the byte position negative, will be positive at the end
        state->current_byte_position_in_buffer = -(int32_t)old_size;
        input = logical->superframe_staging.data();
    }

    const uint32_t samples_per_frame = runtime->decoder->get(DecoderQuery::AT9_SAMPLE_PER_FRAME);
    const uint32_t samples_per_superframe = runtime->decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME);
    // we need to account for sampled skipped at the beginning or the end of the buffer
    uint32_t decoded_size = samples_per_superframe;
    uint32_t decoded_start_offset = 0;

    // some games (like Muramasa) send the atrac9 files with the header, we need to skip it
    if (std::memcmp(input, "RIFF", 4) == 0
        && std::memcmp(input + 8, "WAVE", 4) == 0) {
        // file header is 12 bytes long
        input += 3 * sizeof(uint32_t);
        state->current_byte_position_in_buffer += 3 * sizeof(uint32_t);

        while (std::memcmp(input, "data", 4) != 0) {
            // each chunk has a 4-byte identifier followed by its size (minus 8) in an int32
            const int32_t header_data = 2 * sizeof(uint32_t) + *reinterpret_cast<int32_t *>(input + 4);
            state->current_byte_position_in_buffer += header_data;
            input += header_data;
        }

        state->current_byte_position_in_buffer += 2 * sizeof(uint32_t);
        return true;
    }

    // if the superframe is across two buffers, I don't know how to interpret the skipped samples (which are in the middle of the frame)...
    if (logical->superframe_staging.empty()) {
        // remove skipped samples at the beginning and the end of the buffer
        // in case you have more than a superframe of samples skipped (I don't know if this can happen)
        const uint32_t sample_index = (state->current_byte_position_in_buffer / superframe_size) * samples_per_superframe;
        if (bufparam.samples_discard_start_off > sample_index) {
            // first chunk
            const uint32_t skipped_samples = std::min(samples_per_superframe, static_cast<uint32_t>(bufparam.samples_discard_start_off - sample_index));
            decoded_start_offset += skipped_samples;
            decoded_size -= skipped_samples;
        }

        const uint32_t samples_left_after = (frame_bytes_gotten / superframe_size - 1) * samples_per_superframe;
        if (bufparam.samples_discard_end_off > samples_left_after) {
            // last chunk
            decoded_size -= std::min(decoded_size, static_cast<uint32_t>(bufparam.samples_discard_end_off - samples_left_after));
        }
    }

    runtime->decoded_superframe_samples.resize(static_cast<size_t>(runtime->decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME)) * sizeof(float) * 2);
    uint32_t decoded_superframe_pos = 0;
    bool got_decode_error = false;
    // decode a whole superframe at a time
    for (uint32_t frame = 0; frame < runtime->decoder->get(DecoderQuery::AT9_FRAMES_IN_SUPERFRAME); frame++) {
        if (!runtime->decoder->send(input, 0)) {
            got_decode_error = true;
            break;
        }

        // convert from int16 to float
        const uint32_t channel_count = runtime->decoder->get(DecoderQuery::CHANNELS);
        runtime->temporary_bytes.resize(static_cast<size_t>(samples_per_frame) * sizeof(int16_t) * channel_count);
        DecoderSize decoder_size;
        runtime->decoder->receive(runtime->temporary_bytes.data(), &decoder_size);

        SwrContext *swr;
        if (channel_count == 1) {
            if (!swr_mono_to_stereo) {
                AVChannelLayout layout_mono = AV_CHANNEL_LAYOUT_MONO;
                AVChannelLayout layout_stereo = AV_CHANNEL_LAYOUT_STEREO;

                int ret = swr_alloc_set_opts2(&swr_mono_to_stereo,
                    &layout_stereo, AV_SAMPLE_FMT_FLT, 480000,
                    &layout_mono, AV_SAMPLE_FMT_S16, 480000,
                    0, nullptr);
                assert(ret == 0);
                ret = swr_init(swr_mono_to_stereo);
                assert(ret == 0);
            }

            swr = swr_mono_to_stereo;
        } else {
            if (!swr_stereo) {
                AVChannelLayout layout_stereo = AV_CHANNEL_LAYOUT_STEREO;
                int ret = swr_alloc_set_opts2(&swr_stereo,
                    &layout_stereo, AV_SAMPLE_FMT_FLT, 480000,
                    &layout_stereo, AV_SAMPLE_FMT_S16, 480000,
                    0, nullptr);
                assert(ret == 0);
                ret = swr_init(swr_stereo);
                assert(ret == 0);
            }

            swr = swr_stereo;
        }

        const uint8_t *swr_data_in = runtime->temporary_bytes.data();
        uint8_t *swr_data_out = runtime->decoded_superframe_samples.data() + decoded_superframe_pos;
        swr_convert(swr, &swr_data_out, decoder_size.samples, &swr_data_in, decoder_size.samples);

        decoded_superframe_pos += decoder_size.samples * sizeof(float) * 2;
        input += runtime->decoder->get_es_size();
        state->current_byte_position_in_buffer += runtime->decoder->get_es_size();
    }

    const int32_t sample_rate = data.parent->rack->system->sample_rate;
    if (params->playback_scalar != 1 || static_cast<int>(std::round(params->playback_frequency)) != sample_rate) {
        LOG_INFO_ONCE("The currently running game requests playback rate scaling when decoding audio. Audio might crackle.");
        double src_sample_rate = params->playback_frequency;
        if (params->playback_scalar != 1.0f) {
            src_sample_rate *= params->playback_scalar;
        }

        ensure_stereo_rate_resampler(runtime->rate_resampler, logical->rate_resampler,
            static_cast<int>(src_sample_rate), sample_rate);
        // sssume skipped samples happen before playback-rate scaling
        decoded_size = process_stereo_rate_resampler(runtime->rate_resampler, logical->rate_resampler,
            runtime->decoded_superframe_samples.data() + decoded_start_offset * sizeof(float) * 2, decoded_size,
            logical->decoded_pcm);

    } else {
        logical->decoded_pcm.append_bytes(runtime->decoded_superframe_samples.data() + decoded_start_offset * sizeof(float) * 2, decoded_size);
    }

    if (got_decode_error) {
        voice_lock.unlock();
        scheduler_lock.unlock();

        data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_DECODE_ERROR, state->current_byte_position_in_buffer,
            params->buffer_params[state->current_buffer].buffer.address());

        scheduler_lock.lock();
        voice_lock.lock();

        // flush or we'll get an error next time we want to decode
        runtime->decoder->flush();
    }

    logical->superframe_staging.clear();

    state->samples_generated_since_key_on += decoded_size * params->channels;
    state->samples_generated_total += decoded_size * params->channels;
    state->bytes_consumed_since_key_on += superframe_size;
    state->total_bytes_consumed += superframe_size;
    runtime->decoder->export_state(&logical->saved_state);

    return true;
}

bool Atrac9Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    const SceNgsAT9Params *params = data.get_parameters<SceNgsAT9Params>(mem);
    SceNgsAT9States *state = data.get_state<SceNgsAT9States>();
    Atrac9LogicalState *logical = data.get_logical_state<Atrac9LogicalState>();
    Atrac9RuntimeState *runtime = data.get_runtime_state<Atrac9RuntimeState>();
    assert(state);

    if (state->current_buffer == -1
        || !params->buffer_params[state->current_buffer].buffer) {
        return true;
    }

    logical->decoded_pcm.compact();

    bool is_finished = false;
    // call decode more data until we either have an error or reached end of data
    while (static_cast<int32_t>(logical->decoded_pcm.available_frames()) < data.parent->rack->system->granularity) {
        if (!decode_more_data(kern, mem, thread_id, data, params, state, logical, runtime, scheduler_lock, voice_lock)) {
            is_finished = true;
            break;
        }
    }

    const uint32_t granularity = static_cast<uint32_t>(data.parent->rack->system->granularity);
    const uint32_t available_samples = logical->decoded_pcm.available_frames();
    const uint32_t samples_to_be_passed = std::min<uint32_t>(available_samples, granularity);

    if (available_samples >= granularity) {
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

    return is_finished;
}

void Atrac9Module::free_swr_contexts() {
    if (swr_mono_to_stereo) {
        swr_free(&swr_mono_to_stereo);
        swr_mono_to_stereo = nullptr;
    }
    if (swr_stereo) {
        swr_free(&swr_stereo);
        swr_stereo = nullptr;
    }
}

void Atrac9Module::cleanup_voice_state(ModuleData &data) {
    if (auto *runtime = static_cast<Atrac9RuntimeState *>(data.runtime_state.get())) {
        destroy_stereo_rate_resampler(runtime->rate_resampler);
    }
    data.runtime_state.reset();
}

} // namespace ngs
