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

#include <module/module.h>

#include <codec/state.h>
#include <kernel/state.h>
#include <util/lock_and_find.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceVideodecUser);

typedef std::shared_ptr<H264DecoderState> H264DecoderPtr;
typedef std::map<SceUID, H264DecoderPtr> H264DecoderStates;

struct VideodecState {
    std::mutex mutex;
    H264DecoderStates decoders;
};

enum {
    SCE_AVCDEC_ERROR_INVALID_PARAM = 0x80620002,
};

enum SceVideodecType {
    SCE_VIDEODEC_TYPE_HW_AVCDEC = 0x1001
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

EXPORT(int, sceAvcdecCreateDecoder, uint32_t codec_type, SceAvcdecCtrl *decoder, const SceAvcdecQueryDecoderInfo *query) {
    TRACY_FUNC(sceAvcdecCreateDecoder, codec_type, decoder, query);
    assert(codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    SceUID handle = emuenv.kernel.get_next_uid();
    decoder->handle = handle;

    state->decoders[handle] = std::make_shared<H264DecoderState>(query->horizontal, query->vertical);

    return 0;
}

EXPORT(int, sceAvcdecCreateDecoderInternal) {
    TRACY_FUNC(sceAvcdecCreateDecoderInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecCreateDecoderNongameapp) {
    TRACY_FUNC(sceAvcdecCreateDecoderNongameapp);
    return UNIMPLEMENTED();
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

EXPORT(int, sceAvcdecDecodeAuNalAuInternal) {
    TRACY_FUNC(sceAvcdecDecodeAuNalAuInternal);
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
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    return H264DecoderState::buffer_size(
        { { decoder_info->get(DecoderQuery::WIDTH), decoder_info->get(DecoderQuery::HEIGHT) } });
}

EXPORT(int, sceAvcdecDecodeFlush, SceAvcdecCtrl *decoder) {
    TRACY_FUNC(sceAvcdecDecodeFlush, decoder);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    decoder_info->flush();
    decoder_info->is_stopped = true;

    return 0;
}

EXPORT(int, sceAvcdecDecodeGetPictureInternal) {
    TRACY_FUNC(sceAvcdecDecodeGetPictureInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeGetPictureNongameapp) {
    TRACY_FUNC(sceAvcdecDecodeGetPictureNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeGetPictureWithWorkPictureInternal) {
    TRACY_FUNC(sceAvcdecDecodeGetPictureWithWorkPictureInternal);
    return UNIMPLEMENTED();
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
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    const H264DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);
    if (!decoder_info)
        return RET_ERROR(SCE_AVCDEC_ERROR_INVALID_PARAM);

    if (!decoder_info->is_stopped) {
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

EXPORT(int, sceAvcdecDecodeStopWithWorkPicture) {
    TRACY_FUNC(sceAvcdecDecodeStopWithWorkPicture);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeWithWorkPicture) {
    TRACY_FUNC(sceAvcdecDecodeWithWorkPicture);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDeleteDecoder, SceAvcdecCtrl *decoder) {
    TRACY_FUNC(sceAvcdecDeleteDecoder, decoder);
    const auto state = emuenv.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    state->decoders.erase(decoder->handle);

    return 0;
}

EXPORT(int, sceAvcdecGetSeiPictureTimingInternal) {
    TRACY_FUNC(sceAvcdecGetSeiPictureTimingInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecGetSeiUserDataNongameapp) {
    TRACY_FUNC(sceAvcdecGetSeiUserDataNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecQueryDecoderMemSize, uint32_t codec_type, const SceAvcdecQueryDecoderInfo *query_info, SceAvcdecDecoderInfo *decoder_info) {
    TRACY_FUNC(sceAvcdecQueryDecoderMemSize, codec_type, query_info, decoder_info);
    assert(codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC);

    decoder_info->frameMemSize = H264DecoderState::buffer_size({ { query_info->horizontal, query_info->vertical } }) * query_info->numOfRefFrames;

    return 0;
}

EXPORT(int, sceAvcdecQueryDecoderMemSizeInternal) {
    TRACY_FUNC(sceAvcdecQueryDecoderMemSizeInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecQueryDecoderMemSizeNongameapp) {
    TRACY_FUNC(sceAvcdecQueryDecoderMemSizeNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecRegisterCallbackInternal) {
    TRACY_FUNC(sceAvcdecRegisterCallbackInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecRegisterCallbackNongameapp) {
    TRACY_FUNC(sceAvcdecRegisterCallbackNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetDecodeMode) {
    TRACY_FUNC(sceAvcdecSetDecodeMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetDecodeModeInternal) {
    TRACY_FUNC(sceAvcdecSetDecodeModeInternal);
    return UNIMPLEMENTED();
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

EXPORT(int, sceAvcdecUnregisterCallbackInternal) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackNongameapp) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackWithCbidInternal) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackWithCbidInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackWithCbidNongameapp) {
    TRACY_FUNC(sceAvcdecUnregisterCallbackWithCbidNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecCreateDecoder) {
    TRACY_FUNC(sceM4vdecCreateDecoder);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecCreateDecoderInternal) {
    TRACY_FUNC(sceM4vdecCreateDecoderInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecCsc) {
    TRACY_FUNC(sceM4vdecCsc);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecode) {
    TRACY_FUNC(sceM4vdecDecode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeAvailableSize) {
    TRACY_FUNC(sceM4vdecDecodeAvailableSize);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeFlush) {
    TRACY_FUNC(sceM4vdecDecodeFlush);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeStop) {
    TRACY_FUNC(sceM4vdecDecodeStop);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeStopWithWorkPicture) {
    TRACY_FUNC(sceM4vdecDecodeStopWithWorkPicture);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeWithWorkPicture) {
    TRACY_FUNC(sceM4vdecDecodeWithWorkPicture);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDeleteDecoder) {
    TRACY_FUNC(sceM4vdecDeleteDecoder);
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecQueryDecoderMemSize) {
    TRACY_FUNC(sceM4vdecQueryDecoderMemSize);
    STUBBED("fake size");
    return 53;
}

EXPORT(int, sceM4vdecQueryDecoderMemSizeInternal) {
    TRACY_FUNC(sceM4vdecQueryDecoderMemSizeInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibrary) {
    TRACY_FUNC(sceVideodecInitLibrary);
    emuenv.kernel.obj_store.create<VideodecState>();
    return 0;
}

EXPORT(int, sceVideodecInitLibraryInternal) {
    TRACY_FUNC(sceVideodecInitLibraryInternal);
    emuenv.kernel.obj_store.create<VideodecState>();
    return 0;
}

EXPORT(int, sceVideodecInitLibraryNongameapp) {
    TRACY_FUNC(sceVideodecInitLibraryNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMem) {
    TRACY_FUNC(sceVideodecInitLibraryWithUnmapMem);
    emuenv.kernel.obj_store.create<VideodecState>();
    return 0;
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMemInternal) {
    TRACY_FUNC(sceVideodecInitLibraryWithUnmapMemInternal);
    emuenv.kernel.obj_store.create<VideodecState>();
    return 0;
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMemNongameapp) {
    TRACY_FUNC(sceVideodecInitLibraryWithUnmapMemNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryInstanceNongameapp) {
    TRACY_FUNC(sceVideodecQueryInstanceNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryMemSize) {
    TRACY_FUNC(sceVideodecQueryMemSize);
    STUBBED("fake size");
    return 53;
}

EXPORT(int, sceVideodecQueryMemSizeInternal) {
    TRACY_FUNC(sceVideodecQueryMemSizeInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryMemSizeNongameapp) {
    TRACY_FUNC(sceVideodecQueryMemSizeNongameapp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecSetConfig) {
    TRACY_FUNC(sceVideodecSetConfig);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecSetConfigInternal) {
    TRACY_FUNC(sceVideodecSetConfigInternal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecTermLibrary) {
    TRACY_FUNC(sceVideodecTermLibrary);
    emuenv.kernel.obj_store.erase<VideodecState>();
    return 0;
}
