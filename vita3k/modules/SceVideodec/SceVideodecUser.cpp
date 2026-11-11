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

#include <module/module.h>

#include <codec/state.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <util/lock_and_find.h>

#include <atomic>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceVideodecUser);

typedef std::shared_ptr<H264DecoderState> H264DecoderPtr;
typedef std::map<SceUID, H264DecoderPtr> H264DecoderStates;

struct VideodecConfiguration {
    std::mutex mutex;
    std::atomic<uint32_t> avc_mode{ 0 };
    uint32_t avc_backend = 2;
    bool avc_initialized = false;
    bool m4v_initialized = false;
    uint32_t m4v_input_capacity = 0;
};

enum {
    SCE_M4VDEC_ERROR_INVALID_PARAM = 0x80620902,
    SCE_M4VDEC_ERROR_OUT_OF_MEMORY = 0x80620903,
    SCE_M4VDEC_ERROR_INVALID_STATE = 0x80620904,
    SCE_M4VDEC_ERROR_UNSUPPORT_IMAGE_SIZE = 0x80620905,
    SCE_M4VDEC_ERROR_INVALID_POINTER = 0x80620909,
    SCE_M4VDEC_ERROR_ES_BUFFER_FULL = 0x8062090a,
    SCE_M4VDEC_ERROR_NOT_INITIALIZE = 0x8062090c,
    SCE_M4VDEC_ERROR_INVALID_STREAM = 0x8062090d,
    SCE_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE = 0x8062090e,
};

struct M4vInstance {
    std::mutex mutex;
    std::unique_ptr<M4vDecoderState> decoder;
    M4vDecodePipeline pipeline;
    std::optional<H264DecodedFrame> pending;
    M4vFrameMetadata metadata;
    uint32_t max_es_size = 0;
    uint32_t input_capacity = 0;
    bool deleted = false;
    bool first_packet = true;
    bool first_picture = true;
    uint32_t diagnostic_calls = 0;
};

struct VideodecInternalDecoder {
    std::mutex mutex;
    H264DecoderPtr decoder;
    VideodecConfiguration *configuration = nullptr;
    std::map<SceUID, uint32_t> callbacks;
    size_t pending_pictures = 0;
    uint64_t last_output_pts = ~0ull;
};

struct VideodecState {
    std::mutex mutex;
    H264DecoderStates decoders;
    std::map<SceUID, std::shared_ptr<VideodecInternalDecoder>> internal_decoders;
    std::map<SceUID, std::shared_ptr<M4vInstance>> m4v_decoders;
};

static VideodecConfiguration *videodec_configuration(EmuEnvState &emuenv) {
    emuenv.kernel.obj_store.create<VideodecConfiguration>();
    return emuenv.kernel.obj_store.get<VideodecConfiguration>();
}

enum {
    SCE_AVCDEC_ERROR_INVALID_PARAM = 0x80620002,
    SCE_AVCDEC_ERROR_INVALID_STATE = 0x80620004,
    SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE = 0x80620005,
    SCE_AVCDEC_ERROR_INVALID_POINTER = 0x80620009,
    SCE_AVCDEC_ERROR_ES_BUFFER_FULL = 0x8062000a,
    SCE_AVCDEC_ERROR_INVALID_STREAM = 0x8062000d,
    SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE = 0x8062000e,
    SCE_AVCDEC_ERROR_INTERLACED = 0x80620010,
    SCE_VIDEODEC_ERROR_INVALID_PARAM = 0x80620802,
    SCE_VIDEODEC_ERROR_ALREADY_USED = 0x80620808,
    SCE_VIDEODEC_ERROR_INVALID_POINTER = 0x80620809,
    SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE = 0x8062080e,
};

enum SceVideodecType {
    SCE_VIDEODEC_TYPE_HW_AVCDEC = 0x1001,
    SCE_VIDEODEC_TYPE_HW_M4VDEC = 0x1002
};

struct SceVideodecTimeStamp {
    uint32_t upper;
    uint32_t lower;
};

enum SceCscCoefficient : uint8_t {
    SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT = 0,
    SCE_AVCDEC_CSC_COEFFICIENT_ITU601 = 1,
    SCE_AVCDEC_CSC_COEFFICIENT_ITU709 = 2,
    SCE_AVCDEC_CSC_COEFFICIENT_ITU601_FULL = 3,
    SCE_AVCDEC_CSC_COEFFICIENT_ITU709_FULL = 4,
};

struct SceAvcdecFrameOptionRGBA {
    uint8_t alpha;
    SceCscCoefficient cscCoefficient;
    uint8_t reserved[14];
};

union SceAvcdecFrameOption {
    uint8_t reserved[16];
    SceAvcdecFrameOptionRGBA rgba;
};

struct SceAvcdecQueryDecoderInfo {
    uint32_t horizontal;
    uint32_t vertical;
    uint32_t numOfRefFrames; //!< Number of reference frames
};

struct SceAvcdecDecoderInfo {
    uint32_t frameMemSize;
};

struct SceAvcdecInfo {
    uint32_t numUnitsInTick;
    uint32_t timeScale;
    uint8_t fixedFrameRateFlag;

    uint8_t aspectRatioIdc;
    uint16_t sarWidth;
    uint16_t sarHeight;

    uint8_t colourPrimaries;
    uint8_t transferCharacteristics;
    uint8_t matrixCoefficients;

    uint8_t videoFullRangeFlag;

    uint8_t picStruct[2];
    uint8_t ctType;

    SceVideodecTimeStamp pts;
};

struct SceAvcdecBuf {
    Ptr<void> pBuf;
    uint32_t size;
};

struct SceAvcdecCtrl {
    SceUID handle;
    SceAvcdecBuf frameBuf;
};

struct SceAvcdecAu {
    SceVideodecTimeStamp pts;
    SceVideodecTimeStamp dts;
    SceAvcdecBuf es;
};

struct SceAvcdecNalAu {
    SceVideodecTimeStamp pts;
    SceVideodecTimeStamp dts;
    SceAvcdecBuf extra[2];
    SceAvcdecBuf payload;
    uint32_t nal_length_size;
    uint32_t flags;
};

struct SceVideodecQueryInitInfo {
    uint32_t size;
    uint32_t horizontal;
    uint32_t vertical;
    uint32_t num_of_ref_frames;
    uint32_t num_of_streams;
    uint32_t reserved[3];
};

static_assert(sizeof(SceAvcdecNalAu) == 48);
static_assert(sizeof(SceVideodecQueryInitInfo) == 32);

enum ScePixelType : uint32_t {
    SCE_AVCDEC_PIXEL_RGBA8888 = 0x0U, // A:B:G:R=8:8:8:8    32bit   U8U8U8U8_ABGR
    SCE_AVCDEC_PIXEL_BGRA8888 = 0x8U, // A:R:G:B=8:8:8:8    32bit   U8U8U8U8_ARGB
    SCE_AVCDEC_PIXEL_YUV420_RASTER = 0x10U, // YUV420P3
    SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER = 0x20U, // YVU420P2
};

struct SceAvcdecFrame {
    ScePixelType pixelType;
    uint32_t framePitch;
    uint32_t frameWidth;
    uint32_t frameHeight;

    uint32_t horizontalSize;
    uint32_t verticalSize;

    uint32_t frameCropLeftOffset;
    uint32_t frameCropRightOffset;
    uint32_t frameCropTopOffset;
    uint32_t frameCropBottomOffset;

    SceAvcdecFrameOption opt;

    Ptr<void> pPicture[2];
};

struct SceAvcdecPicture {
    uint32_t size;
    SceAvcdecFrame frame;
    SceAvcdecInfo info;
};

struct SceAvcdecArrayPicture {
    uint32_t numOfOutput;
    uint32_t numOfElm;
    Ptr<Ptr<SceAvcdecPicture>> pPicture;
};

struct SceM4vdecQueryDecoderInfo {
    uint32_t horizontal;
    uint32_t vertical;
    uint32_t maxEsSize;
};

struct SceM4vdecInfo {
    uint32_t vopTimeIncrementResolution;
    uint8_t fixedVopRate;
    uint8_t aspectRatioInfo;
    uint16_t parWidth;
    uint16_t parHeight;
    uint8_t colourPrimaries;
    uint8_t transferCharacteristics;
    uint8_t matrixCoefficients;
    uint8_t videoRange;
    uint8_t infoFlags;
    uint8_t padding;
    SceVideodecTimeStamp outputPts;
};

struct SceM4vdecPicture {
    uint32_t size;
    SceAvcdecFrame frame;
    SceM4vdecInfo info;
};

struct SceM4vdecArrayPicture {
    uint32_t numOfOutput;
    uint32_t numOfElm;
    Ptr<Ptr<SceM4vdecPicture>> pPicture;
};

static_assert(sizeof(SceM4vdecQueryDecoderInfo) == 12);
static_assert(sizeof(SceAvcdecAu) == 24);
static_assert(sizeof(SceM4vdecInfo) == 24);
static_assert(sizeof(SceM4vdecPicture) == 92);
static_assert(offsetof(SceM4vdecPicture, info) == 68);
static_assert(sizeof(SceM4vdecArrayPicture) == 12);

static bool avc_guest_buffer(const MemState &mem, const SceAvcdecBuf &buffer, std::span<const uint8_t> &data) {
    if (buffer.size == 0) {
        data = {};
        return true;
    }
    const Address address = buffer.pBuf.address();
    const uint64_t end = static_cast<uint64_t>(address) + buffer.size;
    if (!address || end > 0xfffff000ULL || !is_valid_addr_range(mem, address, static_cast<Address>(end)))
        return false;
    data = { buffer.pBuf.cast<uint8_t>().get(mem), buffer.size };
    return true;
}

static int avc_validate_work_pictures(const char *export_name, const MemState &mem, const SceAvcdecArrayPicture *work) {
    if (!work)
        return 0;
    if (work->numOfElm > 2)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    if (!work->numOfElm)
        return 0;
    std::span<const uint8_t> bytes;
    if (!avc_guest_buffer(mem, { work->pPicture.cast<void>(), work->numOfElm * static_cast<uint32_t>(sizeof(Ptr<SceAvcdecPicture>)) }, bytes))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    const auto entries = work->pPicture.get(mem);
    for (uint32_t i = 0; i < work->numOfElm; ++i) {
        if (!avc_guest_buffer(mem, { entries[i].cast<void>(), sizeof(SceAvcdecPicture) }, bytes))
            return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
        if (entries[i].get(mem)->size != sizeof(SceAvcdecPicture))
            return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE);
    }
    return 0;
}

static void avc_notify_callbacks(EmuEnvState &emuenv, const std::vector<SceUID> &callbacks, size_t count, int event) {
    for (const SceUID id : callbacks) {
        const CallbackPtr callback = emuenv.kernel.objects.find<Callback>(id);
        const ThreadStatePtr owner = callback ? emuenv.kernel.get_thread(callback->thread_id) : nullptr;
        if (!owner) {
            LOG_WARN("AVC callback {} or its owner no longer exists", id);
            continue;
        }
        for (size_t i = 0; i < count; ++i)
            owner->notify_callback(*callback, event);
    }
}

static std::vector<SceUID> avc_picture_callbacks(VideodecInternalDecoder &decoder, size_t produced) {
    std::vector<SceUID> callbacks;
    if (!produced)
        return callbacks;
    for (const auto &[id, mask] : decoder.callbacks) {
        if (mask & 0x2)
            callbacks.push_back(id);
    }
    if (callbacks.empty())
        decoder.pending_pictures += produced;
    return callbacks;
}

static int avc_get_internal_picture(const char *export_name, MemState &mem, VideodecInternalDecoder &internal,
    SceAvcdecArrayPicture *pictures, size_t &produced, std::vector<SceUID> &callbacks) {
    if (!pictures)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    pictures->numOfOutput = 0;
    if (pictures->numOfElm > 1)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    if (!pictures->numOfElm)
        return 0;
    std::span<const uint8_t> array_bytes, picture_bytes, output_bytes;
    if (!avc_guest_buffer(mem, { pictures->pPicture.cast<void>(), sizeof(Ptr<SceAvcdecPicture>) }, array_bytes))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    const auto picture_ptr = pictures->pPicture.get(mem)[0];
    if (!avc_guest_buffer(mem, { picture_ptr.cast<void>(), sizeof(SceAvcdecPicture) }, picture_bytes))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    auto &picture = *picture_ptr.get(mem);
    LOG_INFO_ONCE("Internal AVC output: structure size {}, format {:#x}, pitch {}, surface {}x{}, address {:#x}",
        picture.size, static_cast<uint32_t>(picture.frame.pixelType), picture.frame.framePitch,
        picture.frame.frameWidth, picture.frame.frameHeight, picture.frame.pPicture[0].address());
    if (picture.size != sizeof(SceAvcdecPicture))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE);
    auto &frame = picture.frame;
    const uint32_t type = static_cast<uint32_t>(frame.pixelType);
    if ((type & 0x40000) != 0) {
        LOG_ERROR("Separate AVC output planes are not implemented");
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    }
    H264OutputFormat format;
    switch (type & 0xff) {
    case SCE_AVCDEC_PIXEL_YUV420_RASTER: format = H264OutputFormat::yuv420p; break;
    case SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER: format = H264OutputFormat::nv12; break;
    case SCE_AVCDEC_PIXEL_RGBA8888: format = H264OutputFormat::rgba; break;
    case SCE_AVCDEC_PIXEL_BGRA8888: format = H264OutputFormat::bgra; break;
    default: return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    }
    if ((frame.framePitch | frame.frameWidth | frame.frameHeight) & 15 || frame.frameWidth < 64 || frame.frameHeight < 64 || frame.framePitch < frame.frameWidth || frame.framePitch > 2048 || frame.frameWidth > 1920 || frame.frameHeight > 1088)
        return RET_ERROR(SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE);

    std::lock_guard<std::mutex> codec_lock(internal.decoder->codec_mutex);
    if (internal.decoder->decoded_frames.empty())
        return 0;
    const auto &decoded = internal.decoder->decoded_frames.front();
    if (decoded.interlaced && !(internal.configuration->avc_mode.load() & 0x80))
        return RET_ERROR(SCE_AVCDEC_ERROR_INTERLACED);
    if (decoded.interlaced)
        LOG_WARN_ONCE("AVC interlaced picture timing metadata is not available from the decoder");
    const uint32_t aligned_width = (decoded.width + 15) & ~15u;
    const uint32_t aligned_height = (decoded.height + 7) & ~7u;
    const bool native_size = !(type & 0x80000) && aligned_width <= frame.frameWidth && aligned_height <= frame.frameHeight;
    const uint32_t width = native_size ? decoded.width : frame.frameWidth;
    const uint32_t height = native_size ? decoded.height : frame.frameHeight;
    const uint32_t surface_height = native_size ? aligned_height : frame.frameHeight;
    const bool rgb = format == H264OutputFormat::rgba || format == H264OutputFormat::bgra;
    const uint32_t size = frame.framePitch * surface_height * (rgb ? 4 : 3) / (rgb ? 1 : 2);
    if (!avc_guest_buffer(mem, { frame.pPicture[0], size }, output_bytes))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    auto output = std::span<uint8_t>(frame.pPicture[0].cast<uint8_t>().get(mem), size);
    const uint8_t alpha = rgb && (type & 0x80000000u) ? frame.opt.rgba.alpha : 255;
    if (rgb && (type & 0x80000000u) && frame.opt.rgba.cscCoefficient != SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT) {
        LOG_ERROR("AVC output colour conversion coefficient {} is not implemented", static_cast<uint32_t>(frame.opt.rgba.cscCoefficient));
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    }
    if (!h264_render_frame(decoded, output, frame.framePitch, surface_height, width, height, format, alpha))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_STATE);
    frame.horizontalSize = decoded.width;
    frame.verticalSize = decoded.height;
    frame.frameCropLeftOffset = frame.frameCropRightOffset = 0;
    frame.frameCropTopOffset = frame.frameCropBottomOffset = 0;
    std::memset(&picture.info, 0xff, sizeof(picture.info));
    picture.info.ctType = 0;
    picture.info.videoFullRangeFlag = 0;
    picture.info.pts.upper = static_cast<uint32_t>(decoded.pts >> 32);
    picture.info.pts.lower = static_cast<uint32_t>(decoded.pts);
    if (decoded.pts != ~0ull && internal.last_output_pts != ~0ull && decoded.pts < internal.last_output_pts)
        LOG_WARN("AVC output timestamp moved backwards: {} -> {}", internal.last_output_pts, decoded.pts);
    if (decoded.pts != ~0ull)
        internal.last_output_pts = decoded.pts;
    internal.decoder->decoded_frames.pop_front();
    if (internal.pending_pictures)
        --internal.pending_pictures;
    pictures->numOfOutput = 1;
    const H264AsyncResult result = internal.decoder->collect_frames(produced);
    callbacks = avc_picture_callbacks(internal, produced);
    if (result != H264AsyncResult::success)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_STREAM);
    return 0;
}

static int avc_stop_decoder(const char *export_name, EmuEnvState &emuenv, VideodecInternalDecoder &internal,
    SceAvcdecArrayPicture *picture) {
    if (!picture)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    size_t produced = 0, additional = 0;
    std::vector<SceUID> callbacks, more_callbacks;
    int result = 0;
    {
        std::lock_guard<std::mutex> lock(internal.mutex);
        if (!internal.decoder->is_stopped) {
            const H264AsyncResult drain = internal.decoder->drain_nal(produced);
            callbacks = avc_picture_callbacks(internal, produced);
            if (drain == H264AsyncResult::full)
                result = RET_ERROR(SCE_AVCDEC_ERROR_ES_BUFFER_FULL);
            else if (drain != H264AsyncResult::success)
                result = RET_ERROR(SCE_AVCDEC_ERROR_INVALID_STREAM);
        }
        if (result == 0)
            result = avc_get_internal_picture(export_name, emuenv.mem, internal, picture, additional, more_callbacks);
    }
    avc_notify_callbacks(emuenv, callbacks, produced, 2);
    avc_notify_callbacks(emuenv, more_callbacks, additional, 2);
    return result;
}

EXPORT(int, sceAvcdecCreateDecoder, uint32_t codec_type, SceAvcdecCtrl *decoder, const SceAvcdecQueryDecoderInfo *query) {
    TRACY_FUNC(sceAvcdecCreateDecoder, codec_type, decoder, query);

    if (query)
        LOG_INFO("AVC decoder request: type {:#x}, {}x{}, {} reference frames", codec_type, query->horizontal, query->vertical, query->numOfRefFrames);

    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    if (!decoder || !query)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (query->horizontal > 1920 || query->vertical > 1088 || query->horizontal * query->vertical > 1280 * 720 || query->numOfRefFrames > 16)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    SceUID handle = emuenv.kernel.get_next_uid();
    decoder->handle = handle;

    state->decoders[handle] = std::make_shared<H264DecoderState>(query->horizontal, query->vertical);

    return 0;
}

EXPORT(int, sceAvcdecCreateDecoderInternal, uint32_t codec_type, SceAvcdecCtrl *decoder, const SceAvcdecQueryDecoderInfo *query) {
    TRACY_FUNC(sceAvcdecCreateDecoderInternal, codec_type, decoder, query);
    if (decoder && query)
        LOG_INFO("Internal AVC decoder request: type {:#x}, {}x{}, {} reference frames, frame buffer {:#x} ({} bytes)",
            codec_type, query->horizontal, query->vertical, query->numOfRefFrames, decoder->frameBuf.pBuf.address(), decoder->frameBuf.size);
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    if (!decoder || !query)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!avc_library_memory_size(query->horizontal, query->vertical, query->numOfRefFrames, 1))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    auto internal = std::make_shared<VideodecInternalDecoder>();
    internal->decoder = std::make_shared<H264DecoderState>(query->horizontal, query->vertical);
    internal->configuration = videodec_configuration(emuenv);
    const SceUID handle = emuenv.kernel.get_next_uid();
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->decoders.emplace(handle, internal->decoder);
        state->internal_decoders.emplace(handle, internal);
    }
    decoder->handle = handle;
    return 0;
}

EXPORT(int, sceAvcdecDecodeAuNalAuInternal, SceAvcdecCtrl *decoder, const SceAvcdecNalAu *au, const uint32_t *timeout) {
    TRACY_FUNC(sceAvcdecDecodeAuNalAuInternal, decoder, au, timeout);
    if (decoder && au)
        LOG_INFO_ONCE("AVC NAL input: handle {}, flags {:#x}, NAL length bytes {}, payload {} bytes, extra buffers {} and {} bytes",
            decoder->handle, au->flags, au->nal_length_size, au->payload.size, au->extra[0].size, au->extra[1].size);
    if (!decoder || !au)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!decoder->handle)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (!internal)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    std::span<const uint8_t> payload, extra0, extra1;
    if (!avc_guest_buffer(emuenv.mem, au->payload, payload) || ((au->flags & 1) && !avc_guest_buffer(emuenv.mem, au->extra[0], extra0)) || ((au->flags & 2) && !avc_guest_buffer(emuenv.mem, au->extra[1], extra1)))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    const uint64_t pts = static_cast<uint64_t>(au->pts.upper) << 32 | au->pts.lower;
    const uint64_t dts = static_cast<uint64_t>(au->dts.upper) << 32 | au->dts.lower;
    size_t produced = 0;
    H264AsyncResult result;
    std::vector<SceUID> callbacks;
    {
        std::lock_guard<std::mutex> lock(internal->mutex);
        result = internal->decoder->send_nal(payload, extra0, extra1, au->nal_length_size, pts, dts, produced);
        callbacks = avc_picture_callbacks(*internal, produced);
    }
    avc_notify_callbacks(emuenv, callbacks, produced, 2);
    switch (result) {
    case H264AsyncResult::success: return 0;
    case H264AsyncResult::full: return RET_ERROR(SCE_AVCDEC_ERROR_ES_BUFFER_FULL);
    case H264AsyncResult::invalid_input:
        LOG_ERROR("Invalid AVC NAL input: flags {:#x}, length size {}, payload {} bytes, extra buffers {} and {} bytes",
            au->flags, au->nal_length_size, au->payload.size, au->extra[0].size, au->extra[1].size);
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    case H264AsyncResult::decode_error: return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_STREAM);
    }
    return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_STATE);
}

EXPORT(int, sceAvcdecCsc) {
    TRACY_FUNC(sceAvcdecCsc);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecCscInternal) {
    TRACY_FUNC(sceAvcdecCscInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecode, SceAvcdecCtrl *decoder, const SceAvcdecAu *au, SceAvcdecArrayPicture *picture) {
    TRACY_FUNC(sceAvcdecDecode, decoder, au, picture);

    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (decoder->handle == 0)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    H264DecoderOptions options = {};
    options.pts_upper = au->pts.upper;
    options.pts_lower = au->pts.lower;
    options.dts_upper = au->dts.upper;
    options.dts_lower = au->dts.lower;

    // This is quite long...
    SceAvcdecPicture *pPicture = picture->pPicture.get(emuenv.mem)[0].get(emuenv.mem);
    uint8_t *output = pPicture->frame.pPicture[0].cast<uint8_t>().get(emuenv.mem);

    if ((pPicture->frame.pixelType & (SCE_AVCDEC_PIXEL_YUV420_RASTER | SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)) == 0) {
        LOG_ERROR_ONCE("Avcdec rgba output is not implemented");
        picture->numOfOutput++;
        return 0;
    }
    bool is_yuvp3 = static_cast<bool>(pPicture->frame.pixelType & SCE_AVCDEC_PIXEL_YUV420_RASTER);
    decoder_info->set_output_format(is_yuvp3);

    decoder_info->configure(&options);
    const auto send = decoder_info->send(au->es.pBuf.cast<uint8_t>().get(emuenv.mem), au->es.size);
    decoder_info->set_res(pPicture->frame.frameWidth, pPicture->frame.frameHeight);
    if (send && decoder_info->receive(output)) {
        decoder_info->get_res(pPicture->frame.horizontalSize, pPicture->frame.verticalSize);
        decoder_info->get_pts(pPicture->info.pts.upper, pPicture->info.pts.lower);
        picture->numOfOutput++;
    }

    decoder_info->is_stopped = false;

    return 0;
}

EXPORT(int, sceAvcdecDecodeAuInternal) {
    TRACY_FUNC(sceAvcdecDecodeAuInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecCreateDecoderNongameapp) {
    TRACY_FUNC(sceAvcdecCreateDecoderNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAuNalAuNongameapp) {
    TRACY_FUNC(sceAvcdecDecodeAuNalAuNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAuNongameapp) {
    TRACY_FUNC(sceAvcdecDecodeAuNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAvailableSize, SceAvcdecCtrl *decoder) {
    TRACY_FUNC(sceAvcdecDecodeAvailableSize, decoder);

    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (decoder->handle == 0)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    return H264DecoderState::buffer_size(
        { { decoder_info->get(DecoderQuery::WIDTH), decoder_info->get(DecoderQuery::HEIGHT) } });
}

EXPORT(int, sceAvcdecDecodeFlush, SceAvcdecCtrl *decoder) {
    TRACY_FUNC(sceAvcdecDecodeFlush, decoder);

    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (decoder->handle == 0)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (internal) {
        std::lock_guard<std::mutex> lock(internal->mutex);
        internal->decoder->flush();
        internal->decoder->clear_frames();
        internal->decoder->is_stopped = true;
        internal->pending_pictures = 0;
        internal->last_output_pts = ~0ull;
        return 0;
    }
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    decoder_info->flush();
    decoder_info->is_stopped = true;

    return 0;
}

EXPORT(int, sceAvcdecDecodeGetPictureInternal, SceAvcdecCtrl *decoder, SceAvcdecArrayPicture *pictures, const uint32_t *timeout) {
    TRACY_FUNC(sceAvcdecDecodeGetPictureInternal, decoder, pictures, timeout);
    if (!decoder || !pictures)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (!internal)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    size_t produced = 0;
    std::vector<SceUID> callbacks, event_callbacks;
    int result;
    {
        std::lock_guard<std::mutex> lock(internal->mutex);
        result = avc_get_internal_picture(export_name, emuenv.mem, *internal, pictures, produced, callbacks);
        if (pictures->numOfOutput) {
            for (const auto &[id, mask] : internal->callbacks)
                if (mask & 0x3b00)
                    event_callbacks.push_back(id);
        }
    }
    avc_notify_callbacks(emuenv, callbacks, produced, 2);
    avc_notify_callbacks(emuenv, event_callbacks, 1, 0x100);
    return result;
}

EXPORT(int, sceAvcdecDecodeGetPictureNongameapp) {
    TRACY_FUNC(sceAvcdecDecodeGetPictureNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeGetPictureWithWorkPictureInternal, SceAvcdecCtrl *decoder,
    SceAvcdecArrayPicture *pictures, SceAvcdecArrayPicture *work, const uint32_t *timeout) {
    TRACY_FUNC(sceAvcdecDecodeGetPictureWithWorkPictureInternal, decoder, pictures, work, timeout);
    const int validation = avc_validate_work_pictures(export_name, emuenv.mem, work);
    if (validation != 0)
        return validation;
    return CALL_EXPORT(sceAvcdecDecodeGetPictureInternal, decoder, pictures, timeout);
}

EXPORT(int, sceAvcdecDecodeNalAu) {
    TRACY_FUNC(sceAvcdecDecodeNalAu);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeNalAuWithWorkPicture) {
    TRACY_FUNC(sceAvcdecDecodeNalAuWithWorkPicture);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeSetTrickModeNongameapp) {
    TRACY_FUNC(sceAvcdecDecodeSetTrickModeNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp) {
    TRACY_FUNC(sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeStop, SceAvcdecCtrl *decoder, SceAvcdecArrayPicture *picture) {
    TRACY_FUNC(sceAvcdecDecodeStop, decoder, picture);

    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (decoder->handle == 0)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (internal)
        return avc_stop_decoder(export_name, emuenv, *internal, picture);
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    if (!picture)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!decoder_info->is_stopped && picture->numOfElm && picture->pPicture) {
        SceAvcdecPicture *pPicture = picture->pPicture.get(emuenv.mem)[0].get(emuenv.mem);

        // we get the values from the last frame, maybe we should slightly increase the pts value?
        decoder_info->get_res(pPicture->frame.horizontalSize, pPicture->frame.verticalSize);
        decoder_info->get_pts(pPicture->info.pts.upper, pPicture->info.pts.lower);

        picture->numOfOutput = 1;
    } else {
        picture->numOfOutput = 0;
    }
    decoder_info->is_stopped = true;

    return 0;
}

EXPORT(int, sceAvcdecDecodeStopWithWorkPicture, SceAvcdecCtrl *decoder,
    SceAvcdecArrayPicture *picture, SceAvcdecArrayPicture *work) {
    TRACY_FUNC(sceAvcdecDecodeStopWithWorkPicture, decoder, picture, work);
    const int validation = avc_validate_work_pictures(export_name, emuenv.mem, work);
    if (validation != 0)
        return validation;
    if (!decoder || !picture)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!decoder->handle)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (internal)
        return avc_stop_decoder(export_name, emuenv, *internal, picture);
    const auto decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    VideodecInternalDecoder synchronous;
    synchronous.decoder = decoder_info;
    return avc_stop_decoder(export_name, emuenv, synchronous, picture);
}

EXPORT(int, sceAvcdecDecodeWithWorkPicture) {
    TRACY_FUNC(sceAvcdecDecodeWithWorkPicture);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDeleteDecoder, SceAvcdecCtrl *decoder) {
    TRACY_FUNC(sceAvcdecDeleteDecoder, decoder);

    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (decoder->handle == 0)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->decoders.erase(decoder->handle) == 0)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    state->internal_decoders.erase(decoder->handle);

    return 0;
}

EXPORT(int, sceAvcdecQueryDecoderMemSizeInternal, uint32_t codec_type, const SceAvcdecQueryDecoderInfo *query, SceAvcdecDecoderInfo *decoder_info) {
    TRACY_FUNC(sceAvcdecQueryDecoderMemSizeInternal, codec_type, query, decoder_info);
    if (query)
        LOG_INFO("Internal AVC frame memory query: type {:#x}, {}x{}, {} reference frames", codec_type, query->horizontal, query->vertical, query->numOfRefFrames);
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    if (!query || !decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!avc_library_memory_size(query->horizontal, query->vertical, query->numOfRefFrames, 1))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    if (query->horizontal == 1920 && query->vertical == 1088 && query->numOfRefFrames >= 4) {
        decoder_info->frameMemSize = 0x2000000;
        return 0;
    }
    LOG_ERROR("Internal AVC frame memory calculation is not implemented for {}x{} and {} references",
        query->horizontal, query->vertical, query->numOfRefFrames);
    return RET_ERROR(SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE);
}

EXPORT(int, sceAvcdecGetSeiUserDataNongameapp) {
    TRACY_FUNC(sceAvcdecGetSeiUserDataNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecQueryDecoderMemSize, uint32_t codec_type, const SceAvcdecQueryDecoderInfo *query_info, SceAvcdecDecoderInfo *decoder_info) {
    TRACY_FUNC(sceAvcdecQueryDecoderMemSize, codec_type, query_info, decoder_info);

    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    if (!query_info || !decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);

    if (query_info->horizontal > 1920 || query_info->vertical > 1088 || query_info->horizontal * query_info->vertical > 1280 * 720 || query_info->numOfRefFrames > 16)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    std::uint32_t numOfRefFrames = query_info->numOfRefFrames;
    if (numOfRefFrames == 0) {
        numOfRefFrames = 1;
    }

    decoder_info->frameMemSize = H264DecoderState::buffer_size({ { query_info->horizontal, query_info->vertical } }) * numOfRefFrames;
    decoder_info->frameMemSize = decoder_info->frameMemSize + 0x3ffffU & 0xfffc0000; // alignment to the next multiple of 0x40000 (256-KiB)

    return 0;
}

EXPORT(int, sceAvcdecGetSeiPictureTimingInternal) {
    TRACY_FUNC(sceAvcdecGetSeiPictureTimingInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecQueryDecoderMemSizeNongameapp) {
    TRACY_FUNC(sceAvcdecQueryDecoderMemSizeNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecRegisterCallbackInternal, SceAvcdecCtrl *decoder, SceUID callback_id, uint32_t mask) {
    TRACY_FUNC(sceAvcdecRegisterCallbackInternal, decoder, callback_id, mask);
    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!decoder->handle || !mask)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    const CallbackPtr callback = emuenv.kernel.objects.find<Callback>(callback_id);
    if (!callback || !emuenv.kernel.get_thread(callback->thread_id))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (!internal)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    size_t pending = 0;
    {
        std::lock_guard<std::mutex> lock(internal->mutex);
        internal->callbacks[callback_id] = mask;
        if (mask & 0x2) {
            pending = internal->pending_pictures;
            internal->pending_pictures = 0;
        }
    }
    avc_notify_callbacks(emuenv, { callback_id }, pending, 2);
    return 0;
}

EXPORT(int, sceAvcdecRegisterCallbackNongameapp) {
    TRACY_FUNC(sceAvcdecRegisterCallbackNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetDecodeMode, uint32_t codec_type, uint32_t mode) {
    TRACY_FUNC(sceAvcdecSetDecodeMode, codec_type, mode);
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC || (mode & ~0x80u))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    videodec_configuration(emuenv)->avc_mode.store(mode);
    return 0;
}

EXPORT(int, sceAvcdecSetDecodeModeInternal, uint32_t codec_type, uint32_t mode) {
    TRACY_FUNC(sceAvcdecSetDecodeModeInternal, codec_type, mode);
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC || (mode & ~0x80u)) {
        LOG_ERROR("Unsupported internal video decode mode {:#x} for type {:#x}", mode, codec_type);
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    }
    videodec_configuration(emuenv)->avc_mode.store(mode);
    return 0;
}

EXPORT(int, sceAvcdecSetInterlacedStreamMode) {
    TRACY_FUNC(sceAvcdecSetInterlacedStreamMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetLowDelayModeNongameapp) {
    TRACY_FUNC(sceAvcdecSetLowDelayModeNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetRecoveryPointSEIMode) {
    TRACY_FUNC(sceAvcdecSetRecoveryPointSEIMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackInternal, SceAvcdecCtrl *decoder) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackInternal, decoder);
    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!decoder->handle)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (!internal)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    std::lock_guard<std::mutex> lock(internal->mutex);
    internal->callbacks.clear();
    return 0;
}

EXPORT(int, sceAvcdecUnregisterCallbackNongameapp) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackWithCbidInternal, SceAvcdecCtrl *decoder, SceUID callback_id) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackWithCbidInternal, decoder, callback_id);
    if (!decoder)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_POINTER);
    if (!decoder->handle)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const auto internal = lock_and_find(decoder->handle, state->internal_decoders, state->mutex);
    if (!internal)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    std::lock_guard<std::mutex> lock(internal->mutex);
    if (!internal->callbacks.erase(callback_id))
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);
    return 0;
}

EXPORT(int, sceAvcdecUnregisterCallbackWithCbidNongameapp) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackWithCbidNongameapp);
    return UNIMPLEMENTED();
}

// FFmpeg owns its working memory. This is an HLE guest reservation, not the
// hardware decoder's memory requirement; callers may also supply no buffer.
static constexpr uint32_t m4v_hle_frame_reservation = 0x40000;

template <typename T>
static T *m4v_guest_object(const MemState &mem, Ptr<T> ptr) {
    std::span<const uint8_t> bytes;
    return avc_guest_buffer(mem, { ptr.template cast<void>(), sizeof(T) }, bytes) ? ptr.get(mem) : nullptr;
}

static bool m4v_valid_query(const SceM4vdecQueryDecoderInfo &query) {
    return query.horizontal >= 64 && query.horizontal <= 1280 && query.vertical >= 64 && query.vertical <= 720 && ((query.horizontal | query.vertical) & 15) == 0 && query.maxEsSize >= 2048 && query.maxEsSize <= 0x17c000;
}

static int m4v_surface(const char *export_name, const MemState &mem, SceAvcdecFrame &frame, VideoCscSurface &surface) {
    const uint32_t type = static_cast<uint32_t>(frame.pixelType);
    if (type & 0x40000)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    H264OutputFormat format;
    switch (type & 0xff) {
    case SCE_AVCDEC_PIXEL_RGBA8888: format = H264OutputFormat::rgba; break;
    case SCE_AVCDEC_PIXEL_BGRA8888: format = H264OutputFormat::bgra; break;
    case SCE_AVCDEC_PIXEL_YUV420_RASTER: format = H264OutputFormat::yuv420p; break;
    case SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER: format = H264OutputFormat::nv12; break;
    default: return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    }
    if ((frame.framePitch | frame.frameWidth | frame.frameHeight) & 15 || frame.frameWidth < 16 || frame.frameHeight < 16 || frame.frameWidth > 1920 || frame.frameHeight > 1088 || frame.framePitch < frame.frameWidth || frame.framePitch > 2048)
        return RET_ERROR(SCE_M4VDEC_ERROR_UNSUPPORT_IMAGE_SIZE);
    const bool rgb = format == H264OutputFormat::rgba || format == H264OutputFormat::bgra;
    if (rgb && (type & 0x80000000u) && frame.opt.rgba.cscCoefficient != SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    const uint32_t pixels = frame.framePitch * frame.frameHeight;
    const uint32_t size = rgb ? pixels * 4 : pixels * 3 / 2;
    std::span<const uint8_t> bytes;
    if (!avc_guest_buffer(mem, { frame.pPicture[0], size }, bytes))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    surface = { std::span<uint8_t>(frame.pPicture[0].cast<uint8_t>().get(mem), size),
        frame.frameWidth, frame.frameHeight, frame.framePitch, format };
    return 0;
}

static int m4v_picture_array(const char *export_name, const MemState &mem, SceM4vdecArrayPicture *array,
    uint32_t max_count, SceM4vdecPicture **picture) {
    if (!array)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    if (array->numOfElm > max_count)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    if (!array->numOfElm)
        return 0;
    std::span<const uint8_t> bytes;
    if (!avc_guest_buffer(mem, { array->pPicture.cast<void>(), array->numOfElm * static_cast<uint32_t>(sizeof(Ptr<SceM4vdecPicture>)) }, bytes))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    const auto entries = array->pPicture.get(mem);
    for (uint32_t i = 0; i < array->numOfElm; ++i) {
        if (!avc_guest_buffer(mem, { entries[i].cast<void>(), sizeof(SceM4vdecPicture) }, bytes))
            return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
        auto *item = entries[i].get(mem);
        if (item->size != sizeof(SceM4vdecPicture))
            return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE);
        VideoCscSurface surface{};
        const int error = m4v_surface(export_name, mem, item->frame, surface);
        if (error)
            return error;
        if (picture && i == 0)
            *picture = item;
    }
    return 0;
}

static std::shared_ptr<M4vInstance> m4v_find(EmuEnvState &emuenv, SceUID handle) {
    const auto configuration = videodec_configuration(emuenv);
    std::lock_guard<std::mutex> configuration_lock(configuration->mutex);
    if (!configuration->m4v_initialized)
        return nullptr;
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    return lock_and_find(handle, state->m4v_decoders, state->mutex);
}

static int m4v_create(const char *export_name, EmuEnvState &emuenv, uint32_t type,
    SceAvcdecCtrl *ctrl, const SceM4vdecQueryDecoderInfo *query) {
    if (!ctrl || !query)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    if (type != SCE_VIDEODEC_TYPE_HW_M4VDEC || !m4v_valid_query(*query))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    const auto configuration = videodec_configuration(emuenv);
    std::lock_guard<std::mutex> configuration_lock(configuration->mutex);
    if (!configuration->m4v_initialized)
        return RET_ERROR(SCE_M4VDEC_ERROR_NOT_INITIALIZE);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    if (ctrl->frameBuf.pBuf.address()) {
        std::span<const uint8_t> bytes;
        if ((ctrl->frameBuf.pBuf.address() & 255) || ctrl->frameBuf.size < m4v_hle_frame_reservation || !avc_guest_buffer(emuenv.mem, ctrl->frameBuf, bytes))
            return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    } else if (ctrl->frameBuf.size) {
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    }
    auto decoder = M4vDecoderState::create(query->horizontal, query->vertical);
    if (!decoder)
        return RET_ERROR(SCE_M4VDEC_ERROR_OUT_OF_MEMORY);
    auto instance = std::make_shared<M4vInstance>();
    instance->decoder = std::move(decoder);
    instance->max_es_size = query->maxEsSize;
    instance->input_capacity = std::min(configuration->m4v_input_capacity, query->maxEsSize);
    const SceUID handle = emuenv.kernel.get_next_uid();
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->m4v_decoders.emplace(handle, instance);
    }
    ctrl->handle = handle;
    return 0;
}

static int m4v_output(const char *export_name, M4vInstance &instance, SceM4vdecArrayPicture *array,
    SceM4vdecPicture *picture, VideoCscSurface &surface) {
    if (!array || !picture)
        return 0;
    if (!instance.pending) {
        H264DecodedFrame decoded;
        if (!instance.decoder->take_frame(decoded, &instance.metadata))
            return 0;
        instance.pending = std::move(decoded);
    }
    const H264DecodedFrame &decoded = *instance.pending;
    auto &frame = picture->frame;
    const uint32_t type = static_cast<uint32_t>(frame.pixelType);
    if (instance.first_picture) {
        LOG_INFO("MPEG-4 output: {}x{}, format {:#x}, pitch {}, surface {}x{}, pts {:#x}",
            decoded.width, decoded.height, type, frame.framePitch, frame.frameWidth, frame.frameHeight, decoded.pts);
        instance.first_picture = false;
    }
    const uint32_t aligned_width = (decoded.width + 15) & ~15u;
    const uint32_t aligned_height = (decoded.height + 7) & ~7u;
    const bool native = !(type & 0x80000) && aligned_width <= frame.frameWidth && aligned_height <= frame.frameHeight;
    const uint32_t width = native ? decoded.width : frame.frameWidth;
    const uint32_t height = native ? decoded.height : frame.frameHeight;
    const uint32_t surface_height = native ? aligned_height : frame.frameHeight;
    const bool rgb = surface.format == H264OutputFormat::rgba || surface.format == H264OutputFormat::bgra;
    const uint8_t alpha = rgb && (type & 0x80000000u) ? frame.opt.rgba.alpha : 255;
    if (!h264_render_frame(decoded, surface.data, frame.framePitch, surface_height, width, height, surface.format, alpha))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_STATE);
    frame.horizontalSize = decoded.width;
    frame.verticalSize = decoded.height;
    frame.frameCropLeftOffset = frame.frameCropRightOffset = frame.frameCropTopOffset = frame.frameCropBottomOffset = 0;
    std::memset(&picture->info, 0xff, sizeof(picture->info));
    const auto &meta = instance.metadata;
    if (meta.sar_num > 0 && meta.sar_den > 0 && meta.sar_num <= 65535 && meta.sar_den <= 65535) {
        picture->info.parWidth = static_cast<uint16_t>(meta.sar_num);
        picture->info.parHeight = static_cast<uint16_t>(meta.sar_den);
        picture->info.aspectRatioInfo = 15;
        constexpr int ratios[][2] = { { 1, 1 }, { 12, 11 }, { 10, 11 }, { 16, 11 }, { 40, 33 } };
        for (uint8_t i = 0; i < 5; ++i)
            if (static_cast<int64_t>(meta.sar_num) * ratios[i][1] == static_cast<int64_t>(meta.sar_den) * ratios[i][0])
                picture->info.aspectRatioInfo = i + 1;
    }
    if (meta.color_primaries >= 0 && meta.color_primaries < 255)
        picture->info.colourPrimaries = static_cast<uint8_t>(meta.color_primaries);
    if (meta.color_transfer >= 0 && meta.color_transfer < 255)
        picture->info.transferCharacteristics = static_cast<uint8_t>(meta.color_transfer);
    if (meta.color_matrix >= 0 && meta.color_matrix < 255)
        picture->info.matrixCoefficients = static_cast<uint8_t>(meta.color_matrix);
    if (meta.color_range != M4vColorRange::unknown)
        picture->info.videoRange = 0;
    picture->info.outputPts.upper = static_cast<uint32_t>(meta.output_pts >> 32);
    picture->info.outputPts.lower = static_cast<uint32_t>(meta.output_pts);
    instance.pending.reset();
    array->numOfOutput = 1;
    return 0;
}

EXPORT(int, sceM4vdecCreateDecoder, uint32_t codec_type, Ptr<SceAvcdecCtrl> decoder, Ptr<const SceM4vdecQueryDecoderInfo> query) {
    TRACY_FUNC(sceM4vdecCreateDecoder, codec_type, decoder, query);
    return m4v_create(export_name, emuenv, codec_type, m4v_guest_object(emuenv.mem, decoder), m4v_guest_object(emuenv.mem, query));
}

EXPORT(int, sceM4vdecCreateDecoderInternal, uint32_t codec_type, Ptr<SceAvcdecCtrl> decoder, Ptr<const SceM4vdecQueryDecoderInfo> query) {
    TRACY_FUNC(sceM4vdecCreateDecoderInternal, codec_type, decoder, query);
    return m4v_create(export_name, emuenv, codec_type, m4v_guest_object(emuenv.mem, decoder), m4v_guest_object(emuenv.mem, query));
}

EXPORT(int, sceM4vdecCsc, Ptr<SceM4vdecPicture> destination_ptr, Ptr<const SceM4vdecPicture> source_ptr) {
    TRACY_FUNC(sceM4vdecCsc, destination_ptr, source_ptr);
    auto *destination = m4v_guest_object(emuenv.mem, destination_ptr);
    auto *source = m4v_guest_object(emuenv.mem, source_ptr);
    if (!destination || !source)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    if (destination->size != sizeof(SceM4vdecPicture) || source->size != sizeof(SceM4vdecPicture))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE);
    auto source_frame = source->frame;
    VideoCscSurface input{}, output{};
    int result = m4v_surface(export_name, emuenv.mem, source_frame, input);
    if (result)
        return result;
    result = m4v_surface(export_name, emuenv.mem, destination->frame, output);
    if (result)
        return result;
    const uint32_t type = static_cast<uint32_t>(destination->frame.pixelType);
    const uint8_t alpha = type & 0x80000000u ? destination->frame.opt.rgba.alpha : 255;
    if (!video_convert_surface(input, output, 0, alpha))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_STATE);
    destination->info = source->info;
    destination->frame.horizontalSize = source->frame.horizontalSize;
    destination->frame.verticalSize = source->frame.verticalSize;
    destination->frame.frameCropLeftOffset = source->frame.frameCropLeftOffset;
    destination->frame.frameCropRightOffset = source->frame.frameCropRightOffset;
    destination->frame.frameCropTopOffset = source->frame.frameCropTopOffset;
    destination->frame.frameCropBottomOffset = source->frame.frameCropBottomOffset;
    return 0;
}

static int m4v_decode(const char *export_name, EmuEnvState &emuenv, const SceAvcdecCtrl *ctrl,
    const SceAvcdecAu *au, SceM4vdecArrayPicture *array, SceM4vdecArrayPicture *work, bool stop) {
    if (!ctrl || (!au && !array))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    const auto instance = m4v_find(emuenv, ctrl->handle);
    if (!instance)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    SceM4vdecPicture *picture = nullptr;
    if (array) {
        array->numOfOutput = 0;
        const int error = m4v_picture_array(export_name, emuenv.mem, array, 1, &picture);
        if (error)
            return error;
    }
    if (work) {
        if (!work->numOfElm)
            return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
        const int error = m4v_picture_array(export_name, emuenv.mem, work, 2, nullptr);
        if (error)
            return error;
    }
    VideoCscSurface surface{};
    if (picture) {
        const int error = m4v_surface(export_name, emuenv.mem, picture->frame, surface);
        if (error)
            return error;
    }
    std::span<const uint8_t> data;
    if (au && au->es.size) {
        if (au->es.size > instance->max_es_size)
            return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
        if (!avc_guest_buffer(emuenv.mem, au->es, data))
            return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    } else if (au && au->es.pBuf.address()) {
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    }
    std::lock_guard<std::mutex> lock(instance->mutex);
    if (instance->deleted)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    H264AsyncResult result = H264AsyncResult::success;
    bool output_ready = true;
    if (stop) {
        result = instance->decoder->drain();
        instance->pipeline.finish();
    } else if (!data.empty()) {
        if (instance->first_packet) {
            bool vol = false;
            for (size_t i = 0; i + 3 < data.size(); ++i)
                if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 && (data[i + 3] & 0xf0) == 0x20) {
                    vol = true;
                    break;
                }
            LOG_INFO("MPEG-4 first packet: {} bytes, VOL start code {}", data.size(), vol ? "present" : "absent");
            instance->first_packet = false;
        }
        const uint64_t pts = static_cast<uint64_t>(au->pts.upper) << 32 | au->pts.lower;
        const uint64_t dts = static_cast<uint64_t>(au->dts.upper) << 32 | au->dts.lower;
        result = instance->decoder->send_packet(data, pts, dts);
        if (result == H264AsyncResult::success)
            output_ready = instance->pipeline.submit();
    } else {
        instance->pipeline.finish();
    }
    if (result == H264AsyncResult::invalid_input)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    if (result == H264AsyncResult::decode_error)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_STREAM);
    const int output_result = output_ready ? m4v_output(export_name, *instance, array, picture, surface) : 0;
    if (output_result)
        return output_result;
    if (instance->diagnostic_calls < 12) {
        ++instance->diagnostic_calls;
        LOG_TRACE("MPEG-4 decode {}: input {} bytes, output {}, output pts {:#x}, stop {}",
            ctrl->handle, data.size(), array ? array->numOfOutput : 0,
            array && array->numOfOutput ? instance->metadata.output_pts : ~0ull, stop);
    }
    if (result == H264AsyncResult::full && (!stop || !array || !array->numOfOutput))
        return RET_ERROR(SCE_M4VDEC_ERROR_ES_BUFFER_FULL);
    return 0;
}

EXPORT(int, sceM4vdecDecode, Ptr<const SceAvcdecCtrl> decoder_ptr, Ptr<const SceAvcdecAu> au_ptr, Ptr<SceM4vdecArrayPicture> pictures_ptr) {
    TRACY_FUNC(sceM4vdecDecode, decoder_ptr, au_ptr, pictures_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    auto *au = m4v_guest_object(emuenv.mem, au_ptr);
    auto *pictures = m4v_guest_object(emuenv.mem, pictures_ptr);
    if ((au_ptr && !au) || (pictures_ptr && !pictures))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    return m4v_decode(export_name, emuenv, decoder, au, pictures, nullptr, false);
}

EXPORT(int, sceM4vdecDecodeAvailableSize, Ptr<SceAvcdecCtrl> decoder_ptr) {
    TRACY_FUNC(sceM4vdecDecodeAvailableSize, decoder_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    if (!decoder)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    const auto instance = m4v_find(emuenv, decoder->handle);
    if (!instance)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    std::lock_guard<std::mutex> lock(instance->mutex);
    if (instance->deleted)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    if (instance->pending)
        return 0;
    const H264AsyncResult status = instance->decoder->input_status();
    if (status == H264AsyncResult::decode_error)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_STREAM);
    if (status == H264AsyncResult::full)
        return 0;
    if (status != H264AsyncResult::success)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_STATE);
    return static_cast<int>(instance->input_capacity);
}

EXPORT(int, sceM4vdecDecodeFlush, Ptr<const SceAvcdecCtrl> decoder_ptr) {
    TRACY_FUNC(sceM4vdecDecodeFlush, decoder_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    if (!decoder)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    const auto instance = m4v_find(emuenv, decoder->handle);
    if (!instance)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    std::lock_guard<std::mutex> lock(instance->mutex);
    if (instance->deleted)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    instance->decoder->flush();
    instance->pipeline.finish();
    instance->pending.reset();
    instance->first_packet = true;
    return 0;
}

EXPORT(int, sceM4vdecDecodeStop, Ptr<const SceAvcdecCtrl> decoder_ptr, Ptr<SceM4vdecArrayPicture> pictures_ptr) {
    TRACY_FUNC(sceM4vdecDecodeStop, decoder_ptr, pictures_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    auto *pictures = m4v_guest_object(emuenv.mem, pictures_ptr);
    if (!pictures)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    return m4v_decode(export_name, emuenv, decoder, nullptr, pictures, nullptr, true);
}

EXPORT(int, sceM4vdecDecodeStopWithWorkPicture, Ptr<const SceAvcdecCtrl> decoder_ptr, Ptr<SceM4vdecArrayPicture> pictures_ptr, Ptr<SceM4vdecArrayPicture> work_ptr) {
    TRACY_FUNC(sceM4vdecDecodeStopWithWorkPicture, decoder_ptr, pictures_ptr, work_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    auto *pictures = m4v_guest_object(emuenv.mem, pictures_ptr);
    auto *work = m4v_guest_object(emuenv.mem, work_ptr);
    if (!pictures || (work_ptr && !work))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    return m4v_decode(export_name, emuenv, decoder, nullptr, pictures, work, true);
}

EXPORT(int, sceM4vdecDecodeWithWorkPicture, Ptr<const SceAvcdecCtrl> decoder_ptr, Ptr<const SceAvcdecAu> au_ptr,
    Ptr<SceM4vdecArrayPicture> pictures_ptr, Ptr<SceM4vdecArrayPicture> work_ptr) {
    TRACY_FUNC(sceM4vdecDecodeWithWorkPicture, decoder_ptr, au_ptr, pictures_ptr, work_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    auto *au = m4v_guest_object(emuenv.mem, au_ptr);
    auto *pictures = m4v_guest_object(emuenv.mem, pictures_ptr);
    auto *work = m4v_guest_object(emuenv.mem, work_ptr);
    if ((au_ptr && !au) || (pictures_ptr && !pictures))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    if (work_ptr && !work)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    return m4v_decode(export_name, emuenv, decoder, au, pictures, work, false);
}

EXPORT(int, sceM4vdecDeleteDecoder, Ptr<SceAvcdecCtrl> decoder_ptr) {
    TRACY_FUNC(sceM4vdecDeleteDecoder, decoder_ptr);
    auto *decoder = m4v_guest_object(emuenv.mem, decoder_ptr);
    if (!decoder)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    const auto configuration = videodec_configuration(emuenv);
    std::lock_guard<std::mutex> configuration_lock(configuration->mutex);
    if (!configuration->m4v_initialized)
        return RET_ERROR(SCE_M4VDEC_ERROR_NOT_INITIALIZE);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> state_lock(state->mutex);
    const auto it = state->m4v_decoders.find(decoder->handle);
    if (it == state->m4v_decoders.end())
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    const auto instance = it->second;
    std::lock_guard<std::mutex> lock(instance->mutex);
    instance->deleted = true;
    state->m4v_decoders.erase(it);
    decoder->handle = 0;
    return 0;
}

static int m4v_query_memory(const char *export_name, uint32_t type, const SceM4vdecQueryDecoderInfo *query, uint32_t *size) {
    if (!query || !size)
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_POINTER);
    if (type != SCE_VIDEODEC_TYPE_HW_M4VDEC || !m4v_valid_query(*query))
        return RET_ERROR(SCE_M4VDEC_ERROR_INVALID_PARAM);
    *size = m4v_hle_frame_reservation;
    LOG_INFO_ONCE("MPEG-4 decoder guest reservation is an HLE policy ({} bytes), not a hardware size", *size);
    return 0;
}

EXPORT(int, sceM4vdecQueryDecoderMemSize, uint32_t type, Ptr<const SceM4vdecQueryDecoderInfo> query, Ptr<uint32_t> size) {
    TRACY_FUNC(sceM4vdecQueryDecoderMemSize, type, query, size);
    return m4v_query_memory(export_name, type, m4v_guest_object(emuenv.mem, query), m4v_guest_object(emuenv.mem, size));
}

EXPORT(int, sceM4vdecQueryDecoderMemSizeInternal, uint32_t type, Ptr<const SceM4vdecQueryDecoderInfo> query, Ptr<uint32_t> size) {
    TRACY_FUNC(sceM4vdecQueryDecoderMemSizeInternal, type, query, size);
    return m4v_query_memory(export_name, type, m4v_guest_object(emuenv.mem, query), m4v_guest_object(emuenv.mem, size));
}

static int videodec_init_library(const char *export_name, EmuEnvState &emuenv, uint32_t codec_type,
    const SceVideodecQueryInitInfo *query) {
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC && codec_type != SCE_VIDEODEC_TYPE_HW_M4VDEC)
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    if (codec_type == SCE_VIDEODEC_TYPE_HW_M4VDEC) {
        if (!query)
            return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_POINTER);
        if (query->size != 20)
            return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE);
        if (!m4v_library_memory_size(query->horizontal, query->vertical, query->num_of_ref_frames, query->num_of_streams))
            return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    }
    const auto configuration = videodec_configuration(emuenv);
    std::lock_guard<std::mutex> lock(configuration->mutex);
    emuenv.kernel.obj_store.create<VideodecState>();
    if (codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC)
        configuration->avc_initialized = true;
    else {
        if (!configuration->m4v_initialized) {
            configuration->m4v_input_capacity = *m4v_input_buffer_capacity(query->horizontal, query->vertical);
        }
        configuration->m4v_initialized = true;
    }
    return 0;
}

EXPORT(int, sceVideodecInitLibrary, uint32_t codec_type, const SceVideodecQueryInitInfo *query) {
    TRACY_FUNC(sceVideodecInitLibrary, codec_type, query);
    return videodec_init_library(export_name, emuenv, codec_type, query);
}

EXPORT(int, sceVideodecInitLibraryInternal, uint32_t codec_type, const SceVideodecQueryInitInfo *query) {
    TRACY_FUNC(sceVideodecInitLibraryInternal, codec_type, query);
    return videodec_init_library(export_name, emuenv, codec_type, query);
}

EXPORT(int, sceVideodecInitLibraryNongameapp) {
    TRACY_FUNC(sceVideodecInitLibraryNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMem, uint32_t codec_type, SceAvcdecCtrl *ctrl, const SceVideodecQueryInitInfo *query) {
    TRACY_FUNC(sceVideodecInitLibraryWithUnmapMem, codec_type, ctrl, query);
    return videodec_init_library(export_name, emuenv, codec_type, query);
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMemInternal, uint32_t codec_type, SceAvcdecCtrl *ctrl, const SceVideodecQueryInitInfo *query) {
    TRACY_FUNC(sceVideodecInitLibraryWithUnmapMemInternal, codec_type, ctrl, query);
    return videodec_init_library(export_name, emuenv, codec_type, query);
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMemNongameapp) {
    TRACY_FUNC(sceVideodecInitLibraryWithUnmapMemNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryInstanceNongameapp) {
    TRACY_FUNC(sceVideodecQueryInstanceNongameapp);
    return UNIMPLEMENTED();
}

static int query_video_library_memory(const char *export_name, uint32_t codec_type, const SceVideodecQueryInitInfo *query, uint32_t *memory_size) {
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC && codec_type != SCE_VIDEODEC_TYPE_HW_M4VDEC)
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    if (!query || !memory_size)
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_POINTER);
    if (query->size != 20)
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE);

    const auto size = codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC
        ? avc_library_memory_size(query->horizontal, query->vertical, query->num_of_ref_frames, query->num_of_streams)
        : m4v_library_memory_size(query->horizontal, query->vertical, query->num_of_ref_frames, query->num_of_streams);
    if (!size)
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    *memory_size = *size;
    LOG_INFO("Video library memory size: type {:#x}, {} bytes", codec_type, *size);
    return 0;
}

EXPORT(int, sceVideodecQueryMemSize, uint32_t codec_type, const SceVideodecQueryInitInfo *query, uint32_t *memory_size) {
    TRACY_FUNC(sceVideodecQueryMemSize, codec_type, query, memory_size);
    return query_video_library_memory(export_name, codec_type, query, memory_size);
}

EXPORT(int, sceVideodecQueryMemSizeInternal, uint32_t codec_type, const SceVideodecQueryInitInfo *query, uint32_t *memory_size) {
    TRACY_FUNC(sceVideodecQueryMemSizeInternal, codec_type, query, memory_size);
    if (query)
        LOG_INFO("Video memory query: type {:#x}, size {}, {}x{}, {} reference frames, {} streams",
            codec_type, query->size, query->horizontal, query->vertical, query->num_of_ref_frames, query->num_of_streams);
    return query_video_library_memory(export_name, codec_type, query, memory_size);
}

EXPORT(int, sceVideodecQueryMemSizeNongameapp) {
    TRACY_FUNC(sceVideodecQueryMemSizeNongameapp);
    return UNIMPLEMENTED();
}

static int videodec_set_config(const char *export_name, EmuEnvState &emuenv, uint32_t codec_type, uint32_t config) {
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC || (config != 1 && config != 2))
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    if (config != 2) {
        LOG_ERROR("AVC single-stream codec engine configuration is not implemented");
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    }
    const auto state = videodec_configuration(emuenv);
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->avc_initialized)
        return RET_ERROR(SCE_VIDEODEC_ERROR_ALREADY_USED);
    state->avc_backend = config;
    return 0;
}

EXPORT(int, sceVideodecSetConfig, uint32_t codec_type, uint32_t config) {
    TRACY_FUNC(sceVideodecSetConfig, codec_type, config);
    return videodec_set_config(export_name, emuenv, codec_type, config);
}

EXPORT(int, sceVideodecSetConfigInternal, uint32_t codec_type, uint32_t config) {
    TRACY_FUNC(sceVideodecSetConfigInternal, codec_type, config);
    return videodec_set_config(export_name, emuenv, codec_type, config);
}

EXPORT(int, sceVideodecTermLibrary, uint32_t codec_type) {
    TRACY_FUNC(sceVideodecTermLibrary, codec_type);
    if (codec_type != SCE_VIDEODEC_TYPE_HW_AVCDEC && codec_type != SCE_VIDEODEC_TYPE_HW_M4VDEC)
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    const auto configuration = videodec_configuration(emuenv);
    std::lock_guard<std::mutex> configuration_lock(configuration->mutex);
    if ((codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC && !configuration->avc_initialized) || (codec_type == SCE_VIDEODEC_TYPE_HW_M4VDEC && !configuration->m4v_initialized))
        return RET_ERROR(SCE_VIDEODEC_ERROR_INVALID_PARAM);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> state_lock(state->mutex);
    if (codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
        state->decoders.clear();
        state->internal_decoders.clear();
        configuration->avc_initialized = false;
    } else {
        for (auto &[handle, instance] : state->m4v_decoders) {
            std::lock_guard<std::mutex> lock(instance->mutex);
            instance->deleted = true;
        }
        state->m4v_decoders.clear();
        configuration->m4v_initialized = false;
        configuration->m4v_input_capacity = 0;
    }
    return 0;
}
