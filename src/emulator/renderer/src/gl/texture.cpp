#include <renderer/functions.h>
#include <renderer/profile.h>

#include "functions.h"
#include "types.h"

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/log.h>

namespace renderer::gl {
namespace texture {

void bind_texture(GLTextureCacheState &cache, const emu::SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    glBindTexture(GL_TEXTURE_2D, cache.textures[0]);
    configure_bound_texture(gxm_texture);
    upload_bound_texture(gxm_texture, mem);
}

void configure_bound_texture(const emu::SceGxmTexture &gxm_texture) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const SceGxmTextureAddrMode uaddr = (SceGxmTextureAddrMode)(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = (SceGxmTextureAddrMode)(gxm_texture.vaddr_mode);
    const GLint *const swizzle = translate_swizzle(fmt);

    // TODO Support mip-mapping.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, translate_wrap_mode(uaddr));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, translate_wrap_mode(vaddr));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.min_filter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, translate_minmag_filter((SceGxmTextureFilter)gxm_texture.mag_filter));
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    const GLenum internal_format = translate_internal_format(fmt);
    const auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    const auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const GLenum format = translate_format(fmt);
    const GLenum type = translate_type(fmt);

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
}

static bool can_texture_be_unswizzled_without_decode(SceGxmTextureBaseFormat fmt) {
    return (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P8 || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5 ||
        fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5 || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8 ||
        fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8);
}

void upload_bound_texture(const emu::SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    const auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const Ptr<uint8_t> data(gxm_texture.data_addr << 2);
    uint8_t *texture_data = data.get(mem);
    std::vector<uint8_t> texture_pixels_lineared; // TODO Move to context to avoid frequent allocation?
    std::vector<uint32_t> palette_texture_pixels;

    const void *pixels = nullptr;
    
    size_t stride = 0;
    const auto base_format = gxm::get_base_format(fmt);
    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t bytes_per_pixel = (bpp + 7) >> 3;

        switch (gxm_texture.texture_type()) {
        case SCE_GXM_TEXTURE_SWIZZLED:
        case SCE_GXM_TEXTURE_TILED: {
            const bool is_swizzled = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_SWIZZLED);

            if (is_swizzled && !can_texture_be_unswizzled_without_decode(base_format))
                break;

            // Convert data
            texture_pixels_lineared.resize(width * height * bytes_per_pixel);

            if (is_swizzled)
                renderer::texture::swizzled_texture_to_linear_texture(texture_pixels_lineared.data(), texture_data, width, height, bpp);
            else
                renderer::texture::tiled_texture_to_linear_texture(texture_pixels_lineared.data(), texture_data, width, height, bpp);

            pixels = texture_pixels_lineared.data();
            stride = width;
            texture_data = texture_pixels_lineared.data();
        
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
            renderer::texture::palette_texture_to_rgba_8(reinterpret_cast<uint32_t*>(palette_texture_pixels.data()),
                texture_data, width, height, palette_bytes);
        } else {
            renderer::texture::palette_texture_to_rgba_4(reinterpret_cast<uint32_t*>(palette_texture_pixels.data()),
                texture_data, width, height, palette_bytes);
        }
        pixels = palette_texture_pixels.data();
        stride = width;
    }

    const GLenum format = translate_format(fmt);
    const GLenum type = translate_type(fmt);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(stride));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

} // namespace texture
} // namespace renderer::gl
