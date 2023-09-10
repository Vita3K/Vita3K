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
    uint32_t size;
    int32_t decoder_count;
    int32_t options;
};

struct SceJpegPitch {
    uint32_t x;
    uint32_t y;
};

enum SceJpegColorSpace : int32_t {
    SCE_JPEG_COLORSPACE_UNKNOWN = 0x00000,
    SCE_JPEG_COLORSPACE_GRAYSCALE = 0x10000,
    SCE_JPEG_COLORSPACE_YUV = 0x20000,
    SCE_JPEG_COLORSPACE_YUV444 = 0x20101,
    SCE_JPEG_COLORSPACE_YUV422 = 0x20102,
    SCE_JPEG_COLORSPACE_YUV420 = 0x20202,
};

struct SceJpegOutputInfo {
    SceJpegColorSpace color_space;
    uint16_t width;
    uint16_t height;
    uint32_t output_size;
    uint32_t unknown1;
    uint32_t unknown2;
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
    int decodeMode, uint8_t *pTempBuffer, SceSize tempBufferSize, void *pCoefBuffer, SceSize coefBufferSize) {
    TRACY_FUNC(sceJpegDecodeMJpeg, pJpeg, isize, pRGBA, osize, decodeMode, pTempBuffer, tempBufferSize, pCoefBuffer, coefBufferSize);

    const auto state = emuenv.kernel.obj_store.get<MJpegState>();

    DecoderSize size = {};

    // allocates i think an extra frame but i want to be careful here
    std::vector<uint8_t> temporary(osize);

    state->decoder->send(pJpeg, isize);
    state->decoder->receive(temporary.data(), &size);

    convert_yuv_to_rgb(temporary.data(), pRGBA, size.width, size.height, state->decoder->get_color_space());

    // Top 16 bits = width, bottom 16 bits = height.
    return (size.width << 16u) | size.height;
}

EXPORT(int, sceJpegDecodeMJpegYCbCr, const uint8_t *jpeg_data, uint32_t jpeg_size,
    uint8_t *output, uint32_t output_size, int mode, void *buffer, uint32_t buffer_size) {
    TRACY_FUNC(sceJpegDecodeMJpegYCbCr, jpeg_data, jpeg_size, output, output_size, mode, buffer, buffer_size);
    const auto state = emuenv.kernel.obj_store.get<MJpegState>();

    DecoderSize size = {};

    state->decoder->send(jpeg_data, jpeg_size);
    state->decoder->receive(output, &size);

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

// TODO: There is still a lack of information about what each bit of mode does.
EXPORT(int, sceJpegGetOutputInfo, const uint8_t *jpeg_data, uint32_t jpeg_size,
    int32_t format, int32_t mode, SceJpegOutputInfo *output) {
    TRACY_FUNC(sceJpegGetOutputInfo, jpeg_data, jpeg_size, format, mode, output);
    const auto state = emuenv.kernel.obj_store.get<MJpegState>();

    if (!jpeg_data || !output) {
        return SCE_JPEG_ERROR_INVALID_POINTER;
    }
    if (!jpeg_size) {
        return SCE_JPEG_ERROR_OUT_OF_MEMORY;
    }
    // If format is 0, the image is RGBA; if -1, it is YUV. Otherwise, the color format is incorrect.
    if (format != 0 && format != -1) {
        return SCE_JPEG_ERROR_INVALID_COLOR_FORMAT;
    }
    // The third bit of mode assumes that the format requires RGBA.
    if ((mode & 4) && (format != 0)) {
        return SCE_JPEG_ERROR_INVALID_COLOR_FORMAT;
    }

    DecoderSize size = {};

    state->decoder->send(jpeg_data, jpeg_size);
    state->decoder->receive(nullptr, &size);

    output->width = size.width;
    output->height = size.height;
    output->color_space = convert_color_space_decoder_to_jpeg(state->decoder->get_color_space());
    if (format == 0) {
        output->output_size = size.width * size.height * 4;
    } else {
        switch (output->color_space) {
        case SCE_JPEG_COLORSPACE_GRAYSCALE:
            output->output_size = size.width * size.height;
            break;
        case SCE_JPEG_COLORSPACE_YUV444:
            output->output_size = size.width * size.height * 3;
            break;
        case SCE_JPEG_COLORSPACE_YUV422:
            output->output_size = size.width * size.height * 2;
            break;
        case SCE_JPEG_COLORSPACE_YUV420:
            output->output_size = size.width * size.height * 3 / 2;
            break;
        }
    }

    // The 5-7 bits of mode are assumed the downscaling related flags.
    if (mode & 0x70) {
        return STUBBED("The handling of image downscaling in this function is not yet implemented.");
    }

    return 0;
}

// TODO: Decoder options are ignored for the time being.
EXPORT(int, sceJpegInitMJpeg, int32_t decoder_count) {
    TRACY_FUNC(sceJpegInitMJpeg, decoder_count);
    emuenv.kernel.obj_store.create<MJpegState>();
    const auto state = emuenv.kernel.obj_store.get<MJpegState>();
    state->decoder = std::make_shared<MjpegDecoderState>();

    return 0;
}

EXPORT(int, sceJpegInitMJpegWithParam, const SceJpegMJpegInitInfo *info) {
    TRACY_FUNC(sceJpegInitMJpegWithParam, info);
    emuenv.kernel.obj_store.create<MJpegState>();
    const auto state = emuenv.kernel.obj_store.get<MJpegState>();
    state->decoder = std::make_shared<MjpegDecoderState>();

    return 0;
}

EXPORT(int, sceJpegMJpegCsc, uint8_t *rgba, const uint8_t *yuv,
    int32_t size, int32_t image_width, int32_t format, int32_t sampling) {
    TRACY_FUNC(sceJpegMJpegCsc, rgba, yuv, size, image_width, format, sampling);
    uint32_t width = size >> 16u;
    uint32_t height = size & (~0u >> 16u);

    convert_yuv_to_rgb(yuv, rgba, width, height, convert_color_space_jpeg_to_decoder(static_cast<SceJpegColorSpace>(SCE_JPEG_COLORSPACE_YUV | sampling)));

    return 0;
}

EXPORT(int, sceJpegSplitDecodeMJpeg) {
    TRACY_FUNC(sceJpegSplitDecodeMJpeg);
    return UNIMPLEMENTED();
}
