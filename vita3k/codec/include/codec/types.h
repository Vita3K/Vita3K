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

#pragma once

#include <mem/ptr.h>

#include <util/types.h>

enum SceJpegEncErrorCode {
    SCE_JPEGENC_ERROR_IMAGE_SIZE = 0x80650200,
    SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER = 0x80650201,
    SCE_JPEGENC_ERROR_INVALID_COMPRATIO = 0x80650202,
    SCE_JPEGENC_ERROR_INVALID_PIXELFORMAT = 0x80650203,
    SCE_JPEGENC_ERROR_INVALID_HEADER_MODE = 0x80650204,
    SCE_JPEGENC_ERROR_INVALID_POINTER = 0x80650205,
    SCE_JPEGENC_ERROR_NOT_PHY_CONTINUOUS_MEMORY = 0x80650206
};

enum SceJpegEncoderPixelFormat : int32_t {
    SCE_JPEGENC_PIXEL_RGBA8888 = 0,
    SCE_JPEGENC_PIXEL_BGRA8888 = 4,
    SCE_JPEGENC_PIXEL_YCBCR420 = 8,
    SCE_JPEGENC_PIXEL_YCBCR422 = 9,
    SCE_JPEGENC_PITCH_HW_CSC = 16,
};

enum SceJpegEncoderHeaderMode : int32_t {
    SCE_JPEGENC_HEADER_MODE_JPEG = 0,
    SCE_JPEGENC_HEADER_MODE_MJPEG = 1
};

enum SceJpegEncoderInitParamOption : int32_t {
    SCE_JPEGENC_INIT_PARAM_OPTION_NONE = 0,
    SCE_JPEGENC_INIT_PARAM_OPTION_LPDDR2_MEMORY = 1
};

struct SceJpegEncoderInitParam {
    uint32_t size;
    int32_t inWidth;
    int32_t inHeight;
    int32_t pixelFormat;
    Ptr<uint8_t> outBuffer;
    uint32_t outSize;
    SceJpegEncoderInitParamOption option;
};
