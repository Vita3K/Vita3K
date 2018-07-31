#include "functions.h"

#include "profile.h"

#include <renderer/texture_cache_state.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <algorithm> // find
#include <cstring> // memcmp
#include <numeric> // accumulate

static bool operator==(const SceGxmTexture &a, const SceGxmTexture &b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

namespace renderer {
static size_t bits_per_pixel(SceGxmTextureBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        return 16;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
        return 32;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return 64;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return 16; // NOTE: I'm not sure this is right.
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return 24;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return 32;
    }

    return 0;
}

static GLenum translate_wrap_mode(SceGxmTextureAddrMode src) {
    R_PROFILE(__func__);

    switch (src) {
    case SCE_GXM_TEXTURE_ADDR_REPEAT:
        return GL_REPEAT;
    case SCE_GXM_TEXTURE_ADDR_CLAMP:
        return GL_CLAMP_TO_EDGE;
    case SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP:
        return GL_CLAMP_TO_EDGE; // FIXME: GL_MIRROR_CLAMP_TO_EDGE is not supported in OpenGL 4.1 core.
    case SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER:
        return GL_REPEAT; // FIXME: Is this correct?
    case SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER:
        return GL_CLAMP_TO_BORDER;
    case SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER:
        return GL_CLAMP_TO_BORDER; // FIXME: Is this correct?
    case SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER:
        return GL_CLAMP_TO_BORDER; // FIXME: Is this correct?
    default:
        LOG_WARN("Unsupported texture wrap mode translated: {}", log_hex(src));
        return GL_CLAMP_TO_EDGE;
    }
}

static GLenum translate_minmag_filter(SceGxmTextureFilter src) {
    R_PROFILE(__func__);

    switch (src) {
    case SCE_GXM_TEXTURE_FILTER_POINT:
        return GL_NEAREST;
    case SCE_GXM_TEXTURE_FILTER_LINEAR:
        return GL_LINEAR;
    default:
        LOG_WARN("Unsupported texture min/mag filter translated: {}", log_hex(src));
        return GL_LINEAR;
    }
}

static void palette_texture_to_rgba_4(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette) {
    LOG_WARN("4-bit palettes are not yet tested.");

    const size_t stride = ((width + 7) & ~7) / 2; // NOTE: This is correct only with linear textures.
    for (size_t y = 0; y < height; ++y) {
        uint32_t *const dst_row = &dst[y * width];
        const uint8_t *const src_row = &src[y * stride];
        for (size_t x = 0; x < width; ++x) {
            const uint8_t lohi = src_row[x / 2];
            const uint8_t lo = lohi & 0xf;
            const uint8_t hi = lohi >> 4;
            dst_row[x + 0] = palette[lo];
            dst_row[x + 1] = palette[hi];
        }
    }
}

static void palette_texture_to_rgba_8(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette) {
    const size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    for (size_t y = 0; y < height; ++y) {
        uint32_t *const dst_row = &dst[y * width];
        const uint8_t *const src_row = &src[y * stride];
        for (size_t x = 0; x < width; ++x) {
            dst_row[x] = palette[src_row[x]];
        }
    }
}

static const uint32_t *get_texture_palette(const SceGxmTexture &texture, const MemState &mem) {
    const Ptr<const uint32_t> palette_ptr(texture.palette_addr << 6);
    return palette_ptr.get(mem);
}

static TextureCacheHash hash_data(const void *data, size_t size) {
    const uint8_t *const begin = static_cast<const uint8_t *>(data);
    const uint8_t *const end = begin + size;
    return std::accumulate(begin, end, TextureCacheHash(0));
}

static TextureCacheHash hash_palette_data(const SceGxmTexture &texture, size_t count, const MemState &mem) {
    const uint32_t *const palette_bytes = get_texture_palette(texture, mem);
    const TextureCacheHash palette_hash = hash_data(palette_bytes, count * sizeof(uint32_t));
    return palette_hash;
}

static TextureCacheHash hash_texture_data(const SceGxmTexture &texture, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const size_t width = gxm::get_width(&texture);
    const size_t height = gxm::get_height(&texture);
    const size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    const size_t bpp = bits_per_pixel(base_format);
    const size_t size = (bpp * stride * height) / 8;
    const Ptr<const void> data(texture.data_addr << 2);
    const TextureCacheHash data_hash = hash_data(data.get(mem), size);

    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return data_hash ^ hash_palette_data(texture, 16, mem);
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return data_hash ^ hash_palette_data(texture, 256, mem);
    default:
        return data_hash;
    }
}

static size_t find_lru(const TextureCacheTimestamps &timestamps, TextureCacheTimestamp current_time) {
    R_PROFILE(__func__);

    uint64_t oldest_age = current_time - timestamps.front();
    size_t oldest_index = 0;

    for (size_t index = 1; index < timestamps.size(); ++index) {
        const uint64_t age = current_time - timestamps[index];
        if (age > oldest_age) {
            oldest_age = age;
            oldest_index = index;
        }
    }

    return oldest_index;
}

static void configure_bound_texture(const SceGxmTexture &gxm_texture) {
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
    const unsigned int width = gxm::get_width(&gxm_texture);
    const unsigned int height = gxm::get_height(&gxm_texture);
    const GLenum format = translate_format(fmt);
    const GLenum type = translate_type(fmt);

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
}

static void upload_bound_texture(const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const unsigned int width = gxm::get_width(&gxm_texture);
    const unsigned int height = gxm::get_height(&gxm_texture);
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

    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

bool init(TextureCacheState &cache) {
    return cache.textures.init(&glGenTextures, &glDeleteTextures);
}

void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem, bool enabled) {
    R_PROFILE(__func__);

    if (!enabled) {
        glBindTexture(GL_TEXTURE_2D, cache.textures[0]);
        configure_bound_texture(gxm_texture);
        upload_bound_texture(gxm_texture, mem);
        return;
    }

    size_t index = 0;
    bool configure = false;
    bool upload = false;
    // TODO Palettes are probably quicker to hash than texture data, so if we find games use palette animation this could be optimised:
    // Skip data hash if palette hashes differ.
    const TextureCacheHash hash = hash_texture_data(gxm_texture, mem);

    // Try to find GXM texture in cache.
    const TextureCacheGxmTextures::const_iterator gxm_begin = cache.gxm_textures.cbegin();
    const TextureCacheGxmTextures::const_iterator gxm_end = gxm_begin + cache.used;
    const TextureCacheGxmTextures::const_iterator cached_gxm_texture = std::find(gxm_begin, gxm_end, gxm_texture);
    if (cached_gxm_texture == gxm_end) {
        // Texture not found in cache.
        if (cache.used < TextureCacheSize) {
            // Cache is not full. Add texture to cache.
            index = cache.used;
            ++cache.used;
        } else {
            // Cache is full. Find least recently used texture.
            index = find_lru(cache.timestamps, cache.timestamp);
            LOG_DEBUG("Evicting texture {} (t = {}) from cache. Current t = {}.", index, cache.timestamps[index], cache.timestamp);
        }
        configure = true;
        upload = true;
        cache.gxm_textures[index] = gxm_texture;
    } else {
        // Texture is cached.
        index = cached_gxm_texture - gxm_begin;
        configure = false;
        upload = (hash != cache.hashes[index]);
    }

    const GLuint gl_texture = cache.textures[index];
    glBindTexture(GL_TEXTURE_2D, gl_texture);

    if (configure) {
        configure_bound_texture(gxm_texture);
    }
    if (upload) {
        upload_bound_texture(gxm_texture, mem);
        cache.hashes[index] = hash;
    }

    cache.timestamps[index] = cache.timestamp;
}
} // namespace renderer
