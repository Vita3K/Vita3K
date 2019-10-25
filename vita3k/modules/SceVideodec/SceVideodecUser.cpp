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

#include "SceVideodecUser.h"

#include <psp2/videodec.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace emu {
struct SceAvcdecBuf {
    Ptr<void> pBuf;
    uint32_t size;
};

struct SceAvcdecCtrl {
    uint32_t handle;
    emu::SceAvcdecBuf frameBuf;
};

struct SceAvcdecAu {
    SceVideodecTimeStamp pts;
    SceVideodecTimeStamp dts;
    emu::SceAvcdecBuf es;
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
    uint32_t       size;
    SceAvcdecFrame frame;
    SceAvcdecInfo  info;
};

struct SceAvcdecArrayPicture {
    uint32_t numOfOutput;
    uint32_t numOfElm;
    Ptr<Ptr<emu::SceAvcdecPicture>> pPicture;
};
}

inline uint32_t calculate_buffer_size(uint32_t width, uint32_t height, uint32_t frameRefs) {
    return width * height * 3 / 2 * frameRefs;
}

void send_decoder_data(MemState &mem, DecoderPtr &decoder_info, const emu::SceAvcdecAu *au) {
    int error = 0;

    std::vector<uint8_t> au_frame(au->es.size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(au_frame.data(), au->es.pBuf.get(mem), au->es.size);

    uint64_t pts = static_cast<uint64_t>(au->pts.upper) << 32u | static_cast<uint64_t>(au->pts.lower);
    uint64_t dts = static_cast<uint64_t>(au->dts.upper) << 32u | static_cast<uint64_t>(au->dts.lower);
    if (pts == ~0ull)
        pts = AV_NOPTS_VALUE;
    if (dts == ~0ull)
        dts = AV_NOPTS_VALUE;

    AVPacket *packet = av_packet_alloc();
    error = av_parser_parse2(
        decoder_info->parser, // AVCodecParserContext *s,
        decoder_info->context, // AVCodecContext *avctx,
        &packet->data, // uint8_t **poutbuf,
        &packet->size, // int *poutbuf_size,
        au_frame.data(), // const uint8_t *buf,
        au_frame.size(), // int buf_size,
        pts, // int64_t pts,
        dts, // int64_t dts,
        0 // int64_t pos
    );
    assert(error >= 0);

    error = avcodec_send_packet(decoder_info->context, packet);
    assert(error == 0);

    av_packet_free(&packet);
}

void receive_decoder_data(MemState &mem, DecoderPtr &decoder_info, emu::SceAvcdecArrayPicture *picture) {
    AVFrame *frame = av_frame_alloc();

    if (avcodec_receive_frame(decoder_info->context, frame) == 0) {
        emu::SceAvcdecFrame *avc_frame = &picture->pPicture.get(mem)[0].get(mem)->frame;
        auto *avc_data = reinterpret_cast<uint8_t *>(avc_frame->pPicture[0].get(mem));
        for (uint32_t a = 0; a < frame->height; a++) {
            memcpy(avc_data, &frame->data[0][frame->linesize[0] * a], frame->width);
            avc_data += frame->width;
        }
        for (uint32_t a = 0; a < frame->height / 2; a++) {
            memcpy(avc_data, &frame->data[1][frame->linesize[1] * a], frame->width / 2);
            avc_data += frame->width / 2;
        }
        for (uint32_t a = 0; a < frame->height / 2; a++) {
            memcpy(avc_data, &frame->data[2][frame->linesize[2] * a], frame->width / 2);
            avc_data += frame->width / 2;
        }
        picture->numOfOutput++;
    }

    av_frame_free(&frame);
}

EXPORT(int, sceAvcdecCreateDecoder, uint32_t codec_type, emu::SceAvcdecCtrl *decoder, const SceAvcdecQueryDecoderInfo *query) {
    assert(codec_type == SCE_VIDEODEC_TYPE_HW_AVCDEC);

    SceUID handle = host.kernel.get_next_uid();
    decoder->handle = handle;

    host.kernel.decoders[handle] = std::make_shared<DecoderState>();
    DecoderPtr &decoder_info = host.kernel.decoders[handle];
    decoder_info->frame_width = query->horizontal;
    decoder_info->frame_height = query->vertical;
    decoder_info->frame_ref_count = query->numOfRefFrames;

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    assert(codec);

    decoder_info->parser = av_parser_init(codec->id);
    assert(decoder_info->parser);
    decoder_info->frame_width = decoder_info->frame_width;
    decoder_info->frame_height = decoder_info->frame_height;
    decoder_info->parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

    decoder_info->context = avcodec_alloc_context3(codec);
    assert(decoder_info->context);
    decoder_info->context->width = query->horizontal;
    decoder_info->context->height = query->vertical;

    int result = avcodec_open2(decoder_info->context, codec, nullptr);
    assert(result == 0);

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

EXPORT(int, sceAvcdecDecode, emu::SceAvcdecCtrl *decoder, const emu::SceAvcdecAu *au, emu::SceAvcdecArrayPicture *picture) {
    DecoderPtr &decoder_info = host.kernel.decoders[decoder->handle];

    // TODO: decoding can be done async I think
    send_decoder_data(host.mem, decoder_info, au);
    receive_decoder_data(host.mem, decoder_info, picture);

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

EXPORT(int, sceAvcdecDecodeAvailableSize, emu::SceAvcdecCtrl *decoder) {
    DecoderPtr &decoder_info = host.kernel.decoders[decoder->handle];

    return calculate_buffer_size(
        decoder_info->frame_width, decoder_info->frame_height, decoder_info->frame_ref_count);
}

EXPORT(int, sceAvcdecDecodeFlush, emu::SceAvcdecCtrl *decoder) {
    auto &decoder_info = host.kernel.decoders[decoder->handle];

    avcodec_flush_buffers(decoder_info->context);

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

EXPORT(int, sceAvcdecDecodeStop, emu::SceAvcdecCtrl *decoder, emu::SceAvcdecArrayPicture *picture) {
    auto &decoder_info = host.kernel.decoders[decoder->handle];

    receive_decoder_data(host.mem, decoder_info, picture);

    return 0;
}

EXPORT(int, sceAvcdecDecodeStopWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDecodeWithWorkPicture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcdecDeleteDecoder, emu::SceAvcdecCtrl *decoder) {
    auto &decoder_info = host.kernel.decoders[decoder->handle];

    avcodec_close(decoder_info->context);
    av_parser_close(decoder_info->parser);

    host.kernel.decoders.erase(decoder->handle);

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

    decoder_info->frameMemSize =
        calculate_buffer_size(query_info->horizontal, query_info->vertical, query_info->numOfRefFrames);

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
    return UNIMPLEMENTED();
}

EXPORT(int, sceM4vdecQueryDecoderMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideodecInitLibrary) {
    return STUBBED("EMPTY");
}

EXPORT(int, sceVideodecInitLibraryInternal) {
    return UNIMPLEMENTED();
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
    return UNIMPLEMENTED();
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
    return UNIMPLEMENTED();
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
