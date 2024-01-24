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

#include <ngs/modules/atrac9.h>
#include <util/log.h>

extern "C" {
#include <libswresample/swresample.h>
}

namespace ngs {

SwrContext *Atrac9Module::swr_mono_to_stereo = nullptr;
SwrContext *Atrac9Module::swr_stereo = nullptr;

void Atrac9Module::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    SceNgsAT9States *state = data.get_state<SceNgsAT9States>();
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE) {
        state->samples_generated_since_key_on = 0;
        state->bytes_consumed_since_key_on = 0;
        state->current_byte_position_in_buffer = 0;
        state->current_loop_count = 0;
        state->current_buffer = 0;

        memset(&state->saved_state, 0, sizeof(state->saved_state));
        if (last_state == state)
            last_state = nullptr;
    } else if (data.parent->is_keyed_off) {
        state->current_byte_position_in_buffer = 0;
        state->current_loop_count = 0;
        state->current_buffer = 0;
    }
}

void Atrac9Module::on_param_change(const MemState &mem, ModuleData &data) {
    SceNgsAT9States *state = data.get_state<SceNgsAT9States>();
    const SceNgsAT9Params *old_params = reinterpret_cast<SceNgsAT9Params *>(data.last_info.data());
    const SceNgsAT9Params *new_params = static_cast<SceNgsAT9Params *>(data.info.data.get(mem));

    // if playback scaling changed, reset the resampler
    if (state->swr && (old_params->playback_frequency != new_params->playback_frequency || old_params->playback_scalar != new_params->playback_scalar)) {
        swr_free(&state->swr);
    }
}

bool Atrac9Module::decode_more_data(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, const SceNgsAT9Params *params, SceNgsAT9States *state, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    const SceNgsAT9BufferParams &bufparam = params->buffer_params[state->current_buffer];

    if (!data.extra_storage.empty()) {
        data.extra_storage.erase(data.extra_storage.begin(), data.extra_storage.begin() + state->decoded_passed * sizeof(float) * 2);
        state->decoded_passed = 0;
    }

    if (decoder && last_state != nullptr && last_state != state) {
        // we changed voices, need to export the previous data
        decoder->export_state(&last_state->saved_state);
    }

    // re-create the decoder if necessary
    if (!decoder || params->config_data != last_config) {
        decoder = std::make_unique<Atrac9DecoderState>(params->config_data);
        last_config = params->config_data;
    }

    if (last_state != state) {
        // we changed voices, need to import the new decoder state
        decoder->load_state(&state->saved_state);
        last_state = state;
    }

    if (state->current_byte_position_in_buffer >= bufparam.bytes_count) {
        const int32_t prev_index = state->current_buffer;

        voice_lock.unlock();
        scheduler_lock.unlock();

        state->current_loop_count++;
        state->current_byte_position_in_buffer = 0;

        if ((bufparam.loop_count != -1) && (state->current_loop_count > bufparam.loop_count)) {
            state->current_buffer = bufparam.next_buffer_index;
            state->current_loop_count = 0;

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
            data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_LOOPED_BUFFER, state->current_loop_count,
                params->buffer_params[state->current_buffer].buffer.address());
        }

        scheduler_lock.lock();
        voice_lock.lock();

        // re-call this function
        return true;
    }
    // now we are sure we have a buffer with some data in it
    uint8_t *input = bufparam.buffer.cast<uint8_t>().get(mem) + state->current_byte_position_in_buffer;

    const uint32_t superframe_size = decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);
    uint32_t frame_bytes_gotten = bufparam.bytes_count - state->current_byte_position_in_buffer;
    if (frame_bytes_gotten < superframe_size || !temp_buffer.empty()) {
        // the superframe overlaps two buffers...
        uint32_t bytes_transferred = std::min<uint32_t>(frame_bytes_gotten, superframe_size - temp_buffer.size());
        uint32_t old_size = temp_buffer.size();
        temp_buffer.resize(old_size + bytes_transferred);
        memcpy(temp_buffer.data() + old_size, input, bytes_transferred);

        if (temp_buffer.size() < superframe_size) {
            // continue getting data
            state->current_byte_position_in_buffer = bufparam.bytes_count;
            return true;
        }
        // make the byte position negative, will be positive at the end
        state->current_byte_position_in_buffer = -(int32_t)old_size;
        input = temp_buffer.data();
    }

    size_t curr_pos = state->decoded_samples_pending * sizeof(float) * 2;

    const uint32_t samples_per_frame = decoder->get(DecoderQuery::AT9_SAMPLE_PER_FRAME);
    const uint32_t samples_per_superframe = decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME);
    // we need to account for sampled skipped at the beginning or the end of the buffer
    uint32_t decoded_size = samples_per_superframe;
    uint32_t decoded_start_offset = 0;

    // some games (like Muramasa) send the atrac9 files with the header, we need to skip it
    if (memcmp(input, "RIFF", 4) == 0
        && memcmp(input + 8, "WAVE", 4) == 0) {
        // file header is 12 bytes long
        input += 3 * sizeof(uint32_t);
        state->current_byte_position_in_buffer += 3 * sizeof(uint32_t);

        while (memcmp(input, "data", 4) != 0) {
            // each chunk has a 4-byte identifier followed by its size (minus 8) in an int32
            const int32_t header_data = 2 * sizeof(uint32_t) + *reinterpret_cast<int32_t *>(input + 4);
            state->current_byte_position_in_buffer += header_data;
            input += header_data;
        }

        state->current_byte_position_in_buffer += 2 * sizeof(uint32_t);
        return true;
    }

    // if the superframe is across two buffers, I don't know how to interpret the skipped samples (which are in the middle of the frame)...
    if (temp_buffer.empty()) {
        // remove skipped samples at the beginning and the end of the buffer
        // in case you have more than a superframe of samples skipped (I don't know if this can happen)
        const uint32_t sample_index = (state->current_byte_position_in_buffer / superframe_size) * samples_per_superframe;
        if (bufparam.samples_discard_start_off > sample_index) {
            // first chunk
            const uint32_t skipped_samples = std::min(samples_per_superframe, bufparam.samples_discard_start_off - sample_index);
            decoded_start_offset += skipped_samples;
            decoded_size -= skipped_samples;
        }

        const uint32_t samples_left_after = (frame_bytes_gotten / superframe_size - 1) * samples_per_superframe;
        if (bufparam.samples_discard_end_off > samples_left_after) {
            // last chunk
            decoded_size -= std::min(decoded_size, bufparam.samples_discard_end_off - samples_left_after);
        }
    }

    std::vector<uint8_t> decoded_superframe_samples(decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME) * sizeof(float) * 2);
    uint32_t decoded_superframe_pos = 0;
    bool got_decode_error = false;
    // decode a whole superframe at a time
    for (uint32_t frame = 0; frame < decoder->get(DecoderQuery::AT9_FRAMES_IN_SUPERFRAME); frame++) {
        if (!decoder->send(input, 0)) {
            got_decode_error = true;
            break;
        }

        // convert from int16 to float
        uint32_t const channel_count = decoder->get(DecoderQuery::CHANNELS);
        std::vector<uint8_t> temporary_bytes(samples_per_frame * sizeof(int16_t) * channel_count);
        DecoderSize decoder_size;
        decoder->receive(temporary_bytes.data(), &decoder_size);

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

        const uint8_t *swr_data_in = temporary_bytes.data();
        uint8_t *swr_data_out = decoded_superframe_samples.data() + decoded_superframe_pos;
        const int result = swr_convert(swr, &swr_data_out, decoder_size.samples, &swr_data_in, decoder_size.samples);

        decoded_superframe_pos += decoder_size.samples * sizeof(float) * 2;
        input += decoder->get_es_size();
        state->current_byte_position_in_buffer += decoder->get_es_size();
    }

    const int32_t sample_rate = data.parent->rack->system->sample_rate;
    if (params->playback_scalar != 1 || static_cast<int>(round(params->playback_frequency)) != sample_rate) {
        LOG_INFO_ONCE("The currently running game requests playback rate scaling when decoding audio. Audio might crackle.");

        // resample the audio
        int src_sample_rate = static_cast<int>(params->playback_frequency);
        if (params->playback_scalar != 1.0f)
            src_sample_rate *= params->playback_scalar;

        if (!state->swr) {
            AVChannelLayout layout_stereo = AV_CHANNEL_LAYOUT_STEREO;
            int ret = swr_alloc_set_opts2(&state->swr,
                &layout_stereo, AV_SAMPLE_FMT_FLT, sample_rate,
                &layout_stereo, AV_SAMPLE_FMT_FLT, src_sample_rate,
                0, nullptr);
            assert(ret == 0);

            ret = swr_init(state->swr);
            assert(ret == 0);
        }
        // assume the skipped samples happen before the scaling
        int scaled_samples_amount = swr_get_out_samples(state->swr, decoded_size);
        std::vector<uint8_t> scaled_data(scaled_samples_amount * sizeof(float) * 2, 0);

        uint8_t *scaled_dest_data = scaled_data.data();
        const uint8_t *scaled_src_data = decoded_superframe_samples.data() + decoded_start_offset * sizeof(float) * 2;
        scaled_samples_amount = swr_convert(state->swr, &scaled_dest_data, scaled_samples_amount, &scaled_src_data, decoded_size);
        assert(scaled_samples_amount > 0);

        // Allocate memory to accommodate the result of the scaling process into the queue for the final audio buffer
        data.extra_storage.resize(curr_pos + scaled_samples_amount * sizeof(float) * 2);

        // Pass scaled audio data into the queue for the final audio buffer
        memcpy(data.extra_storage.data() + curr_pos, scaled_data.data(), scaled_samples_amount * sizeof(float) * 2);
        decoded_size = scaled_samples_amount;

    } else {
        data.extra_storage.resize(curr_pos + decoded_size * sizeof(float) * 2);

        memcpy(data.extra_storage.data() + curr_pos, decoded_superframe_samples.data() + decoded_start_offset * sizeof(float) * 2, decoded_size * sizeof(float) * 2);
    }

    if (got_decode_error) {
        voice_lock.unlock();
        scheduler_lock.unlock();

        data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_DECODE_ERROR, state->current_byte_position_in_buffer,
            params->buffer_params[state->current_buffer].buffer.address());

        scheduler_lock.lock();
        voice_lock.lock();

        // flush or we'll get en error next time we cant to decode
        decoder->flush();
    }

    temp_buffer.clear();

    state->samples_generated_since_key_on += decoded_size * params->channels;
    state->samples_generated_total += decoded_size * params->channels;
    state->bytes_consumed_since_key_on += superframe_size;
    state->total_bytes_consumed += superframe_size;

    state->decoded_samples_pending += decoded_size;
    return true;
}

bool Atrac9Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    const SceNgsAT9Params *params = data.get_parameters<SceNgsAT9Params>(mem);
    SceNgsAT9States *state = data.get_state<SceNgsAT9States>();
    assert(state);

    if (state->current_buffer == -1
        || !params->buffer_params[state->current_buffer].buffer) {
        return true;
    }

    bool is_finished = false;
    // call decode more data until we either have an error or reached end of data
    while (static_cast<int32_t>(state->decoded_samples_pending) < data.parent->rack->system->granularity) {
        if (!decode_more_data(kern, mem, thread_id, data, params, state, scheduler_lock, voice_lock)) {
            is_finished = true;
            break;
        }
    }

    // make sure the buffer is big enough
    data.fill_to_fit_granularity();

    uint8_t *data_ptr = data.extra_storage.data() + 2 * sizeof(float) * state->decoded_passed;
    uint32_t samples_to_be_passed = data.parent->rack->system->granularity;

    data.parent->products[0].data = data_ptr;

    state->decoded_samples_pending = (state->decoded_samples_pending < samples_to_be_passed) ? 0 : (state->decoded_samples_pending - samples_to_be_passed);
    state->decoded_passed += samples_to_be_passed;

    return is_finished;
}
} // namespace ngs
