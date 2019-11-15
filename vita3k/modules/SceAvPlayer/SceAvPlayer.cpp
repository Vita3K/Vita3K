// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include "SceAvPlayer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <util/log.h>
#include <io/functions.h>
#include <util/lock_and_find.h>
#include <kernel/thread/thread_functions.h>

#include <psp2/io/fcntl.h>

#include <algorithm>

// Some games seem to run at a 60fps loop when the video is really 30fps, leading to x2 speed. This should help.
// If your computer sucks and your video actually runs at 4 fps no matter what, turn this off.
constexpr bool HALF_FRAMERATE_AT_30FPS = true;

// Defines stop/pause behaviour. If true, GetVideo/AudioData will return false when stopped (instead of returning the last frame).
constexpr bool REJECT_DATA_ON_STOP = true;

namespace emu {
    typedef Ptr<void> (*SceAvPlayerAllocator)(void *arguments, uint32_t alignment, uint32_t size);
    typedef void (*SceAvPlayerDeallocator)(void *arguments, void *memory);

    typedef int (*SceAvPlayerOpenFile)(void *arguments, const char *filename);
    typedef int (*SceAvPlayerCloseFile)(void *arguments);
    typedef int (*SceAvPlayerReadFile)(void *arguments, uint8_t *buffer, uint64_t offset, uint32_t size);
    typedef uint64_t (*SceAvPlayerGetFileSize)(void *arguments);

    typedef void (*SceAvPlayerEventCallback)(void *arguments, int32_t event_id, int32_t source_id, void *event_data);

    enum class DebugLevel {
        NONE,
        INFO,
        WARNINGS,
        ALL,
    };

    struct SceAvPlayerMemoryAllocator {
        Ptr<void> user_data;

        // All of these should be cast to SceAvPlayerAllocator or SceAvPlayerDeallocator types.
        Ptr<void> general_allocator;
        Ptr<void> general_deallocator;
        Ptr<void> texture_allocator;
        Ptr<void> texture_deallocator;
    };

    struct SceAvPlayerFileManager {
        Ptr<void> user_data;

        // Cast to SceAvPlayerOpenFile, SceAvPlayerCloseFile, SceAvPlayerReadFile and SceAvPlayerGetFileSize.
        Ptr<void> open_file;
        Ptr<void> close_file;
        Ptr<void> read_file;
        Ptr<void> file_size;
    };

    struct SceAvPlayerEventManager {
        Ptr<void> user_data;

        // Cast to SceAvPlayerEventCallback.
        Ptr<void> event_callback;
    };

    struct SceAvPlayerInfo {
        SceAvPlayerMemoryAllocator memory_allocator;
        SceAvPlayerFileManager file_manager;
        SceAvPlayerEventManager event_manager;
        DebugLevel debug_level;
        uint32_t base_priority;
        int32_t frame_buffer_count;
        int32_t auto_start;
        uint8_t unknown0[3];
        uint32_t unknown1;
    };

    struct SceAvPlayerAudio {
        uint16_t channels;
        uint16_t unknown;
        uint32_t sample_rate;
        uint32_t size;
        char language[4];
    };

    struct SceAvPlayerVideo {
        uint32_t width;
        uint32_t height;
        float aspect_ratio;
        char language[4];
    };

    struct SceAvPlayerTextPosition {
        uint16_t top;
        uint16_t left;
        uint16_t bottom;
        uint16_t right;
    };

    struct SceAvPlayerTimedText {
        char language[4];
        uint16_t text_size;
        uint16_t font_size;
        emu::SceAvPlayerTextPosition position;
    };

    union SceAvPlayerStreamDetails {
        SceAvPlayerAudio audio;
        SceAvPlayerVideo video;
        SceAvPlayerTimedText text;
    };

    struct SceAvPlayerFrameInfo {
        Ptr<uint8_t> data;
        uint32_t unknown;
        uint64_t timestamp;
        emu::SceAvPlayerStreamDetails stream_details;
    };
}

inline bool is_effectively_30fps(AVRational frame_rate) {
    float frame_rate_value = static_cast<float>(frame_rate.num) / static_cast<float>(frame_rate.den);

    return frame_rate_value >= 29 && frame_rate_value <= 30;
}

// yes this is in two places at once please forgive me for the time being
inline void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest) {
    for (uint32_t a = 0; a < frame->height; a++) {
        memcpy(dest, &frame->data[0][frame->linesize[0] * a], frame->width);
        dest += frame->width;
    }
    for (uint32_t a = 0; a < frame->height / 2; a++) {
        memcpy(dest, &frame->data[1][frame->linesize[1] * a], frame->width / 2);
        dest += frame->width / 2;
    }
    for (uint32_t a = 0; a < frame->height / 2; a++) {
        memcpy(dest, &frame->data[2][frame->linesize[2] * a], frame->width / 2);
        dest += frame->width / 2;
    }
}

static void free_video(const PlayerPtr &player) {
    if (player->video_context) {
        avcodec_close(player->video_context);
        avcodec_free_context(&player->video_context);
    }

    if (player->audio_context) {
        avcodec_close(player->audio_context);
        avcodec_free_context(&player->audio_context);
    }

    if (player->format) {
        avformat_close_input(&player->format);
    }

    while (!player->video_packets.empty()) {
        AVPacket *packet = player->video_packets.front();
        av_packet_free(&packet);
        player->video_packets.pop();
    }

    while (!player->audio_packets.empty()) {
        AVPacket *packet = player->audio_packets.front();
        av_packet_free(&packet);
        player->audio_packets.pop();
    }
}

static void switch_video(const PlayerPtr &player, const std::string &new_video) {
    player->video_playing = new_video;

    int error;

    free_video(player);

    error = avformat_open_input(&player->format, new_video.c_str(), nullptr, nullptr);
    assert(error == 0);

    // Check if stream opened successfully...
    error = avformat_find_stream_info(player->format, nullptr);
    assert(error >= 0);

    player->video_stream_id = av_find_best_stream(player->format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    player->audio_stream_id = av_find_best_stream(player->format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (player->video_stream_id >= 0) {
        AVStream *video_stream = player->format->streams[player->video_stream_id];
        AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        player->video_context = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(player->video_context, video_stream->codecpar);
        avcodec_open2(player->video_context, video_codec, nullptr);
    }

    if (player->audio_stream_id >= 0) {
        AVStream *audio_stream = player->format->streams[player->audio_stream_id];
        AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        player->audio_context = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(player->audio_context, audio_stream->codecpar);
        avcodec_open2(player->audio_context, audio_codec, nullptr);
    }
}

static AVPacket *receive_packet(const PlayerPtr &player, AVMediaType media_type) {
    int32_t stream_index = media_type == AVMEDIA_TYPE_VIDEO ? player->video_stream_id : player->audio_stream_id;
    std::queue<AVPacket *> &this_queue
        = media_type == AVMEDIA_TYPE_VIDEO ? player->video_packets : player->audio_packets;
    std::queue<AVPacket *> &opposite_queue
        = media_type == AVMEDIA_TYPE_VIDEO ? player->audio_packets : player->video_packets;

    if (!this_queue.empty()) {
        AVPacket *packet = this_queue.front();
        this_queue.pop();
        return packet;
    }

    while (true) {
        AVPacket *packet = av_packet_alloc();
        if (av_read_frame(player->format, packet) != 0)
            return nullptr;

        if (packet->stream_index == stream_index)
            return packet;
        else
            opposite_queue.push(packet);
    }
}

static Ptr<uint8_t> get_buffer(const PlayerPtr &player, AVMediaType media_type,
    MemState &mem, uint32_t size, bool hold_frame = false) {
    uint32_t &buffer_size =
        media_type == AVMEDIA_TYPE_VIDEO ? player->video_buffer_size : player->audio_buffer_size;
    uint32_t &ring_index =
        media_type == AVMEDIA_TYPE_VIDEO ? player->video_buffer_ring_index : player->audio_buffer_ring_index;
    Ptr<uint8_t> *buffers =
        media_type == AVMEDIA_TYPE_VIDEO ? player->video_buffer : player->audio_buffer;

    if (buffer_size < size) {
        buffer_size = size;
        for (uint32_t a = 0; a < PlayerState::RING_BUFFER_COUNT; a++) {
            if (buffers[a])
                free(mem, buffers[a]);
            std::string alloc_name = fmt::format("AvPlayer {} Media Ring {}",
                media_type == AVMEDIA_TYPE_VIDEO ? "Video" : "Audio", a);

            buffers[a] = alloc(mem, size, alloc_name.c_str());
        }
    }

    Ptr<uint8_t> buffer = buffers[ring_index % PlayerState::RING_BUFFER_COUNT];
    if (!hold_frame)
        ring_index++;
    return buffer;
}

static void close(MemState &mem, const PlayerPtr &player) {
    free_video(player);

    player->video_playing = "";
    player->videos_queue = { };

    // Note that on shutdown these do not need to be called. free_video() is enough to function as a destructor.
    for (uint32_t a = 0; a < PlayerState::RING_BUFFER_COUNT; a++) {
        if (player->video_buffer[a])
            free(mem, player->video_buffer[a]);
        if (player->audio_buffer[a])
            free(mem, player->audio_buffer[a]);
    }

}

EXPORT(int, sceAvPlayerAddSource, SceUID player_handle, const char *path) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);

    std::string expanded_path = expand_path(host.io, path, host.pref_path);
    if (fs::exists(expanded_path)) {
        LOG_INFO("Queued video: '{}'.", expanded_path);
        if (player_info->video_playing.empty())
            switch_video(player_info, expanded_path);
        else
            player_info->videos_queue.push(expanded_path);

        return 0;
    } else {
        LOG_INFO("Cannot find video '{}'.", expanded_path);
        return -1;
    }
}

EXPORT(int, sceAvPlayerClose, SceUID player_handle) {
    const PlayerPtr &player_info = host.kernel.players[player_handle];
    close(host.mem, player_info);

    host.kernel.players.erase(player_handle);

    return 0;
}

EXPORT(uint64_t, sceAvPlayerCurrentTime, SceUID player_handle) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);

    return player_info->last_timestamp;
}

EXPORT(int, sceAvPlayerDisableStream) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvPlayerEnableStream) {
    return UNIMPLEMENTED();
}

EXPORT(bool, sceAvPlayerGetAudioData, SceUID player_handle, emu::SceAvPlayerFrameInfo *frame_info) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);

    Ptr<uint8_t> buffer;

    if (player_info->audio_stream_id < 0)
        return false;

    if (player_info->video_playing.empty())
        return false;

    int error;

    if (player_info->paused || player_info->stopped) {
        if (REJECT_DATA_ON_STOP) {
            return false;
        } else {
            // This is probably incorrect and will make weird noises :P
            buffer = get_buffer(player_info, AVMEDIA_TYPE_AUDIO,
                host.mem, player_info->last_sample_count * sizeof(int16_t) * player_info->last_channels, true);
        }
    } else {
        AVPacket *packet{};
        while (!packet) {
            packet = receive_packet(player_info, AVMEDIA_TYPE_AUDIO);

            if (!packet) {
                if (player_info->videos_queue.empty()) {
                    // Stop playing videos or
                    player_info->video_playing = "";
                    return false;
                } else {
                    // Play the next video (if there is any).
                    switch_video(player_info, player_info->videos_queue.front());
                    player_info->videos_queue.pop();
                }
            }
        }

        error = avcodec_send_packet(player_info->audio_context, packet);
        assert(error == 0);
        av_packet_free(&packet);

        // Receive frame.
        AVFrame *frame = av_frame_alloc();
        error = avcodec_receive_frame(player_info->audio_context, frame);

        if (error == 0) {
            LOG_WARN_IF(frame->format != AV_SAMPLE_FMT_FLTP, "Unknown audio format {}.", frame->format);

            buffer = get_buffer(player_info, AVMEDIA_TYPE_AUDIO,
                host.mem, frame->nb_samples * sizeof(float) * frame->channels, false);

            auto *avc_data = buffer.cast<int16_t>().get(host.mem);

            player_info->last_channels = frame->channels;
            player_info->last_sample_count = frame->nb_samples;
            player_info->last_sample_rate = frame->sample_rate;

            // Rinne says pcm data is interleaved, I will assume it is float pcm for now...
            for (uint32_t a = 0; a < frame->nb_samples; a++) {
                for (uint32_t b = 0; b < frame->channels; b++) {
                    auto *frame_data = reinterpret_cast<float *>(frame->data[b]);
                    float current_sample = frame_data[a];
                    int16_t pcm_sample = current_sample * INT16_MAX;

                    avc_data[a * frame->channels + b] = pcm_sample;
                }
            }
        }

        av_frame_free(&frame);

        if (error != 0)
            return false;
    }

    frame_info->timestamp = player_info->last_timestamp;
    frame_info->stream_details.audio.channels = player_info->last_channels;
    frame_info->stream_details.audio.sample_rate = player_info->last_sample_rate;
    frame_info->stream_details.audio.size = player_info->last_sample_count * sizeof(uint16_t);
    frame_info->data = buffer;

    strcpy(frame_info->stream_details.audio.language, "ENG");

    return true;
}

EXPORT(int, sceAvPlayerGetStreamInfo) {
    return UNIMPLEMENTED();
}

EXPORT(bool, sceAvPlayerGetVideoData, SceUID player_handle, emu::SceAvPlayerFrameInfo *frame_info) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);

    if (player_info->video_stream_id < 0)
        return false;

    if (player_info->video_playing.empty())
        return false;

    int error;

    uint32_t width = player_info->video_context->width;
    uint32_t height = player_info->video_context->height;
    Ptr<uint8_t> buffer;

    AVRational frame_rate = player_info->format->streams[player_info->video_stream_id]->avg_frame_rate;

    if (player_info->stopped || player_info->paused) {
        if (REJECT_DATA_ON_STOP) {
            return false;
        } else {
            buffer = get_buffer(player_info, AVMEDIA_TYPE_VIDEO, host.mem, width * height * 3 / 2, true);
        }
    } else if ((HALF_FRAMERATE_AT_30FPS && is_effectively_30fps(frame_rate) && player_info->is_odd_frame)) {
        buffer = get_buffer(player_info, AVMEDIA_TYPE_VIDEO, host.mem, width * height * 3 / 2, true);
    } else {
        buffer = get_buffer(player_info, AVMEDIA_TYPE_VIDEO, host.mem, width * height * 3 / 2, false);

        AVPacket *packet{};
        while (!packet) {
            packet = receive_packet(player_info, AVMEDIA_TYPE_VIDEO);

            if (!packet) {
                if (player_info->videos_queue.empty()) {
                    // Stop playing videos or
                    player_info->video_playing = "";
                    return false;
                } else {
                    // Play the next video (if there is any).
                    switch_video(player_info, player_info->videos_queue.front());
                    player_info->videos_queue.pop();
                }
            }
        }

        error = avcodec_send_packet(player_info->video_context, packet);
        assert(error == 0);
        av_packet_free(&packet);

        // Receive frame.
        AVFrame *frame = av_frame_alloc();
        error = avcodec_receive_frame(player_info->video_context, frame);

        if (error == 0) {
            copy_yuv_data_from_frame(frame, buffer.get(host.mem));
            player_info->last_timestamp = frame->best_effort_timestamp;
        }

        av_frame_free(&frame);

        if (error != 0)
            return false;
    }

    frame_info->timestamp = player_info->last_timestamp;
    frame_info->stream_details.video.width = width;
    frame_info->stream_details.video.height = height;
    frame_info->stream_details.video.aspect_ratio =
        static_cast<float>(width) / static_cast<float>(height);
    strcpy(frame_info->stream_details.video.language, "ENG");
    frame_info->data = buffer;

    player_info->is_odd_frame = !player_info->is_odd_frame;

    return true;
}

EXPORT(int, sceAvPlayerGetVideoDataEx) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceAvPlayerInit, emu::SceAvPlayerInfo *info) {
    SceUID player_handle = host.kernel.get_next_uid();
    host.kernel.players[player_handle] = std::make_shared<PlayerState>();
    const PlayerPtr &player_info = host.kernel.players[player_handle];

    // Framebuffer count is defined in info. I'm being safe now and forcing it to 4 (even though its usually 2).

    player_info->general_allocator = info->memory_allocator.general_allocator;
    player_info->general_deallocator = info->memory_allocator.general_deallocator;
    player_info->texture_allocator = info->memory_allocator.texture_allocator;
    player_info->texture_deallocator = info->memory_allocator.texture_deallocator;

    // Result is defined as a void *, but I just call it SceUID because it is easier to deal with. Same size.
    return player_handle;
}

EXPORT(bool, sceAvPlayerIsActive, SceUID player_handle) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);

    return !player_info->video_playing.empty();
}

EXPORT(int, sceAvPlayerJumpToTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvPlayerPause, SceUID player_handle) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);
    player_info->paused = true;

    return 0;
}

EXPORT(int, sceAvPlayerPostInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvPlayerResume, SceUID player_handle) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);
    player_info->paused = false;

    return 0;
}

EXPORT(int, sceAvPlayerSetLooping, SceUID player_handle, bool do_loop) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);
    player_info->do_loop = do_loop;

    return STUBBED("LOOPING NOT IMPLEMENTED");
}

EXPORT(int, sceAvPlayerSetTrickSpeed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvPlayerStart, SceUID player_handle) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);
    player_info->stopped = false;

    return 0;
}

EXPORT(int, sceAvPlayerStop, SceUID player_handle) {
    const PlayerPtr &player_info = lock_and_find(player_handle, host.kernel.players, host.kernel.mutex);
    player_info->stopped = true;

    return 0;
}

EXPORT(int, sceAvPlayerStreamCount) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceAvPlayerAddSource)
BRIDGE_IMPL(sceAvPlayerClose)
BRIDGE_IMPL(sceAvPlayerCurrentTime)
BRIDGE_IMPL(sceAvPlayerDisableStream)
BRIDGE_IMPL(sceAvPlayerEnableStream)
BRIDGE_IMPL(sceAvPlayerGetAudioData)
BRIDGE_IMPL(sceAvPlayerGetStreamInfo)
BRIDGE_IMPL(sceAvPlayerGetVideoData)
BRIDGE_IMPL(sceAvPlayerGetVideoDataEx)
BRIDGE_IMPL(sceAvPlayerInit)
BRIDGE_IMPL(sceAvPlayerIsActive)
BRIDGE_IMPL(sceAvPlayerJumpToTime)
BRIDGE_IMPL(sceAvPlayerPause)
BRIDGE_IMPL(sceAvPlayerPostInit)
BRIDGE_IMPL(sceAvPlayerResume)
BRIDGE_IMPL(sceAvPlayerSetLooping)
BRIDGE_IMPL(sceAvPlayerSetTrickSpeed)
BRIDGE_IMPL(sceAvPlayerStart)
BRIDGE_IMPL(sceAvPlayerStop)
BRIDGE_IMPL(sceAvPlayerStreamCount)
