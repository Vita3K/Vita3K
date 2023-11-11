// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
#include <codec/types.h>
#include <kernel/state.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceJpegUser);

typedef std::shared_ptr<MjpegDecoderState> DecoderPtr;

struct MJpegState {
    bool initialized = false;
    DecoderPtr decoder;
};

struct SceJpegMJpegInitInfo {
    SceSize size;
    int maxSplitDecoder;
    int option;
};

struct SceJpegPitch {
    uint32_t x;
    uint32_t y;
};

enum SceJpegColorSpace : int {
    SCE_JPEG_COLORSPACE_UNKNOWN = 0x00000,
    SCE_JPEG_COLORSPACE_GRAYSCALE = 0x10000,
    SCE_JPEG_COLORSPACE_YUV = 0x20000,
    SCE_JPEG_COLORSPACE_YUV444 = 0x20101,
    SCE_JPEG_COLORSPACE_YUV422 = 0x20102,
    SCE_JPEG_COLORSPACE_YUV420 = 0x20202,
};

enum SceJpegDHTMode : int {
    SCE_JPEG_MJPEG_WITH_DHT,
    SCE_JPEG_MJPEG_WITHOUT_DHT,
    SCE_JPEG_MJPEG_ANY_SAMPLING_WITHOUT_DHT,
    SCE_JPEG_MJPEG_ANY_SAMPLING
};

enum SceJpegDownscaleMode : int {
    SCE_JPEG_MJPEG_DOWNSCALE_1_2 = 1 << 4,
    SCE_JPEG_MJPEG_DOWNSCALE_1_4 = 1 << 5,
    SCE_JPEG_MJPEG_DOWNSCALE_1_8 = 1 << 6,
    SCE_JPEG_MJPEG_DOWNSCALE_ANY = 0b111 << 4
};

enum SceJpegFormat : int {
    // YUV format
    SCE_JPEG_NO_CSC_OUTPUT = -1,
    SCE_JPEG_PIXEL_RGBA8888 = 0,
    SCE_JPEG_PIXEL_BGRA8888 = 4
};

enum SceJpegColorConversion : int {
    SCE_JPEG_COLORSPACE_JFIF = 0,
    SCE_JPEG_COLORSPACE_BT601 = 0x10
};

struct SceJpegOutputInfo {
    SceJpegColorSpace color_space;
    uint16_t width;
    uint16_t height;
    uint32_t output_size;
    uint32_t temp_buffer_size;
    uint32_t coef_buffer_size;
    SceJpegPitch pitch[4];
};

SceJpegColorSpace convert_color_space_decoder_to_jpeg(DecoderColorSpace color_space) {
    switch (color_space) {
    case COLORSPACE_GRAYSCALE:
        return SCE_JPEG_COLORSPACE_GRAYSCALE;
    case COLORSPACE_YUV444P:
        return SCE_JPEG_COLORSPACE_YUV444;
    case COLORSPACE_YUV422P:
        return SCE_JPEG_COLORSPACE_YUV422;
    case COLORSPACE_YUV420P:
        return SCE_JPEG_COLORSPACE_YUV420;
    default:
        return SCE_JPEG_COLORSPACE_UNKNOWN;
    }
}

DecoderColorSpace convert_color_space_jpeg_to_decoder(SceJpegColorSpace color_space) {
    switch (color_space) {
    case SCE_JPEG_COLORSPACE_GRAYSCALE:
        return COLORSPACE_GRAYSCALE;
    case SCE_JPEG_COLORSPACE_YUV444:
        return COLORSPACE_YUV444P;
    case SCE_JPEG_COLORSPACE_YUV422:
        return COLORSPACE_YUV422P;
    case SCE_JPEG_COLORSPACE_YUV420:
        return COLORSPACE_YUV420P;
    default:
        return COLORSPACE_UNKNOWN;
    }
}

EXPORT(int, sceJpegCreateSplitDecoder) {
    TRACY_FUNC(sceJpegCreateSplitDecoder);
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegCsc) {
    TRACY_FUNC(sceJpegCsc);
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegDecodeMJpeg, const unsigned char *pJpeg, SceSize isize, uint8_t *pRGBA, SceSize osize,
    int decodeMode, void *pTempBuffer, SceSize tempBufferSize, void *pCoefBuffer, SceSize coefBufferSize) {
    TRACY_FUNC(sceJpegDecodeMJpeg, pJpeg, isize, pRGBA, osize, decodeMode, pTempBuffer, tempBufferSize, pCoefBuffer, coefBufferSize);

    if (decodeMode & SCE_JPEG_MJPEG_DOWNSCALE_ANY)
        return STUBBED("JPEG downscaling is not implemented");

    const auto state = emuenv.kernel.obj_store.get<MJpegState>();

    DecoderSize size = {};

    // the yuv data will always be smaller than the rgba data, so osize is an upper bound
    std::vector<uint8_t> temporary(osize);

    state->decoder->send(pJpeg, isize);
    state->decoder->receive(temporary.data(), &size);

    convert_yuv_to_rgb(temporary.data(), pRGBA, size.width, size.height, state->decoder->get_color_space());

    // Top 16 bits = width, bottom 16 bits = height.
    return (size.width << 16u) | size.height;
}

EXPORT(int, sceJpegDecodeMJpegYCbCr, const uint8_t *pJpeg, SceSize isize,
    uint8_t *pYCbCr, SceSize osize, int decodeMode, void *pCoefBuffer, SceSize coefBufferSize) {
    TRACY_FUNC(sceJpegDecodeMJpegYCbCr, pJpeg, isize, pYCbCr, osize, decodeMode, pCoefBuffer, coefBufferSize);

    if (decodeMode & SCE_JPEG_MJPEG_DOWNSCALE_ANY)
        return STUBBED("JPEG downscaling is not implemented");

    const auto state = emuenv.kernel.obj_store.get<MJpegState>();

    DecoderSize size = {};

    state->decoder->send(pJpeg, isize);
    state->decoder->receive(pYCbCr, &size);

    // Top 16 bits = width, bottom 16 bits = height.
    return (size.width << 16u) | size.height;
}

EXPORT(int, sceJpegDeleteSplitDecoder) {
    TRACY_FUNC(sceJpegDeleteSplitDecoder);
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegFinishMJpeg) {
    TRACY_FUNC(sceJpegFinishMJpeg);
    emuenv.kernel.obj_store.erase<MJpegState>();

    return 0;
}

EXPORT(int, sceJpegGetOutputInfo, const uint8_t *pJpeg, SceSize isize,
    SceJpegFormat format, int decodeMode, SceJpegOutputInfo *output) {
    TRACY_FUNC(sceJpegGetOutputInfo, pJpeg, isize, format, decodeMode, output);
    const auto state = emuenv.kernel.obj_store.get<MJpegState>();

    if (!pJpeg || !output || !isize)
        return RET_ERROR(SCE_JPEG_ERROR_INVALID_POINTER);

    memset(output, 0, sizeof(SceJpegOutputInfo));

    if (format != SCE_JPEG_NO_CSC_OUTPUT && format != SCE_JPEG_PIXEL_RGBA8888 && format != SCE_JPEG_PIXEL_BGRA8888)
        return RET_ERROR(SCE_JPEG_ERROR_INVALID_COLOR_FORMAT);

    if (format == SCE_JPEG_PIXEL_BGRA8888)
        return STUBBED("SCE_JPEG_PIXEL_BGRA8888 is not implemented");

    DecoderSize size = {};

    state->decoder->send(pJpeg, isize);
    state->decoder->receive(nullptr, &size);

    output->width = size.width;
    output->height = size.height;
    output->color_space = convert_color_space_decoder_to_jpeg(state->decoder->get_color_space());
    output->pitch[0] = {
        .x = size.width,
        .y = size.height
    };
    // Should be 0 most of the time but I believe it causes more problems
    // for it to be 0 when it shouldn't than the opposite
    output->coef_buffer_size = 0x100;
    if (format == SCE_JPEG_PIXEL_RGBA8888) {
        output->output_size = size.width * size.height * 4;
        // put something greater than 0
        output->temp_buffer_size = 0x100;
    } else {
        switch (output->color_space) {
        case SCE_JPEG_COLORSPACE_GRAYSCALE:
            output->output_size = size.width * size.height;
            break;
        case SCE_JPEG_COLORSPACE_YUV444:
            output->output_size = size.width * size.height * 3;
            output->pitch[1] = output->pitch[0];
            output->pitch[2] = output->pitch[0];
            break;
        case SCE_JPEG_COLORSPACE_YUV422:
            output->output_size = size.width * size.height * 2;
            output->pitch[1] = {
                .x = size.width / 2,
                .y = size.height
            };
            output->pitch[2] = output->pitch[1];
            break;
        case SCE_JPEG_COLORSPACE_YUV420:
            output->output_size = size.width * size.height * 3 / 2;
            output->pitch[1] = {
                .x = size.width / 2,
                .y = size.height / 2
            };
            output->pitch[2] = output->pitch[1];
            break;
        }
    }

    if (decodeMode & SCE_JPEG_MJPEG_DOWNSCALE_ANY)
        return STUBBED("JPEG downscaling is not implemented");

    return 0;
}

EXPORT(int, sceJpegInitMJpeg, int maxSplitDecoder) {
    TRACY_FUNC(sceJpegInitMJpeg, maxSplitDecoder);
    if (maxSplitDecoder > 0)
        STUBBED("Ignoring non-zero maxSplitDecoder parameter");

    emuenv.kernel.obj_store.create<MJpegState>();
    const auto state = emuenv.kernel.obj_store.get<MJpegState>();
    state->decoder = std::make_shared<MjpegDecoderState>();

    return 0;
}

EXPORT(int, sceJpegInitMJpegWithParam, const SceJpegMJpegInitInfo *param) {
    TRACY_FUNC(sceJpegInitMJpegWithParam, param);
    return CALL_EXPORT(sceJpegInitMJpeg, param->maxSplitDecoder);
}

EXPORT(int, sceJpegMJpegCsc, uint8_t *pRGBA, const uint8_t *pYCbCr,
    uint32_t xysize, int iFrameWidth, int colorOption, int sampling) {
    TRACY_FUNC(sceJpegMJpegCsc, pRGBA, pYCbCr, xysize, iFrameWidth, colorOption, sampling);

    if (colorOption & SCE_JPEG_COLORSPACE_BT601) {
        STUBBED("Unhandled BT601 color conversion");
        colorOption &= ~SCE_JPEG_COLORSPACE_BT601;
    }

    if (colorOption != SCE_JPEG_PIXEL_RGBA8888 && colorOption != SCE_JPEG_PIXEL_BGRA8888)
        return RET_ERROR(SCE_JPEG_ERROR_INVALID_COLOR_FORMAT);

    if (colorOption == SCE_JPEG_PIXEL_BGRA8888)
        return STUBBED("SCE_JPEG_PIXEL_BGRA8888 is not implemented");

    uint32_t width = xysize >> 16;
    uint32_t height = xysize & 0xFFFF;

    if (width != iFrameWidth)
        STUBBED("Mismatch between width and frameWidth, image will look corrupted");

    convert_yuv_to_rgb(pYCbCr, pRGBA, width, height, convert_color_space_jpeg_to_decoder(static_cast<SceJpegColorSpace>(SCE_JPEG_COLORSPACE_YUV | sampling)));

    return 0;
}

EXPORT(int, sceJpegSplitDecodeMJpeg) {
    TRACY_FUNC(sceJpegSplitDecodeMJpeg);
    return UNIMPLEMENTED();
}
