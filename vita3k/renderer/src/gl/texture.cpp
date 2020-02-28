#include <renderer/functions.h>
#include <renderer/profile.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/log.h>

namespace renderer {
namespace gl {
namespace texture {

void bind_texture(GLTextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    glBindTexture(GL_TEXTURE_2D, cache.textures[0]);
    configure_bound_texture(gxm_texture);
    upload_bound_texture(gxm_texture, mem);
}

static bool can_texture_be_unswizzled_without_decode(SceGxmTextureBaseFormat fmt) {
    return (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P8 || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5 || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5 || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8 || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8);
}

void configure_bound_texture(const SceGxmTexture &gxm_texture) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const SceGxmTextureAddrMode uaddr = (SceGxmTextureAddrMode)(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = (SceGxmTextureAddrMode)(gxm_texture.vaddr_mode);
    const GLint *const swizzle = translate_swizzle(fmt);

    // TODO Support mip-mapping.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, gxm_texture.mip_count);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, translate_wrap_mode(uaddr));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, translate_wrap_mode(vaddr));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.min_filter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.mag_filter));
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    const GLenum internal_format = translate_internal_format(fmt);
    auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const GLenum format = translate_format(fmt);
    const GLenum type = translate_type(fmt);
    const bool is_swizzle = gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED;
    const auto base_fmt = gxm::get_base_format(fmt);

    size_t compressed_size = 0;
    uint32_t mip_index = 0;

    while (mip_index < gxm_texture.mip_count + 1 && width && height) {
        if (!is_swizzle && renderer::texture::is_compressed_format(base_fmt, width, height, compressed_size)) {
            glCompressedTexImage2D(GL_TEXTURE_2D, mip_index, internal_format, width, height, 0, static_cast<GLsizei>(compressed_size), nullptr);
        } else if (!is_swizzle || (is_swizzle && can_texture_be_unswizzled_without_decode(base_fmt))) {
            glTexImage2D(GL_TEXTURE_2D, mip_index, internal_format, width, height, 0, format, type, nullptr);
        } else {
            if (is_swizzle) {
                // Data feed will later be RGBA
                glTexImage2D(GL_TEXTURE_2D, mip_index, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }
        }

        mip_index++;
        width /= 2;
        height /= 2;
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
    }

    return 0;
}

void upload_bound_texture(const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const Ptr<uint8_t> data(gxm_texture.data_addr << 2);
    uint8_t *texture_data = data.get(mem);
    std::vector<uint8_t> texture_data_decompressed;
    std::vector<uint8_t> texture_pixels_lineared; // TODO Move to context to avoid frequent allocation?
    std::vector<uint32_t> palette_texture_pixels;
    std::vector<uint8_t> yuv_texture_pixels;

    const void *pixels = nullptr;

    size_t stride = 0;
    const auto base_format = gxm::get_base_format(fmt);
    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t bytes_per_pixel = (bpp + 7) >> 3;

    const bool is_swizzled = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED);
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !can_texture_be_unswizzled_without_decode(base_format);

    uint32_t mip_index = 0;
    size_t source_size = 0;

    while (mip_index < gxm_texture.mip_count + 1 && width && height) {
        pixels = texture_data;

        switch (gxm_texture.texture_type()) {
        case SCE_GXM_TEXTURE_SWIZZLED:
        case SCE_GXM_TEXTURE_TILED: {
            if (need_decompress_and_unswizzle_on_cpu) {
                // Must decompress them
                texture_data_decompressed.resize(width * height * 4);
                source_size = decompress_compressed_swizz_texture(base_format, texture_data_decompressed.data(), pixels, width, height);
                bytes_per_pixel = 4;
                bpp = 32;
                pixels = texture_data_decompressed.data();
            }

            // Convert data
            texture_pixels_lineared.resize(width * height * bytes_per_pixel);

            if (is_swizzled)
                renderer::texture::swizzled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), width, height,
                    static_cast<std::uint8_t>(bpp));
            else
                renderer::texture::tiled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), width, height,
                    static_cast<std::uint8_t>(bpp));

            pixels = texture_pixels_lineared.data();
            stride = width;

            if (need_decompress_and_unswizzle_on_cpu) {
                texture_data_decompressed.clear();
            }

            break;
        }

        default:
            pixels = texture_data;
            stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.

            break;
        }

        if (gxm::is_paletted_format(fmt)) {
            const auto base_format = gxm::get_base_format(fmt);

            const uint32_t *const palette_bytes = renderer::texture::get_texture_palette(gxm_texture, mem);
            palette_texture_pixels.resize(width * height * 4);
            if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
                renderer::texture::palette_texture_to_rgba_8(palette_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height, palette_bytes);
            } else {
                renderer::texture::palette_texture_to_rgba_4(palette_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height, palette_bytes);
            }
            pixels = palette_texture_pixels.data();
            stride = width;
        }

        if (gxm::is_yuv_format(fmt)) {
            switch (fmt) {
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1: {
                yuv_texture_pixels.resize(width * height * 3);
                renderer::texture::yuv420_texture_to_rgb(yuv_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height);
                pixels = yuv_texture_pixels.data();
                stride = width;
                break;
            }

            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC1:

            case SCE_GXM_TEXTURE_FORMAT_YUYV422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVYU422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_UYVY422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_VYUY422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUYV422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVYU422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_UYVY422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_VYUY422_CSC1:
                assert(false);
            default:
                assert(false);
            }
        }

        const GLenum format = translate_format(fmt);
        const GLenum type = translate_type(fmt);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(stride));

        if (need_decompress_and_unswizzle_on_cpu)
            glTexSubImage2D(GL_TEXTURE_2D, mip_index, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        else {
            size_t compressed_size = 0;
            if (renderer::texture::is_compressed_format(gxm::get_base_format(fmt), width, height, compressed_size)) {
                source_size = compressed_size;
                glCompressedTexSubImage2D(GL_TEXTURE_2D, mip_index, 0, 0, width, height, format, static_cast<GLsizei>(compressed_size), pixels);
            } else {
                source_size = (width * height * (bpp + 7)) >> 3;
                glTexSubImage2D(GL_TEXTURE_2D, mip_index, 0, 0, width, height, format, type, pixels);
            }
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        mip_index++;
        width /= 2;
        height /= 2;

        texture_data += source_size;
    }
}

} // namespace texture
} // namespace gl
} // namespace renderer
