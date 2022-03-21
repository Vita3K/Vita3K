// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <renderer/functions.h>
#include <renderer/profile.h>
#include <renderer/pvrt-dec.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/align.h>
#include <util/log.h>

#include <stb_image_write.h>

static constexpr bool log_parameter = false;

namespace renderer::gl {
namespace texture {
GLenum get_gl_texture_type(const SceGxmTexture &gxm_texture) {
    const std::uint32_t type = gxm_texture.texture_type();
    return ((type == SCE_GXM_TEXTURE_CUBE) || (type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
}

void bind_texture(GLTextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);
    glBindTexture(get_gl_texture_type(gxm_texture), cache.textures[0]);
    configure_bound_texture(gxm_texture);
    upload_bound_texture(gxm_texture, mem);
}

static bool can_texture_be_unswizzled_without_decode(SceGxmTextureBaseFormat fmt) {
    return (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8);
}

static bool is_block_compressed_format(SceGxmTextureBaseFormat fmt) {
    return (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC2
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC3
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP);
}

void configure_bound_texture(const SceGxmTexture &gxm_texture) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(fmt);
    const SceGxmTextureAddrMode uaddr = (SceGxmTextureAddrMode)(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = (SceGxmTextureAddrMode)(gxm_texture.vaddr_mode);
    const GLint *const swizzle = translate_swizzle(fmt);
    uint32_t mip_count = gxm_texture.true_mip_count();

    const GLenum texture_bind_type = get_gl_texture_type(gxm_texture);

    // TODO Support mip-mapping.
    if (mip_count)
        glTexParameteri(texture_bind_type, GL_TEXTURE_MAX_LEVEL, mip_count - 1);

    glTexParameteri(texture_bind_type, GL_TEXTURE_WRAP_S, translate_wrap_mode(uaddr));
    glTexParameteri(texture_bind_type, GL_TEXTURE_WRAP_T, translate_wrap_mode(vaddr));
    glTexParameterf(texture_bind_type, GL_TEXTURE_LOD_BIAS, (gxm_texture.lod_bias - 31) / 8.f);
    glTexParameteri(texture_bind_type, GL_TEXTURE_MIN_LOD, gxm_texture.lod_min0 | (gxm_texture.lod_min1 << 2));
    glTexParameteri(texture_bind_type, GL_TEXTURE_MIN_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.min_filter));
    glTexParameteri(texture_bind_type, GL_TEXTURE_MAG_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.mag_filter));
    glTexParameteriv(texture_bind_type, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    const GLenum internal_format = translate_internal_format(base_format);
    auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const GLenum format = translate_format(base_format);
    const GLenum type = translate_type(base_format);
    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const auto base_fmt = gxm::get_base_format(fmt);

    std::uint32_t org_width = width;
    std::uint32_t org_height = height;

    size_t compressed_size = 0;
    uint32_t mip_index = 0;

    // GXM's cube map index is same as OpenGL: right, left, top, bottom, front, back
    GLenum upload_type = GL_TEXTURE_2D;

    std::uint32_t face_total_count = 1;
    std::uint32_t face_iterated = 0;

    if ((texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
        upload_type = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        face_total_count = 6;
    }

    while (face_iterated < face_total_count && width && height) {
        if (!is_swizzled && renderer::texture::is_compressed_format(base_fmt, width, height, compressed_size)) {
            glCompressedTexImage2D(upload_type, mip_index, internal_format, width, height, 0, static_cast<GLsizei>(compressed_size), nullptr);
        } else if (!is_swizzled || (is_swizzled && can_texture_be_unswizzled_without_decode(base_fmt))) {
            glTexImage2D(upload_type, mip_index, internal_format, width, height, 0, format, type, nullptr);
        } else {
            if (is_swizzled) {
                // Data feed will later be RGBA
                glTexImage2D(upload_type, mip_index, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }
        }

        mip_index++;
        width /= 2;
        height /= 2;

        if (mip_index == mip_count) {
            mip_index = 0;
            face_iterated++;

            upload_type++;

            width = org_width;
            height = org_height;
        }
    }
}

/**
 * \brief Try to decompress texture to 32-bit RGBA.
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * height * 4 (bytes).
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Size of source taken.
 */
static size_t decompress_compressed_swizz_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const std::uint32_t width, const std::uint32_t height) {
    int ubc_type = 0;

    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        ubc_type = 1;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        ubc_type = 2;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        ubc_type = 3;
        break;

    default:
        break;
    }

    if (ubc_type) {
        renderer::texture::decompress_bc_swizz_image(width, height, reinterpret_cast<const std::uint8_t *>(data),
            reinterpret_cast<std::uint32_t *>(dest), ubc_type);
        return (((width + 3) / 4) * ((height + 3) / 4) * ((ubc_type > 1) ? 16 : 8));
    } else if ((fmt >= SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) && (fmt <= SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)) {
        pvr::PVRTDecompressPVRTC(data, (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP), width, height,
            (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP), reinterpret_cast<uint8_t *>(dest));

        const bool is_2bpp = (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP);

        const std::uint32_t num_xword = (width + (is_2bpp ? 7 : 3)) / (is_2bpp ? 8 : 4);
        const std::uint32_t num_yword = (height + 3) / 4;

        return num_xword * num_yword * 8;
    }

    return 0;
}

/**
 * \brief Try to decompress texture to 16-bit RGB floating point color.
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * height * 4 (bytes).
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Void.
 */
static void decompress_packed_float_e5m9m9m9(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height) {
    const uint32_t *in = reinterpret_cast<const uint32_t *>(data);
    uint16_t *out = reinterpret_cast<uint16_t *>(dest);

    for (uint32_t in_offset = 0, out_offset = 0; in_offset < width * height; ++in_offset) {
        const uint32_t packed = in[in_offset];
        const uint16_t exponent = static_cast<uint16_t>(packed >> 17);

        out[out_offset++] = exponent | ((packed & (0x1FF << 18)) >> 17);
        out[out_offset++] = exponent | ((packed & (0x1FF << 9)) >> 8);
        out[out_offset++] = exponent | ((packed & 0x1FF) << 1);
    }
}

static void convert_x8u24_to_u24x8(void *dest, const void *data, const uint32_t width, const uint32_t height, const size_t row_length_in_pixels) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[col];
            const uint32_t value = (src_value << 8) | (src_value >> 24);
            *dst++ = value;
        }

        src += row_length_in_pixels;
    }
}

static void convert_f32m_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height, const size_t row_length_in_pixels) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[col];
            const uint32_t value = src_value & 0x7FFFFFFF;
            *dst++ = value;
        }

        src += row_length_in_pixels;
    }
}

void upload_bound_texture(const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(fmt);
    auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const Ptr<uint8_t> data(gxm_texture.data_addr << 2);
    uint8_t *texture_data = data.get(mem);

    if (!texture_data) {
        return;
    }

    std::vector<uint8_t> texture_data_decompressed;
    std::vector<uint8_t> texture_pixels_lineared; // TODO Move to context to avoid frequent allocation?
    std::vector<uint32_t> palette_texture_pixels;
    std::vector<uint8_t> yuv_texture_pixels;

    const void *pixels = nullptr;

    size_t pixels_per_stride = 0;
    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t bytes_per_pixel = (bpp + 7) >> 3;

    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !can_texture_be_unswizzled_without_decode(base_format);

    uint32_t mip_index = 0;
    uint32_t total_mip = gxm_texture.true_mip_count();
    uint32_t face_uploaded_count = 0;
    uint32_t face_total_count;
    size_t source_size = 0;
    size_t total_source_so_far = 0;

    // Modified during decompression
    std::uint32_t org_width = width;
    std::uint32_t org_height = height;

    std::uint32_t org_width_const = width;
    std::uint32_t org_height_const = height;

    std::uint32_t face_align_bytes = 4;

    if (texture_type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        total_mip = 1;
    }

    // GXM's cube map index is same as OpenGL: right, left, top, bottom, front, back
    GLenum upload_type = GL_TEXTURE_2D;

    face_total_count = 1;

    if ((texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
        upload_type = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        face_total_count = 6;

        const bool twok_align_cond1 = ((width >= 32) && (height >= 32) && ((bytes_per_pixel == 1) || (is_block_compressed_format(base_format))));
        const bool twok_align_cond2 = ((width >= 16) && (height >= 16) && ((bytes_per_pixel == 2) || (bytes_per_pixel == 4)));
        const bool twok_align_cond3 = ((width >= 8) && (height >= 8) && (bytes_per_pixel == 8));

        if (twok_align_cond1 || twok_align_cond2 || twok_align_cond3) {
            face_align_bytes = 2048;
        }
    }

    while ((face_uploaded_count < face_total_count) && width && height) {
        pixels = texture_data;

        if (gxm::is_paletted_format(base_format)) {
            palette_texture_pixels.resize(width * height * 4);
            if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
                renderer::texture::palette_texture_to_rgba_8(palette_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height, renderer::texture::get_texture_palette(gxm_texture, mem));
            } else {
                renderer::texture::palette_texture_to_rgba_4(reinterpret_cast<uint32_t *>(palette_texture_pixels.data()),
                    reinterpret_cast<const uint8_t *>(pixels), width, height, renderer::texture::get_texture_palette(gxm_texture, mem));
            }
            pixels = palette_texture_pixels.data();
            bytes_per_pixel = 4;
            bpp = 32;
        }

        switch (texture_type) {
        case SCE_GXM_TEXTURE_SWIZZLED:
        case SCE_GXM_TEXTURE_CUBE:
        case SCE_GXM_TEXTURE_TILED:
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
        case SCE_GXM_TEXTURE_CUBE_ARBITRARY: {
            org_width = width;
            org_height = height;

            if ((texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
                width = nearest_power_of_two(width);
                height = nearest_power_of_two(height);
            }

            if (need_decompress_and_unswizzle_on_cpu) {
                // Must decompress them
                texture_data_decompressed.resize(width * height * 4);
                source_size = decompress_compressed_swizz_texture(base_format, texture_data_decompressed.data(), pixels, width, height);
                bytes_per_pixel = 4;
                bpp = 32;
                pixels = texture_data_decompressed.data();
            }

            pixels_per_stride = width;

            switch (base_format) {
            case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
            case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
            case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
            case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
                break;
            case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
                texture_data_decompressed.resize(width * height * 6);
                decompress_packed_float_e5m9m9m9(base_format, texture_data_decompressed.data(), pixels, width, height);
                pixels = texture_data_decompressed.data();
                break;
            case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
                // X8 = [24-31], D24 = [0-23], technically this is GL_UNSIGNED_INT_24_8_REV which does not exist
                // TODO: Requires shader to convert the normalized value read by GL to unsigned int. Just multiply by 2^24-1 when reading and you're done.
                texture_data_decompressed.resize(width * height * 4);
                convert_x8u24_to_u24x8(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride);
                pixels = texture_data_decompressed.data();
                break;
            case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
                // Convert F32M to F32
                texture_data_decompressed.resize(width * height * 4);
                convert_f32m_to_f32(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride);
                pixels = texture_data_decompressed.data();
                break;
            default:
                // Convert data
                texture_pixels_lineared.resize(width * height * bytes_per_pixel);

                if (is_swizzled)
                    renderer::texture::swizzled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), width, height,
                        static_cast<std::uint8_t>(bpp));
                else
                    renderer::texture::tiled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), width, height,
                        static_cast<std::uint8_t>(bpp));

                pixels = texture_pixels_lineared.data();

                if (need_decompress_and_unswizzle_on_cpu)
                    texture_data_decompressed.clear();
                break;
            }

            if ((texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
                width = org_width;
                height = org_height;
            }

            break;
        }
        case SCE_GXM_TEXTURE_LINEAR:
            pixels_per_stride = (width + 7) & ~7;

            break;
        case SCE_GXM_TEXTURE_LINEAR_STRIDED:
            pixels_per_stride = gxm::get_stride_in_bytes(&gxm_texture) / bytes_per_pixel;

            break;
        default:
            LOG_ERROR("Unimplemented Texture type: {} ", log_hex(gxm_texture.texture_type()));
            pixels_per_stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.

            break;
        }

        if (gxm::is_paletted_format(base_format)) {
            pixels_per_stride = width;
        }

        if (gxm::is_yuv_format(base_format)) {
            switch (fmt) {
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC1: {
                yuv_texture_pixels.resize(width * height * 3);
                renderer::texture::yuv420_texture_to_rgb(yuv_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height);
                pixels = yuv_texture_pixels.data();
                pixels_per_stride = width;
                break;
            }

            case SCE_GXM_TEXTURE_FORMAT_YUYV422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVYU422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_UYVY422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_VYUY422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUYV422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVYU422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_UYVY422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_VYUY422_CSC1:
                LOG_ERROR("Yuv Texture format not implemented: {}", fmt);
                assert(false);
            default:
                assert(false);
            }
        }

        const GLenum format = translate_format(base_format);
        const GLenum type = translate_type(base_format);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_stride));

        if (need_decompress_and_unswizzle_on_cpu)
            glTexSubImage2D(upload_type, mip_index, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        else {
            size_t compressed_size = 0;
            if (renderer::texture::is_compressed_format(base_format, width, height, compressed_size)) {
                source_size = compressed_size;
                glCompressedTexSubImage2D(upload_type, mip_index, 0, 0, width, height, format, static_cast<GLsizei>(compressed_size), pixels);
            } else {
                source_size = (width * height * ((bpp + 7) >> 3));
                glTexSubImage2D(upload_type, mip_index, 0, 0, width, height, format, type, pixels);
            }
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        mip_index++;
        width /= 2;
        height /= 2;

        texture_data += source_size;
        total_source_so_far += source_size;

        if (mip_index == total_mip) {
            mip_index = 0;
            face_uploaded_count++;

            width = org_width_const;
            height = org_height_const;

            upload_type++;

            size_t source_unaligned_size = total_source_so_far;
            total_source_so_far = align(total_source_so_far, face_align_bytes);

            texture_data += total_source_so_far - source_unaligned_size;
        }
    }
}

// Dumps bound texture to a file
void dump(const SceGxmTexture &gxm_texture, const MemState &mem, const std::string &parameter_name, const std::string &base_path, const std::string &title_id, Sha256Hash program_hash) {
    static uint32_t g_tex_index = 0;
    static std::vector<uint8_t> g_pixels; // re-use the same vector instead of allocating one every time
    static std::map<TextureCacheHash, uint32_t> g_dumped_hashes;

    int tex_index = g_tex_index;

    const TextureCacheHash hash = renderer::texture::hash_texture_data(gxm_texture, mem);

    if (g_dumped_hashes.find(hash) != g_dumped_hashes.end()) {
        if (log_parameter && parameter_name != "") {
            LOG_TRACE("Setting {} of {} by texture {}", parameter_name, hex(program_hash).data(), g_dumped_hashes[hash]);
        }
        return;
    } else {
        g_dumped_hashes.emplace(hash, tex_index);
        ++g_tex_index;
    }

    const size_t width = gxm::get_width(&gxm_texture);
    const size_t height = gxm::get_height(&gxm_texture);

    const SceGxmTextureFormat format = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);

    const bool is_swizzled = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED) || (gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE) || (gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !can_texture_be_unswizzled_without_decode(base_format);

    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    if (gxm::is_paletted_format(base_format)) {
        stride = width;
    }
    size_t size = (bpp * stride * height) / 8;

    if (gxm::is_yuv_format(base_format)) {
        bpp = 24;
        size = width * height * 3;
    } else if (need_decompress_and_unswizzle_on_cpu || gxm::is_paletted_format(base_format)) {
        bpp = 32;
        size = width * height * 4;
    }
    const size_t components = bpp / 8;

    g_pixels.resize(size);

    auto gl_format = texture::translate_format(base_format);
    auto gl_type = texture::translate_type(base_format);

    if (need_decompress_and_unswizzle_on_cpu) {
        gl_format = GL_RGBA;
        gl_type = GL_UNSIGNED_BYTE;
    }
    glGetTexImage(GL_TEXTURE_2D, 0, gl_format, gl_type, (void *)g_pixels.data());

    // TODO: Create the texturelog path elsewhere on init once and pass it here whole
    // TODO: Same for shaderlog path
    const fs::path texturelog_path{ fs::path(base_path) / "texturelog" / title_id };
    if (!fs::exists(texturelog_path))
        fs::create_directories(texturelog_path);

    const auto tex_filename = fmt::format("tex_{}_{:08X}_{}.png", tex_index, hash, hex(program_hash).data());
    const auto filepath = texturelog_path / tex_filename;

    if (!stbi_write_png(filepath.string().c_str(), width, height, components, (void *)g_pixels.data(), stride * components))
        LOG_WARN("Failed to save texture: {}", filepath.string());
}

} // namespace texture
} // namespace renderer::gl
