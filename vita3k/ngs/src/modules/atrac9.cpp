#include <ngs/modules/atrac9.h>
#include <util/log.h>
#include <util/bytes.h>

namespace emu::ngs::atrac9 {
    std::unique_ptr<emu::ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }
    
    static const std::array<float, 16> ATRAC9_PLAYBACK_FREQ_TAB = {
        11025.0f, 12000.0f, 16000.0f, 22050.0f, 24000.0f, 32000.0f, 44100.0f, 48000.0f, 44100.0f, 48000.0f,
        64000.0f, 88200.0f, 96000.0f, 128000.0f, 176400.0f, 192000.0f
    };

    struct FFMPEG_ATRAC9Info {
        std::uint32_t version;
        std::uint8_t magic;
        std::uint8_t info1;
        std::uint8_t info2;
        std::uint8_t info3;
        std::uint32_t padding[2];

        explicit FFMPEG_ATRAC9Info()
            : version(2)
            , magic(0xFE)
            , info1(0)
            , info2(0)
            , info3(0) {
        }

        void set_sample_rate_index(const std::uint8_t index) {
            info1 &= ~(0b1111 << 4);
            info1 |= ((index & 0b1111) << 4);
        }

        bool set_sample_rate(const float rate) {
            auto result = std::find(ATRAC9_PLAYBACK_FREQ_TAB.begin(), ATRAC9_PLAYBACK_FREQ_TAB.end(), rate);

            if (result == ATRAC9_PLAYBACK_FREQ_TAB.end()) {
                return false;
            }

            set_sample_rate_index(std::distance(ATRAC9_PLAYBACK_FREQ_TAB.begin(), result));
            return true;
        }

        void set_block_rate_index(const std::uint8_t index) {
            info1 &= ~(0b111 << 1);
            info1 |= ((index & 0b111) << 1);
        }

        void set_avg_frame_size_in_bytes(const std::uint16_t size) {
            info2 = size & 0b11111111;

            info3 &= ~(0b111 << 5);
            info3 |= ((size & (0b111 << 8)) << 5);
        }

        void set_superframe_index(const std::uint8_t idx) {
            info3 &= ~(0b11 << 3);
            info3 |= ((idx & 0b11) << 3);
        }
    };

    static_assert(sizeof(FFMPEG_ATRAC9Info) == 16);
    
    Module::Module() 
        : emu::ngs::Module(emu::ngs::BUSS_ATRAC9)
        , context(nullptr) {
        if (!init()) {
            LOG_WARN("ATRAC9 module is not supported!");
        }
    }

    bool Module::init() {
        codec = avcodec_find_decoder(AV_CODEC_ID_ATRAC9);
        
        if (!codec) {
            LOG_ERROR("Can't find ATRAC9 decoder on FFMPEG!");
            return false;
        }

        context = avcodec_alloc_context3(codec);

        if (!context) {
            LOG_ERROR("Can't alloc context for ATRAC9 module!");
            return false;
        }

        return true;
    }

    void Module::process(const MemState &mem, Voice *voice) {
        // av_log_set_level(AV_LOG_ERROR);
        Parameters *params = voice->get_parameters<Parameters>(mem);
       
        // Let's open our context
        FFMPEG_ATRAC9Info info;

        if (!info.set_sample_rate(params->playback_frequency)) {
            LOG_ERROR("Frequency {} for ATRAC9 is not supported!", params->playback_frequency);
            return;
        }

        // https://github.com/FFmpeg/FFmpeg/blob/78a24fd0140e2b41f09a560be1dcb0203f1b518d/libavcodec/atrac9tab.h#L42
        if (params->channels == 1) {
            // The mono
            info.set_block_rate_index(0);
        } else {
            info.set_block_rate_index(1);
        }

        // Total of frame in this is calculated by 1 << superframe_index.
        // I only want to limit it to 1 frame per decode
        info.set_superframe_index(0);
        info.set_avg_frame_size_in_bytes(1);

        context->block_align = 384;     // TODO: No hardcode
        context->extradata_size = 12;
        context->extradata = reinterpret_cast<std::uint8_t*>(&info);

        const int res = avcodec_open2(context, codec, nullptr);

        // Im just so happy right now that i figures out how FFMPEG parses these things. Let's get to work
        AVPacket packet;
        av_init_packet(&packet);
    }
};