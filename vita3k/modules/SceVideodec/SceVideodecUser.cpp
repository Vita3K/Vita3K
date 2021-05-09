// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include "SceVideodecUser.h"
#include <codec/state.h>
#include <util/lock_and_find.h>

typedef std::shared_ptr<DecoderState> DecoderPtr;
typedef std::map<SceUID, DecoderPtr> DecoderStates;

struct VideodecState {
    std::mutex mutex;
    DecoderStates decoders;
};

enum SceVideodecType {
    SCE_VIDEODEC_TYPE_HW_AVCDEC = 0x1001
};

struct SceVideodecTimeStamp {
    uint32_t upper;
    uint32_t lower;
};

struct SceAvcdecFrameOptionRGBA {
    uint8_t alpha;
    uint8_t cscCoefficient;
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

struct SceAvcdecFrame {
    uint32_t pixelType;
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
    assert(codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC);
    const auto state = host.kernel.obj_store.get<VideodecState>();
    SceUID handle = host.kernel.get_next_uid();
    decoder->handle = handle;

    state->decoders[handle] = std::make_shared<H264DecoderState>(query->horizontal, query->vertical);

    return 0;
}

EXPORT(int, sceAvcdecCreateDecoderInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecCreateDecoderNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecCsc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecCscInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecode, SceAvcdecCtrl *decoder, const SceAvcdecAu *au, SceAvcdecArrayPicture *picture) {
    const auto state = host.kernel.obj_store.get<VideodecState>();
    const DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);

    H264DecoderOptions options = {};
    options.pts_upper = au->pts.upper;
    options.pts_lower = au->pts.lower;
    options.dts_upper = au->dts.upper;
    options.dts_lower = au->dts.lower;

    // This is quite long...
    uint8_t *output = picture->pPicture.get(host.mem)[0].get(host.mem)->frame.pPicture[0].cast<uint8_t>().get(host.mem);

    // TODO: decoding can be done async I think
    decoder_info->configure(&options);
    decoder_info->send(reinterpret_cast<uint8_t *>(au->es.pBuf.get(host.mem)), au->es.size);
    decoder_info->receive(output);

    picture->numOfOutput++;

    return 0;
}

EXPORT(int, sceAvcdecDecodeAuInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAuNalAuInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAuNalAuNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAuNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeAvailableSize, SceAvcdecCtrl *decoder) {
    const auto state = host.kernel.obj_store.get<VideodecState>();
    const DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);

    return H264DecoderState::buffer_size(
        { decoder_info->get(DecoderQuery::WIDTH), decoder_info->get(DecoderQuery::HEIGHT) });
}

EXPORT(int, sceAvcdecDecodeFlush, SceAvcdecCtrl *decoder) {
    const auto state = host.kernel.obj_store.get<VideodecState>();
    const DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);

    decoder_info->flush();

    return 0;
}

EXPORT(int, sceAvcdecDecodeGetPictureInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeGetPictureNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeGetPictureWithWorkPictureInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeNalAu) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeNalAuWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeSetTrickModeNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeStop, SceAvcdecCtrl *decoder, SceAvcdecArrayPicture *picture) {
    const auto state = host.kernel.obj_store.get<VideodecState>();
    const DecoderPtr &decoder_info = lock_and_find(decoder->handle, state->decoders, state->mutex);

    uint8_t *output = picture->pPicture.get(host.mem)[0].get(host.mem)->frame.pPicture[0].cast<uint8_t>().get(host.mem);
    decoder_info->receive(output);

    return 0;
}

EXPORT(int, sceAvcdecDecodeStopWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDeleteDecoder, SceAvcdecCtrl *decoder) {
    const auto state = host.kernel.obj_store.get<VideodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    state->decoders.erase(decoder->handle);

    return 0;
}

EXPORT(int, sceAvcdecGetSeiPictureTimingInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecGetSeiUserDataNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecQueryDecoderMemSize, uint32_t codec_type, const SceAvcdecQueryDecoderInfo *query_info, SceAvcdecDecoderInfo *decoder_info) {
    assert(codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC);

    decoder_info->frameMemSize = H264DecoderState::buffer_size({ query_info->horizontal, query_info->vertical }) * query_info->numOfRefFrames;

    return 0;
}

EXPORT(int, sceAvcdecQueryDecoderMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecQueryDecoderMemSizeNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecRegisterCallbackInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecRegisterCallbackNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetDecodeMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetDecodeModeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetInterlacedStreamMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetLowDelayModeNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecSetRecoveryPointSEIMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackWithCbidInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecUnregisterCallbackWithCbidNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecCreateDecoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecCreateDecoderInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecCsc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeAvailableSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeStopWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDecodeWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecDeleteDecoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecQueryDecoderMemSize) {
    STUBBED("fake size");
    return 53;
}

EXPORT(int, sceM4vdecQueryDecoderMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibrary) {
    host.kernel.obj_store.create<VideodecState>();
    return 0;
}

EXPORT(int, sceVideodecInitLibraryInternal) {
    host.kernel.obj_store.create<VideodecState>();
    return 0;
}

EXPORT(int, sceVideodecInitLibraryNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMemInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMemNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryInstanceNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryMemSize) {
    STUBBED("fake size");
    return 53;
}

EXPORT(int, sceVideodecQueryMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecQueryMemSizeNongameapp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecSetConfig) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecSetConfigInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecTermLibrary) {
    host.kernel.obj_store.erase<VideodecState>();
    return 0;
}

BRIDGE_IMPL(sceAvcdecCreateDecoder)
BRIDGE_IMPL(sceAvcdecCreateDecoderInternal)
BRIDGE_IMPL(sceAvcdecCreateDecoderNongameapp)
BRIDGE_IMPL(sceAvcdecCsc)
BRIDGE_IMPL(sceAvcdecCscInternal)
BRIDGE_IMPL(sceAvcdecDecode)
BRIDGE_IMPL(sceAvcdecDecodeAuInternal)
BRIDGE_IMPL(sceAvcdecDecodeAuNalAuInternal)
BRIDGE_IMPL(sceAvcdecDecodeAuNalAuNongameapp)
BRIDGE_IMPL(sceAvcdecDecodeAuNongameapp)
BRIDGE_IMPL(sceAvcdecDecodeAvailableSize)
BRIDGE_IMPL(sceAvcdecDecodeFlush)
BRIDGE_IMPL(sceAvcdecDecodeGetPictureInternal)
BRIDGE_IMPL(sceAvcdecDecodeGetPictureNongameapp)
BRIDGE_IMPL(sceAvcdecDecodeGetPictureWithWorkPictureInternal)
BRIDGE_IMPL(sceAvcdecDecodeNalAu)
BRIDGE_IMPL(sceAvcdecDecodeNalAuWithWorkPicture)
BRIDGE_IMPL(sceAvcdecDecodeSetTrickModeNongameapp)
BRIDGE_IMPL(sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp)
BRIDGE_IMPL(sceAvcdecDecodeStop)
BRIDGE_IMPL(sceAvcdecDecodeStopWithWorkPicture)
BRIDGE_IMPL(sceAvcdecDecodeWithWorkPicture)
BRIDGE_IMPL(sceAvcdecDeleteDecoder)
BRIDGE_IMPL(sceAvcdecGetSeiPictureTimingInternal)
BRIDGE_IMPL(sceAvcdecGetSeiUserDataNongameapp)
BRIDGE_IMPL(sceAvcdecQueryDecoderMemSize)
BRIDGE_IMPL(sceAvcdecQueryDecoderMemSizeInternal)
BRIDGE_IMPL(sceAvcdecQueryDecoderMemSizeNongameapp)
BRIDGE_IMPL(sceAvcdecRegisterCallbackInternal)
BRIDGE_IMPL(sceAvcdecRegisterCallbackNongameapp)
BRIDGE_IMPL(sceAvcdecSetDecodeMode)
BRIDGE_IMPL(sceAvcdecSetDecodeModeInternal)
BRIDGE_IMPL(sceAvcdecSetInterlacedStreamMode)
BRIDGE_IMPL(sceAvcdecSetLowDelayModeNongameapp)
BRIDGE_IMPL(sceAvcdecSetRecoveryPointSEIMode)
BRIDGE_IMPL(sceAvcdecUnregisterCallbackInternal)
BRIDGE_IMPL(sceAvcdecUnregisterCallbackNongameapp)
BRIDGE_IMPL(sceAvcdecUnregisterCallbackWithCbidInternal)
BRIDGE_IMPL(sceAvcdecUnregisterCallbackWithCbidNongameapp)
BRIDGE_IMPL(sceM4vdecCreateDecoder)
BRIDGE_IMPL(sceM4vdecCreateDecoderInternal)
BRIDGE_IMPL(sceM4vdecCsc)
BRIDGE_IMPL(sceM4vdecDecode)
BRIDGE_IMPL(sceM4vdecDecodeAvailableSize)
BRIDGE_IMPL(sceM4vdecDecodeFlush)
BRIDGE_IMPL(sceM4vdecDecodeStop)
BRIDGE_IMPL(sceM4vdecDecodeStopWithWorkPicture)
BRIDGE_IMPL(sceM4vdecDecodeWithWorkPicture)
BRIDGE_IMPL(sceM4vdecDeleteDecoder)
BRIDGE_IMPL(sceM4vdecQueryDecoderMemSize)
BRIDGE_IMPL(sceM4vdecQueryDecoderMemSizeInternal)
BRIDGE_IMPL(sceVideodecInitLibrary)
BRIDGE_IMPL(sceVideodecInitLibraryInternal)
BRIDGE_IMPL(sceVideodecInitLibraryNongameapp)
BRIDGE_IMPL(sceVideodecInitLibraryWithUnmapMem)
BRIDGE_IMPL(sceVideodecInitLibraryWithUnmapMemInternal)
BRIDGE_IMPL(sceVideodecInitLibraryWithUnmapMemNongameapp)
BRIDGE_IMPL(sceVideodecQueryInstanceNongameapp)
BRIDGE_IMPL(sceVideodecQueryMemSize)
BRIDGE_IMPL(sceVideodecQueryMemSizeInternal)
BRIDGE_IMPL(sceVideodecQueryMemSizeNongameapp)
BRIDGE_IMPL(sceVideodecSetConfig)
BRIDGE_IMPL(sceVideodecSetConfigInternal)
BRIDGE_IMPL(sceVideodecTermLibrary)
