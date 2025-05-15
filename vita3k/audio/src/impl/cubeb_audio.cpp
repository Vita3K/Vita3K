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

#include "audio/impl/cubeb_audio.h"

#include "kernel/thread/thread_state.h"

#include "util/log.h"

static long impl_cubeb_audio_callback(cubeb_stream *stream, void *user_data, const void *input, void *output, long nframes) {
    assert(user_data != nullptr);
    assert(stream != nullptr);
    CubebAudioOutPort *port = static_cast<CubebAudioOutPort *>(user_data);
    uint8_t *output_buffer = static_cast<uint8_t *>(output);

    int bytes_given = 0;
    const int bytes_to_give = nframes * port->spec.channels * sizeof(uint16_t);
    while (bytes_given < bytes_to_give) {
        if (port->nb_buffers_ready == 0) {
            // no data available, should we wait for it or return nothing?
            // return nothing for now
            break;
        }

        AudioBuffer &audio_buffer = port->audio_buffers[port->next_audio_buffer];
        // compute the number of bytes we can copy from this buffer to the output
        const int bytes_to_copy = std::min(bytes_to_give - bytes_given, port->len_bytes - audio_buffer.buffer_position);
        memcpy(&output_buffer[bytes_given], &audio_buffer.buffer[audio_buffer.buffer_position], bytes_to_copy);
        audio_buffer.buffer_position += bytes_to_copy;

        if (audio_buffer.buffer_position == port->len_bytes) {
            // if we are done with this buffer, tell it
            std::unique_lock<std::mutex> lock(port->mutex);
            port->next_audio_buffer = (port->next_audio_buffer + 1) % port->audio_buffers.size();
            port->nb_buffers_ready--;
            lock.unlock();
            port->cond_var.notify_one();
        }

        bytes_given += bytes_to_copy;
    }

    return nframes;
}

static void impl_cubeb_state_callback(cubeb_stream *stm, void *user, cubeb_state state) {
    // we must give this function as a parameter to cubeb, but we don't care about it
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

    uint32_t latency;
    cubeb_get_min_latency(cubeb_ctx, &port->spec, &latency);

    if (cubeb_stream_init(cubeb_ctx, &port->out_stream, "Vita3K audio out", nullptr, nullptr, nullptr,
            &port->spec, latency, impl_cubeb_audio_callback, impl_cubeb_state_callback, port.get())
        != CUBEB_OK) {
        LOG_ERROR("Could not initialize cubeb stream");
        return nullptr;
    }

    port->len_bytes = nb_sample * nb_channels * sizeof(uint16_t);

    // allocate enough buffers to be able to satisfy a callback (+1 to make sure one buffer can be ready)
    const int nb_buffers = (latency + nb_sample - 1) / nb_sample + 1;
    port->audio_buffers.resize(nb_buffers);
    for (AudioBuffer &audio_buffer : port->audio_buffers) {
        // initialize all the buffers
        audio_buffer.buffer.resize(port->len_bytes);
        audio_buffer.buffer_position = 0;
    }

    cubeb_stream_start(port->out_stream);
    return port;
}

void CubebAudioAdapter::audio_output(ThreadState &thread, AudioOutPort &out_port, const void *buffer) {
    CubebAudioOutPort &port = static_cast<CubebAudioOutPort &>(out_port);

    std::unique_lock<std::mutex> lock(port.mutex);
    if (port.nb_buffers_ready == port.audio_buffers.size()) {
        // is it really useful to update the thread status?
        thread.update_status(ThreadStatus::wait);
        port.cond_var.wait(lock);
        thread.update_status(ThreadStatus::run);
    }

    assert(port.nb_buffers_ready < port.audio_buffers.size());
    if (buffer) {
        // the buffer can be empty to drain the port
        int next_buffer_pos = (port.next_audio_buffer + port.nb_buffers_ready) % port.audio_buffers.size();
        // we could unlock the lock here and re-lock it right after, but will this be faster?
        memcpy(port.audio_buffers[next_buffer_pos].buffer.data(), buffer, port.len_bytes);
        port.audio_buffers[next_buffer_pos].buffer_position = 0;
        port.nb_buffers_ready++;
    }

    lock.unlock();
    port.cond_var.notify_one();
}

void CubebAudioAdapter::set_volume(AudioOutPort &out_port, float volume) {
    CubebAudioOutPort &port = static_cast<CubebAudioOutPort &>(out_port);
    cubeb_stream_set_volume(port.out_stream, volume);
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
