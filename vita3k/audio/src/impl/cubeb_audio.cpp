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

#include "audio/impl/cubeb_audio.h"
#include "util/log.h"

#include <algorithm>
#include <cstdint>

// scales S16LE samples in place, used to push the volume past the 1.0 ceiling cubeb enforces natively
static void apply_extra_gain(uint8_t *buffer, int nb_bytes, float gain) {
    if (gain == 1.0f)
        return;

    int16_t *samples = reinterpret_cast<int16_t *>(buffer);
    const int nb_samples = nb_bytes / sizeof(int16_t);
    for (int i = 0; i < nb_samples; i++) {
        const float amplified = samples[i] * gain;
        samples[i] = static_cast<int16_t>(std::clamp(amplified, static_cast<float>(INT16_MIN), static_cast<float>(INT16_MAX)));
    }
}

static long impl_cubeb_audio_callback(cubeb_stream *stream, void *user_data, const void *input, void *output, long nframes) {
    assert(user_data != nullptr);
    assert(stream != nullptr);
    CubebAudioOutPort *port = static_cast<CubebAudioOutPort *>(user_data);
    uint8_t *output_buffer = static_cast<uint8_t *>(output);

    const size_t bytes_to_give = static_cast<size_t>(nframes) * port->spec.channels * sizeof(uint16_t);
    const size_t bytes_given = port->buffers.pop(output_buffer, bytes_to_give);
    apply_extra_gain(output_buffer, static_cast<int>(bytes_given), port->extra_gain.load(std::memory_order_relaxed));

    return nframes;
}

static void impl_cubeb_state_callback(cubeb_stream *stm, void *user, cubeb_state state) {
    if (state == CUBEB_STATE_ERROR) {
        LOG_ERROR("Cubeb audio stream encountered an error");
        auto &port = *static_cast<CubebAudioOutPort *>(user);
        port.stopping = true;
        port.buffers.wake();
    }
}

CubebAudioOutPort::~CubebAudioOutPort() {
    if (out_stream) {
        cubeb_stream_stop(out_stream);
        cubeb_stream_destroy(out_stream);
    }
}

CubebAudioAdapter::CubebAudioAdapter(AudioState &audio_state)
    : AudioAdapter(audio_state) {}

CubebAudioAdapter::~CubebAudioAdapter() {
    if (cubeb_ctx)
        cubeb_destroy(cubeb_ctx);
}

bool CubebAudioAdapter::init() {
    if (cubeb_init(&cubeb_ctx, "Vita3K audio", nullptr) != CUBEB_OK) {
        LOG_ERROR("Could not initialize cubeb context");
        return false;
    }

    return true;
}

AudioOutPortPtr CubebAudioAdapter::open_port(int nb_channels, int freq, int nb_sample) {
    std::shared_ptr<CubebAudioOutPort> port = std::make_shared<CubebAudioOutPort>();
    port->spec = {
        // all the ps vita samples are signed 16 bits low edian
        .format = CUBEB_SAMPLE_S16LE,
        .rate = static_cast<uint32_t>(freq),
        .channels = static_cast<uint32_t>(nb_channels),
        .layout = CUBEB_LAYOUT_UNDEFINED,
        // we could use the params of sceAudioOutOpenPort to select some prefs, although I don't think it will change anything
        .prefs = CUBEB_STREAM_PREF_NONE
    };

    uint32_t latency = 0;
    if (cubeb_get_min_latency(cubeb_ctx, &port->spec, &latency) != CUBEB_OK) {
        LOG_ERROR("Could not determine cubeb stream latency");
        return nullptr;
    }

    if (cubeb_stream_init(cubeb_ctx, &port->out_stream, "Vita3K audio out", nullptr, nullptr, nullptr,
            &port->spec, latency, impl_cubeb_audio_callback, impl_cubeb_state_callback, port.get())
        != CUBEB_OK) {
        LOG_ERROR("Could not initialize cubeb stream");
        return nullptr;
    }

    port->len_bytes = nb_sample * nb_channels * sizeof(uint16_t);

    // allocate enough buffers to be able to satisfy a callback (+1 to make sure one buffer can be ready)
    const size_t nb_buffers = (uint64_t(latency) + nb_sample - 1) / nb_sample + 1;
    port->buffers.configure(nb_buffers, port->len_bytes);

    if (cubeb_stream_start(port->out_stream) != CUBEB_OK) {
        LOG_ERROR("Could not start cubeb stream");
        return nullptr;
    }
    return port;
}

void CubebAudioAdapter::audio_output(AudioOutPort &out_port, const void *buffer) {
    CubebAudioOutPort &port = static_cast<CubebAudioOutPort &>(out_port);

    if (buffer)
        port.buffers.push(buffer, port.stopping);
}

void CubebAudioAdapter::set_volume(AudioOutPort &out_port, float volume) {
    CubebAudioOutPort &port = static_cast<CubebAudioOutPort &>(out_port);
    // cubeb rejects volumes outside [0.0, 1.0]. Anything past that is instead
    // applied to the samples in software by the audio callback (extra_gain).
    cubeb_stream_set_volume(port.out_stream, std::min(volume, 1.0f));
    port.extra_gain.store(std::max(volume, 1.0f), std::memory_order_relaxed);
}

void CubebAudioAdapter::switch_state(const bool pause) {
    for (auto &[_, out_port] : state.out_ports) {
        CubebAudioOutPort &port = static_cast<CubebAudioOutPort &>(*out_port);
        if (pause)
            cubeb_stream_stop(port.out_stream);
        else
            cubeb_stream_start(port.out_stream);
    }
}

void CubebAudioAdapter::wake_all_ports() {
    for (auto &[_, port_ptr] : state.out_ports) {
        auto &port = static_cast<CubebAudioOutPort &>(*port_ptr);
        port.buffers.wake();
    }
}
