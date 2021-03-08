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
void bind_texture(GLTextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);
    glBindTexture(GL_TEXTURE_2D, cache.textures[0]);
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
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, (gxm_texture.lod_bias - 31) / 8.f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, gxm_texture.lod_min0 | (gxm_texture.lod_min1 << 2));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.min_filter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.mag_filter));
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    const GLenum internal_format = translate_internal_format(fmt);
    auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const GLenum format = translate_format(fmt);
    const GLenum type = translate_type(fmt);
    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY);
    const auto base_fmt = gxm::get_base_format(fmt);

    size_t compressed_size = 0;
    uint32_t mip_index = 0;

    while (mip_index < gxm_texture.mip_count + 1 && width && height) {
        if (!is_swizzled && renderer::texture::is_compressed_format(base_fmt, width, height, compressed_size)) {
            glCompressedTexImage2D(GL_TEXTURE_2D, mip_index, internal_format, width, height, 0, static_cast<GLsizei>(compressed_size), nullptr);
        } else if (!is_swizzled || (is_swizzled && can_texture_be_unswizzled_without_decode(base_fmt))) {
            glTexImage2D(GL_TEXTURE_2D, mip_index, internal_format, width, height, 0, format, type, nullptr);
        } else {
            if (is_swizzled) {
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
    } else if ((fmt >= SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) && (fmt <= SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)) {
        // TODO, is not perfect for PVRT-II.
        pvr::PVRTDecompressPVRTC(data, (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP), width, height,
            (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP), reinterpret_cast<uint8_t *>(dest));
        // TODO, calcule return is not sur.
        return ((width + 3) / 4) * ((height + 3) / 4);
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
void decompress_packed_float_e5m9m9m9(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height) {
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

    size_t pixels_per_stride = 0;
    const auto base_format = gxm::get_base_format(fmt);
    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t bytes_per_pixel = (bpp + 7) >> 3;

    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY);
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !can_texture_be_unswizzled_without_decode(base_format);

    uint32_t mip_index = 0;
    uint32_t total_mip = gxm_texture.mip_count;
    size_t source_size = 0;
    std::uint32_t org_width = width;
    std::uint32_t org_height = height;

    if (texture_type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        total_mip = 0;
    }

    while (mip_index < total_mip + 1 && width && height) {
        pixels = texture_data;

        if (gxm::is_paletted_format(fmt)) {
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
        case SCE_GXM_TEXTURE_TILED:
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY: {
            if (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) {
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

            pixels_per_stride = width;

            if (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) {
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

        if (gxm::is_paletted_format(fmt)) {
            pixels_per_stride = width;
        }

        if (gxm::is_yuv_format(fmt)) {
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
                assert(false);
            default:
                assert(false);
            }
        }

        const GLenum format = translate_format(fmt);
        const GLenum type = translate_type(fmt);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_stride));

        if (need_decompress_and_unswizzle_on_cpu)
            glTexSubImage2D(GL_TEXTURE_2D, mip_index, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        else {
            size_t compressed_size = 0;
            if (renderer::texture::is_compressed_format(base_format, width, height, compressed_size)) {
                source_size = compressed_size;
                glCompressedTexSubImage2D(GL_TEXTURE_2D, mip_index, 0, 0, width, height, format, static_cast<GLsizei>(compressed_size), pixels);
            } else {
                source_size = (width * height * ((bpp + 7) >> 3));
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

    const bool is_swizzled = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED) || (gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY);
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !can_texture_be_unswizzled_without_decode(base_format);

    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    if (gxm::is_paletted_format(format)) {
        stride = width;
    }
    size_t size = (bpp * stride * height) / 8;

    if (gxm::is_yuv_format(format)) {
        bpp = 24;
        size = width * height * 3;
    } else if (need_decompress_and_unswizzle_on_cpu || gxm::is_paletted_format(format)) {
        bpp = 32;
        size = width * height * 4;
    }
    const size_t components = bpp / 8;

    g_pixels.resize(size);

    auto gl_format = texture::translate_format(format);
    auto gl_type = texture::translate_type(format);

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
