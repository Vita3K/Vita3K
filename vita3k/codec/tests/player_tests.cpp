#include <codec/state.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(condition)                                                                 \
    do {                                                                                 \
        if (!(condition)) {                                                              \
            std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
            std::abort();                                                                \
        }                                                                                \
    } while (0)

static void timestamps(const char *path) {
    PlayerState player;
    player.queue(path);
    CHECK(player.get_framerate_microseconds() == 41667);
    for (int i = 0; i < 12; ++i) {
        auto data = player.receive_video();
        CHECK(!data.empty());
        const uint64_t expected = av_rescale_q(i, AVRational{ 1, 24 }, AVRational{ 1, 1000 });
        if (player.last_timestamp != expected) {
            std::fprintf(stderr, "Frame %d: timestamp=%llu ms, expected=%llu ms\n", i,
                (unsigned long long)player.last_timestamp, (unsigned long long)expected);
            std::abort();
        }
    }
}

template <class T>
static void set_looping(T &player, bool loop) {
    if constexpr (requires { player.looping = loop; })
        player.looping = loop;
    else {
        std::fprintf(stderr, "Decoder has no looping implementation\n");
        std::abort();
    }
}

static void drain(const char *path, bool loop, unsigned cycles, unsigned expected_frames = 24) {
    PlayerState player;
    player.queue(path);
    if (!expected_frames) {
        const auto count = player.format->streams[player.video_stream_id]->nb_frames;
        CHECK(count > 0);
        expected_frames = static_cast<unsigned>(count);
    }
    if (loop)
        set_looping(player, true);
    unsigned frames = 0, resets = 0, audio_frames = 0;
    uint64_t previous = 0;
    for (unsigned iteration = 0; iteration < 10000 && !player.video_playing.empty(); ++iteration) {
        const auto video = player.receive_video();
        if (!video.empty()) {
            if (frames && player.last_timestamp < previous)
                ++resets;
            previous = player.last_timestamp;
            ++frames;
        }
        for (unsigned n = 0; n < 3; ++n) {
            const auto audio = player.receive_audio();
            if (!audio.empty())
                ++audio_frames;
        }
        if (loop && frames >= expected_frames * cycles)
            break;
    }
    std::printf("frames=%u audio_frames=%u resets=%u active=%d\n", frames, audio_frames, resets, !player.video_playing.empty());
    CHECK(frames == expected_frames * cycles);
    CHECK(resets == (loop ? cycles - 1 : 0));
    CHECK(loop || player.video_playing.empty());
    if (loop) {
        set_looping(player, false);
        for (unsigned i = 0; i < 1000 && !player.video_playing.empty(); ++i) {
            player.receive_video();
            player.receive_audio();
            player.receive_audio();
            player.receive_audio();
        }
        CHECK(player.video_playing.empty());
    }
    player.free_video();
    CHECK(player.receive_video().empty());
    CHECK(player.receive_audio().empty());
}

static void delayed_stream(const char *path, bool video_first) {
    PlayerState player;
    player.queue(path);
    unsigned first = 0;
    while (first < 1000 && (video_first ? !player.receive_video().empty() : !player.receive_audio().empty()))
        ++first;
    CHECK(first == (video_first ? 24u : 47u));
    CHECK(!player.video_playing.empty());

    unsigned second = 0;
    while (second < 1000 && (video_first ? !player.receive_audio().empty() : !player.receive_video().empty()))
        ++second;
    CHECK(second == (video_first ? 47u : 24u));
    CHECK(player.video_playing.empty());
    std::printf("Delayed %s consumer receives all %u frames after the other stream finishes\n", video_first ? "audio" : "video", second);
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    const std::string mode = argv[1];
    if (mode == "timestamps")
        timestamps(argv[2]);
    else if (mode == "drain")
        drain(argv[2], false, 1);
    else if (mode == "loop")
        drain(argv[2], true, 3);
    else if (mode == "delayed") {
        delayed_stream(argv[2], true);
        delayed_stream(argv[2], false);
    } else if (mode == "source") {
        timestamps(argv[2]);
        drain(argv[2], false, 1, 0);
        drain(argv[2], true, 2, 0);
    } else
        std::abort();
    std::puts("PASS");
}
