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

#include <bgm_player/functions.h>

#include <audio/state.h>
#include <codec/state.h>
#include <cubeb/cubeb.h>

#include <io/VitaIoDevice.h>
#include <io/device.h>
#include <io/state.h>

#include <fstream>
#include <limits>
#include <stdexcept>

namespace bgm_player {

struct PcmBuffer {
    std::vector<uint8_t> data;
    size_t filled_size = 0;
    size_t buffer_position = 0;
};

struct PcmData {
    static constexpr int NUM_BUFFERS = 8;
    PcmBuffer buffers[NUM_BUFFERS];
    uint32_t write_buffer_index = 0;
    uint32_t next_audio_buffer = 0;
    uint32_t nb_buffers_ready = 0;
    std::condition_variable buffers_cv;
};

struct At9Stream {
    std::shared_ptr<Atrac9DecoderState> decoder;
    std::shared_ptr<BgmSource> source;
    std::vector<uint8_t> es_data;
    uint64_t data_size = 0;
    uint64_t data_offset = 0;
    uint64_t current_pos = 0;
    uint64_t buffer_pos = 0;
    size_t buffer_size = 0;
    uint32_t super_frame_size = 0;
    uint32_t channels = 0;
    uint32_t sample_per_frame = 0;
    bool initialized = false;
    std::atomic<bool> stop_requested{ false };
    PcmData pcm_data;

    std::condition_variable init_cv;
    std::mutex mutex;
};

// Functon to decode the next frame
static uint32_t decode_next_frame(At9Stream *at9_stream) {
    std::unique_lock<std::mutex> lock(at9_stream->mutex);

    // Check if we reached the end of the data stream and reset the position
    if (at9_stream->current_pos >= at9_stream->data_size)
        at9_stream->current_pos = 0;

    auto &pcm_data = at9_stream->pcm_data;

    if (pcm_data.nb_buffers_ready >= PcmData::NUM_BUFFERS) {
        // Wait for a buffer to be available
        pcm_data.buffers_cv.wait(lock, [&] {
            return pcm_data.nb_buffers_ready < PcmData::NUM_BUFFERS;
        });
    }

    // Check if we should stop decoding
    if (at9_stream->stop_requested || !at9_stream->initialized)
        return 0;

    auto &write_buffer = pcm_data.buffers[pcm_data.write_buffer_index];
    DecoderSize size{};
    if (at9_stream->current_pos < at9_stream->buffer_pos
        || at9_stream->current_pos - at9_stream->buffer_pos >= at9_stream->buffer_size
        || at9_stream->buffer_size - static_cast<size_t>(at9_stream->current_pos - at9_stream->buffer_pos) < at9_stream->super_frame_size) {
        const auto remaining = at9_stream->data_size - at9_stream->current_pos;
        const auto read_size = static_cast<size_t>(std::min<uint64_t>(remaining, 64 * 1024 + at9_stream->super_frame_size));
        at9_stream->es_data.resize(read_size + at9_stream->super_frame_size, 0);
        std::fill(at9_stream->es_data.begin() + read_size, at9_stream->es_data.end(), 0);
        try {
            if (at9_stream->source->read(at9_stream->current_pos + at9_stream->data_offset, at9_stream->es_data.data(), read_size) != read_size) {
                LOG_ERROR("Failed to read BGM: unexpected end of file");
                at9_stream->initialized = false;
                return 0;
            }
        } catch (const std::runtime_error &error) {
            LOG_ERROR("Failed to read BGM: {}", error.what());
            at9_stream->initialized = false;
            return 0;
        }
        at9_stream->buffer_pos = at9_stream->current_pos;
        at9_stream->buffer_size = read_size;
    }
    const uint8_t *frame_data = at9_stream->es_data.data() + static_cast<size_t>(at9_stream->current_pos - at9_stream->buffer_pos);

    if (!at9_stream->decoder->send(frame_data)
        || !at9_stream->decoder->receive(write_buffer.data.data(), &size)) {
        LOG_ERROR("Failed to decode frame");
        at9_stream->initialized = false;
        return 0;
    }

    uint32_t es_size_used = std::min(at9_stream->decoder->get_es_size(), at9_stream->super_frame_size);
    if (es_size_used == 0) {
        LOG_ERROR("BGM decoder consumed no input");
        at9_stream->initialized = false;
        return 0;
    }
    at9_stream->current_pos += es_size_used;

    if (at9_stream->channels == 1) {
        auto *samples = reinterpret_cast<uint16_t *>(write_buffer.data.data());
        for (size_t i = size.samples; i > 0; --i) {
            const uint16_t sample = samples[i - 1];
            samples[(i - 1) * 2] = sample;
            samples[(i - 1) * 2 + 1] = sample;
        }
    }
    write_buffer.filled_size = static_cast<size_t>(size.samples) * 2 * sizeof(uint16_t);
    write_buffer.buffer_position = 0;

    // Set the next buffer to be used for writing
    pcm_data.write_buffer_index = (pcm_data.write_buffer_index + 1) % PcmData::NUM_BUFFERS;
    pcm_data.nb_buffers_ready++;

    // Unlock the mutex to allow the audio callback to access the buffer
    lock.unlock();

    // Notify the audio callback that a new buffer is available
    pcm_data.buffers_cv.notify_one();

    // return the number of samples decoded
    return size.samples;
}

// Callback function called by Cubeb when audio data is needed
static long audio_callback(cubeb_stream *stream, void *user_ptr, const void *input_buffer, void *output_buffer, long nframes) {
    At9Stream *at9 = static_cast<At9Stream *>(user_ptr);
    auto &pcm_data = at9->pcm_data;
    uint8_t *output = static_cast<uint8_t *>(output_buffer);

    std::unique_lock<std::mutex> lock(at9->mutex);

    // Set the bytes needed for the output buffer
    const size_t bytes_needed = static_cast<size_t>(nframes) * 2 * sizeof(uint16_t);
    size_t bytes_given = 0;

    while (bytes_given < bytes_needed && (pcm_data.nb_buffers_ready > 0)) {
        auto &buf = pcm_data.buffers[pcm_data.next_audio_buffer];

        // Set the bytes available in the buffer
        size_t bytes_available = buf.filled_size - buf.buffer_position;
        size_t bytes_to_copy = std::min(bytes_needed - bytes_given, bytes_available);

        // Copy the data from the buffer to the output buffer
        std::memcpy(output + bytes_given, buf.data.data() + buf.buffer_position, bytes_to_copy);

        // Update the buffer position and bytes given
        buf.buffer_position += bytes_to_copy;
        bytes_given += bytes_to_copy;

        // Check if have reached the end of the buffer
        if (buf.buffer_position == buf.filled_size) {
            // Reset the buffer position and filled size
            buf.filled_size = 0;
            buf.buffer_position = 0;

            // Move to the next buffer
            pcm_data.next_audio_buffer = (pcm_data.next_audio_buffer + 1) % PcmData::NUM_BUFFERS;

            // Decrement the number of buffers ready
            pcm_data.nb_buffers_ready--;

            lock.unlock();

            // Notify the audio callback that a buffer is available for writing again
            pcm_data.buffers_cv.notify_one();

            lock.lock();
        }
    }

    // If we have not given enough bytes, fill the rest with zeros
    if (bytes_given < bytes_needed) {
        std::memset(output + bytes_given, 0, bytes_needed - bytes_given);
    }

    return nframes;
}

static std::thread playback_handle_thread;
static cubeb_stream *stream = nullptr;
static cubeb *ctx = nullptr;
static At9Stream at9_stream;

// Function to handle the playback thread
static void pcm_playback_handle_thread() {
    // While the stream is not stopped, decode the next frame
    while (!at9_stream.stop_requested) {
        // Wait for the stream to be ready
        std::unique_lock<std::mutex> lock(at9_stream.mutex);
        at9_stream.init_cv.wait(lock, [] { return at9_stream.initialized || at9_stream.stop_requested; });

        // If the stream is stopped, exit the thread
        if (at9_stream.stop_requested)
            break;
        lock.unlock();

        while (decode_next_frame(&at9_stream) != 0) {
        }
    }
}

// Callback called when the stream state changes
static void state_callback(cubeb_stream *stream, void *user_ptr, cubeb_state state) {
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

// Function to stop the background music
void stop_bgm() {
    if (stream)
        cubeb_stream_stop(stream);

    // Reset the stream state
    {
        // We lock the mutex to protect access to the stream state
        std::lock_guard<std::mutex> lock(at9_stream.mutex);
        at9_stream.es_data.clear();
        at9_stream.source.reset();
        at9_stream.data_size = 0;
        at9_stream.current_pos = 0;
        at9_stream.buffer_pos = 0;
        at9_stream.buffer_size = 0;
        at9_stream.initialized = false;
        at9_stream.pcm_data.write_buffer_index = 0;
        at9_stream.pcm_data.next_audio_buffer = 0;
        at9_stream.pcm_data.nb_buffers_ready = 0;
    }

    // Notify the audio callback that the stream has stopped
    at9_stream.pcm_data.buffers_cv.notify_one();
}

// Function to destroy the Cubeb context
void destroy_bgm_player() {
    clear_current_bgm();
    if (!stream)
        return;

    {
        // We lock the mutex to protect access to stop_requested and other variables
        std::lock_guard<std::mutex> lock(at9_stream.mutex);
        at9_stream.stop_requested = true;
    }

    // Wake up the playback thread if it's waiting on init_cv
    at9_stream.init_cv.notify_one();

    // Wait for the playback handle thread to finish
    if (playback_handle_thread.joinable())
        playback_handle_thread.join();

    // Destroy the stream and Cubeb context
    cubeb_stream_destroy(stream);
    stream = nullptr;
    cubeb_destroy(ctx);
    ctx = nullptr;

    // at9_stream is static and outlives the player (the process survives on Android).
    // It holds a mutex and a condition variable, so it can't be reset by assignment: rebuild it in place.
    std::destroy_at(&at9_stream);
    std::construct_at(&at9_stream);
}

void switch_bgm_state(const bool pause) {
    if (!stream) {
        LOG_ERROR("The background music stream is not initialized!");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(at9_stream.mutex);
        if (!at9_stream.initialized)
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

void init_bgm_player(const float vol) {
    // Create a new Cubeb context
    if (cubeb_init(&ctx, "BGM Player", nullptr) != CUBEB_OK) {
        LOG_ERROR("Failed to initialize Cubeb context");
        return;
    }

    // Configure the audio output parameters
    cubeb_stream_params output_params{
        .format = CUBEB_SAMPLE_S16LE, // Format PCM 16-bit, little-endian
        .rate = 48000, // Sample rate 48 kHz
        .channels = 2, // Stereo
        .layout = CUBEB_LAYOUT_STEREO, // Stereo layout
        .prefs = CUBEB_STREAM_PREF_NONE // No special preferences
    };

    // Get the minimum latency for the output parameters
    uint32_t latency;
    cubeb_get_min_latency(ctx, &output_params, &latency);

    // Lock the mutex to protect initialization of the Cubeb stream
    std::lock_guard<std::mutex> lock(at9_stream.mutex);
    if (cubeb_stream_init(ctx, &stream, "BGM Stream", nullptr, nullptr,
            nullptr, &output_params, latency, audio_callback, state_callback, &at9_stream)
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

static bool init_bgm_streaming(const std::shared_ptr<BgmSource> &source) {
    if (!source || source->size() <= sizeof(At9Header)) {
        LOG_ERROR("AT9 header too small");
        return false;
    }

    At9Header header{};
    try {
        if (source->read(0, &header, sizeof(header)) != sizeof(header)) {
            LOG_ERROR("Failed to read AT9 header: unexpected end of file");
            return false;
        }
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Failed to read AT9 header: {}", error.what());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(at9_stream.mutex);

        // initialize the At9Stream structure
        at9_stream.decoder = std::make_shared<Atrac9DecoderState>(header.fmt.config_data);
        at9_stream.source = source;
        at9_stream.data_offset = sizeof(header);
        at9_stream.data_size = source->size() - sizeof(header);
        at9_stream.current_pos = 0;
        at9_stream.buffer_pos = 0;
        at9_stream.buffer_size = 0;
        at9_stream.super_frame_size = at9_stream.decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);
        at9_stream.channels = at9_stream.decoder->get(DecoderQuery::CHANNELS);
        at9_stream.sample_per_frame = at9_stream.decoder->get(DecoderQuery::AT9_SAMPLE_PER_FRAME);
        if ((at9_stream.channels != 1 && at9_stream.channels != 2)
            || at9_stream.super_frame_size == 0 || at9_stream.super_frame_size > 64 * 1024
            || at9_stream.sample_per_frame == 0) {
            LOG_ERROR("Unsupported BGM stream parameters: channels={}, superframe={}, samples={}",
                at9_stream.channels, at9_stream.super_frame_size, at9_stream.sample_per_frame);
            at9_stream.decoder.reset();
            at9_stream.source.reset();
            at9_stream.es_data.clear();
            at9_stream.data_size = 0;
            at9_stream.initialized = false;
            return false;
        }

        // Initialize the PCM data buffers
        for (auto &data : at9_stream.pcm_data.buffers) {
            data.buffer_position = 0;
            data.filled_size = 0;
            data.data.clear();
            data.data.resize(static_cast<size_t>(at9_stream.sample_per_frame) * 2 * sizeof(uint16_t));
        }

        at9_stream.initialized = true;
    }
    at9_stream.init_cv.notify_one();

    return true;
}

static std::pair<std::string, std::string> current_path_bgm = { "pd0", "data/systembgm/initialsetup.at9" };
static std::shared_ptr<BgmSource> current_bgm_source;
static bool custom_bgm_source = false;
void clear_current_bgm() {
    stop_bgm();
    current_bgm_source.reset();
    custom_bgm_source = false;
}

void set_current_bgm_path(const std::pair<std::string, std::string> &path) {
    current_path_bgm = path;
    current_bgm_source.reset();
    custom_bgm_source = false;
}

void set_current_bgm_source(std::shared_ptr<BgmSource> source) {
    current_bgm_source = std::move(source);
    custom_bgm_source = true;
}

class HostBgmSource final : public BgmSource {
public:
    explicit HostBgmSource(const fs::path &path)
        : input_(path, std::ios::binary | std::ios::ate) {
        if (!input_ || input_.tellg() < 0)
            return;
        size_ = static_cast<uint64_t>(input_.tellg());
        valid_ = true;
    }

    bool valid() const { return valid_; }
    uint64_t size() const override { return size_; }

    size_t read(uint64_t offset, void *data, size_t length) override {
        if (offset > size_ || length > size_ - offset
            || offset > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max())
            || length > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
            LOG_ERROR("Invalid BGM read range: offset={}, length={}, size={}", offset, length, size_);
            return 0;
        }
        input_.clear();
        input_.seekg(static_cast<std::streamoff>(offset));
        if (!input_.read(static_cast<char *>(data), static_cast<std::streamsize>(length))) {
            LOG_ERROR("Cannot read BGM file at offset {}", offset);
            return 0;
        }
        return length;
    }

private:
    std::ifstream input_;
    uint64_t size_ = 0;
    bool valid_ = false;
};

bool init_bgm(const fs::path &pref_path, const bool is_enable) {
    VitaIoDevice device = VitaIoDevice::_INVALID;
    if (!boost::describe::enum_from_string(current_path_bgm.first.c_str(), device))
        return false;
    const auto &path = current_path_bgm.second;

    // Check if the path is initialsetup.at9 and if the stream is already initialized
    {
        std::lock_guard<std::mutex> lock(at9_stream.mutex);
        if ((path.find("initialsetup.at9") != std::string::npos) && at9_stream.initialized)
            return false;
    }

    // Stop current Background Music
    stop_bgm();

    if (!is_enable)
        return false;

    std::shared_ptr<BgmSource> source = current_bgm_source;
    if (!custom_bgm_source) {
        auto host_source = std::make_shared<HostBgmSource>(device::construct_emulated_path(device, path, pref_path));
        if (!host_source->valid()) {
            LOG_ERROR("Failed to open AT9 file {}:{}", current_path_bgm.first, path);
            return false;
        }
        source = std::move(host_source);
    } else if (!source) {
        LOG_ERROR("Protected theme BGM is unavailable: {}:{}", current_path_bgm.first, path);
        return false;
    }

    if (!init_bgm_streaming(source)) {
        LOG_ERROR("Failed to initialize AT9 decoder");
        return false;
    }

    return true;
}

} // namespace bgm_player
