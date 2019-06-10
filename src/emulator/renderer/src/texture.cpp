#include "functions.h"

#include "profile.h"

// TODO: Remove dependance on this
#include <renderer/texture_cache_state.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/log.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <set>

namespace renderer {
namespace texture {

void bind_texture(TextureCacheState &cache, const emu::SceGxmTexture &gxm_texture, const MemState &mem) {
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
    const GLenum *const swizzle = translate_swizzle(fmt);

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

void upload_bound_texture(const emu::SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    const auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    const Ptr<const uint8_t> data(gxm_texture.data_addr << 2);
    const uint8_t *const texture_data = data.get(mem);
    std::vector<uint32_t> palette_texture_pixels; // TODO Move to context to avoid frequent allocation?

    const void *pixels = nullptr;
    size_t stride = 0;
    if (gxm::is_paletted_format(fmt)) {
        const auto base_format = gxm::get_base_format(fmt);
        const uint32_t *const palette_bytes = get_texture_palette(gxm_texture, mem);
        palette_texture_pixels.resize(width * height);
        if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
            palette_texture_to_rgba_8(palette_texture_pixels.data(), texture_data, width, height, palette_bytes);
        } else {
            palette_texture_to_rgba_4(palette_texture_pixels.data(), texture_data, width, height, palette_bytes);
        }
        pixels = palette_texture_pixels.data();
        stride = width;
    } else {
        pixels = texture_data;
        stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    }

    const GLenum format = translate_format(fmt);
    const GLenum type = translate_type(fmt);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<gl::GLint>(stride));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}


// Dumps bound texture to a file
void dump(const emu::SceGxmTexture &gxm_texture, const MemState &mem, const std::string &base_path, const std::string &title_id) {
    static uint32_t g_tex_index = 0;
    static std::vector<uint8_t> g_pixels; // re-use the same vector instead of allocating one every time
    static std::set<TextureCacheHash> g_dumped_hashes;

    const TextureCacheHash hash = hash_texture_data(gxm_texture, mem);

    if (g_dumped_hashes.find(hash) != g_dumped_hashes.end())
        return;

    g_dumped_hashes.insert(hash);

    const size_t width = gxm::get_width(&gxm_texture);
    const size_t height = gxm::get_height(&gxm_texture);

    const SceGxmTextureFormat format = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const size_t bpp = texture::bits_per_pixel(base_format);
    const size_t components = bpp / 8;
    const size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    const size_t size = (bpp * stride * height) / 8;

    g_pixels.resize(size);

    const auto gl_format = texture::translate_format(format);
    const auto gl_type = texture::translate_type(format);
    glGetTexImage(GL_TEXTURE_2D, 0, gl_format, gl_type, (void *)g_pixels.data());

    // TODO: Create the texturelog path elsewhere on init once and pass it here whole
    // TODO: Same for shaderlog path
    const fs::path texturelog_path{ fs::path(base_path) / "texturelog" / title_id };
    if (!fs::exists(texturelog_path))
        fs::create_directories(texturelog_path);

    const auto tex_filename = fmt::format("tex_{}_{:08X}.png", g_tex_index, hash);
    const auto filepath = texturelog_path / tex_filename;

    if (!stbi_write_png(filepath.string().c_str(), width, height, components, (void *)g_pixels.data(), stride * components))
        LOG_WARN("Failed to save texture: {}", filepath.string());

    ++g_tex_index;
}

} // namespace texture
} // namespace renderer
