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

enum {
    SCE_AVCENC_ERROR_INVALID_PARAM = 0x80621002,
    SCE_AVCENC_ERROR_INVALID_STATE = 0x80621004,
    SCE_AVCENC_ERROR_UNSUPPORT_IMAGE_SIZE = 0x80621005,
    SCE_AVCENC_ERROR_INVALID_POINTER = 0x80621009,
    SCE_AVCENC_ERROR_INVALID_ARGUMENT_SIZE = 0x8062100e,
};

struct AvcencCscFrame {
    uint32_t size;
    uint32_t pixel_type;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t options[16];
    Ptr<void> picture[2];
    uint32_t encode_mode;
};

static_assert(sizeof(AvcencCscFrame) == 48);

static std::optional<H264OutputFormat> avcenc_csc_format(uint32_t type) {
    switch (type & 0xff) {
    case 0x00: return H264OutputFormat::rgba;
    case 0x08: return H264OutputFormat::bgra;
    case 0x10: return H264OutputFormat::yuv420p;
    case 0x20: return H264OutputFormat::nv12;
    default: return std::nullopt;
    }
}

static bool avcenc_csc_surface(MemState &mem, const AvcencCscFrame &frame,
    H264OutputFormat format, VideoCscSurface &surface) {
    const bool rgb = format == H264OutputFormat::rgba || format == H264OutputFormat::bgra;
    const uint64_t pixels = static_cast<uint64_t>(frame.pitch) * frame.height;
    const uint64_t size = rgb ? pixels * 4 : pixels * 3 / 2;
    const Address address = frame.picture[0].address();
    const uint64_t end = static_cast<uint64_t>(address) + size;
    if (!address || end > 0xfffff000ULL || !is_valid_addr_range(mem, address, static_cast<Address>(end)))
        return false;
    surface = { { frame.picture[0].cast<uint8_t>().get(mem), static_cast<size_t>(size) },
        frame.width, frame.height, frame.pitch, format };
    return true;
}

EXPORT(int, sceAvcencCreateEncoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencCreateEncoderBasic) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencCreateEncoderInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencCsc, AvcencCscFrame *destination, const AvcencCscFrame *source) {
    if (!destination || !source)
        return RET_ERROR(SCE_AVCENC_ERROR_INVALID_POINTER);
    LOG_INFO_ONCE("AVC encoder CSC: source {:#x} {}x{} pitch {}, destination {:#x} {}x{} pitch {}",
        source->pixel_type, source->width, source->height, source->pitch,
        destination->pixel_type, destination->width, destination->height, destination->pitch);
    if (source->size != sizeof(AvcencCscFrame) || destination->size != sizeof(AvcencCscFrame))
        return RET_ERROR(SCE_AVCENC_ERROR_INVALID_ARGUMENT_SIZE);
    const auto src_format = avcenc_csc_format(source->pixel_type);
    const auto dst_format = avcenc_csc_format(destination->pixel_type);
    constexpr uint32_t supported_flags = 0x80010000;
    if (!src_format || !dst_format || (source->pixel_type & ~(supported_flags | 0xffu)) || (destination->pixel_type & ~(supported_flags | 0xffu))) {
        LOG_ERROR("Unsupported AVC encoder CSC formats {:#x} -> {:#x}", source->pixel_type, destination->pixel_type);
        return RET_ERROR(SCE_AVCENC_ERROR_INVALID_PARAM);
    }
    if (((source->width | source->height | source->pitch | destination->width | destination->height | destination->pitch) & 15) || !source->width || !source->height || !destination->width || !destination->height || source->width > source->pitch || destination->width > destination->pitch || source->width > 1920 || destination->width > 1920 || source->height > 1088 || destination->height > 1088 || source->pitch > 2048 || destination->pitch > 2048)
        return RET_ERROR(SCE_AVCENC_ERROR_UNSUPPORT_IMAGE_SIZE);
    const bool src_rgb = *src_format == H264OutputFormat::rgba || *src_format == H264OutputFormat::bgra;
    const bool dst_rgb = *dst_format == H264OutputFormat::rgba || *dst_format == H264OutputFormat::bgra;
    uint32_t coefficient = dst_rgb && (destination->pixel_type & 0x80000000u) ? destination->options[1] : 0;
    if (src_rgb && (source->pixel_type & 0x80000000u) && source->options[1])
        coefficient = source->options[1];
    if (coefficient > 2) {
        LOG_ERROR("Unsupported AVC encoder CSC coefficient {}", coefficient);
        return RET_ERROR(SCE_AVCENC_ERROR_INVALID_PARAM);
    }
    VideoCscSurface src{}, dst{};
    if (!avcenc_csc_surface(emuenv.mem, *source, *src_format, src) || !avcenc_csc_surface(emuenv.mem, *destination, *dst_format, dst))
        return RET_ERROR(SCE_AVCENC_ERROR_INVALID_POINTER);
    const uint8_t alpha = dst_rgb && (destination->pixel_type & 0x80000000u) ? destination->options[0] : 255;
    if (!video_convert_surface(src, dst, coefficient, alpha))
        return RET_ERROR(SCE_AVCENC_ERROR_INVALID_STATE);
    return 0;
}

EXPORT(int, sceAvcencDeleteEncoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencEncode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencEncodeFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencEncodeStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencGetNalUnit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencQueryEncoderMemSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencQueryEncoderMemSizeBasic) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencQueryEncoderMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencSetAvailablePreset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencSetEncoderParameter) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibrary) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibraryInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibraryWithUnmapMem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibraryWithUnmapMemInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencQueryMemSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencQueryMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencTermLibrary) {
    return UNIMPLEMENTED();
}
