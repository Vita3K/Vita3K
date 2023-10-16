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

using namespace texture;

bool GLTextureCache::init(const bool hashless_texture_cache) {
    TextureCache::init(hashless_texture_cache);
    backend = Backend::OpenGL;

    return textures.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures));
}

void GLTextureCache::select(size_t index, const SceGxmTexture &texture) {
    const GLuint gl_texture = textures[index];
    glBindTexture(get_gl_texture_type(texture), gl_texture);
}

void GLTextureCache::configure_texture(const SceGxmTexture &gxm_texture) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(fmt);
    const SceGxmTextureAddrMode uaddr = (SceGxmTextureAddrMode)(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = (SceGxmTextureAddrMode)(gxm_texture.vaddr_mode);
    uint32_t width = gxm::get_width(gxm_texture);
    uint32_t height = gxm::get_height(gxm_texture);
    const GLint *const swizzle = translate_swizzle(fmt);
    uint32_t mip_count = renderer::texture::get_upload_mip(gxm_texture.true_mip_count(), width, height);

    const GLenum texture_bind_type = get_gl_texture_type(gxm_texture);

    const GLenum min_filter = translate_minmag_filter((SceGxmTextureFilter)gxm_texture.min_filter);
    const GLenum mag_filter = translate_minmag_filter((SceGxmTextureFilter)gxm_texture.mag_filter);

    // TODO Support mip-mapping.
    if (mip_count)
        glTexParameteri(texture_bind_type, GL_TEXTURE_MAX_LEVEL, mip_count - 1);

    glTexParameteri(texture_bind_type, GL_TEXTURE_WRAP_S, translate_wrap_mode(uaddr));
    glTexParameteri(texture_bind_type, GL_TEXTURE_WRAP_T, translate_wrap_mode(vaddr));
    glTexParameterf(texture_bind_type, GL_TEXTURE_LOD_BIAS, (static_cast<float>(gxm_texture.lod_bias) - 31.f) / 8.f);
    glTexParameteri(texture_bind_type, GL_TEXTURE_MIN_LOD, gxm_texture.lod_min0 | (gxm_texture.lod_min1 << 2));
    glTexParameteri(texture_bind_type, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(texture_bind_type, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteriv(texture_bind_type, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    // anisotropic filtering
    // when using nearest filter, disable anisotropy as the pixels can contain data other than color
    if (anisotropic_filtering > 1 && (min_filter != GL_NEAREST || mag_filter != GL_NEAREST))
        // we don't need to check for the existence of this extension because it is considered an ubiquitous extension
        // for now we apply anisotropic filtering to all textures
        glTexParameterf(texture_bind_type, GL_TEXTURE_MAX_ANISOTROPY_EXT, static_cast<float>(anisotropic_filtering));

    const GLenum internal_format = translate_internal_format(base_format);
    const GLenum format = translate_format(base_format);
    const GLenum type = translate_type(base_format);
    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const auto base_fmt = gxm::get_base_format(fmt);

    std::uint32_t org_width = width;
    std::uint32_t org_height = height;

    uint32_t mip_index = 0;

    bool compressed = gxm::is_bcn_format(base_fmt);

    // GXM's cube map index is same as OpenGL: right, left, top, bottom, front, back
    GLenum upload_type = GL_TEXTURE_2D;

    std::uint32_t face_total_count = 1;
    std::uint32_t face_iterated = 0;

    if ((texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
        upload_type = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        face_total_count = 6;
    }

    while (face_iterated < face_total_count && width && height) {
        if (compressed) {
            size_t compressed_size = renderer::texture::get_compressed_size(base_fmt, width, height);
            glCompressedTexImage2D(upload_type, mip_index, internal_format, width, height, 0, static_cast<GLsizei>(compressed_size), nullptr);
        } else {
            glTexImage2D(upload_type, mip_index, internal_format, width, height, 0, format, type, nullptr);
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

void GLTextureCache::upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) {
    R_PROFILE(__func__);

    GLenum upload_type = GL_TEXTURE_2D;
    if (face > 0)
        // GXM's cube map index is same as OpenGL: right, left, top, bottom, front, back
        upload_type = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (face - 1);

    if (gxm::is_bcn_format(base_format)) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_stride));

        const GLint block_size = (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC4 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC4)
            ? 8
            : 16;
        glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, block_size);
        glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 4);
        glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 4);

        const GLenum format = translate_format(base_format);
        size_t compressed_size = renderer::texture::get_compressed_size(base_format, pixels_per_stride, height);
        glCompressedTexSubImage2D(upload_type, mip_index, 0, 0, width, height, format, static_cast<GLsizei>(compressed_size), pixels);

        glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
        glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
        glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    } else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_stride));

        const GLenum format = translate_format(base_format);
        const GLenum type = translate_type(base_format);
        glTexSubImage2D(upload_type, mip_index, 0, 0, width, height, format, type, pixels);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
}

namespace texture {

GLenum get_gl_texture_type(const SceGxmTexture &gxm_texture) {
    const std::uint32_t type = gxm_texture.texture_type();
    return ((type == SCE_GXM_TEXTURE_CUBE) || (type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
}

void bind_texture_without_cache(GLTextureCache &cache, const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);
    glBindTexture(get_gl_texture_type(gxm_texture), cache.textures[0]);
    cache.select(0, gxm_texture);
    cache.configure_texture(gxm_texture);
    cache.upload_texture(gxm_texture, mem);
}

// Dumps bound texture to a file
void dump(const SceGxmTexture &gxm_texture, const MemState &mem, const std::string &parameter_name, const std::string &log_path, const std::string &title_id, Sha256Hash program_hash) {
    static uint32_t g_tex_index = 0;
    static std::vector<uint8_t> g_pixels; // re-use the same vector instead of allocating one every time
    static std::map<uint64_t, uint64_t> g_dumped_hashes;

    int tex_index = g_tex_index;

    const uint32_t texture_size = gxm::texture_size_first_mip(gxm_texture);
    const uint64_t hash = renderer::texture::hash_texture_data(gxm_texture, texture_size, mem);

    if (g_dumped_hashes.contains(hash)) {
        if (log_parameter && parameter_name != "") {
            LOG_TRACE("Setting {} of {} by texture {}", parameter_name, hex_string(program_hash), g_dumped_hashes[hash]);
        }
        return;
    } else {
        g_dumped_hashes.emplace(hash, tex_index);
        ++g_tex_index;
    }

    const size_t width = gxm::get_width(gxm_texture);
    const size_t height = gxm::get_height(gxm_texture);

    size_t size = width * height * 4;

    g_pixels.resize(size);

    GLenum gl_format = GL_RGBA;
    GLenum gl_type = GL_UNSIGNED_BYTE;

    glGetnTexImage(GL_TEXTURE_2D, 0, gl_format, gl_type, size, (void *)g_pixels.data());

    // TODO: Create the texturelog path elsewhere on init once and pass it here whole
    // TODO: Same for shaderlog path
    const fs::path texturelog_path{ fs::path(log_path) / "texturelog" / title_id };
    if (!fs::exists(texturelog_path))
        fs::create_directories(texturelog_path);

    const auto tex_filename = fmt::format("tex_{}_{:08X}_{}.png", tex_index, hash, hex_string(program_hash));
    const auto filepath = texturelog_path / tex_filename;

    if (!stbi_write_png(filepath.string().c_str(), static_cast<int>(width), static_cast<int>(height), 4, (void *)g_pixels.data(), static_cast<int>(width * 4)))
        LOG_WARN("Failed to save texture: {}", filepath.string());
}

} // namespace texture
} // namespace renderer::gl
