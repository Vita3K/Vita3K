// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include "private.h"

#include <audio/state.h>
#include <codec/state.h>
#include <cubeb/cubeb.h>
#include <gui/functions.h>
#include <io/VitaIoDevice.h>

namespace gui {

// Structure to store PCM data
struct PcmData {
    std::vector<uint8_t> data;
    uint32_t position;
    std::mutex mutex;
};

static long data_callback(cubeb_stream *stream, void *user_ptr, const void *input_buffer, void *output_buffer, long nframes) {
    PcmData *pcm = static_cast<PcmData *>(user_ptr);
    // Lock the mutex to access pcm->position and pcm->data
    std::lock_guard<std::mutex> lock(pcm->mutex);

    // Calculate the number of bytes to copy (2 bytes per sample, 2 channels) and the length of the PCM data
    const uint32_t bytes_to_copy = nframes * 2 * sizeof(uint16_t);
    const uint32_t length = pcm->data.size();

    // If we reach the end of the buffer, return to zero (loop)
    if (pcm->position >= length)
        pcm->position = 0;

    // Calculate how much data is left to copy
    const size_t bytes_to_copy_now = std::min(bytes_to_copy, length - pcm->position);

    // Copy the PCM data into the output buffer
    std::memcpy(output_buffer, pcm->data.data() + pcm->position, bytes_to_copy_now);

    // Update the position
    pcm->position += bytes_to_copy_now;

    // If we copied less data than the buffer, fill the rest with silence
    if (bytes_to_copy_now < bytes_to_copy)
        std::memset((uint8_t *)output_buffer + bytes_to_copy_now, 0, bytes_to_copy - bytes_to_copy_now);

    // Return the number of frames copied
    return nframes;
}

// Callback called when the stream state changes
void state_callback(cubeb_stream *stream, void *user_ptr, cubeb_state state) {
    switch (state) {
    case CUBEB_STATE_DRAINED:
        LOG_INFO("Playback drained.");
        break;
    case CUBEB_STATE_ERROR:
        LOG_ERROR("Playback error.");
        break;
    default:
        break;
    }
}

std::thread playback_handle_thread;
cubeb_stream *stream = nullptr;
bool stop_requested;
std::condition_variable stop_condition;
cubeb *ctx = nullptr;
PcmData pcm_data;

void stop_bgm() {
    if (!stream) {
        return;
    }

    {
        // We lock the mutex to protect access to stop_requested
        std::lock_guard<std::mutex> lock(pcm_data.mutex);
        stop_requested = true;
    }

    // Notify the condition to wake up the thread
    stop_condition.notify_one();

    // Wait for the playback handle thread to finish
    if (playback_handle_thread.joinable())
        playback_handle_thread.join();

    // Stop and destroy the stream
    cubeb_stream_stop(stream);
    cubeb_stream_destroy(stream);
    stream = nullptr;

    // Reset the stop indicator
    stop_requested = false;
}

void switch_bgm_state(const bool pause) {
    if (!stream) {
        LOG_ERROR("The background music stream is not initialized!");
        return;
    }

    if (pause)
        cubeb_stream_stop(stream);
    else
        cubeb_stream_start(stream);
}

void set_bgm_volume(const float vol) {
    if (!stream) {
        LOG_ERROR("The background music stream is not initialized!");
        return;
    }

    cubeb_stream_set_volume(stream, vol / 100.f);
}

static void pcm_playback_handle_thread() {
    std::unique_lock<std::mutex> lock(pcm_data.mutex);

    // Wait until stop is requested
    stop_condition.wait(lock, [] { return stop_requested; });

    // Destroy the context
    cubeb_destroy(ctx);
}

void init_bgm_player(const float vol) {
    // Create a new Cubeb context
    if (cubeb_init(&ctx, "BGM Player", nullptr) != CUBEB_OK) {
        LOG_ERROR("Failed to initialize Cubeb context");
        return;
    }

    // Configure the audio output parameters
    cubeb_stream_params output_params;
    output_params.format = CUBEB_SAMPLE_S16LE; // Format PCM 16-bit, little-endian
    output_params.rate = 48000; // Sample rate 48 kHz
    output_params.channels = 2; // Stereo
    output_params.layout = CUBEB_LAYOUT_STEREO;

    // Get the minimum latency for the output parameters
    uint32_t latency;
    cubeb_get_min_latency(ctx, &output_params, &latency);

    // Lock the mutex to protect initialization of the Cubeb stream
    std::lock_guard<std::mutex> lock(pcm_data.mutex);
    if (cubeb_stream_init(ctx, &stream, "BGM Stream", nullptr, nullptr,
            nullptr, &output_params, latency, data_callback, state_callback, &pcm_data)
        != CUBEB_OK) {
        LOG_ERROR("Failed to initialize Cubeb stream");
        return;
    }

    set_bgm_volume(vol);

    // Start playback in a new thread
    playback_handle_thread = std::thread(pcm_playback_handle_thread);
}

struct RiffHeader {
    unsigned char chunk_id[4]; // Identifier for the "RIFF" chunk
    uint32_t chunk_size; // Size of the chunk
    unsigned char format[4]; // Format of the file
};

struct FmtChunk {
    unsigned char chunk_id[4]; // Identifier for the "fmt" chunk
    uint32_t chunk_data_size; // Size of the chunk
    uint16_t format_tag; // Format of the audio data
    uint16_t num_channels; // Number of channels
    uint32_t sample_rate; // Sample rate
    uint32_t byte_rate; // Byte rate
    uint16_t block_align; // Block alignment
    uint16_t bits_per_sample; // Bits per sample
    uint16_t extension_size; // Size of the extension
    uint16_t samples_per_block; // Samples per block
    uint32_t channel_mask; // Channel mask
    char codec_id[16]; // Codec ID
    uint32_t version; // Version
    uint32_t config_data; // Configuration data
    uint32_t reserved; // Reserved
};

struct FactChunk {
    unsigned char chunk_id[4]; // Identifier for the "fact" chunk
    uint32_t chunk_data_size; // Size of the chunk
    uint32_t total_samples; // Total number of samples in the file
    uint32_t input_overlap_delay; // Input overlap delay
    uint32_t input_overlap_encoder_delay; // Input overlap encoder delay
};

struct SmplChunk {
    unsigned char chunk_id[4]; // Identifier for the "smpl" chunk
    uint32_t chunk_size; // Size of the chunk
    uint32_t manufacturer; // Manufacturer code (MMA Manufacturer code)
    uint32_t product; // Product code
    uint32_t sample_period; // Period of one sample in nanoseconds
    uint32_t midi_unity_note; // MIDI note to play the sample at its original pitch
    uint32_t midi_pitch_fraction; // Fraction of the MIDI note
    uint32_t smpte_format; // SMPTE format for synchronization
    uint32_t smpte_offset; // SMPTE offset
    uint32_t num_sample_loops; // Number of sample loops
    uint32_t sampler_data; // Size of sampler-specific data (in bytes)
    uint32_t identifier; // Unique identifier for the loop
    uint32_t type; // Loop type (e.g., 0 for forward)
    uint32_t start; // Loop start point (in samples)
    uint32_t end; // Loop end point (in samples)
    uint32_t fraction; // Fraction of a sample length for fine-tuning
    uint32_t play_count; // Number of times to repeat the loop (0 for infinite)
};

struct DataChunk {
    unsigned char chunk_id[4]; // Identifier for the "data" chunk
    uint32_t size; // Size of the audio data
};

struct At9Header {
    RiffHeader riff;
    FmtChunk fmt;
    FactChunk fact;
    SmplChunk smpl;
    DataChunk data;
};

// Size of the AT9 header
static constexpr uint32_t AT9_HEADER_SIZE = sizeof(At9Header);

static bool decode_bgm(uint8_t *at9_data) {
    // Get the header of the AT9 file
    At9Header header;
    memcpy(&header, at9_data, AT9_HEADER_SIZE);

    // Set data size
    const uint32_t data_size = header.data.size;

    // Set the pointer to the ES data after the header
    at9_data += AT9_HEADER_SIZE;

    // Create a new decoder with the configuration data from the AT9 header
    Atrac9DecoderState decoder(header.fmt.config_data);

    const uint32_t super_frame_size = decoder.get(DecoderQuery::AT9_SUPERFRAME_SIZE);
    const auto es_size_max = std::min(super_frame_size, 1024u);

    uint32_t total_bytes_read = 0;

    // Get the maximum size of the PCM buffer for a single super frame
    const uint64_t max_pcm_size = static_cast<uint64_t>(decoder.get(DecoderQuery::AT9_SAMPLE_PER_FRAME) * decoder.get(DecoderQuery::CHANNELS)) * sizeof(int16_t);
    std::vector<uint8_t> bgm_data;

    // Decode the AT9 data
    while (total_bytes_read < data_size) {
        DecoderSize size;
        std::vector<uint8_t> pcm_buffer(max_pcm_size);

        // Send the data to the decoder and receive the decoded PCM data
        if (!decoder.send(at9_data)
            || !decoder.receive(pcm_buffer.data(), &size)) {
            LOG_ERROR("Error at offset {} while sending or decoding AT9.", total_bytes_read);
            return false;
        }

        // Get es size used, update the total bytes read and the es data to the next super frame
        const uint32_t es_size_used = std::min(decoder.get_es_size(), es_size_max);
        total_bytes_read += es_size_used;
        at9_data += es_size_used;

        // Get the size of the PCM data given by the decoder and insert it into the PCM  data buffer
        const uint64_t pcm_size_given = static_cast<uint64_t>(size.samples * decoder.get(DecoderQuery::CHANNELS)) * sizeof(int16_t);
        bgm_data.insert(bgm_data.end(), pcm_buffer.begin(), pcm_buffer.begin() + pcm_size_given);
    }

    // Check if PCM data is valid
    if (bgm_data.empty()) {
        LOG_ERROR("Error: PCM data is empty!");
        return false;
    }

    // Change the BGM theme with the new PCM data
    std::lock_guard<std::mutex> lock(pcm_data.mutex);
    pcm_data.data = bgm_data;

    return true;
}

bool init_bgm(EmuEnvState &emuenv, const std::pair<std::string, std::string> path_bgm) {
    // Clear the PCM data buffer and reset the position
    pcm_data.position = 0;
    pcm_data.data.clear();

    // Read the At9 file
    vfs::FileBuffer at9_buffer;
    const auto device = VitaIoDevice::_from_string(path_bgm.first.c_str());
    const auto path = path_bgm.second;
    if (!vfs::read_file(device, at9_buffer, emuenv.pref_path, path)) {
        LOG_ERROR_IF(device == VitaIoDevice::ux0, "Failed to read theme BGM file: {}:{}", path_bgm.first, path);
        return false;
    }

    // Decode the At9 buffer to PCM data
    return decode_bgm(at9_buffer.data());
}

} // namespace gui
