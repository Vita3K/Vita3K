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

#include <ngs/modules/atrac9.h>
#include <util/bytes.h>
#include <util/log.h>

namespace ngs::atrac9 {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_ATRAC9)
    , last_config(0) {}

void get_buffer_parameter(const std::uint32_t start_sample, const std::uint32_t num_samples, const std::uint32_t info, SkipBufferInfo &parameter) {
    const std::uint8_t sample_rate_index = ((info & (0b1111 << 12)) >> 12);
    const std::uint8_t block_rate_index = ((info & (0b111 << 9)) >> 9);
    const std::uint16_t frame_bytes = ((((info & 0xFF0000) >> 16) << 3) | ((info & (0b111 << 29)) >> 29)) + 1;
    const std::uint8_t superframe_index = (info & (0b11 << 27)) >> 27;

    // Calculate bytes per superframe.
    const std::uint32_t frame_per_superframe = 1 << superframe_index;
    const std::uint32_t bytes_per_superframe = frame_bytes * frame_per_superframe;

    // Calculate total superframe
    static const std::int8_t sample_rate_index_to_frame_sample_power[] = {
        6, 6, 7, 7, 7, 8, 8, 8, 6, 6, 7, 7, 7, 8, 8, 8
    };

    const std::uint32_t samples_per_frame = 1 << sample_rate_index_to_frame_sample_power[sample_rate_index];
    const std::uint32_t samples_per_superframe = samples_per_frame * frame_per_superframe;

    const std::uint32_t num_superframe = (num_samples + samples_per_superframe - 1) / samples_per_superframe;
    parameter.num_bytes = num_superframe * bytes_per_superframe;
    parameter.is_super_packet = (frame_per_superframe == 1) ? 0 : 1;

    const std::uint32_t start_superframe = (start_sample / superframe_index);
    parameter.start_byte_offset = start_superframe * bytes_per_superframe;
    parameter.start_skip = (start_sample - (start_superframe * samples_per_superframe));
    parameter.end_skip = (num_superframe * samples_per_superframe) - num_samples;
}

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

    assert(state);

    if ((state->current_buffer == -1) || (params->buffer_params[state->current_buffer].buffer.address() == 0)) {
        return true;
    }

    bool finished = false;
    // making this maybe to early...
    if (!decoder || (params->config_data != last_config)) {
        decoder = std::make_unique<Atrac9DecoderState>(params->config_data);
        last_config = params->config_data;
    }

    auto try_cycle_to_next_buffer = [&]() {
        if (state->current_byte_position_in_buffer >= params->buffer_params[state->current_buffer].bytes_count) {
            const std::int32_t prev_index = state->current_buffer;

            if (params->buffer_params[state->current_buffer].loop_count != -1) {
                state->current_loop_count++;

                if (state->current_loop_count > params->buffer_params[state->current_buffer].loop_count) {
                    state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
                    state->current_loop_count = 0;
                    state->current_byte_position_in_buffer = 0;

                    data.parent->voice_lock->unlock();

                    if (state->current_buffer == -1) {
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_DONE_ALL, 0, 0);
                        finished = true;
                        // TODO: Free all occupied input routes
                        //unroute_occupied(mem, voice);
                    } else {
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_DONE_ONE_BUFFER, prev_index,
                            params->buffer_params[state->current_buffer].buffer.address());
                    }

                    data.parent->voice_lock->lock();
                }
            } else {
                data.parent->voice_lock->unlock();
                data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_START_LOOP, state->current_loop_count,
                    params->buffer_params[state->current_buffer].buffer.address());
                data.parent->voice_lock->lock();
            }

            state->current_byte_position_in_buffer = 0;
        }
    };

    if (static_cast<std::uint32_t>(state->decoded_samples_pending) < data.parent->rack->system->granularity) {
        if (!data.extra_storage.empty()) {
            data.extra_storage.erase(data.extra_storage.begin(), data.extra_storage.begin() + state->decoded_passed * 8);
        }

        state->decoded_passed = 0;

        while (static_cast<std::int32_t>(state->decoded_samples_pending) < data.parent->rack->system->granularity) {
            const ngs::atrac9::BufferParameters *bufparam = &params->buffer_params[state->current_buffer];

            if ((state->current_buffer == -1) || (bufparam->bytes_count == 0)) {
                // Fill it then break
                data.fill_to_fit_granularity();
                finished = true;
                break;
            }

            std::uint32_t superframe_size = decoder->get_superframe_size();

            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            if (bufparam->bytes_count > 0) {
                auto *input = bufparam->buffer.cast<uint8_t>().get(mem) + state->current_byte_position_in_buffer;

                std::uint32_t frame_bytes_gotten = bufparam->bytes_count - state->current_byte_position_in_buffer;
                state->current_byte_position_in_buffer += superframe_size;

                std::vector<std::uint8_t> temporary_bytes;

                if (frame_bytes_gotten < superframe_size) {
                    while (frame_bytes_gotten < superframe_size) {
                        if (temporary_bytes.empty()) {
                            temporary_bytes.resize(frame_bytes_gotten);
                            std::memcpy(temporary_bytes.data(), input, frame_bytes_gotten);
                        }

                        try_cycle_to_next_buffer();

                        bufparam = &params->buffer_params[state->current_buffer];

                        if ((state->current_buffer == -1) || (bufparam->bytes_count == 0)) {
                            break;
                        }

                        std::uint32_t bytes_to_get_next = std::min<std::uint32_t>(superframe_size - frame_bytes_gotten, bufparam->bytes_count);
                        std::uint8_t *ptr_append = reinterpret_cast<std::uint8_t *>(bufparam->buffer.get(mem));

                        temporary_bytes.insert(temporary_bytes.end(), ptr_append, ptr_append + bytes_to_get_next);
                        frame_bytes_gotten += bytes_to_get_next;

                        state->current_byte_position_in_buffer += bytes_to_get_next;
                    }
                }

                if (!temporary_bytes.empty()) {
                    input = temporary_bytes.data();
                }

                const std::size_t curr_pos = state->decoded_samples_pending * sizeof(float) * 2;

                data.extra_storage.resize(curr_pos + decoder->get_samples_per_superframe() * sizeof(float) * 2);

                if (decoder->send(input, decoder->get_superframe_size())) {
                    decoder->receive(data.extra_storage.data() + curr_pos, nullptr);
                } else {
                    data.parent->voice_lock->unlock();
                    data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_DECODE_ERROR, state->current_byte_position_in_buffer,
                        params->buffer_params[state->current_buffer].buffer.address());
                    data.parent->voice_lock->lock();
                }

                state->samples_generated_since_key_on += decoder->get_samples_per_superframe();
                state->bytes_consumed_since_key_on += decoder->get_superframe_size();
                state->total_bytes_consumed += decoder->get_superframe_size();

                state->decoded_samples_pending += decoder->get_samples_per_superframe();
            }

            try_cycle_to_next_buffer();
        }
    }

    std::uint8_t *data_ptr = data.extra_storage.data() + 2 * sizeof(float) * state->decoded_passed;
    std::uint32_t samples_to_be_passed = data.parent->rack->system->granularity;

    data.parent->products[0].data = data_ptr;

    state->decoded_samples_pending = (state->decoded_samples_pending < samples_to_be_passed) ? 0 : (state->decoded_samples_pending - samples_to_be_passed);
    state->decoded_passed += samples_to_be_passed;

    return finished;
}
} // namespace ngs::atrac9
